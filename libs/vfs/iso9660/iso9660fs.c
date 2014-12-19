/**
 * Copyright (c) 2014 Himanshu Chauhan
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
 * @file iso9660fs.c
 * @author Himanshu Chauhan <hschauhan@nulltrace.org>
 * @brief ISO9660 filesystem driver
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <vmm_heap.h>
#include <vmm_wallclock.h>
#include <libs/stringlib.h>
#include <libs/vfs.h>

#define MODULE_DESC			"ISO Filesystem Driver"
#define MODULE_AUTHOR			"Himanshu Chauhan"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VFS_IPRIORITY + 1)
#define	MODULE_INIT			iso9660fs_init
#define	MODULE_EXIT			iso9660fs_exit

/* data type encodings */
typedef u8 INT8;
typedef s8 SINT8;
typedef u16 INT16_LSB;
typedef u16 INT16_MSB;
typedef struct {
	u16 lsb;
	u16 msb;
} INT16_LSB_MSB;
typedef s16 SINT16_LSB;
typedef s16 SINT16_MSB;
typedef struct {
	s16 lsb;
	s16 msb;
} SINT16_LSB_MSB;
typedef u32 INT32_LSB;
typedef u32 INT32_MSB;
typedef struct {
	u32 lsb;
	u32 msb;
} INT32_LSB_MSB;
typedef s32 SINT32_LSB;
typedef s32 SINT32_MSB;
typedef struct {
	s32 lsb;
	s32 msb;
} SINT32_LSB_MSB;

/* first 16 sectors are for system use */
#define VOL_DESC_START_OFFS	(16 * 2048)

/* ASCII encoded date type format used in Primary volume descriptor */
struct dec_datetime {
	u8 year[4]; /* from 1 to 9999 */
	u8 month[2];
	u8 day[2];
	u8 hour[2];
	u8 minute[2];
	u8 second[2];
	u8 hsecond[2]; /* hundreths of a second (0-99) */
	/*
	 * This alone is non-ascii. It is time zone offset
	 * from GMT in 15 minute intervals, starting at
	 * interval -48(west) and running up to interval
	 * 52(east). So value 0 indicate interval -48 which
	 * equals GMT-12hours, and value 100 indicates interval
	 * 52 which equals GMT+13 hours.
	 */
	u8 timezone;
} __packed;

struct dir_entry_datetime {
	INT8 year;
	INT8 month;
	INT8 day;
	INT8 hour;
	INT8 minute;
	INT8 second;
	INT8 timezone;
} __packed;

/* Types of volume descriptors */
enum vol_desc_types {
	VOL_DESC_BOOT,
	VOL_DESC_PRIMARY,
	VOL_DESC_SUPPLEMENTARY,
	VOL_DESC_PART_DESC,
	VOL_DESC_SET_TERMINATOR = 255,
};

/* The primary volume descriptor */
struct primary_vol_desc {
	INT8 type; /* type: boot, primary etc. */
	INT8 ident[5]; /* string */
	INT8 version;
	INT8 unused1;
	INT8 system_id[32]; /* strA encoded */
	INT8 vol_id[32]; /* strD encoded */
	INT32_LSB_MSB unused2;
	INT32_LSB_MSB vol_space_size; /* number of logical blocks */
	INT8 unused3[32];
	INT16_LSB_MSB vol_set_size;
	INT16_LSB_MSB vol_seq_no;
	INT16_LSB_MSB logical_blk_size; /* Means blk size can be > 2KiB! */
	INT32_LSB_MSB path_tbl_size; /* in bytes */
	INT32_LSB typel_path_tbl_loc;
	INT32_LSB typel_opt_path_tbl_loc;
	INT32_MSB typem_path_tbl_loc;
	INT32_MSB typem_opt_path_tbl_loc;
	INT8 root_dir_entry[34];
	INT8 vol_set_id[128];
	INT8 pub_id[128];
	INT8 data_prep_id[128];
	INT8 app_id[128];
	INT8 copyright_id[38];
	INT8 abstract_id[36];
	INT8 bib_id[37];
	struct dec_datetime vol_creat_date;
	struct dec_datetime vol_mod_date;
	struct dec_datetime vol_exp_date;
	struct dec_datetime vol_eff_date;
	INT8 fstruct_version;
	INT8 unused4;
	INT8 appused[512];
	INT8 resvd[653];
} __packed;

