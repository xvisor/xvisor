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
 * @file ext4_main.c
 * @author Anup Patel. (anup@brainfault.org)
 * @brief Ext4 filesystem driver
 *
 * Ext2, Ext3, and Ext4 are widely used filesystems in all unix-like OSes
 * such as Linux, FreeBSD, NetBSD, etc.
 *
 * The Ext4 filesystem is backward compatible to Ext2 and Ext3. In fact,
 * Ext2 and Ext3 can be mounted directly as Ext4 with features disabled.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/vfs.h>

#include "ext4_control.h"
#include "ext4_node.h"

#define MODULE_DESC			"Ext4 Filesystem Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VFS_IPRIORITY + 1)
#define	MODULE_INIT			ext4fs_init
#define	MODULE_EXIT			ext4fs_exit

/* 
 * Mount point operations 
 */

int ext4fs_mount(struct mount *m, const char *dev, u32 flags)
{
	int rc;
	u16 rootmode;
	struct ext4fs_control *ctrl;
	struct ext4fs_node *root;

	ctrl = vmm_zalloc(sizeof(struct ext4fs_control));
	if (!ctrl) {
		return VMM_ENOMEM;
	}

	/* Setup control info */
	rc = ext4fs_control_init(ctrl, m->m_dev);
	if (rc) {
		goto fail;
	}

	/* Setup root node */
	root = m->m_root->v_data;
	rc = ext4fs_node_init(root);
	if (rc) {
		goto fail;
	}
	rc = ext4fs_node_load(ctrl, 2, root);
	if (rc) {
		goto fail;
	}

	rootmode = __le16(root->inode.mode);

	m->m_root->v_mode = 0;

	switch (rootmode & EXT2_S_IFMASK) {
	case EXT2_S_IFSOCK:
		m->m_root->v_type = VSOCK;
		m->m_root->v_mode |= S_IFSOCK;
		break;
	case EXT2_S_IFLNK:
		m->m_root->v_type = VLNK;
		m->m_root->v_mode |= S_IFLNK;
		break;
	case EXT2_S_IFREG:
		m->m_root->v_type = VREG;
		m->m_root->v_mode |= S_IFREG;
		break;
	case EXT2_S_IFBLK:
		m->m_root->v_type = VBLK;
		m->m_root->v_mode |= S_IFBLK;
		break;
	case EXT2_S_IFDIR:
		m->m_root->v_type = VDIR;
		m->m_root->v_mode |= S_IFDIR;
		break;
	case EXT2_S_IFCHR:
		m->m_root->v_type = VCHR;
		m->m_root->v_mode |= S_IFCHR;
		break;
	case EXT2_S_IFIFO:
		m->m_root->v_type = VFIFO;
		m->m_root->v_mode |= S_IFIFO;
		break;
	default:
		m->m_root->v_type = VUNK;
		break;
	};

	m->m_root->v_mode |= (rootmode & EXT2_S_IRUSR) ? S_IRUSR : 0;
	m->m_root->v_mode |= (rootmode & EXT2_S_IWUSR) ? S_IWUSR : 0;
	m->m_root->v_mode |= (rootmode & EXT2_S_IXUSR) ? S_IXUSR : 0;
	m->m_root->v_mode |= (rootmode & EXT2_S_IRGRP) ? S_IRGRP : 0;
	m->m_root->v_mode |= (rootmode & EXT2_S_IWGRP) ? S_IWGRP : 0;
	m->m_root->v_mode |= (rootmode & EXT2_S_IXGRP) ? S_IXGRP : 0;
	m->m_root->v_mode |= (rootmode & EXT2_S_IROTH) ? S_IROTH : 0;
	m->m_root->v_mode |= (rootmode & EXT2_S_IWOTH) ? S_IWOTH : 0;
	m->m_root->v_mode |= (rootmode & EXT2_S_IXOTH) ? S_IXOTH : 0;

	m->m_root->v_ctime = __le32(root->inode.ctime);
	m->m_root->v_atime = __le32(root->inode.atime);
	m->m_root->v_mtime = __le32(root->inode.mtime);

	m->m_root->v_size = ext4fs_node_get_size(root);

	/* Save control as mount point data */
	m->m_data = ctrl;

	return VMM_OK;

fail:
	vmm_free(ctrl);
	return rc;
}

