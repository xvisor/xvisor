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
 * @file vfs.c
 * @author Anup Patel. (anup@brainfault.org)
 * @brief Light-weight virtual filesystem implementation
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_scheduler.h>
#include <vmm_modules.h>
#include <arch_atomic.h>
#include <libs/stringlib.h>
#include <libs/bitmap.h>
#include <libs/vfs.h>

#define MODULE_DESC			"Light-weight VFS Library"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		VFS_IPRIORITY
#define	MODULE_INIT			vfs_init
#define	MODULE_EXIT			vfs_exit

/** file descriptor structure */
struct file {
	struct vmm_mutex f_lock;	/* file lock */
	u32 f_flags;			/* open flag */
	loff_t f_offset;		/* current position in file */
	struct vnode *f_vnode;		/* vnode */
};

/* size of vnode hash table, must power 2 */
#define VFS_VNODE_HASH_SIZE		(32)

struct vfs_ctrl {
	struct vmm_mutex fs_list_lock;
	struct dlist fs_list;
	struct vmm_mutex mnt_list_lock;
	struct dlist mnt_list;
	struct vmm_mutex vnode_list_lock[VFS_VNODE_HASH_SIZE];
	struct list_head vnode_list[VFS_VNODE_HASH_SIZE];
	struct vmm_mutex fd_bmap_lock;
	unsigned long *fd_bmap;
	struct file fd[VFS_MAX_FD];
};

static struct vfs_ctrl vfsc;

/** Compare two path strings and return matched length. */
static int count_match(const char *path, char *mount_root)
{
	int len = 0;

	while (*path && *mount_root) {
		if ((*path++) != (*mount_root++))
			break;
		len++;
	}

	if (*mount_root != '\0') {
		return 0;
	}

	if ((len == 1) && (*(path - 1) == '/')) {
		return 1;
	}

	if ((*path == '\0') || (*path == '/')) {
		return len;
	}

	return 0;
}

static int vfs_findroot(const char *path, struct mount **mp, char **root)
{
	struct dlist *l;
	struct mount *m, *tmp;
	int len, max_len = 0;

	if (!path || !mp || !root) {
		return VMM_EFAIL;
	}

	/* find mount point from nearest path */
	m = NULL;

	vmm_mutex_lock(&vfsc.mnt_list_lock);

	list_for_each(l, &vfsc.mnt_list) {
		tmp = list_entry(l, struct mount, m_link);
		len = count_match(path, tmp->m_path);
		if (len > max_len) {
			max_len = len;
			m = tmp;
		}
	}

	vmm_mutex_unlock(&vfsc.mnt_list_lock);

	if (m == NULL) {
		return VMM_EFAIL;
	}

	*root = (char *)(path + max_len);
	while (**root == '/') {
		(*root)++;
	}
	*mp = m;

	return VMM_OK;
}

static int vfs_fd_alloc(void)
{
	int i, ret = -1;

	vmm_mutex_lock(&vfsc.fd_bmap_lock);

	for (i = 0; i < VFS_MAX_FD; i++) {
		if (!bitmap_isset(vfsc.fd_bmap, i)) {
			ret = i;
			break;
		}
	}

	vmm_mutex_unlock(&vfsc.fd_bmap_lock);

	return ret;
}

static void vfs_fd_free(int fd)
{
	if (-1 < fd && fd < VFS_MAX_FD) {
		vmm_mutex_lock(&vfsc.fd_bmap_lock);

		if (bitmap_isset(vfsc.fd_bmap, fd)) {
			vmm_mutex_lock(&vfsc.fd[fd].f_lock);
			vfsc.fd[fd].f_flags = 0;
			vfsc.fd[fd].f_offset = 0;
			vfsc.fd[fd].f_vnode = NULL;
			vmm_mutex_unlock(&vfsc.fd[fd].f_lock);
			bitmap_clear(vfsc.fd_bmap, fd, 1);
		}

		vmm_mutex_unlock(&vfsc.fd_bmap_lock);
	}
}

static struct file *vfs_fd_to_file(int fd)
{
	return (-1 < fd && fd < VFS_MAX_FD) ? &vfsc.fd[fd] : NULL;
}

/** Compute hash value from mount point and path name. */
static u32 vfs_vnode_hash(struct mount *m, const char *path)
{
	u32 val = 0;

	if (path) {
		while (*path) {
			val = ((val << 5) + val) + *path++;
		}
	}

	return (val ^ (u32)m) & (VFS_VNODE_HASH_SIZE - 1);
}

static struct vnode *vfs_vnode_vget(struct mount *m, const char *path)
{
	int err, len;
	u32 hash;
	struct vnode *v;

	v = NULL;
	hash = vfs_vnode_hash(m, path);

	if (!(v = vmm_zalloc(sizeof(struct vnode)))) {
		return NULL;
	}

	len = strlen(path) + 1;

	INIT_LIST_HEAD(&v->v_link);
	INIT_MUTEX(&v->v_lock);
	v->v_mount = m;
	arch_atomic_write(&v->v_refcnt, 1);
	strncpy(v->v_path, path, len);

	/* request to allocate fs specific data for vnode. */
	vmm_mutex_lock(&m->m_lock);
	err = m->m_fs->vget(m, v);
	vmm_mutex_unlock(&m->m_lock);
	if (err) {
		vmm_free(v);
		return NULL;
	}

	arch_atomic_add(&m->m_refcnt, 1);

	vmm_mutex_lock(&vfsc.vnode_list_lock[hash]);
	list_add(&v->v_link, &vfsc.vnode_list[hash]);
	vmm_mutex_unlock(&vfsc.vnode_list_lock[hash]);

