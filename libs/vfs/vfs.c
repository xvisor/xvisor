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
#include <vmm_spinlocks.h>
#include <vmm_scheduler.h>
#include <vmm_modules.h>
#include <arch_atomic.h>
#include <libs/stringlib.h>
#include <libs/vfs.h>

#define MODULE_DESC			"Light-weight VFS Library"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		VFS_IPRIORITY
#define	MODULE_INIT			vfs_init
#define	MODULE_EXIT			vfs_exit

/* size of vnode hash table, must power 2 */
#define VNODE_HASH_SIZE			(32)

struct vfs_ctrl {
	vmm_spinlock_t fs_list_lock;
	struct dlist fs_list;
	vmm_spinlock_t mnt_list_lock;
	struct dlist mnt_list;
	vmm_spinlock_t vnode_list_lock[VNODE_HASH_SIZE];
	struct list_head vnode_list[VNODE_HASH_SIZE];
};

static struct vfs_ctrl vfsc;

/** Compute hash value from mount point and path name. */
static u32 vfs_vnode_hash(struct mount *m, const char *path)
{
	u32 val = 0;

	if (path) {
		while (*path) {
			val = ((val << 5) + val) + *path++;
		}
	}

	return (val ^ (u32)m) & (VNODE_HASH_SIZE - 1);
}

static struct vnode *vfs_vnode_vget(struct mount *m, const char *path)
{
	int len;
	u32 hash;
	irq_flags_t flags;
	struct vnode *v;

	v = NULL;
	hash = vfs_vnode_hash(m, path);

	if (!(v = vmm_zalloc(sizeof(struct vnode)))) {
		return NULL;
	}

	len = strlen(path) + 1;
	if (!(v->v_path = vmm_malloc(len))) {
		vmm_free(v);
		return NULL;
	}

	INIT_LIST_HEAD(&v->v_link);
	v->v_mount = m;
	v->v_fs = m->m_fs;
	arch_atomic_write(&v->v_refcnt, 1);
	strncpy(v->v_path, path, len);

	/* request to allocate fs specific data for vnode. */
	if (m->m_fs->vget(m, v)) {
		vmm_free(v->v_path);
		vmm_free(v);
		return NULL;
	}

	arch_atomic_add(&v->v_mount->m_refcnt, 1);

	vmm_spin_lock_irqsave(&vfsc.vnode_list_lock[hash], flags);
	list_add(&v->v_link, &vfsc.vnode_list[hash]);
	vmm_spin_unlock_irqrestore(&vfsc.vnode_list_lock[hash], flags);

	return v;
}

static struct vnode *vfs_vnode_lookup(struct mount *m, const char *path)
{
	u32 hash;
	irq_flags_t flags;
	struct dlist *l;
	struct vnode *v;

	v = NULL;
	hash = vfs_vnode_hash(m, path);

	vmm_spin_lock_irqsave(&vfsc.vnode_list_lock[hash], flags);

	list_for_each(l, &vfsc.vnode_list[hash]) {
		v = list_entry(l, struct vnode, v_link);
		if ((v->v_mount == m) && 
		    (!strncmp(v->v_path, path, VFS_MAX_PATH))) {
			break;
		}
	}

	vmm_spin_unlock_irqrestore(&vfsc.vnode_list_lock[hash], flags);

	if (v) {
		arch_atomic_add(&v->v_refcnt, 1);
	}

	return v;
}

static void vfs_vnode_vref(struct vnode *v)
{
	arch_atomic_add(&v->v_refcnt, 1);
}

static void vfs_vnode_vput(struct vnode *v)
{
	u32 hash;
	irq_flags_t flags;

	if (arch_atomic_sub_return(&v->v_refcnt, 1)) {
		return;
	}

	hash = vfs_vnode_hash(v->v_mount, v->v_path);

	vmm_spin_lock_irqsave(&vfsc.vnode_list_lock[hash], flags);
	list_del(&v->v_link);
	vmm_spin_unlock_irqrestore(&vfsc.vnode_list_lock[hash], flags);

	arch_atomic_sub(&v->v_mount->m_refcnt, 1);

	/* deallocate fs specific vnode data */
	v->v_mount->m_fs->inactive(v);

	vmm_free(v->v_path);
	vmm_free(v);
}

