/**
 * Copyright (c) 2013 Anup Patel.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file fat_main.c
 * @author Anup Patel. (anup@brainfault.org)
 * @brief FAT filesystem driver
 *
 * FAT (or File Allocation Table) filesystem is a well-known filesystem. 
 * It is widely used in pluggable devices such as USB Pen driver, MMC/SD cards.
 *
 * For more info, visit http://en.wikipedia.org/wiki/File_Allocation_Table
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <libs/vfs.h>

#include "fat_control.h"
#include "fat_node.h"

#define MODULE_DESC			"FAT Filesystem Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VFS_IPRIORITY + 1)
#define	MODULE_INIT			fatfs_init
#define	MODULE_EXIT			fatfs_exit

/* 
 * Mount point operations 
 */

static int fatfs_mount(struct mount *m, const char *dev, u32 flags)
{
	int rc;
	struct fatfs_control *ctrl;
	struct fatfs_node *root;

	ctrl = vmm_zalloc(sizeof(struct fatfs_control));
	if (!ctrl) {
		return VMM_ENOMEM;
	}

	/* Setup control info */
	rc = fatfs_control_init(ctrl, m->m_dev);
	if (rc) {
		goto fail;
	}

	/* Get the root fatfs node */
	root = m->m_root->v_data;
	rc = fatfs_node_init(ctrl, root);
	if (rc) {
		goto fail;
	}

	/* Handcraft the root fatfs node */
	root->parent = NULL;
	root->parent_dent_off = 0;
	root->parent_dent_len = sizeof(struct fat_dirent);
	memset(&root->parent_dent, 0, sizeof(struct fat_dirent));
	root->parent_dent.file_attributes = FAT_DIRENT_SUBDIR;
	if (ctrl->type == FAT_TYPE_32) {
		root->first_cluster = ctrl->first_root_cluster;
		root->parent_dent.first_cluster_hi = 
				__le16((root->first_cluster >> 16) & 0xFFFF);
		root->parent_dent.first_cluster_lo = 
					__le16(root->first_cluster & 0xFFFF);
		root->parent_dent.file_size = 0x0;
	} else {
		root->first_cluster = 0x0;
		root->parent_dent.first_cluster_hi = 0x0;
		root->parent_dent.file_size = 0x0;
	}
	root->parent_dent_dirty = FALSE;

	/* Handcraft the root vfs node */
	m->m_root->v_type = VDIR;
	m->m_root->v_mode =  S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
	m->m_root->v_ctime = 0;
	m->m_root->v_atime = 0;
	m->m_root->v_mtime = 0;
	m->m_root->v_size = fatfs_node_get_size(root);

	/* Save control as mount point data */
	m->m_data = ctrl;

	return VMM_OK;

fail:
	vmm_free(ctrl);
	return rc;
}

static int fatfs_unmount(struct mount *m)
{
	int rc;
	struct fatfs_control *ctrl = m->m_data;

	if (!ctrl) {
		return VMM_EFAIL;
	}

	rc = fatfs_control_exit(ctrl);

	vmm_free(ctrl);

	return rc;
}

static int fatfs_msync(struct mount *m)
{
	struct fatfs_control *ctrl = m->m_data;

	if (!ctrl) {
		return VMM_EFAIL;
	}

	return fatfs_control_sync(ctrl);
}

static int fatfs_vget(struct mount *m, struct vnode *v)
{
	int rc;
	struct fatfs_node *node;
	struct fatfs_control *ctrl = m->m_data;

	node = vmm_zalloc(sizeof(struct fatfs_node));
	if (!node) {
		return VMM_ENOMEM;
	}

	rc = fatfs_node_init(ctrl, node);

	v->v_data = node;

	return rc;
}

static int fatfs_vput(struct mount *m, struct vnode *v)
{
	int rc;
	struct fatfs_node *node = v->v_data;

	if (!node) {
		return VMM_EFAIL;
	}

	rc = fatfs_node_exit(node);

	vmm_free(node);

	return rc;
}

/* 
 * Vnode operations 
 */

static size_t fatfs_read(struct vnode *v, loff_t off, void *buf, size_t len)
{
	struct fatfs_node *node = v->v_data;
	u32 filesize = fatfs_node_get_size(node);

	if (filesize <= (u32)off) {
		return 0;
	}

	if (filesize < (u32)(len + off)) {
		len = filesize - off;
	}

	return fatfs_node_read(node, (u32)off, len, buf);
}

static size_t fatfs_write(struct vnode *v, loff_t off, void *buf, size_t len)
{
	u32 wlen;
	struct fatfs_node *node = v->v_data;

	wlen = fatfs_node_write(node, (u32)off, len, buf);

	/* Size and mtime might have changed */
	v->v_size = fatfs_node_get_size(node);
	v->v_mtime = fatfs_pack_timestamp(node->parent_dent.lmodify_date_year,
					  node->parent_dent.lmodify_date_month,
					  node->parent_dent.lmodify_date_day,
					  node->parent_dent.lmodify_time_hours,
					  node->parent_dent.lmodify_time_minutes,
					  node->parent_dent.lmodify_time_seconds);

	return wlen;
}