	return v;
}

static struct vnode *vfs_vnode_lookup(struct mount *m, const char *path)
{
	u32 hash;
	bool found = FALSE;
	struct dlist *l;
	struct vnode *v = NULL;

	hash = vfs_vnode_hash(m, path);

	vmm_mutex_lock(&vfsc.vnode_list_lock[hash]);

	list_for_each(l, &vfsc.vnode_list[hash]) {
		v = list_entry(l, struct vnode, v_link);
		if ((v->v_mount == m) && 
		    (!strncmp(v->v_path, path, VFS_MAX_PATH))) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&vfsc.vnode_list_lock[hash]);

	if (!found) {
		return NULL;
	}

	arch_atomic_add(&v->v_refcnt, 1);

	return v;
}

static void vfs_vnode_vref(struct vnode *v)
{
	arch_atomic_add(&v->v_refcnt, 1);
}

static void vfs_vnode_vput(struct vnode *v)
{
	u32 hash;

	if (arch_atomic_sub_return(&v->v_refcnt, 1)) {
		return;
	}

	hash = vfs_vnode_hash(v->v_mount, v->v_path);

	vmm_mutex_lock(&vfsc.vnode_list_lock[hash]);
	list_del(&v->v_link);
	vmm_mutex_unlock(&vfsc.vnode_list_lock[hash]);

	/* deallocate fs specific data from this vnode */
	vmm_mutex_lock(&v->v_mount->m_lock);
	v->v_mount->m_fs->vput(v->v_mount, v);
	vmm_mutex_unlock(&v->v_mount->m_lock);

	arch_atomic_sub(&v->v_mount->m_refcnt, 1);

	vmm_free(v);
}

/** Get stat from vnode pointer. */
static int vfs_vnode_stat(struct vnode *v, struct stat *st)
{
	u32 mode;

	memset(st, 0, sizeof(struct stat));

	st->st_ino = (u32)v;
	vmm_mutex_lock(&v->v_lock);
	st->st_size = v->v_size;
	mode = v->v_mode & (S_IRWXU|S_IRWXG|S_IRWXO);
	st->st_ctime = v->v_ctime;
	st->st_atime = v->v_atime;
	st->st_mtime = v->v_mtime;
	vmm_mutex_unlock(&v->v_lock);

	switch (v->v_type) {
	case VREG:
		mode |= S_IFREG;
		break;
	case VDIR:
		mode |= S_IFDIR;
		break;
	case VBLK:
		mode |= S_IFBLK;
		break;
	case VCHR:
		mode |= S_IFCHR;
		break;
	case VLNK:
		mode |= S_IFLNK;
		break;
	case VSOCK:
		mode |= S_IFSOCK;
		break;
	case VFIFO:
		mode |= S_IFIFO;
		break;
	default:
		return VMM_EFAIL;
	};
	st->st_mode = mode;

	if (v->v_type == VCHR || v->v_type == VBLK)
		st->st_dev = (u32)v->v_data;

	st->st_uid = 0;
	st->st_gid = 0;

	return 0;
}

/** check permission on vnode pointer. */
static int vfs_vnode_access(struct vnode *v, u32 mode)
{
	u32 vmode;

	vmm_mutex_lock(&v->v_lock);
	vmode = v->v_mode;
	vmm_mutex_unlock(&v->v_lock);

	if ((mode & R_OK) && !(vmode & (S_IRUSR|S_IRGRP|S_IROTH))) {
		return VMM_EACCESS;
	}

	if (mode & W_OK) {
		if (v->v_mount->m_flags & MOUNT_RDONLY) {
			return VMM_EACCESS;
		}

		if (!(vmode & (S_IWUSR|S_IWGRP|S_IWOTH))) {
			return VMM_EACCESS;
		}
	}

	if ((mode & X_OK) && !(vmode & (S_IXUSR|S_IXGRP|S_IXOTH))) {
		return VMM_EACCESS;
	}

	return 0;
}

static void vfs_vnode_release(struct vnode *v)
{
	char *p;
	char path[VFS_MAX_PATH];
	struct mount *m;

	if (!v) {
		return;
	}

	m = v->v_mount;

	if (m->m_root == v) {
		vfs_vnode_vput(v);
		return;
	}

	strncpy(path, v->v_path, sizeof(path));

	vfs_vnode_vput(v);

	while (1) {
		p = strrchr(path, '/');
		*p = '\0';

		if (path[0] == '\0') {
			break;
		}

		v = vfs_vnode_lookup(m, path);
		if (!v) {
			continue;
		}

		/* vput for previous lookup */
		vfs_vnode_vput(v);

		/* vput for previous acquire */
		vfs_vnode_vput(v);
	}

	/* vput for mount point root */
	vfs_vnode_vput(m->m_root);
}