struct dir_entry {
	INT8 len; /* length of entry */
	INT8 ex_attr_len;
	INT32_LSB_MSB start_lba;
	INT32_LSB_MSB dlen; /* length of data from start_lba */
	struct dir_entry_datetime datetime;
	INT8 file_flags;
	INT8 file_unit_size;
	INT8 interleave_gap;
	INT16_LSB_MSB seq_no;
	INT8 ident_len; /* length of file name, terminates with ';' */
	/* After this the identifier and system use bytes will be there
	 * which are variable len */
	INT8 vdata[223];
} __packed;

/* Rockridge PX data */
struct rrip_px_data {
	u8 signature[2]; /* PX (50,58) */
	u8 len; /* PX entry len */
	u8 sus_version;
	u64 f_mode; /* POSIX file mode (st_mode) */
	u64 f_links; /* POSIX file links (st_nlink) */
	u64 f_user; /* POSIX user id */
	u64 f_grid; /* POSIX group id */
	u64 f_sernum; /* POSIX file serial number */
} __packed;

struct iso9660_mount_data {
	struct primary_vol_desc vol_desc;
	/* Fixed size entry in volume descriptor */
	struct dir_entry *root_dir_entry;
	/* list of directory entry pointed by root_dir_entry */
	struct dir_entry *root_dir;
	u32 root_dir_offset;
	u32 root_dir_len;
	struct vmm_blockdev *mdev;
};

/* Mount operation */
static int iso9660fs_mount(struct mount *m, const char *dev, u32 flags)
{
	u64 read_count;
	int retval = VMM_OK, rd;
	struct iso9660_mount_data *mdata;

	if (dev == NULL) {
		return VMM_EINVALID;
	}

	if (vmm_blockdev_total_size(m->m_dev) <=
				sizeof(struct primary_vol_desc)) {
		return VMM_EFAIL;
	}

	mdata = vmm_zalloc(sizeof(struct iso9660_mount_data));

	if (!mdata)
		return VMM_ENOMEM;

	mdata->mdev = m->m_dev;

	read_count = vmm_blockdev_read(m->m_dev, (u8 *)(&mdata->vol_desc),
				       VOL_DESC_START_OFFS,
				       sizeof(struct primary_vol_desc));
	if (read_count != sizeof(struct primary_vol_desc)) {
		retval = VMM_EIO;
		goto _fail;
	}

	if (mdata->vol_desc.type != VOL_DESC_PRIMARY) {
		retval = VMM_EINVALID;
		goto _fail;
	}

	if (strncmp((const char *)&mdata->vol_desc.ident[0], "CD001", 5) != 0) {
		retval = VMM_EINVALID;
		goto _fail;
	}

	mdata->root_dir_entry = (struct dir_entry *)
		(&mdata->vol_desc.root_dir_entry[0]);
	mdata->root_dir_offset = mdata->root_dir_entry->start_lba.lsb * 2048;
	mdata->root_dir_len = mdata->root_dir_entry->dlen.lsb;

	mdata->root_dir = vmm_zalloc(mdata->root_dir_len);
	if (!mdata->root_dir) {
		retval = VMM_ENOMEM;
		goto _fail;
	}

	rd = vmm_blockdev_read(m->m_dev, (u8 *)mdata->root_dir,
			       mdata->root_dir_offset,
			       mdata->root_dir_len);
	if (!rd || rd != mdata->root_dir_len) {
		retval = VMM_EIO;
		goto _fail;
	}

	/* We don't support writing to ISO9660 fs */
	m->m_flags = MOUNT_RDONLY;
	m->m_root->v_data = NULL;
	m->m_data = mdata;

	return VMM_OK;

 _fail:
	if (mdata) {
		if (mdata->root_dir) vmm_free(mdata->root_dir);
		vmm_free(mdata);
	}

	return retval;
}

static int iso9660fs_unmount(struct mount *m)
{
	struct iso9660_mount_data *mdata = m->m_data;

	if (mdata) {
		if (mdata->root_dir) vmm_free(mdata->root_dir);
		vmm_free(mdata);
	}

	m->m_data = NULL;

	return VMM_OK;
}

static int iso9660fs_msync(struct mount *m)
{
	/* Not required (read-only filesystem) */
	return VMM_OK;
}

