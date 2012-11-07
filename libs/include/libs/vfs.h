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

#include <vmm_mutex.h>
#include <block/vmm_blockdev.h>
#include <libs/list.h>

#define VFS_IPRIORITY		(VMM_BLOCKDEV_CLASS_IPRIORITY+1)
#define VFS_MAX_PATH		(256)
#define	VFS_MAX_NAME		(64)
#define VFS_MAX_FD		(32)

/** file type bits */
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

/** permission bits */
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

/** open only flags */
#define O_RDONLY		(1<<0)   		/* open for reading only */
#define O_WRONLY		(1<<1)			/* open for writing only */
#define O_RDWR			(O_RDONLY|O_WRONLY)	/* open for reading and writing */
#define O_ACCMODE		(O_RDWR)		/* mask for above modes */

#define O_CREAT			(1<<8)			/* create if nonexistent */
#define O_EXCL			(1<<9)			/* error if already exists */
#define O_NOCTTY		(1<<10)			/* do not assign a controlling terminal */
#define O_TRUNC			(1<<11)			/* truncate to zero length */
#define O_APPEND		(1<<12)			/* set append mode */
#define O_DSYNC			(1<<13)			/* synchronized I/O data integrity writes */
#define O_NONBLOCK		(1<<14)			/* no delay */
#define O_SYNC			(1<<15)			/* synchronized I/O file integrity writes */

/** seek type */
#define SEEK_SET		(0)
#define SEEK_CUR		(1)
#define SEEK_END		(2)

/** access permission */
#define	R_OK			(0x04)
#define	W_OK			(0x02)
#define	X_OK			(0x01)

struct stat;
struct file;
struct dirent;
struct mount;
struct vnode;
struct vattr;
struct filesystem;

/** file status structure */
struct stat {
	u32	st_ino;			/* file serial number */
	loff_t	st_size;     		/* file size */
	u32	st_mode;		/* file mode */
	u32	st_dev;			/* id of device containing file */
	u32	st_uid;			/* user ID of the file owner */
	u32	st_gid;			/* group ID of the file's group */
	u64	st_ctime;		/* file create time */
	u64	st_atime;		/* file access time */
	u64	st_mtime;		/* file modify time */
};

/** file structure */
struct file {
	struct vmm_mutex f_lock;	/* file lock */
	u32 f_flags;			/* open flag */
	loff_t f_offset;		/* current position in file */
	struct vnode *f_vnode;		/* vnode */
};

/** dirent types */
enum dirent_type {
	DT_UNK,
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
	loff_t d_off;			/* offset in actual directory */
	u16 d_reclen;			/* length of directory entry */
	enum dirent_type d_type; 	/* type of file; not supported
					 * by all file system types 
					 */
	char d_name[VFS_MAX_NAME];	/* name must not be longer than this */
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

	struct vmm_mutex m_lock;	/* lock to protect members below
					 * m_lock and mount point operations
					 */
	void *m_data;			/* private data for filesystem */
};

#define mount_fs(m)	((m)->m_fs)
#define mount_data(m)	((m)->m_data)

/** vnode types */
enum vnode_type {
	VREG,				/* regular file  */
	VDIR,	   			/* directory */
	VBLK,	    			/* block device */
	VCHR,	    			/* character device */
	VLNK,	    			/* symbolic link */
	VSOCK,	    			/* socks */
	VFIFO,	    			/* fifo */
	VUNK,				/* unknown */
};

/** vnode flags */
enum vnode_flag {
	VNONE,				/* default vnode flag */
	VROOT,	   			/* root of its filesystem */
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
	atomic_t v_refcnt;		/* reference count */
	char v_path[VFS_MAX_PATH];	/* pointer to path in fs */
	enum vnode_flag v_flags;	/* vnode flags 
					 * (used by internally by vfs) 
					 */
	enum vnode_type v_type;		/* vnode type 
					 * (set once by filesystem lookup()) 
					 */

	struct vmm_mutex v_lock;	/* lock to protect members below
					 * v_lock and vnode operations
					 */
	u64 v_ctime;			/* Create timestamp 
					 * (updated by filesystem create())
					 */
	u64 v_atime;			/* Access timestamp 
					 * (last permission change time) 
					 * (updated by filesystem setattr()) 
					 */
	u64 v_mtime;			/* Modify timestamp
					 * (last write time)
					 * (updated by filesystem write())
					 */
	u32 v_mode;			/* vnode permissions 
					 * (set once by filesystem lookup()) 
					 * (updated by filesystem setattr()) 
					 */
	loff_t v_size;			/* file size 
					 * (updated by filesystem read/write) 
					 */
	void *v_data;			/* private data for fs */
};

/** filesystem structure */
struct filesystem {
	/* filesystem list head */
	struct dlist head;

	/* filesystem name */
	const char *name;

	/* Mount point operations */
	int (*mount)(struct mount *, const char *, u32);
	int (*unmount)(struct mount *);
	int (*sync)(struct mount *); /* Not Used */
	int (*vget)(struct mount *, struct vnode *);
	int (*vput)(struct mount *, struct vnode *);