static int vfs_vnode_acquire(const char *path, struct vnode **vp)
{
	char *p;
	char node[VFS_MAX_PATH];
	struct mount *m;
	struct vnode *dv, *v;
	int err, i, j;

	/* convert a full path name to its mount point and
	 * the local node in the file system.
	 */
	if (vfs_findroot(path, &m, &p)) {
		return VMM_ENOTAVAIL;
	}

	/* find target vnode, started from root directory.
	 * this is done to attach the fs specific data to
	 * the target vnode.
	 */
	if (!m->m_root) {
		return VMM_ENOSYS;
	}
	dv = v = m->m_root;
	vfs_vnode_vref(dv);

	i = 0;
	while (*p != '\0') {
		while (*p == '/') {
			p++;
		}

		node[i] = '/';
		i++;
		j = i;
		while (*p != '\0' && *p != '/') {
			node[i] = *p;
			p++;
			i++;
		}
		node[i] = '\0';	

		/* get a vnode for the target. */
		v = vfs_vnode_lookup(m, node);
		if (v == NULL) {
			v = vfs_vnode_vget(m, node);
			if (v == NULL) {
				vfs_vnode_vput(dv);
				return VMM_ENOMEM;
			}

			/* find a vnode in this directory. */
			vmm_mutex_lock(&v->v_lock);
			vmm_mutex_lock(&dv->v_lock);
			err = dv->v_mount->m_fs->lookup(dv, &node[j], v);
			vmm_mutex_unlock(&dv->v_lock);
			vmm_mutex_unlock(&v->v_lock);
			if (err || (*p == '/' && v->v_type != VDIR)) {
				/* not found */
				vfs_vnode_release(v);
				return err;
			}
		}

		dv = v;
	}

	*vp = v;

	return VMM_OK;
}

int vfs_mount(const char *dir, const char *fsname, const char *dev, u32 flags)
{
	int err;
	struct dlist *l;
	struct vmm_blockdev *bdev;
	struct filesystem *fs;
	struct mount *m, *tm;
	struct vnode *v, *v_covered;

	BUG_ON(!vmm_scheduler_orphan_context());

	if (!dir || *dir == '\0' || !(flags & MOUNT_MASK)) {
		return VMM_EINVALID;
	}

	/* find a file system. */
	if (!(fs = vfs_filesystem_find(fsname))) {
		return VMM_EINVALID;
	}

	/* NULL can be specified as a dev. */
	if (dev != NULL) {
		if (!(bdev = vmm_blockdev_find(dev))) {
			return VMM_EINVALID;
		}
	} else {
		bdev = NULL;
	}

	/* create vfs mount entry. */
	if (!(m = vmm_zalloc(sizeof(struct mount)))) {
		return VMM_ENOMEM;
	}

	INIT_LIST_HEAD(&m->m_link);
	INIT_MUTEX(&m->m_lock);
	m->m_fs = fs;
	m->m_flags = flags & MOUNT_MASK;
	arch_atomic_write(&m->m_refcnt, 0);
	strncpy(m->m_path, dir, sizeof(m->m_path));
	m->m_dev = bdev;

	/* get vnode to be covered in the upper file system. */
	if (*dir == '/' && *(dir + 1) == '\0') {
		/* ignore if it mounts to global root directory. */
		v_covered = NULL;
	} else {
		if (vfs_vnode_acquire(dir, &v_covered) != 0) {
			vmm_free(m);
			return VMM_ENOENT;
		}
		if (v_covered->v_type != VDIR) {
			vfs_vnode_release(v_covered);
			vmm_free(m);
			return VMM_EINVALID;
		}
	}
	m->m_covered = v_covered;

	/* create a root vnode for this file system. */
	if (!(v = vfs_vnode_vget(m, "/"))) {
		if (m->m_covered) {
			vfs_vnode_release(m->m_covered);
		}
		vmm_free(m);
		return VMM_ENOMEM;
	}
	v->v_type = VDIR;
	v->v_flags = VROOT;
	if (!S_ISDIR(v->v_mode) || 
	    (v->v_mode & (S_IRWXU | S_IRWXG | S_IRWXO))) {
		v->v_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
	}
	m->m_root = v;

	/* call a file system specific routine. */
	vmm_mutex_lock(&m->m_lock);
	err = m->m_fs->mount(m, dev, flags);
	vmm_mutex_unlock(&m->m_lock);
	if (err != 0) {
		vfs_vnode_release(m->m_root);
		if (m->m_covered) {
			vfs_vnode_release(m->m_covered);
		}
		vmm_free(m);
		return err;
	}

	if (m->m_flags & MOUNT_RDONLY) {
		m->m_root->v_mode &= ~(S_IWUSR|S_IWGRP|S_IWOTH);
	}

	/* add to mount list */
	vmm_mutex_lock(&vfsc.mnt_list_lock);

	list_for_each(l, &vfsc.mnt_list) {
		tm = list_entry(l, struct mount, m_link);
		if (!strcmp(tm->m_path, dir) ||
		    ((dev != NULL) && (tm->m_dev == bdev))) {
			vmm_mutex_unlock(&vfsc.mnt_list_lock);
			vmm_mutex_lock(&m->m_lock);
			m->m_fs->unmount(m);
			vmm_mutex_unlock(&m->m_lock);
			vfs_vnode_release(m->m_root);
			if (m->m_covered) {
				vfs_vnode_release(m->m_covered);
			}
			vmm_free(m);
			return VMM_EBUSY;
		}
	}

	list_add(&m->m_link, &vfsc.mnt_list);

	vmm_mutex_unlock(&vfsc.mnt_list_lock);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vfs_mount);

#if 0
/* FIXME: If a mount point is removed abruptly (probably due to
 * unplugging of a pluggable block device) then we need to
 * flush all file descriptors and vnodes under this mount point.
 *
 * Not sure whether:
 * vfs_mount_flush() handles all possible scenarios !!!!
 */
static void vfs_mount_flush(struct mount *m)
{
	int i;

	vmm_mutex_lock(&vfsc.fd_bmap_lock);

	for (i = 0; i < VFS_MAX_FD; i++) {
		if (bitmap_isset(vfsc.fd_bmap, i) &&
		    (vfsc.fd[i].f_vnode->v_mount == m)) {
			vmm_mutex_lock(&vfsc.fd[i].f_lock);
			vfsc.fd[i].f_flags = 0;
			vfsc.fd[i].f_offset = 0;
			vfsc.fd[i].f_vnode = NULL;
			vmm_mutex_unlock(&vfsc.fd[i].f_lock);
			bitmap_clear(vfsc.fd_bmap, i, 1);
		}
	}

	vmm_mutex_unlock(&vfsc.fd_bmap_lock);
}
#endif