static void vfs_vnode_flush(struct mount *m)
{
	int i;
	irq_flags_t flags;
	struct dlist *l;
	struct vnode *v;

	for(i = 0; i < VNODE_HASH_SIZE; i++) {
		vmm_spin_lock_irqsave(&vfsc.vnode_list_lock[i], flags);

		list_for_each(l, &vfsc.vnode_list[i]) {
			v = list_entry(l, struct vnode, v_link);
			if(v->v_mount == m) {
				list_del(&v->v_link);

				/* deallocate fs specific vnode data */
				v->v_mount->m_fs->inactive(v);

				vmm_free(v->v_path);
				vmm_free(v);
			}
		}

		vmm_spin_unlock_irqrestore(&vfsc.vnode_list_lock[i], flags);
	}
}

static int vfs_vnode_acquire(const char *path, struct vnode **vp)
{
	char *p;
	char node[VFS_MAX_PATH];
	char name[VFS_MAX_PATH];
	struct mount *m;
	struct vnode *dv, *v;
	int error, i;

	/* convert a full path name to its mount point and
	 * the local node in the file system.
	 */
	if(vfs_findroot(path, &m, &p)) {
		return VMM_ENOTAVAIL;
	}

	strncpy(node, "/", sizeof(node));
	strncat(node, p, sizeof(node));
	if ((v = vfs_vnode_lookup(m, node))) {
		/* vnode is already active */
		*vp = v;
		return VMM_OK;
	}

	/* find target vnode, started from root directory.
	 * this is done to attach the fs specific data to
	 * the target vnode.
	 */
	if ((dv = m->m_root) == NULL) {
		return VMM_ENOSYS;
	}

	vfs_vnode_vref(dv);
	node[0] = '\0';

	while(*p != '\0') {
		/* get lower directory or file name. */
		while(*p == '/') {
			p++;
		}

		for(i = 0; i < VFS_MAX_PATH; i++) {
			if(*p == '\0' || *p == '/') {
				break;
			}
			name[i] = *p++;
		}
		name[i] = '\0';

		/* get a vnode for the target. */
		strncat(node, "/", sizeof(node));
		strncat(node, name, sizeof(node));
		v = vfs_vnode_lookup(m, node);
		if(v == NULL) {
			v = vfs_vnode_vget(m, node);
			if(v == NULL) {
				vfs_vnode_vput(dv);
				return VMM_ENOMEM;
			}

			/* find a vnode in this directory. */
			error = dv->v_mount->m_fs->lookup(dv, name, v);
			if(error || (*p == '/' && v->v_type != VDIR)) {
				/* not found */
				vfs_vnode_vput(v);
				vfs_vnode_vput(dv);
				return error;
			}
		}

		vfs_vnode_vput(dv);
		dv = v;
		while(*p != '\0' && *p != '/') {
			p++;
		}
	}

	*vp = v;

	return 0;
}

static void vfs_vnode_release(struct vnode *v)
{
	char *p;
	char path[VFS_MAX_PATH];
	struct mount *m;
	struct vnode *vt;

	if (!v) {
		return;
	}

	strncpy(path, v->v_path, sizeof(path));
	m = v->v_mount;

	vfs_vnode_vput(v);

	while (path[0] != '\0') {
		p = strrchr(path, '/');
		*p = '\0';

		vt = vfs_vnode_lookup(m, path);
		if (!vt) {
			continue;
		}

		/* vput for previous lookup */
		vfs_vnode_vput(vt);

		/* vput for previous acquire */
		vfs_vnode_vput(vt);
	}
}

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

int vfs_findroot(const char *path, struct mount **mp, char **root)
{
	struct dlist *l;
	irq_flags_t flags;
	struct mount *m, *tmp;
	int len, max_len = 0;

	if (!path || !mp || !root) {
		return VMM_EFAIL;
	}

	/* find mount point from nearest path */
	m = NULL;

	vmm_spin_lock_irqsave(&vfsc.mnt_list_lock, flags);

	list_for_each(l, &vfsc.mnt_list) {
		tmp = list_entry(l, struct mount, m_link);
		len = count_match(path, tmp->m_path);
		if(len > max_len) {
			max_len = len;
			m = tmp;
		}
	}

	vmm_spin_unlock_irqrestore(&vfsc.mnt_list_lock, flags);

	if(m == NULL) {
		return VMM_EFAIL;
	}

	*root = (char *)(path + max_len);
	if(**root == '/') {
		(*root)++;
	}
	*mp = m;

	return VMM_OK;
}