static int ext4fs_unmount(struct mount *m)
{
	int rc;
	struct ext4fs_control *ctrl = m->m_data;

	if (!ctrl) {
		return VMM_EFAIL;
	}

	rc = ext4fs_control_exit(ctrl);

	vmm_free(ctrl);

	return rc;
}

static int ext4fs_msync(struct mount *m)
{
	struct ext4fs_control *ctrl = m->m_data;

	if (!ctrl) {
		return VMM_EFAIL;
	}

	return ext4fs_control_sync(ctrl);
}

static int ext4fs_vget(struct mount *m, struct vnode *v)
{
	int rc;
	struct ext4fs_node *node;

	node = vmm_zalloc(sizeof(struct ext4fs_node));
	if (!node) {
		return VMM_ENOMEM;
	}

	rc = ext4fs_node_init(node);

	v->v_data = node;

	return rc;
}

static int ext4fs_vput(struct mount *m, struct vnode *v)
{
	int rc;
	struct ext4fs_node *node = v->v_data;

	if (!node) {
		return VMM_EFAIL;
	}

	rc = ext4fs_node_exit(node);

	vmm_free(node);

	return rc;
}

/* 
 * Vnode operations 
 */

static size_t ext4fs_read(struct vnode *v, loff_t off, void *buf, size_t len)
{
	struct ext4fs_node *node = v->v_data;
	u64 filesize = ext4fs_node_get_size(node);

	if (filesize <= off) {
		return 0;
	}

	if (filesize < (len + off)) {
		len = filesize - off;
	}

	return ext4fs_node_read(node, off, len, buf);
}

static size_t ext4fs_write(struct vnode *v, loff_t off, void *buf, size_t len)
{
	u32 wlen;
	struct ext4fs_node *node = v->v_data;

	wlen = ext4fs_node_write(node, off, len, buf);

	/* Size and mtime might have changed */
	v->v_size = ext4fs_node_get_size(node);
	v->v_mtime = __le32(node->inode.mtime);

	return wlen;
}

static int ext4fs_truncate(struct vnode *v, loff_t off)
{
	int rc;
	struct ext4fs_node *node = v->v_data;
	u64 fileoff = off;
	u64 filesize = ext4fs_node_get_size(node);

	if (filesize <= fileoff) {
		return VMM_EFAIL;
	}

	rc = ext4fs_node_truncate(node, fileoff);
	if (rc) {
		return rc;
	}

	/* Size and mtime might have changed */
	v->v_size = ext4fs_node_get_size(node);
	v->v_mtime = __le32(node->inode.mtime);

	return VMM_OK;
}

static int ext4fs_sync(struct vnode *v)
{
	struct ext4fs_node *node = v->v_data;

	if (!node) {
		return VMM_EFAIL;
	}

	return ext4fs_node_sync(node);
}

static int ext4fs_readdir(struct vnode *dv, loff_t off, struct dirent *d)
{
	struct ext4fs_node *dnode = dv->v_data;

	return ext4fs_node_read_dirent(dnode, off, d);
}