static int iso9660fs_vget(struct mount *m, struct vnode *v)
{
	/* Not required */
	return VMM_OK;
}

static int iso9660fs_vput(struct mount *m, struct vnode *v)
{
	/* Not required */
	return VMM_OK;
}

/* vnode operations */
static size_t iso9660fs_read(struct vnode *v, loff_t off, void *buf, size_t len)
{
	u64 toff;
	size_t sz = 0;

	if (v->v_type != VREG) {
		return 0;
	}

	if (off >= v->v_size) {
		return 0;
	}

	sz = len;
	if ((v->v_size - off) < sz) {
		sz = v->v_size - off;
	}

	toff = (u64)(v->v_data);
	sz = vmm_blockdev_read(v->v_mount->m_dev, (u8 *)buf, (toff + off), sz);

	return sz;
}

static size_t iso9660fs_write(struct vnode *v, loff_t off, void *buf, size_t len)
{
	/* Not required (read-only filesystem) */
	return 0;
}

static int iso9660fs_truncate(struct vnode *v, loff_t off)
{
	/* Not allowed (read-only filesystem) */
	return VMM_EFAIL;
}

static int iso9660fs_sync(struct vnode *v)
{
	/* Not required (read-only filesystem) */
	return VMM_OK;
}

static struct dir_entry *lookup_dentry(const char *dir_name, struct dir_entry *pdentry)
{
	u8 *root_dir;
	struct dir_entry *dentry;
	u32 toffs = 0, dentry_id_len, nlen;
	char cname[VFS_MAX_NAME] = { 0 };

	if (!dir_name || !pdentry)
		return NULL;

	root_dir = (u8 *)pdentry;

	while (1) {
		dentry = (struct dir_entry *)root_dir;

		if (dentry->len == 0) {
			return NULL;
		}

		if (dentry->ident_len == 1) {
			toffs += dentry->len;
			root_dir += dentry->len;
			continue;
		}

		dentry_id_len = dentry->ident_len;
		nlen = (dentry_id_len >= VFS_MAX_NAME ? VFS_MAX_NAME
			: dentry_id_len);
		memset(cname, 0, sizeof(cname));
		strncpy(cname, (const char *)dentry->vdata, nlen);

		if (strncmp(cname, dir_name, nlen)) {
			toffs += dentry->len;
			root_dir += dentry->len;
			continue;
		}

		break;
	}

	return dentry;
}

static struct dir_entry *path_to_dentry(struct vmm_blockdev *mdev,
					const char *path,
					struct dir_entry *pdentry)
{
	char dirname[VFS_MAX_NAME] = { 0 };
	struct dir_entry *dentry, *d_root;
	int i = 0, rd;
	int len = 0;
	char rpath[VFS_MAX_NAME];

	len = strlen(path);

	if (!len) return NULL;

	vmm_snprintf(rpath, len, "%s", path);
	if (rpath[len - 1] != '/') {
		rpath[len] = '/';
		rpath[len+1] = 0;
	}

	path = rpath;

	if (*path == '/') {
		path++;
		if (!*path) return pdentry;
	}

	while (*path && *path != '/') {
		dirname[i] = *path;
		path++;
		i++;
	}

	dentry = lookup_dentry(dirname, pdentry);
	if (dentry) {
		d_root = vmm_zalloc(dentry->dlen.lsb);
		rd = vmm_blockdev_read(mdev, (u8 *)d_root,
				       (dentry->start_lba.lsb * 2048),
				       dentry->dlen.lsb);
		if (rd != dentry->dlen.lsb) {
			vmm_free(d_root);
			return NULL;
		}
		dentry = path_to_dentry(mdev, path, d_root);
	}

	return dentry;
}