	/* Vnode operations */
	int (*open)(struct vnode *, struct file *);
	int (*close)(struct vnode *, struct file *);
	size_t (*read)(struct vnode *, struct file *, void *, size_t);
	size_t (*write)(struct vnode *, struct file *, void *, size_t);
	int (*truncate)(struct vnode *, loff_t);
	bool (*seek)(struct vnode *, struct file *, loff_t);
	int (*fsync)(struct vnode *, struct file *);
	int (*readdir)(struct vnode *, struct file *, struct dirent *);
	int (*lookup)(struct vnode *, const char *, struct vnode *);
	int (*create)(struct vnode *, const char *, u32);
	int (*remove)(struct vnode *, struct vnode *, const char *);
	int (*rename)(struct vnode *, struct vnode *, const char *, 
			struct vnode *, struct vnode *, const char *);
	int (*mkdir)(struct vnode *, const char *, u32);
	int (*rmdir)(struct vnode *, struct vnode *, const char *);
	int (*getattr)(struct vnode *, struct vattr *); /* Not Used */
	int (*setattr)(struct vnode *, struct vattr *); /* Not Used */
};

/** Create a mount point
 *  Note: Must be called from Orphan (or Thread) context.
 */
int vfs_mount(const char *dir, const char *fsname, const char *dev, u32 flags);

/** Destroy a mount point
 *  Note: Must be called from Orphan (or Thread) context.
 */
int vfs_unmount(const char *path);

/** Get mount point by index 
 *  Note: Must be called from Orphan (or Thread) context.
 */
struct mount *vfs_mount_get(int index);

/** Count number of mount points 
 *  Note: Must be called from Orphan (or Thread) context.
 */
u32 vfs_mount_count(void);

/** Open a file 
 *  Note: Must be called from Orphan (or Thread) context.
 */
int vfs_open(const char *path, u32 flags, u32 mode);

/** Close an open file 
 *  Note: Must be called from Orphan (or Thread) context.
 */
int vfs_close(int fd);

/** Read a file 
 *  Note: Must be called from Orphan (or Thread) context.
 */
size_t vfs_read(int fd, void *buf, size_t len);

/** Write a file 
 *  Note: Must be called from Orphan (or Thread) context.
 */
size_t vfs_write(int fd, void *buf, size_t len);

/** Set current position of a file 
 *  Note: Must be called from Orphan (or Thread) context.
 */
loff_t vfs_lseek(int fd, loff_t off, int whence);

/** Synchronize file 
 *  Note: Must be called from Orphan (or Thread) context.
 */
int vfs_fsync(int fd);

/** Get file status based on file descriptor 
 *  Note: Must be called from Orphan (or Thread) context.
 */
int vfs_fstat(int fd, struct stat *st);

/** Open a directory 
 *  Note: Must be called from Orphan (or Thread) context.
 */
int vfs_opendir(const char *name);

/** Close an open directory 
 *  Note: Must be called from Orphan (or Thread) context.
 */
int vfs_closedir(int fd);

/** Read a directory entry 
 *  Note: Must be called from Orphan (or Thread) context.
 */
int vfs_readdir(int fd, struct dirent *dir);

/** Rewind an open directory 
 *  Note: Must be called from Orphan (or Thread) context.
 */
int vfs_rewinddir(int fd);

/** Make a new directory 
 *  Note: Must be called from Orphan (or Thread) context.
 */
int vfs_mkdir(const char *path, u32 mode);

/** Remove existing directory 
 *  Note: Must be called from Orphan (or Thread) context.
 */
int vfs_rmdir(const char *path);

/** Rename file/directory 
 *  Note: Must be called from Orphan (or Thread) context.
 */
int vfs_rename(char *src, char *dst);

/** Unlink/remove file 
 *  Note: Must be called from Orphan (or Thread) context.
 */
int vfs_unlink(const char *path);

/** Check whether given path is accessible in specified mode 
 *  Note: Must be called from Orphan (or Thread) context.
 */
int vfs_access(const char *path, u32 mode);

/** Get file/directory status based on path 
 *  Note: Must be called from Orphan (or Thread) context.
 */
int vfs_stat(const char *path, struct stat *st);

/** Register filesystem 
 *  Note: Must be called from Orphan (or Thread) context.
 */
int vfs_filesystem_register(struct filesystem *fs);

/** Unregister filesystem 
 *  Note: Must be called from Orphan (or Thread) context.
 */
int vfs_filesystem_unregister(struct filesystem *fs);

/** Find filesystem by name 
 *  Note: Must be called from Orphan (or Thread) context.
 */
struct filesystem *vfs_filesystem_find(const char *name);

/** Get filesystem by index 
 *  Note: Must be called from Orphan (or Thread) context.
 */
struct filesystem *vfs_filesystem_get(int index);

/** Count number of available filesystems 
 *  Note: Must be called from Orphan (or Thread) context.
 */
u32 vfs_filesystem_count(void);

#endif /* __VFS_H_ */