static int ext4fs_lookup(struct vnode *dv, const char *name, struct vnode *v)
{
	int rc;
	u16 filemode;
	struct ext2_dirent dent;
	struct ext4fs_node *node = v->v_data;
	struct ext4fs_node *dnode = dv->v_data;

	rc = ext4fs_node_find_dirent(dnode, name, &dent);
	if (rc) {
		return rc;
	}

	rc = ext4fs_node_load(dnode->ctrl, __le32(dent.inode), node);
	if (rc) {
		return rc;
	}

	filemode = __le16(node->inode.mode);

	v->v_mode = 0;

	switch (filemode & EXT2_S_IFMASK) {
	case EXT2_S_IFSOCK:
		v->v_type = VSOCK;
		v->v_mode |= S_IFSOCK;
		break;
	case EXT2_S_IFLNK:
		v->v_type = VLNK;
		v->v_mode |= S_IFLNK;
		break;
	case EXT2_S_IFREG:
		v->v_type = VREG;
		v->v_mode |= S_IFREG;
		break;
	case EXT2_S_IFBLK:
		v->v_type = VBLK;
		v->v_mode |= S_IFBLK;
		break;
	case EXT2_S_IFDIR:
		v->v_type = VDIR;
		v->v_mode |= S_IFDIR;
		break;
	case EXT2_S_IFCHR:
		v->v_type = VCHR;
		v->v_mode |= S_IFCHR;
		break;
	case EXT2_S_IFIFO:
		v->v_type = VFIFO;
		v->v_mode |= S_IFIFO;
		break;
	default:
		v->v_type = VUNK;
		break;
	};

	v->v_mode |= (filemode & EXT2_S_IRUSR) ? S_IRUSR : 0;
	v->v_mode |= (filemode & EXT2_S_IWUSR) ? S_IWUSR : 0;
	v->v_mode |= (filemode & EXT2_S_IXUSR) ? S_IXUSR : 0;
	v->v_mode |= (filemode & EXT2_S_IRGRP) ? S_IRGRP : 0;
	v->v_mode |= (filemode & EXT2_S_IWGRP) ? S_IWGRP : 0;
	v->v_mode |= (filemode & EXT2_S_IXGRP) ? S_IXGRP : 0;
	v->v_mode |= (filemode & EXT2_S_IROTH) ? S_IROTH : 0;
	v->v_mode |= (filemode & EXT2_S_IWOTH) ? S_IWOTH : 0;
	v->v_mode |= (filemode & EXT2_S_IXOTH) ? S_IXOTH : 0;

	v->v_ctime = __le32(node->inode.ctime);
	v->v_atime = __le32(node->inode.atime);
	v->v_mtime = __le32(node->inode.mtime);

	v->v_size = ext4fs_node_get_size(node);

	return VMM_OK;
}

static int ext4fs_create(struct vnode *dv, const char *name, u32 mode)
{
	int rc;
	u16 filemode;
	u32 inode_no;
	struct ext2_dirent dent;
	struct ext2_inode inode;
	struct ext4fs_node *dnode = dv->v_data;

	rc = ext4fs_node_find_dirent(dnode, name, &dent);
	if (rc != VMM_ENOENT) {
		if (!rc) {
			return VMM_EEXIST;
		} else {
			return rc;
		}
	}

	rc = ext4fs_control_alloc_inode(dnode->ctrl, 
					dnode->inode_no, &inode_no);
	if (rc) {
		return rc;
	}

	memset(&inode, 0, sizeof(inode));

	inode.nlinks = __le16(1);

	filemode = EXT2_S_IFREG;
	filemode |= (mode & S_IRUSR) ? EXT2_S_IRUSR : 0;
	filemode |= (mode & S_IWUSR) ? EXT2_S_IWUSR : 0;
	filemode |= (mode & S_IXUSR) ? EXT2_S_IXUSR : 0;
	filemode |= (mode & S_IRGRP) ? EXT2_S_IRGRP : 0;
	filemode |= (mode & S_IWGRP) ? EXT2_S_IWGRP : 0;
	filemode |= (mode & S_IXGRP) ? EXT2_S_IXGRP : 0;
	filemode |= (mode & S_IROTH) ? EXT2_S_IROTH : 0;
	filemode |= (mode & S_IWOTH) ? EXT2_S_IWOTH : 0;
	filemode |= (mode & S_IXOTH) ? EXT2_S_IXOTH : 0;
	inode.mode = __le16(filemode);

	inode.mtime = __le32(ext4fs_current_timestamp());
	inode.atime = __le32(ext4fs_current_timestamp());
	inode.ctime = __le32(ext4fs_current_timestamp());

	rc = ext4fs_control_write_inode(dnode->ctrl, inode_no, &inode);
	if (rc) {
		ext4fs_control_free_inode(dnode->ctrl, inode_no);
		return rc;
	}

	rc = ext4fs_node_add_dirent(dnode, name, inode_no, 0);
	if (rc) {
		ext4fs_control_free_inode(dnode->ctrl, inode_no);
		return rc;
	}

	return VMM_OK;
}