static int iso9660fs_readdir(struct vnode *dv, loff_t off, struct dirent *d)
{
	struct iso9660_mount_data *mdata = dv->v_mount->m_data;
	char name[VFS_MAX_NAME] = { 0 };
	struct dir_entry *dentry;
	u8 *root_dir = NULL;
	u32 dentry_id_len, nlen;
	u64 mode = 0, toffs = 0;
	struct rrip_px_data *px_data = NULL;

	BUG_ON(mdata == NULL);
	BUG_ON(mdata->root_dir == NULL);

	root_dir = (u8 *)path_to_dentry(mdata->mdev, dv->v_path,
					mdata->root_dir);

	root_dir += off;

	while(1) {
		dentry = (struct dir_entry *)root_dir;

		if (dentry->len == 0) {
			return VMM_ENOENT;
		}

		if (dentry->ident_len == 1) {
			toffs += dentry->len;
			root_dir += dentry->len;
			continue;
		}

		dentry_id_len = dentry->ident_len;
		nlen = (dentry_id_len >= VFS_MAX_NAME ? VFS_MAX_NAME
			: dentry_id_len);
		memset(name, 0, sizeof(name));
		strncpy(name, (const char *)dentry->vdata, nlen);

		if (name[nlen-2] == ';') {
			name[nlen-2] = 0;
			name[nlen-1] = 0;
		}

		if (dentry_id_len % 2)
			px_data = (struct rrip_px_data *)(&dentry->vdata[nlen]);
		else
			px_data =
				(struct rrip_px_data *)(&dentry->vdata[nlen+1]);

		if (px_data->signature[0] == 'P'
		    && px_data->signature[1] == 'X') {
			mode = px_data->f_mode;
		}

		toffs += dentry->len;

		break;
	}


	if ((mode & 00170000) == 0140000) {
		d->d_type = DT_SOCK;
	} else if ((mode & 00170000) == 0120000) {
		d->d_type = DT_LNK;
	} else if ((mode & 00170000) == 0100000) {
		d->d_type = DT_REG;
	} else if ((mode & 00170000) == 0060000) {
		d->d_type = DT_BLK;
	} else if ((mode & 00170000) == 0040000) {
		d->d_type = DT_DIR;
	} else if ((mode & 00170000) == 0020000) {
		d->d_type = DT_CHR;
	} else if ((mode & 00170000) == 0010000) {
		d->d_type = DT_FIFO;
	} else {
		d->d_type = DT_REG;
	}

	memset(d->d_name, 0, sizeof(d->d_name));
	strncpy(d->d_name, name, nlen);

	d->d_off = toffs;
	d->d_reclen = toffs;

	return 0;
}

static u32 iso9660_pack_timestamp(struct dir_entry_datetime *dt)
{
	u32 packed_time;
	packed_time = (u32)vmm_wallclock_mktime(1900+dt->year, dt->month,
						dt->day, dt->hour, dt->minute,
						dt->second);

	return packed_time;
}