int vfs_unmount(const char *path)
{
	int err;
	bool found;
	struct dlist *l;
	struct mount *m;

	BUG_ON(!vmm_scheduler_orphan_context());

	vmm_mutex_lock(&vfsc.mnt_list_lock);

	found = FALSE;
	list_for_each(l, &vfsc.mnt_list) {
		m = list_entry(l, struct mount, m_link);
		if (!strcmp(path, m->m_path)) {
			found = TRUE;
			break;
		}
	}

	/* root fs can not be unmounted. */
	if (!found) {
		vmm_mutex_unlock(&vfsc.mnt_list_lock);
		return VMM_EINVALID;
	}

	/* mount point reference count should be 1 
	 * otherwise it is busy.
	 */
	if (arch_atomic_read(&m->m_refcnt) > 1) {
		vmm_mutex_unlock(&vfsc.mnt_list_lock);
		return VMM_EBUSY;
	}

	/* remove mount point and break */
	list_del(&m->m_link);

	vmm_mutex_unlock(&vfsc.mnt_list_lock);

	/* call filesytem unmount */
	vmm_mutex_lock(&m->m_lock);
	err = m->m_fs->unmount(m);
	vmm_mutex_unlock(&m->m_lock);

	/* releae mount point root */
	vfs_vnode_release(m->m_root);

	/* release covering filesystem vnode */
	if (m->m_covered) {
		vfs_vnode_release(m->m_covered);
	}

	vmm_free(m);

	return err;
}
VMM_EXPORT_SYMBOL(vfs_unmount);

struct mount *vfs_mount_get(int index)
{
	bool found;
	struct dlist *l;
	struct mount *ret;

	BUG_ON(!vmm_scheduler_orphan_context());

	if (index < 0) {
		return NULL;
	}

	vmm_mutex_lock(&vfsc.mnt_list_lock);

	ret = NULL;
	found = FALSE;