static int ext4fs_remove(struct vnode *dv, struct vnode *v, const char *name)
{
	int rc;
	struct ext2_dirent dent;
	struct ext4fs_node *dnode = dv->v_data;
	struct ext4fs_node *node = v->v_data;

	rc = ext4fs_node_find_dirent(dnode, name, &dent);
	if (rc) {
		return rc;
	}

	if (__le32(dent.inode) != node->inode_no) {
		return VMM_EINVALID;
	}

	rc = ext4fs_node_del_dirent(dnode, name);
	if (rc) {
		return rc;
	}

	rc = ext4fs_control_free_inode(dnode->ctrl, node->inode_no);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

static int ext4fs_rename(struct vnode *sv, const char *sname, struct vnode *v,
			 struct vnode *dv, const char *dname)
{
	int rc;
	struct ext2_dirent dent;
	struct ext4fs_node *snode = sv->v_data;
	struct ext4fs_node *dnode = dv->v_data;

	rc = ext4fs_node_find_dirent(dnode, dname, &dent);
	if (rc != VMM_ENOENT) {
		if (!rc) {
			return VMM_EEXIST;
		} else {
			return rc;
		}
	}

	rc = ext4fs_node_find_dirent(snode, sname, &dent);
	if (rc) {
		return rc;
	}

	rc = ext4fs_node_del_dirent(snode, sname);
	if (rc) {
		return rc;
	}

	rc = ext4fs_node_add_dirent(dnode, dname, __le32(dent.inode), 0);
	if (rc) {
		return rc;
	}

	/* FIXME: node being renamed might be a directory, so
	 * we may need to update ".." entry in directory
	 */

	return VMM_OK;
}

static int ext4fs_mkdir(struct vnode *dv, const char *name, u32 mode)
{
	int rc;
	u16 filemode;
	u32 i, inode_no, blkno;
	char buf[64];
	struct ext2_dirent dent;
	struct ext2_inode inode;
	struct ext4fs_node *dnode = dv->v_data;
	struct ext4fs_control *ctrl = dnode->ctrl;

	rc = ext4fs_node_find_dirent(dnode, name, &dent);
	if (rc != VMM_ENOENT) {
		if (!rc) {
			return VMM_EEXIST;
		} else {
			return rc;
		}
	}

	rc = ext4fs_control_alloc_inode(ctrl, dnode->inode_no, &inode_no);
	if (rc) {
		return rc;
	}

	memset(&inode, 0, sizeof(inode));

	inode.nlinks = __le16(1);

	filemode = EXT2_S_IFDIR;
	filemode |= (mode & S_IRUSR) ? EXT2_S_IRUSR : 0;
	filemode |= (mode & S_IWUSR) ? EXT2_S_IWUSR : 0;
	filemode |= (mode & S_IXUSR) ? EXT2_S_IXUSR : 0;
	filemode |= (mode & S_IRGRP) ? EXT2_S_IRGRP : 0;
	filemode |= (mode & S_IWGRP) ? EXT2_S_IWGRP : 0;
	filemode |= (mode & S_IXGRP) ? EXT2_S_IXGRP : 0;
	filemode |= (mode & S_IROTH) ? EXT2_S_IROTH : 0;
	filemode |= (mode & S_IWOTH) ? EXT2_S_IWOTH : 0;
	filemode |= (mode & S_IXOTH) ? EXT2_S_IXOTH : 0;
	inode.mode = __le16(filemode);

	inode.mtime = __le32(ext4fs_current_timestamp());
	inode.atime = __le32(ext4fs_current_timestamp());
	inode.ctime = __le32(ext4fs_current_timestamp());

	rc = ext4fs_control_alloc_block(ctrl, dnode->inode_no, &blkno);
	if (rc) {
		goto failed1;
	}

	memset(buf, 0, sizeof(buf));
	for (i = 0; i < ctrl->block_size; i += sizeof(buf)) {
		rc = ext4fs_devwrite(ctrl, blkno, i, sizeof(buf), buf);
		if (rc) {
			goto failed2;
		}
	}
	i = 0;
	dent.inode = __le32(inode_no);
	dent.filetype = 0;
	dent.namelen = 1;
	dent.direntlen = __le16(sizeof(dent) + 1);
	memcpy(&buf[i], &dent, sizeof(dent));
	i += sizeof(dent);
	memcpy(&buf[i], ".", 1);
	i += 1;
	dent.inode = __le32(dnode->inode_no);
	dent.filetype = 0;
	dent.namelen = 2;
	dent.direntlen = __le16(ctrl->block_size - i);
	memcpy(&buf[i], &dent, sizeof(dent));
	i += sizeof(dent);
	memcpy(&buf[i], "..", 2);
	i += 2;

	rc = ext4fs_devwrite(ctrl, blkno, 0, i, buf);
	if (rc) {
		goto failed2;
	}

	inode.b.blocks.dir_blocks[0] = __le32(blkno);
	inode.size = __le32(ctrl->block_size);
	inode.blockcnt = __le32(ctrl->block_size >> EXT2_SECTOR_BITS);

	rc = ext4fs_control_write_inode(ctrl, inode_no, &inode);
	if (rc) {
		goto failed2;
	}

	rc = ext4fs_node_add_dirent(dnode, name, inode_no, 0);
	if (rc) {
		goto failed2;
	}

	return VMM_OK;

failed2:
	ext4fs_control_free_block(ctrl, blkno);
failed1:
	ext4fs_control_free_inode(ctrl, inode_no);
	return rc;
}

static int ext4fs_rmdir(struct vnode *dv, struct vnode *v, const char *name)
{
	int rc;
	struct ext2_dirent dent;
	struct ext4fs_node *dnode = dv->v_data;
	struct ext4fs_node *node = v->v_data;

	rc = ext4fs_node_find_dirent(dnode, name, &dent);
	if (rc) {
		return rc;
	}

	if (__le32(dent.inode) != node->inode_no) {
		return VMM_EINVALID;
	}

	rc = ext4fs_node_truncate(node, 0);
	if (rc) {
		return rc;
	}

	rc = ext4fs_node_del_dirent(dnode, name);
	if (rc) {
		return rc;
	}

	rc = ext4fs_control_free_inode(dnode->ctrl, node->inode_no);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

static int ext4fs_chmod(struct vnode *v, u32 mode)
{
	u16 filemode;
	struct ext4fs_node *node = v->v_data;

	filemode = 0;
	switch (v->v_type) {
	case VSOCK:
		filemode = EXT2_S_IFSOCK;
		break;
	case VLNK:
		filemode = EXT2_S_IFLNK;
		break;
	case VREG:
		filemode = EXT2_S_IFREG;
		break;
	case VBLK:
		filemode = EXT2_S_IFBLK;
		break;
	case VDIR:
		filemode = EXT2_S_IFDIR;
		break;
	case VCHR:
		filemode = EXT2_S_IFCHR;
		break;
	case VFIFO:
		filemode = EXT2_S_IFIFO;
		break;
	default:
		filemode = 0;
		break;
	};

	filemode |= (mode & S_IRUSR) ? EXT2_S_IRUSR : 0;
	filemode |= (mode & S_IWUSR) ? EXT2_S_IWUSR : 0;
	filemode |= (mode & S_IXUSR) ? EXT2_S_IXUSR : 0;
	filemode |= (mode & S_IRGRP) ? EXT2_S_IRGRP : 0;
	filemode |= (mode & S_IWGRP) ? EXT2_S_IWGRP : 0;
	filemode |= (mode & S_IXGRP) ? EXT2_S_IXGRP : 0;
	filemode |= (mode & S_IROTH) ? EXT2_S_IROTH : 0;
	filemode |= (mode & S_IWOTH) ? EXT2_S_IWOTH : 0;
	filemode |= (mode & S_IXOTH) ? EXT2_S_IXOTH : 0;

	node->inode.mode = __le16(filemode);
	node->inode.atime = __le32(ext4fs_current_timestamp());
	node->inode_dirty = TRUE;
	
	v->v_mode &= ~(S_IRWXU|S_IRWXG|S_IRWXO);
	v->v_mode |= mode;

	return VMM_OK;
}

/* ext4fs filesystem */
static struct filesystem ext4fs = {
	.name		= "ext4",

	/* Mount point operations */
	.mount		= ext4fs_mount,
	.unmount	= ext4fs_unmount,
	.msync		= ext4fs_msync,
	.vget		= ext4fs_vget,
	.vput		= ext4fs_vput,

	/* Vnode operations */
	.read		= ext4fs_read,
	.write		= ext4fs_write,
	.truncate	= ext4fs_truncate,
	.sync		= ext4fs_sync,
	.readdir	= ext4fs_readdir,
	.lookup		= ext4fs_lookup,
	.create		= ext4fs_create,
	.remove		= ext4fs_remove,
	.rename		= ext4fs_rename,
	.mkdir		= ext4fs_mkdir,
	.rmdir		= ext4fs_rmdir,
	.chmod		= ext4fs_chmod,
};

static int __init ext4fs_init(void)
{
	return vfs_filesystem_register(&ext4fs);
}

static void __exit ext4fs_exit(void)
{
	vfs_filesystem_unregister(&ext4fs);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
