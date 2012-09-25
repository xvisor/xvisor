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
 * @file vfs.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Light-weight virtual filesystem interface
 */

#ifndef __VFS_H_
#define __VFS_H_

#include <block/vmm_blockdev.h>
#include <libs/list.h>

#define VFS_IPRIORITY		(VMM_BLOCKDEV_CLASS_IPRIORITY+1)
#define VFS_MAX_PATH		(256)
#define	VFS_MAX_NAME		(64)

/* file type bits */
#define	S_IFDIR			(1<<0)
#define	S_IFCHR			(1<<1)
#define	S_IFBLK			(1<<2)
#define	S_IFREG			(1<<3)
#define	S_IFLNK			(1<<4)
#define	S_IFIFO			(1<<5)
#define	S_IFSOCK		(1<<6)
#define	S_IFMT			(S_IFDIR|S_IFCHR|S_IFBLK|S_IFREG|S_IFLNK|S_IFIFO|S_IFSOCK)

#define S_ISDIR(mode)		((mode) & S_IFDIR )
#define S_ISCHR(mode)		((mode) & S_IFCHR )
#define S_ISBLK(mode)		((mode) & S_IFBLK )
#define S_ISREG(mode)		((mode) & S_IFREG )
#define S_ISLNK(mode)		((mode) & S_IFLNK )
#define S_ISFIFO(mode)		((mode) & S_IFIFO )
#define S_ISSOCK(mode)		((mode) & S_IFSOCK )

/* permission bits */
#define S_IXUSR			(1<<16)
#define S_IWUSR			(1<<17)
#define S_IRUSR			(1<<18)
#define S_IRWXU			(S_IRUSR|S_IWUSR|S_IXUSR)

#define S_IXGRP			(1<<19)
#define S_IWGRP			(1<<20)
#define S_IRGRP			(1<<21)
#define S_IRWXG			(S_IRGRP|S_IWGRP|S_IXGRP)

#define S_IXOTH			(1<<22)
#define S_IWOTH			(1<<23)
#define S_IROTH			(1<<24)
#define S_IRWXO			(S_IROTH|S_IWOTH|S_IXOTH)

struct file;
struct dirent;
struct mount;
struct vnode;
struct vattr;
struct filesystem_ops;
struct filesystem;

/** file structure */
struct file {
	u32 f_flags;			/* open flag */
	s32 f_count;			/* reference count */
	loff_t f_offset;		/* current position in file */
	struct vnode *f_vnode;		/* vnode */
};

/** dirent types */
enum dirent_type {
	DT_UNKNOWN,
	DT_DIR,
	DT_REG,
	DT_BLK,
	DT_CHR,
	DT_FIFO,
	DT_LNK,
	DT_SOCK,
	DT_WHT,
};

/** dirent structure */
struct dirent {
	u32 d_fileno;			/* file number of entry */
	u16 d_reclen;			/* length of this record */
	u16 d_namlen;			/* length of string in d_name */
	enum dirent_type d_type; 	/* file type, see below */
	char d_name[VFS_MAX_NAME];	/* name must be no longer than this */
};

/** directory description */
struct dir {
	s32 fd;
	struct dirent entry;
};

/** mount flags */
#define	MOUNT_RDONLY	(0x00000001)	/* read only filesystem */
#define	MOUNT_RW	(0x00000002)	/* read-write filesystem */
#define	MOUNT_MASK	(0x00000003)	/* mount flag mask value */

/** mount data */
struct mount {
	struct dlist m_link;		/* link to next mount point */
	struct filesystem *m_fs;	/* mounted filesystem */
	struct vmm_blockdev *m_dev;	/* mounted device */
	char m_path[VFS_MAX_PATH];	/* mounted path */
	u32 m_flags;			/* mount flag */
	atomic_t m_refcnt;		/* reference count */
	struct vnode *m_root;		/* root vnode */
	struct vnode *m_covered;	/* vnode covered on parent fs */
	void *m_data;			/* private data for filesystem */
};

