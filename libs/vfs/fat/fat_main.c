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

	/* Setup root node */
	root = m->m_root->v_data;
	rc = fatfs_node_init(root);
	if (rc) {
		goto fail;
	}
	root->ctrl = ctrl;
	root->parent = NULL;
	root->parent_dirent_off = 0;
	root->parent_dirent_len = 0;
	memset(&root->dirent, 0, sizeof(struct fat_dirent));
	if (ctrl->type == FAT_TYPE_32) {
		root->first_cluster = ctrl->first_root_cluster;
	} else {
		root->first_cluster = 0;
	}

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

	node = vmm_zalloc(sizeof(struct fatfs_node));
	if (!node) {
		return VMM_ENOMEM;
	}

	rc = fatfs_node_init(node);

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
	u64 filesize = fatfs_node_get_size(node);

	if (filesize <= off) {
		return 0;
	}

	if (filesize < (len + off)) {
		len = filesize - off;
	}

	return fatfs_node_read(node, off, len, buf);
}

/* FIXME: */
static size_t fatfs_write(struct vnode *v, loff_t off, void *buf, size_t len)
{
	return 0;
}

/* FIXME: */
static int fatfs_truncate(struct vnode *v, loff_t off)
{
	return VMM_EFAIL;
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
	u32 i, rlen, len;
	char lname[VFS_MAX_NAME];
	struct fat_dirent dent;
	struct fat_longname lfn;
	struct fatfs_node *dnode = dv->v_data;
	u64 fileoff = off;

	if (umod64(fileoff, sizeof(struct fat_dirent))) {
		return VMM_EINVALID;
	}

	memset(lname, 0, sizeof(lname));
	d->d_reclen = 0;

	do {
		rlen = fatfs_node_read(dnode, fileoff, 
				sizeof(struct fat_dirent), (char *)&dent);
		if (rlen != sizeof(struct fat_dirent)) {
			return VMM_EIO;
		}

		if (dent.dos_file_name[0] == 0x0) {
			return VMM_ENOENT;
		}

		d->d_reclen += sizeof(struct fat_dirent);
		fileoff += sizeof(struct fat_dirent);

		if ((dent.dos_file_name[0] == 0xE5) ||
		    (dent.dos_file_name[0] == 0x2E)) {
			continue;
		}

		if (dent.file_attributes == FAT_LONGNAME_ATTRIBUTE) {
			memcpy(&lfn, &dent, sizeof(struct fat_longname));
			if (FAT_LONGNAME_LASTSEQ(lfn.seqno)) {
				memset(lname, 0, sizeof(lname));
				lfn.seqno = FAT_LONGNAME_SEQNO(lfn.seqno);
			}
			if ((lfn.seqno < FAT_LONGNAME_MINSEQ) ||
			    (FAT_LONGNAME_MAXSEQ < lfn.seqno)) {
				continue;
			}
			len = (lfn.seqno - 1) * 13;
			lname[len + 0] = (char)__le16(lfn.name_utf16_1[0]);
			lname[len + 1] = (char)__le16(lfn.name_utf16_1[1]);
			lname[len + 2] = (char)__le16(lfn.name_utf16_1[2]);
			lname[len + 3] = (char)__le16(lfn.name_utf16_1[3]);
			lname[len + 4] = (char)__le16(lfn.name_utf16_1[4]);
			lname[len + 5] = (char)__le16(lfn.name_utf16_2[0]);
			lname[len + 6] = (char)__le16(lfn.name_utf16_2[1]);
			lname[len + 7] = (char)__le16(lfn.name_utf16_2[2]);
			lname[len + 8] = (char)__le16(lfn.name_utf16_2[3]);
			lname[len + 9] = (char)__le16(lfn.name_utf16_2[4]);
			lname[len + 10] = (char)__le16(lfn.name_utf16_2[5]);
			lname[len + 11] = (char)__le16(lfn.name_utf16_3[0]);
			lname[len + 12] = (char)__le16(lfn.name_utf16_3[1]);
			continue;
		}

		if (dent.file_attributes & FAT_DIRENT_VOLLABLE) {
			continue;
		}

		if (!strlen(lname)) {
			i = 8;
			while (i && (dent.dos_file_name[i-1] == ' ')) {
				dent.dos_file_name[i-1] = '\0';
				i--;
			}
			i = 3;
			while (i && (dent.dos_extension[i-1] == ' ')) {
				dent.dos_extension[i-1] = '\0';
				i--;
			}
			memcpy(lname, dent.dos_file_name, 8);
			if (dent.dos_extension[0] != '\0') {
				len = strlen(lname);
				lname[len] = '.';
				lname[len + 1] = dent.dos_extension[0];
				lname[len + 2] = dent.dos_extension[1];
				lname[len + 3] = dent.dos_extension[2];
				lname[len + 4] = '\0';
			}
		}

		strncpy(d->d_name, lname, VFS_MAX_NAME);

		break;
	} while (1);

	d->d_off = off;

	if (dent.file_attributes & FAT_DIRENT_SUBDIR) {
		d->d_type = DT_DIR;
	} else {
		d->d_type = DT_REG;
	}

	return VMM_OK;
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
	node->parent_dirent_off = dent_off;
	node->parent_dirent_len = dent_len;
	memcpy(&node->dirent, &dent, sizeof(struct fat_dirent));
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