static int iso9660fs_lookup(struct vnode *dv, const char *name, struct vnode *v)
{
	struct iso9660_mount_data *mdata = dv->v_mount->m_data;
	u64 toffs = 0;
	u8 *root_dir;
	struct dir_entry *dentry;
	u32 dentry_id_len, nlen;
	u64 mode;
	struct rrip_px_data *px_data = NULL;
	char cname[VFS_MAX_NAME] = { 0 };

	BUG_ON(mdata == NULL);
	BUG_ON(mdata->root_dir == NULL);

	root_dir = (u8 *)path_to_dentry(mdata->mdev, dv->v_path,
					mdata->root_dir);

	while (1) {
		dentry = (struct dir_entry *)root_dir;

		if (dentry->len == 0) {
			return VMM_ENOENT;
		}

		if (dentry->ident_len == 1) {
			toffs += dentry->len;
			root_dir += dentry->len;
			continue;
		}

		dentry_id_len = dentry->ident_len;
		nlen = (dentry_id_len >= VFS_MAX_NAME ? VFS_MAX_NAME
			: dentry_id_len);
		memset(cname, 0, sizeof(cname));
		strncpy(cname, (const char *)dentry->vdata, nlen);
		if (cname[nlen-2] == ';') {
			cname[nlen-2] = 0;
			cname[nlen-1] = 0;
			nlen -= 2;
		}

		if (strncmp(cname, name, nlen)) {
			toffs += dentry->len;
			root_dir += dentry->len;
			continue;
		}

		if (dentry_id_len % 2)
			px_data = (struct rrip_px_data *)(&dentry->vdata[nlen]);
		else
			px_data =
				(struct rrip_px_data *)(&dentry->vdata[nlen+1]);

		if (px_data->signature[0] == 'P'
		    && px_data->signature[1] == 'X') {
			mode = px_data->f_mode;
		}

		toffs += dentry->len;

		break;
	}

	v->v_mtime = v->v_ctime = v->v_atime =
		iso9660_pack_timestamp(&dentry->datetime);

	v->v_mode = 0;

	if ((mode & 00170000) == 0140000) {
		v->v_type = VSOCK;
		v->v_mode |= S_IFSOCK;
	} else if ((mode & 00170000) == 0120000) {
		v->v_type = VLNK;
		v->v_mode |= S_IFLNK;
	} else if ((mode & 00170000) == 0100000) {
		v->v_type = VREG;
		v->v_mode |= S_IFREG;
	} else if ((mode & 00170000) == 0060000) {
		v->v_type = VBLK;
		v->v_mode |= S_IFBLK;
	} else if ((mode & 00170000) == 0040000) {
		v->v_type = VDIR;
		v->v_mode |= S_IFDIR;
	} else if ((mode & 00170000) == 0020000) {
		v->v_type = VCHR;
		v->v_mode |= S_IFCHR;
	} else if ((mode & 00170000) == 0010000) {
		v->v_type = VFIFO;
		v->v_mode |= S_IFIFO;
	} else {
		v->v_type = VREG;
	}

	v->v_mode |= (mode & 00400) ? S_IRUSR : 0;
	v->v_mode |= (mode & 00200) ? S_IWUSR : 0;
	v->v_mode |= (mode & 00100) ? S_IXUSR : 0;
	v->v_mode |= (mode & 00040) ? S_IRGRP : 0;
	v->v_mode |= (mode & 00020) ? S_IWGRP : 0;
	v->v_mode |= (mode & 00010) ? S_IXGRP : 0;
	v->v_mode |= (mode & 00004) ? S_IROTH : 0;
	v->v_mode |= (mode & 00002) ? S_IWOTH : 0;
	v->v_mode |= (mode & 00001) ? S_IXOTH : 0;

	v->v_size = dentry->dlen.lsb;

	v->v_data = (void *)((u64)(dentry->start_lba.lsb * 2048));

	return 0;
}

static int iso9660fs_create(struct vnode *dv, const char *filename, u32 mode)
{
	/* Not allowed (read-only filesystem) */
	return VMM_EFAIL;
}

static int iso9660fs_remove(struct vnode *dv, struct vnode *v, const char *name)
{
	/* Not allowed (read-only filesystem) */
	return VMM_EFAIL;
}

static int iso9660fs_rename(struct vnode *sv, const char *sname, struct vnode *v,
			    struct vnode *dv, const char *dname)
{
	/* Not allowed (read-only filesystem) */
	return VMM_EFAIL;
}

static int iso9660fs_mkdir(struct vnode *dv, const char *name, u32 mode)
{
	/* Not allowed (read-only filesystem) */
	return VMM_EFAIL;
}

static int iso9660fs_rmdir(struct vnode *dv, struct vnode *v, const char *name)
{
	/* Not allowed (read-only filesystem) */
	return VMM_EFAIL;
}

static int iso9660fs_chmod(struct vnode *v, u32 mode)
{
	/* Not allowed (read-only filesystem) */
	return VMM_EFAIL;
}

/* iso9660fs filesystem access descriptor */
static struct filesystem iso9660fs = {
	.name		= "iso9660",

	/* Mount point operations */
	.mount		= iso9660fs_mount,
	.unmount	= iso9660fs_unmount,
	.msync		= iso9660fs_msync,
	.vget		= iso9660fs_vget,
	.vput		= iso9660fs_vput,

	/* Vnode operations */
	.read		= iso9660fs_read,
	.write		= iso9660fs_write,
	.truncate	= iso9660fs_truncate,
	.sync		= iso9660fs_sync,
	.readdir	= iso9660fs_readdir,
	.lookup		= iso9660fs_lookup,
	.create		= iso9660fs_create,
	.remove		= iso9660fs_remove,
	.rename		= iso9660fs_rename,
	.mkdir		= iso9660fs_mkdir,
	.rmdir		= iso9660fs_rmdir,
	.chmod		= iso9660fs_chmod,
};

static int __init iso9660fs_init(void)
{
	return vfs_filesystem_register(&iso9660fs);
}

static void __exit iso9660fs_exit(void)
{
	vfs_filesystem_unregister(&iso9660fs);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