static int fatfs_truncate(struct vnode *v, loff_t off)
{
	int rc;
	struct fatfs_node *node = v->v_data;

	if ((u32)off <= fatfs_node_get_size(node)) {
		return VMM_EFAIL;
	}

	rc = fatfs_node_truncate(node, (u32)off);
	if (rc) {
		return rc;
	}

	/* Size and mtime might have changed */
	v->v_size = fatfs_node_get_size(node);
	v->v_mtime = fatfs_pack_timestamp(node->parent_dent.lmodify_date_year,
					  node->parent_dent.lmodify_date_month,
					  node->parent_dent.lmodify_date_day,
					  node->parent_dent.lmodify_time_hours,
					  node->parent_dent.lmodify_time_minutes,
					  node->parent_dent.lmodify_time_seconds);

	return VMM_OK;
}

static int fatfs_sync(struct vnode *v)
{
	struct fatfs_node *node = v->v_data;

	if (!node) {
		return VMM_EFAIL;
	}

	return fatfs_node_sync(node);
}

static int fatfs_readdir(struct vnode *dv, loff_t off, struct dirent *d)
{
	struct fatfs_node *dnode = dv->v_data;

	return fatfs_node_read_dirent(dnode, off, d);
}

static int fatfs_lookup(struct vnode *dv, const char *name, struct vnode *v)
{
	int rc;
	u32 dent_off, dent_len;
	struct fat_dirent dent;
	struct fatfs_node *node = v->v_data;
	struct fatfs_node *dnode = dv->v_data;

	rc = fatfs_node_find_dirent(dnode, name, &dent, &dent_off, &dent_len);
	if (rc) {
		return rc;
	}

	node->ctrl = dnode->ctrl;
	node->parent = dnode;
	node->parent_dent_off = dent_off;
	node->parent_dent_len = dent_len;
	memcpy(&node->parent_dent, &dent, sizeof(struct fat_dirent));
	if (dnode->ctrl->type == FAT_TYPE_32) {
		node->first_cluster = __le16(dent.first_cluster_hi);
		node->first_cluster = node->first_cluster << 16;
	} else {
		node->first_cluster = 0;
	}
	node->first_cluster |= __le16(dent.first_cluster_lo);

	v->v_mode = 0;

	if (dent.file_attributes & FAT_DIRENT_SUBDIR) {
		v->v_type = VDIR;
		v->v_mode |= S_IFDIR;
	} else {
		v->v_type = VREG;
		v->v_mode |= S_IFREG;
	}

	v->v_mode |= (S_IRWXU | S_IRWXG | S_IRWXO);
	if (dent.file_attributes & FAT_DIRENT_READONLY) {
		v->v_mode &= ~(S_IWUSR| S_IWGRP | S_IWOTH);
	}

	v->v_ctime = fatfs_pack_timestamp(dent.create_date_year,
					  dent.create_date_month,
					  dent.create_date_day,
					  dent.create_time_hours,
					  dent.create_time_minutes,
					  dent.create_time_seconds);
	v->v_atime = fatfs_pack_timestamp(dent.laccess_date_year,
					  dent.laccess_date_month,
					  dent.laccess_date_day,
					  0, 0, 0);
	v->v_mtime = fatfs_pack_timestamp(dent.lmodify_date_year,
					  dent.lmodify_date_month,
					  dent.lmodify_date_day,
					  dent.lmodify_time_hours,
					  dent.lmodify_time_minutes,
					  dent.lmodify_time_seconds);

	v->v_size = fatfs_node_get_size(node);

	return VMM_OK;
}

/* FIXME: */
static int fatfs_create(struct vnode *dv, const char *filename, u32 mode)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int fatfs_remove(struct vnode *dv, struct vnode *v, const char *name)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int fatfs_rename(struct vnode *sv, const char *sname, struct vnode *v,
			 struct vnode *dv, const char *dname)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int fatfs_mkdir(struct vnode *dv, const char *name, u32 mode)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int fatfs_rmdir(struct vnode *dv, struct vnode *v, const char *name)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int fatfs_chmod(struct vnode *v, u32 mode)
{
	return VMM_EFAIL;
}

/* fatfs filesystem */
static struct filesystem fatfs = {
	.name		= "fat",

	/* Mount point operations */
	.mount		= fatfs_mount,
	.unmount	= fatfs_unmount,
	.msync		= fatfs_msync,
	.vget		= fatfs_vget,
	.vput		= fatfs_vput,

	/* Vnode operations */
	.read		= fatfs_read,
	.write		= fatfs_write,
	.truncate	= fatfs_truncate,
	.sync		= fatfs_sync,
	.readdir	= fatfs_readdir,
	.lookup		= fatfs_lookup,
	.create		= fatfs_create,
	.remove		= fatfs_remove,
	.rename		= fatfs_rename,
	.mkdir		= fatfs_mkdir,
	.rmdir		= fatfs_rmdir,
	.chmod		= fatfs_chmod,
};

static int __init fatfs_init(void)
{
	return vfs_filesystem_register(&fatfs);
}

static void __exit fatfs_exit(void)
{
	vfs_filesystem_unregister(&fatfs);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