	list_for_each(l, &vfsc.mnt_list) {
		ret = list_entry(l, struct mount, m_link);
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	vmm_mutex_unlock(&vfsc.mnt_list_lock);

	if (!found) {
		return NULL;
	}

	return ret;
}
VMM_EXPORT_SYMBOL(vfs_mount_get);

u32 vfs_mount_count(void)
{
	u32 retval = 0;
	struct dlist *l;

	BUG_ON(!vmm_scheduler_orphan_context());

	vmm_mutex_lock(&vfsc.mnt_list_lock);

	list_for_each(l, &vfsc.mnt_list) {
		retval++;
	}

	vmm_mutex_unlock(&vfsc.mnt_list_lock);

	return retval;
}
VMM_EXPORT_SYMBOL(vfs_mount_count);

static int vfs_lookup_dir(const char *path, struct vnode **vp, char **name)
{
	int err;
	char buf[VFS_MAX_PATH];
	char *file, *dir;
	struct vnode *v;

	/* get the path for directory. */
	strncpy(buf, path, sizeof(buf));
	file = strrchr(buf, '/');

	if (!buf[0]) {
		return VMM_EINVALID;
	}
	if (file == buf) {
		dir = "/";
	} else {
		*file = '\0';
		dir = buf;
	}

	/* get the vnode for directory */
	if ((err = vfs_vnode_acquire(dir, &v)))
		return err;
	if (v->v_type != VDIR) {
		vfs_vnode_release(v);
		return VMM_EINVALID;
	}
	*vp = v;

	/* get the file name */
	*name = strrchr(path, '/') + 1;

	return 0;
}

int vfs_open(const char *path, u32 flags, u32 mode)
{
	int err, fd;
	char *filename;
	struct vnode *v, *dv;
	struct file *f;

	BUG_ON(!vmm_scheduler_orphan_context());

	if (!path || !(flags & O_ACCMODE)) {
		return VMM_EINVALID;
	}

	if (flags & O_CREAT) {
		err = vfs_vnode_acquire(path, &v);
		if (err) {
			/* create new file. */
			if ((err = vfs_lookup_dir(path, &dv, &filename)))
				return err;
			if ((err = vfs_vnode_access(dv, W_OK))) {
				vfs_vnode_release(dv);
				return err;
			}
			mode &= ~S_IFMT;
			mode |= S_IFREG;
			vmm_mutex_lock(&dv->v_lock);
			err = dv->v_mount->m_fs->create(dv, filename, mode);
			vmm_mutex_unlock(&dv->v_lock);
			vfs_vnode_release(dv);
			if (err) {
				return err;
			}
			if ((err = vfs_vnode_acquire(path, &v))) {
				return err;
			}
			flags &= ~O_TRUNC;
		} else {
			/* file already exits */
			if (flags & O_EXCL) {
				vfs_vnode_release(v);
				return VMM_ENOTAVAIL;
			}
			flags &= ~O_CREAT;
		}
	} else {
		if ((err = vfs_vnode_acquire(path, &v))) {
			return err;
		}
		if ((flags & O_WRONLY) || (flags & O_TRUNC)) {
			if ((err = vfs_vnode_access(v, W_OK))) {
				vfs_vnode_release(v);
				return err;
			}
			if (v->v_type == VDIR) {
				/* open directory with writable. */
				vfs_vnode_release(v);
				return VMM_EINVALID;
			}
		}
	}

	/* process truncate request */
	if (flags & O_TRUNC) {
		if (!(flags & O_WRONLY) || (v->v_type == VDIR)) {
			vfs_vnode_release(v);
			return VMM_EINVALID;
		}
		vmm_mutex_lock(&v->v_lock);
		err = v->v_mount->m_fs->truncate(v, 0);
		vmm_mutex_unlock(&v->v_lock);
		if (err) {
			vfs_vnode_release(v);
			return err;
		}
	}

	/* setup file descriptor */
	fd = vfs_fd_alloc();
	if (fd < 0) {
		vfs_vnode_release(v);
		return VMM_ENOMEM;
	}
	f = vfs_fd_to_file(fd);

	vmm_mutex_lock(&f->f_lock);

	/* setup file descriptor contents */
	f->f_vnode = v;
	f->f_flags = flags;
	f->f_offset = 0;

	vmm_mutex_unlock(&f->f_lock);

	return fd;
}
VMM_EXPORT_SYMBOL(vfs_open);

int vfs_close(int fd)
{
	int err;
	struct vnode *v;
	struct file *f;

	BUG_ON(!vmm_scheduler_orphan_context());

	f = vfs_fd_to_file(fd);
	if (!f) {
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&f->f_lock);

	v = f->f_vnode;
	if (!v) {
		vmm_mutex_unlock(&f->f_lock);
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&v->v_lock);
	err = v->v_mount->m_fs->sync(v);
	vmm_mutex_unlock(&v->v_lock);
	if (err) {
		vmm_mutex_unlock(&f->f_lock);
		return err;
	}

	vfs_vnode_release(v);

	vmm_mutex_unlock(&f->f_lock);

	vfs_fd_free(fd);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vfs_close);

size_t vfs_read(int fd, void *buf, size_t len)
{
	size_t ret;
	struct vnode *v;
	struct file *f;

	BUG_ON(!vmm_scheduler_orphan_context());

	if (!buf || !len) {
		return 0;
	}

	f = vfs_fd_to_file(fd);
	if (!f) {
		return 0;
	}

	vmm_mutex_lock(&f->f_lock);

	v = f->f_vnode;
	if (!v) {
		vmm_mutex_unlock(&f->f_lock);
		return 0;
	}
	if (v->v_type != VREG) {
		vmm_mutex_unlock(&f->f_lock);
		return 0;
	}

	if (!(f->f_flags & O_RDONLY)) {
		vmm_mutex_unlock(&f->f_lock);
		return 0;
	}

	vmm_mutex_lock(&v->v_lock);
	ret = v->v_mount->m_fs->read(v, f->f_offset, buf, len);
	vmm_mutex_unlock(&v->v_lock);

	f->f_offset += ret;

	vmm_mutex_unlock(&f->f_lock);

	return ret;
}
VMM_EXPORT_SYMBOL(vfs_read);

size_t vfs_write(int fd, void *buf, size_t len)
{
	size_t ret;
	struct vnode *v;
	struct file *f;

	BUG_ON(!vmm_scheduler_orphan_context());

	if (!buf || !len) {
		return 0;
	}

	f = vfs_fd_to_file(fd);
	if (!f) {
		return 0;
	}

	vmm_mutex_lock(&f->f_lock);

	v = f->f_vnode;
	if (!v) {
		vmm_mutex_unlock(&f->f_lock);
		return 0;
	}
	if (v->v_type != VREG) {
		vmm_mutex_unlock(&f->f_lock);
		return 0;
	}

	if (!(f->f_flags & O_WRONLY)) {
		vmm_mutex_unlock(&f->f_lock);
		return 0;
	}

	vmm_mutex_lock(&v->v_lock);
	ret = v->v_mount->m_fs->write(v, f->f_offset, buf, len);
	vmm_mutex_unlock(&v->v_lock);

	f->f_offset += ret;

	vmm_mutex_unlock(&f->f_lock);

	return ret;
}
VMM_EXPORT_SYMBOL(vfs_write);

loff_t vfs_lseek(int fd, loff_t off, int whence)
{
	loff_t ret;
	struct vnode *v;
	struct file *f;

	BUG_ON(!vmm_scheduler_orphan_context());

	f = vfs_fd_to_file(fd);
	if (!f) {
		return 0;
	}

	vmm_mutex_lock(&f->f_lock);

	v = f->f_vnode;
	if (!v) {
		vmm_mutex_unlock(&f->f_lock);
		return 0;
	}

	vmm_mutex_lock(&v->v_lock);

	switch(whence)
	{
	case SEEK_SET:
		if (off < 0) {
			off = 0;
		} else if (off > (loff_t)v->v_size) {
			off = v->v_size;
		}
		break;

	case SEEK_CUR:
		if ((f->f_offset + off) > (loff_t)v->v_size) {
			off = v->v_size;
		} else if ((f->f_offset + off) < 0) {
			off = 0;
		} else {
			off = f->f_offset + off;
		}
		break;

	case SEEK_END:
		if (off > 0) {
			off = v->v_size;
		} else if ((v->v_size + off) < 0) {
			off = 0;
		} else {
			off = v->v_size + off;
		}
		break;

	default:
		vmm_mutex_unlock(&v->v_lock);
		ret = f->f_offset;
		vmm_mutex_unlock(&f->f_lock);
		return ret;
	}

	if (off <= (loff_t)(v->v_size)) {
		f->f_offset = off;
	}

	vmm_mutex_unlock(&v->v_lock);

	ret = f->f_offset;

	vmm_mutex_unlock(&f->f_lock);

	return ret;
}
VMM_EXPORT_SYMBOL(vfs_lseek);

int vfs_fsync(int fd)
{
	int err;
	struct vnode *v;
	struct file *f;

	BUG_ON(!vmm_scheduler_orphan_context());

	f = vfs_fd_to_file(fd);
	if (!f) {
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&f->f_lock);

	v = f->f_vnode;
	if (!v) {
		vmm_mutex_unlock(&f->f_lock);
		return VMM_EINVALID;
	}

	if (!(f->f_flags & O_WRONLY)) {
		vmm_mutex_unlock(&f->f_lock);
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&v->v_lock);
	err = v->v_mount->m_fs->sync(v);
	vmm_mutex_unlock(&v->v_lock);

	vmm_mutex_unlock(&f->f_lock);

	return err;
}
VMM_EXPORT_SYMBOL(vfs_fsync);

int vfs_fstat(int fd, struct stat *st)
{
	int err;
	struct vnode *v;
	struct file *f;

	BUG_ON(!vmm_scheduler_orphan_context());

	if (!st) {
		return VMM_EINVALID;
	}

	f = vfs_fd_to_file(fd);
	if (!f) {
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&f->f_lock);

	v = f->f_vnode;
	if (!v) {
		vmm_mutex_unlock(&f->f_lock);
		return VMM_EINVALID;
	}

	err = vfs_vnode_stat(v, st);

	vmm_mutex_unlock(&f->f_lock);

	return err;
}
VMM_EXPORT_SYMBOL(vfs_fstat);

int vfs_opendir(const char *name)
{
	int fd;
	struct vnode *v;
	struct file *f;

	BUG_ON(!vmm_scheduler_orphan_context());

	if (!name) {
		return VMM_EINVALID;
	}

	if ((fd = vfs_open(name, O_RDONLY, 0)) < 0) {
		return fd;
	}

	f = vfs_fd_to_file(fd);
	if (!f) {
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&f->f_lock);

	v = f->f_vnode;
	if (!v) {
		vmm_mutex_unlock(&f->f_lock);
		return VMM_EINVALID;
	}

	if (v->v_type != VDIR) {
		vmm_mutex_unlock(&f->f_lock);
		vfs_close(fd);
		return VMM_EINVALID;
	}

	vmm_mutex_unlock(&f->f_lock);

	return fd;
}
VMM_EXPORT_SYMBOL(vfs_opendir);

int vfs_closedir(int fd)
{
	struct vnode *v;
	struct file *f;

	BUG_ON(!vmm_scheduler_orphan_context());

	f = vfs_fd_to_file(fd);
	if (!f) {
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&f->f_lock);

	v = f->f_vnode;
	if (!v) {
		vmm_mutex_unlock(&f->f_lock);
		return VMM_EINVALID;
	}

	if (v->v_type != VDIR) {
		vmm_mutex_unlock(&f->f_lock);
		return VMM_EINVALID;
	}

	vmm_mutex_unlock(&f->f_lock);

	return vfs_close(fd);
}
VMM_EXPORT_SYMBOL(vfs_closedir);

int vfs_readdir(int fd, struct dirent *dir)
{
	int err;
	struct vnode *v;
	struct file *f;

	BUG_ON(!vmm_scheduler_orphan_context());

	if (!dir) {
		return VMM_EINVALID;
	}

	f = vfs_fd_to_file(fd);
	if (!f) {
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&f->f_lock);

	v = f->f_vnode;
	if (!v) {
		vmm_mutex_unlock(&f->f_lock);
		return VMM_EINVALID;
	}

	if (v->v_type != VDIR) {
		vmm_mutex_unlock(&f->f_lock);
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&v->v_lock);
	err = v->v_mount->m_fs->readdir(v, f->f_offset, dir);
	vmm_mutex_unlock(&v->v_lock);
	if (!err) {
		f->f_offset += dir->d_reclen;
	}

	vmm_mutex_unlock(&f->f_lock);

	return err;
}
VMM_EXPORT_SYMBOL(vfs_readdir);

int vfs_rewinddir(int fd)
{
	struct vnode *v;
	struct file *f;

	BUG_ON(!vmm_scheduler_orphan_context());

	f = vfs_fd_to_file(fd);
	if (!f) {
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&f->f_lock);

	v = f->f_vnode;
	if (!v) {
		vmm_mutex_unlock(&f->f_lock);
		return VMM_EINVALID;
	}

	if (v->v_type != VDIR) {
		vmm_mutex_unlock(&f->f_lock);
		return VMM_EINVALID;
	}

	f->f_offset = 0;

	vmm_mutex_unlock(&f->f_lock);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vfs_rewinddir);

int vfs_mkdir(const char *path, u32 mode)
{
	int err;
	char *name;
	struct vnode *v, *dv;

	BUG_ON(!vmm_scheduler_orphan_context());

	if (!path) {
		return VMM_EINVALID;
	}

	if (!(err = vfs_vnode_acquire(path, &v))) {
		vfs_vnode_release(v);
		return VMM_EINVALID;
	}

	/* notice: vp is invalid here */
	if ((err = vfs_lookup_dir(path, &dv, &name))) {
		/* directory already exists */
		return err;
	}

	if ((err = vfs_vnode_access(dv, W_OK))) {
		vfs_vnode_release(dv);
		return err;
	}

	mode &= ~S_IFMT;
	mode |= S_IFDIR;

	vmm_mutex_lock(&dv->v_lock);
	err = dv->v_mount->m_fs->mkdir(dv, name, mode);
	vmm_mutex_unlock(&dv->v_lock);

	vfs_vnode_release(dv);

	return err;
}
VMM_EXPORT_SYMBOL(vfs_mkdir);

static int vfs_check_dir_empty(const char *path)
{
	int err, fd, count;
	struct dirent dir;

	if ((fd = vfs_opendir(path)) < 0) {
		return fd;
	}

	count = 0;
	do {
		err = vfs_readdir(fd, &dir);
		if (err) {
			break;
		}
		if (!strcmp(dir.d_name, ".") || !strcmp(dir.d_name, "..")) {
			count++;
		}
		if (count) {
			break;
		}
	} while (1);

	vfs_closedir(fd);

	if (count) {
		return VMM_EINVALID;
	}

	return VMM_OK;
}

int vfs_rmdir(const char *path)
{
	int err;
	char *name;
	struct vnode *v, *dv;

	BUG_ON(!vmm_scheduler_orphan_context());

	if (!path) {
		return VMM_EINVALID;
	}

	if ((err = vfs_check_dir_empty(path))) {
		return err;
	}

	if ((err = vfs_vnode_acquire(path, &v))) {
		return err;
	}

	if ((v->v_flags == VROOT) || 
	    (arch_atomic_read(&v->v_refcnt) >= 2)) {
		vfs_vnode_release(v);
		return VMM_EBUSY;
	}

	if ((err = vfs_vnode_access(v, W_OK))) {
		vfs_vnode_release(v);
		return err;
	}

	if ((err = vfs_lookup_dir(path, &dv, &name))) {
		vfs_vnode_release(v);
		return err;
	}

	vmm_mutex_lock(&dv->v_lock);
	vmm_mutex_lock(&v->v_lock);
	err = dv->v_mount->m_fs->rmdir(dv, v, name);
	vmm_mutex_unlock(&v->v_lock);
	vmm_mutex_unlock(&dv->v_lock);

	vfs_vnode_release(v);
	vfs_vnode_release(dv);

	return err;
}
VMM_EXPORT_SYMBOL(vfs_rmdir);

int vfs_rename(char *src, char *dest)
{
	int err, len;
	char *sname, *dname;
	struct vnode *v1, *v2, *sv, *dv;

	BUG_ON(!vmm_scheduler_orphan_context());

	/* if source and dest are the same, do nothing */
	if (!strncmp(src, dest, VFS_MAX_PATH)) {
		return VMM_EINVALID;
	}

	/* check if dest is a directory of source */
	len = strlen(dest);
	if ((strlen(src) < len) && !strncmp(src, dest, len)) {
		return VMM_EINVALID;
	}

	/* get source v1 */
	if ((err = vfs_vnode_acquire(src, &v1))) {
		return err;
	}

	/* check source permission */
	if ((err = vfs_vnode_access(v1, W_OK))) {
		vfs_vnode_release(v1);
		return err;
	}

	/* check if source is busy ? */
	if (arch_atomic_read(&v1->v_refcnt) >= 2) {
		vfs_vnode_release(v1);
		return VMM_EBUSY;
	}

	/* release source node */
	vfs_vnode_release(v1);

	/* get sv and sname */
	if ((err = vfs_lookup_dir(src, &sv, &sname))) {
		return err;
	}

	/* check if dest exists */
	err = vfs_vnode_acquire(dest, &v2);
	if (!err) {
		vfs_vnode_release(v2);
		err = VMM_EALREADY;
		goto err1;
	}

	/* get dv and dname */
	if ((err = vfs_lookup_dir(dest, &dv, &dname))) {
		goto err1;
	}

	/* the source and dest must be on same file system */
	if (sv->v_mount != dv->v_mount) {
		err = VMM_EIO;
		goto err2;
	}

	/* call filesystem */
	vmm_mutex_lock(&sv->v_lock);
	vmm_mutex_lock(&dv->v_lock);
	err = sv->v_mount->m_fs->rename(sv, sname, dv, dname);
	vmm_mutex_unlock(&dv->v_lock);
	vmm_mutex_unlock(&sv->v_lock);

err2:
	vfs_vnode_release(dv);
err1:
	vfs_vnode_release(sv);

	return err;
}
VMM_EXPORT_SYMBOL(vfs_rename);

int vfs_unlink(const char *path)
{
	int err;
	char *name;
	struct vnode *v, *dv;

	BUG_ON(!vmm_scheduler_orphan_context());

	if (!path) {
		return VMM_EINVALID;
	}

	if ((err = vfs_vnode_acquire(path, &v))) {
		return err;
	}

	if (v->v_type == VDIR) {
		vfs_vnode_release(v);
		return VMM_EINVALID;
	}

	if ((v->v_flags == VROOT) || 
	    (arch_atomic_read(&v->v_refcnt) >= 2)) {
		vfs_vnode_release(v);
		return VMM_EBUSY;
	}

	if ((err = vfs_vnode_access(v, W_OK))) {
		vfs_vnode_release(v);
		return err;
	}

	if ((err = vfs_lookup_dir(path, &dv, &name))) {
		vfs_vnode_release(v);
		return err;
	}

	vmm_mutex_lock(&v->v_lock);

	err = v->v_mount->m_fs->truncate(v, 0);
	if (err) {
		goto done;
	}

	vmm_mutex_lock(&dv->v_lock);
	err = dv->v_mount->m_fs->remove(dv, v, name);
	vmm_mutex_unlock(&dv->v_lock);

done:
	vmm_mutex_unlock(&v->v_lock);

	vfs_vnode_release(dv);
	vfs_vnode_release(v);

	return err;
}
VMM_EXPORT_SYMBOL(vfs_unlink);

int vfs_access(const char *path, u32 mode)
{
	int err;
	struct vnode *v;

	BUG_ON(!vmm_scheduler_orphan_context());

	if (!path) {
		return VMM_EINVALID;
	}

	if ((err = vfs_vnode_acquire(path, &v))) {
		return err;
	}

	err = vfs_vnode_access(v, mode);

	vfs_vnode_release(v);

	return err;
}
VMM_EXPORT_SYMBOL(vfs_access);

int vfs_stat(const char *path, struct stat *st)
{
	int err;
	struct vnode *v;

	BUG_ON(!vmm_scheduler_orphan_context());

	if (!path || !st) {
		return VMM_EINVALID;
	}

	if ((err = vfs_vnode_acquire(path, &v))) {
		return err;
	}

	err = vfs_vnode_stat(v, st);

	vfs_vnode_release(v);

	return err;
}
VMM_EXPORT_SYMBOL(vfs_stat);

int vfs_filesystem_register(struct filesystem *fs)
{
	bool found;
	struct dlist *l;
	struct filesystem *fst;

	BUG_ON(!vmm_scheduler_orphan_context());

	if (!fs || !fs->name) {
		return VMM_EFAIL;
	}

	fst = NULL;
	found = FALSE;

	vmm_mutex_lock(&vfsc.fs_list_lock);

	list_for_each(l, &vfsc.fs_list) {
		fst = list_entry(l, struct filesystem, head);
		if (strcmp(fst->name, fs->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		vmm_mutex_unlock(&vfsc.fs_list_lock);
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&fs->head);
	list_add_tail(&fs->head, &vfsc.fs_list);

	vmm_mutex_unlock(&vfsc.fs_list_lock);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vfs_filesystem_register);

int vfs_filesystem_unregister(struct filesystem *fs)
{
	bool found;
	struct dlist *l;
	struct filesystem *fst;

	BUG_ON(!vmm_scheduler_orphan_context());

	if (!fs || !fs->name) {
		return VMM_EFAIL;
	}

	vmm_mutex_lock(&vfsc.fs_list_lock);

	if (list_empty(&vfsc.fs_list)) {
		vmm_mutex_unlock(&vfsc.fs_list_lock);
		return VMM_EFAIL;
	}

	fst = NULL;
	found = FALSE;
	list_for_each(l, &vfsc.fs_list) {
		fst = list_entry(l, struct filesystem, head);
		if (strcmp(fst->name, fs->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		vmm_mutex_unlock(&vfsc.fs_list_lock);
		return VMM_ENOTAVAIL;
	}

	list_del(&fs->head);

	vmm_mutex_unlock(&vfsc.fs_list_lock);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vfs_filesystem_unregister);

struct filesystem *vfs_filesystem_find(const char *name)
{
	bool found;
	struct dlist *l;
	struct filesystem *ret;

	BUG_ON(!vmm_scheduler_orphan_context());

	if (!name) {
		return NULL;
	}

	vmm_mutex_lock(&vfsc.fs_list_lock);

	ret = NULL;
	found = FALSE;

	list_for_each(l, &vfsc.fs_list) {
		ret = list_entry(l, struct filesystem, head);
		if (strcmp(name, ret->name) == 0) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&vfsc.fs_list_lock);

	if (!found) {
		return NULL;
	}

	return ret;
}
VMM_EXPORT_SYMBOL(vfs_filesystem_find);

struct filesystem *vfs_filesystem_get(int index)
{
	bool found;
	struct dlist *l;
	struct filesystem *ret;

	BUG_ON(!vmm_scheduler_orphan_context());

	if (index < 0) {
		return NULL;
	}

	vmm_mutex_lock(&vfsc.fs_list_lock);

	ret = NULL;
	found = FALSE;

	list_for_each(l, &vfsc.fs_list) {
		ret = list_entry(l, struct filesystem, head);
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	vmm_mutex_unlock(&vfsc.fs_list_lock);

	if (!found) {
		return NULL;
	}

	return ret;
}
VMM_EXPORT_SYMBOL(vfs_filesystem_get);

u32 vfs_filesystem_count(void)
{
	u32 retval = 0;
	struct dlist *l;

	BUG_ON(!vmm_scheduler_orphan_context());

	vmm_mutex_lock(&vfsc.fs_list_lock);

	list_for_each(l, &vfsc.fs_list) {
		retval++;
	}

	vmm_mutex_unlock(&vfsc.fs_list_lock);

	return retval;
}
VMM_EXPORT_SYMBOL(vfs_filesystem_count);

static int __init vfs_init(void)
{
	int i;

	memset(&vfsc, 0, sizeof(struct vfs_ctrl));

	INIT_MUTEX(&vfsc.fs_list_lock);
	INIT_LIST_HEAD(&vfsc.fs_list);

	INIT_MUTEX(&vfsc.mnt_list_lock);
	INIT_LIST_HEAD(&vfsc.mnt_list);

	for (i = 0; i < VFS_VNODE_HASH_SIZE; i++) {
		INIT_MUTEX(&vfsc.vnode_list_lock[i]);
		INIT_LIST_HEAD(&vfsc.vnode_list[i]);
	};

	INIT_MUTEX(&vfsc.fd_bmap_lock);
	vfsc.fd_bmap = vmm_zalloc(bitmap_estimate_size(VFS_MAX_FD));
	if (!vfsc.fd_bmap) {
		return VMM_ENOMEM;
	}
	bitmap_zero(vfsc.fd_bmap, VFS_MAX_FD);
	memset(&vfsc.fd, 0, sizeof(vfsc.fd));
	for (i = 0; i < VFS_MAX_FD; i++) {
		INIT_MUTEX(&vfsc.fd[i].f_lock);
	}

	return VMM_OK;
}

static void __exit vfs_exit(void)
{
	/* For now nothing to be done for exit. */
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