/** vnode types */
enum vnode_type {
	VREG,				/* regular file  */
	VDIR,	   			/* directory */
	VBLK,	    			/* block device */
	VCHR,	    			/* character device */
	VLNK,	    			/* symbolic link */
	VSOCK,	    			/* socks */
	VFIFO,	    			/* fifo */
};

/** vnode flags */
enum vnode_flag {
	VNONE,				/* default vnode flag */
	VROOT,	   			/* root of its file system */
};

/** vnode attribute structure */
struct vattr {
	enum vnode_type	va_type;	/* vnode type */
	u32 va_mode;			/* file access mode */
};

/** vnode structure */
struct vnode {
	struct dlist v_link;		/* link for hash list */
	struct mount *v_mount;		/* mount point pointer */
	struct filesystem *v_fs;	/* pointer to filesystem */
	loff_t v_size;			/* file size */
	u32 v_mode;			/* file mode permissions */
	enum vnode_type v_type;		/* vnode type */
	enum vnode_flag v_flags;	/* vnode flag */
	atomic_t v_refcnt;		/* reference count */
	char *v_path;			/* pointer to path in fs */
	void *v_data;			/* private data for fs */
};

/** filesystem structure */
struct filesystem {
	/* filesystem list head */
	struct dlist head;

	/* filesystem name */
	const char *name;

	/* VFS operations */
	int (*mount)(struct mount *, const char *, s32);
	int (*unmount)(struct mount *);
	int (*sync)(struct mount *);
	int (*vget)(struct mount *, struct vnode *);

	/* Node operations */
	int (*open)(struct vnode *, int);
	int (*close)(struct vnode *, struct file *);
	int (*read)(struct vnode *, struct file *, void *, loff_t, loff_t *);
	int (*write)(struct vnode *, struct file *, void *, loff_t, loff_t *);
	int (*seek)(struct vnode *, struct file *, loff_t, loff_t);
	int (*ioctl)(struct vnode *, struct file *, int, void *);
	int (*fsync)(struct vnode *, struct file *);
	int (*readdir)(struct vnode *, struct file *, struct dirent *);
	int (*lookup)(struct vnode *, char *, struct vnode *);
	int (*create)(struct vnode *, char *, u32);
	int (*remove)(struct vnode *, struct vnode *, char *);
	int (*rename)(struct vnode *, struct vnode *, char *, struct vnode *, struct vnode *, char *);
	int (*mkdir)(struct vnode *, char *, u32);
	int (*rmdir)(struct vnode *, struct vnode *, char *);
	int (*getattr)(struct vnode *, struct vattr *);
	int (*setattr)(struct vnode *, struct vattr *);
	int (*inactive)(struct vnode *);
	int (*truncate)(struct vnode *, loff_t);
};

/** Get root directory and mount point for specified path. */
int vfs_findroot(const char *path, struct mount **mp, char **root);

/** Create a mount point
 *  Note: Must be called from Orphan (or Thread) context.
 */
int vfs_mount(const char *dir, const char *fsname, const char *dev, u32 flags);

/** Destroy a mount point
 *  Note: Must be called from Orphan (or Thread) context.
 */
int vfs_unmount(const char *path);

/** Get mount point by index */
struct mount *vfs_mount_get(int index);

/** Count number of mount points */
u32 vfs_mount_count(void);

/** Register filesystem */
int vfs_filesystem_register(struct filesystem *fs);

/** Unregister filesystem */
int vfs_filesystem_unregister(struct filesystem *fs);

/** Find filesystem by name */
struct filesystem *vfs_filesystem_find(const char *name);

/** Get filesystem by index */
struct filesystem *vfs_filesystem_get(int index);

/** Count number of available filesystems */
u32 vfs_filesystem_count(void);

#endif /* __VFS_H_ */