int vfs_mount(const char *dir, const char *fsname, const char *dev, u32 flags)
{
	int err;
	irq_flags_t f;
	struct dlist *l;
	struct vmm_blockdev *bdev;
	struct filesystem *fs;
	struct mount *m;
	struct vnode *v, *v_covered;

	BUG_ON(!vmm_scheduler_orphan_context());

	if (!dir || *dir == '\0') {
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
		if(m->m_covered) {
			vfs_vnode_release(m->m_covered);
		}
		vmm_free(m);
		return VMM_ENOMEM;
	}
	v->v_type = VDIR;
	v->v_flags = VROOT;
	if (!S_ISDIR(v->v_mode) || (v->v_mode & (S_IRWXU | S_IRWXG | S_IRWXO))) {
		v->v_mode = S_IFDIR | S_IRWXU | S_IRWXG | S_IRWXO;
	}
	m->m_root = v;

	/* call a file system specific routine. */
	err = m->m_fs->mount(m, dev, flags);
	if (err != 0) {
		vfs_vnode_release(m->m_root);
		if(m->m_covered) {
			vfs_vnode_release(m->m_covered);
		}
		vmm_free(m);
		return err;
	}

	if(m->m_flags & MOUNT_RDONLY) {
		m->m_root->v_mode &= ~(S_IWUSR|S_IWGRP|S_IWOTH);
	}

	/* add to mount list */
	vmm_spin_lock_irqsave(&vfsc.mnt_list_lock, f);

	list_for_each(l, &vfsc.mnt_list) {
		m = list_entry(l, struct mount, m_link);
		if (!strcmp(m->m_path, dir) ||
		    ((dev != NULL) && (m->m_dev == bdev))) {
			vmm_spin_unlock_irqrestore(&vfsc.mnt_list_lock, flags);
			m->m_fs->unmount(m);
			vfs_vnode_release(m->m_root);
			if(m->m_covered) {
				vfs_vnode_release(m->m_covered);
			}
			vmm_free(m);
			return VMM_EBUSY;
		}
	}

	list_add(&m->m_link, &vfsc.mnt_list);

	vmm_spin_unlock_irqrestore(&vfsc.mnt_list_lock, f);

	return VMM_OK;
}

struct mount *vfs_mount_get(int index)
{
	bool found;
	irq_flags_t flags;
	struct dlist *l;
	struct mount *ret;

	if (index < 0) {
		return NULL;
	}

	vmm_spin_lock_irqsave(&vfsc.mnt_list_lock, flags);

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

	vmm_spin_unlock_irqrestore(&vfsc.mnt_list_lock, flags);

	if (!found) {
		return NULL;
	}

	return ret;
}

u32 vfs_mount_count(void)
{
	u32 retval = 0;
	irq_flags_t flags;
	struct dlist *l;

	vmm_spin_lock_irqsave(&vfsc.mnt_list_lock, flags);

	list_for_each(l, &vfsc.mnt_list) {
		retval++;
	}

	vmm_spin_unlock_irqrestore(&vfsc.mnt_list_lock, flags);

	return retval;
}


int vfs_unmount(const char *path)
{
	int err;
	bool found;
	irq_flags_t f;
	struct dlist *l;
	struct mount *m;

	BUG_ON(!vmm_scheduler_orphan_context());

	vmm_spin_lock_irqsave(&vfsc.mnt_list_lock, f);

	found = FALSE;
	list_for_each(l, &vfsc.mnt_list) {
		m = list_entry(l, struct mount, m_link);
		if (!strcmp(path, m->m_path)) {
			found = TRUE;
			break;
		}
	}

	/* root fs can not be unmounted. */
	if (!found || !m->m_covered) {
		vmm_spin_unlock_irqrestore(&vfsc.mnt_list_lock, f);
		return VMM_EINVALID;
	}

	/* remove mount point and break */
	list_del(&m->m_link);

	vmm_spin_unlock_irqrestore(&vfsc.mnt_list_lock, f);

	/* call filesytem unmount */
	err = m->m_fs->unmount(m);
	if(err != 0) {
		return err;
	}

	/* release all vnodes */
	vfs_vnode_flush(m);

	/* release covering filesystem vnode */
	vfs_vnode_release(m->m_covered);

	vmm_free(m);

	return VMM_OK;
}

