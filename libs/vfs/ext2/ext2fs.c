/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file ext2fs.c
 * @author Anup Patel. (anup@brainfault.org)
 * @brief Ext2 filesystem driver
 *
 * Ext2 is a very widely used filesystem in all unix-like OSes such
 * as Linux, FreeBSD, NetBSD, etc.
 * 
 * For more info, visit http://www.nongnu.org/ext2-doc/ext2.html
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/vfs.h>

#define MODULE_DESC			"Ext2 Filesystem Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VFS_IPRIORITY + 1)
#define	MODULE_INIT			ext2fs_init
#define	MODULE_EXIT			ext2fs_exit

/* 
 * Mount point operations 
 */

/* FIXME: */
static int ext2fs_mount(struct mount *m, const char *dev, u32 flags)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int ext2fs_unmount(struct mount *m)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int ext2fs_sync(struct mount *m)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int ext2fs_vget(struct mount *m, struct vnode *v)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int ext2fs_vput(struct mount *m, struct vnode *v)
{
	return VMM_EFAIL;
}

/* 
 * Vnode operations 
 */

/* FIXME: */
static int ext2fs_open(struct vnode *v, struct file *f)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int ext2fs_close(struct vnode *v, struct file *f)
{
	return VMM_EFAIL;
}

/* FIXME: */
static size_t ext2fs_read(struct vnode *v, struct file *f, 
				void *buf, size_t len)
{
	return 0;
}

/* FIXME: */
static size_t ext2fs_write(struct vnode *v, struct file *f, 
				void *buf, size_t len)
{
	return 0;
}

/* FIXME: */
static bool ext2fs_seek(struct vnode *v, struct file *f, loff_t off)
{
	return FALSE;
}

/* FIXME: */
static int ext2fs_fsync(struct vnode *v, struct file *f)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int ext2fs_readdir(struct vnode *dv, struct file *f, struct dirent *d)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int ext2fs_lookup(struct vnode *dv, const char *name, struct vnode *v)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int ext2fs_create(struct vnode *dv, const char *filename, u32 mode)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int ext2fs_remove(struct vnode *dv, struct vnode *v, const char *name)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int ext2fs_rename(struct vnode *dv1, struct vnode *v1, const char *sname, 
			struct vnode *dv2, struct vnode *v2, const char *dname)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int ext2fs_mkdir(struct vnode *dv, const char *name, u32 mode)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int ext2fs_rmdir(struct vnode *dv, struct vnode *v, const char *name)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int ext2fs_getattr(struct vnode *v, struct vattr *a)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int ext2fs_setattr(struct vnode *v, struct vattr *a)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int ext2fs_truncate(struct vnode *v, loff_t off)
{
	return VMM_EFAIL;
}

/* ext2fs filesystem */
static struct filesystem ext2fs = {
	.name		= "ext2",

	/* Mount point operations */
	.mount		= ext2fs_mount,
	.unmount	= ext2fs_unmount,
	.sync		= ext2fs_sync,
	.vget		= ext2fs_vget,
	.vput		= ext2fs_vput,

	/* Vnode operations */
	.open		= ext2fs_open,
	.close		= ext2fs_close,
	.read		= ext2fs_read,
	.write		= ext2fs_write,
	.seek		= ext2fs_seek,
	.fsync		= ext2fs_fsync,
	.readdir	= ext2fs_readdir,
	.lookup		= ext2fs_lookup,
	.create		= ext2fs_create,
	.remove		= ext2fs_remove,
	.rename		= ext2fs_rename,
	.mkdir		= ext2fs_mkdir,
	.rmdir		= ext2fs_rmdir,
	.getattr	= ext2fs_getattr,
	.setattr	= ext2fs_setattr,
	.truncate	= ext2fs_truncate
};

static int __init ext2fs_init(void)
{
	return vfs_filesystem_register(&ext2fs);
}

static void __exit ext2fs_exit(void)
{
	vfs_filesystem_unregister(&ext2fs);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