int vfs_filesystem_register(struct filesystem *fs)
{
	bool found;
	irq_flags_t flags;
	struct dlist *l;
	struct filesystem *fst;

	if (!fs || !fs->name) {
		return VMM_EFAIL;
	}

	fst = NULL;
	found = FALSE;

	vmm_spin_lock_irqsave(&vfsc.fs_list_lock, flags);

	list_for_each(l, &vfsc.fs_list) {
		fst = list_entry(l, struct filesystem, head);
		if (strcmp(fst->name, fs->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		vmm_spin_unlock_irqrestore(&vfsc.fs_list_lock, flags);
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&fs->head);
	list_add_tail(&fs->head, &vfsc.fs_list);

	vmm_spin_unlock_irqrestore(&vfsc.fs_list_lock, flags);

	return VMM_OK;
}

int vfs_filesystem_unregister(struct filesystem *fs)
{
	bool found;
	irq_flags_t flags;
	struct dlist *l;
	struct filesystem *fst;

	if (!fs || !fs->name) {
		return VMM_EFAIL;
	}

	vmm_spin_lock_irqsave(&vfsc.fs_list_lock, flags);

	if (list_empty(&vfsc.fs_list)) {
		vmm_spin_unlock_irqrestore(&vfsc.fs_list_lock, flags);
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
		vmm_spin_unlock_irqrestore(&vfsc.fs_list_lock, flags);
		return VMM_ENOTAVAIL;
	}

	list_del(&fs->head);

	vmm_spin_unlock_irqrestore(&vfsc.fs_list_lock, flags);

	return VMM_OK;
}

struct filesystem *vfs_filesystem_find(const char *name)
{
	bool found;
	irq_flags_t flags;
	struct dlist *l;
	struct filesystem *ret;

	if (!name) {
		return NULL;
	}

	vmm_spin_lock_irqsave(&vfsc.fs_list_lock, flags);

	ret = NULL;
	found = FALSE;

	list_for_each(l, &vfsc.fs_list) {
		ret = list_entry(l, struct filesystem, head);
		if (strcmp(name, ret->name) == 0) {
			found = TRUE;
			break;
		}
	}

	vmm_spin_unlock_irqrestore(&vfsc.fs_list_lock, flags);

	if (!found) {
		return NULL;
	}

	return ret;
}

struct filesystem *vfs_filesystem_get(int index)
{
	bool found;
	irq_flags_t flags;
	struct dlist *l;
	struct filesystem *ret;

	if (index < 0) {
		return NULL;
	}

	vmm_spin_lock_irqsave(&vfsc.fs_list_lock, flags);

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

	vmm_spin_unlock_irqrestore(&vfsc.fs_list_lock, flags);

	if (!found) {
		return NULL;
	}

	return ret;
}

u32 vfs_filesystem_count(void)
{
	u32 retval = 0;
	irq_flags_t flags;
	struct dlist *l;

	vmm_spin_lock_irqsave(&vfsc.fs_list_lock, flags);

	list_for_each(l, &vfsc.fs_list) {
		retval++;
	}

	vmm_spin_unlock_irqrestore(&vfsc.fs_list_lock, flags);

	return retval;
}

static int __init vfs_init(void)
{
	int i;

	memset(&vfsc, 0, sizeof(struct vfs_ctrl));

	INIT_SPIN_LOCK(&vfsc.fs_list_lock);
	INIT_LIST_HEAD(&vfsc.fs_list);

	INIT_SPIN_LOCK(&vfsc.mnt_list_lock);
	INIT_LIST_HEAD(&vfsc.mnt_list);

	for (i = 0; i < VNODE_HASH_SIZE; i++) {
		INIT_SPIN_LOCK(&vfsc.vnode_list_lock[i]);
		INIT_LIST_HEAD(&vfsc.vnode_list[i]);
	};

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
