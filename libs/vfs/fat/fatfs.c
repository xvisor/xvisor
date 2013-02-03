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
 * @file fatfs.c
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
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_wallclock.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <libs/vfs.h>

#define MODULE_DESC			"CPIO Filesystem Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VFS_IPRIORITY + 1)
#define	MODULE_INIT			fatfs_init
#define	MODULE_EXIT			fatfs_exit

#define __le32(x)			vmm_le32_to_cpu(x)
#define __le16(x)			vmm_le16_to_cpu(x)

/* Important offsets */
#define FAT_BOOTSECTOR_OFFSET		0x000

/* Enumeration of possible values for Media Type field in boot sector */
enum fat_media_types {
	FAT_DOUBLE_SIDED_1_44_MB=0xF0,
	FAT_FIXED_DISK=0xF8,
	FAT_DOUBLE_SIDED_720_KB=0xF9,
	FAT_SINGLE_SIDED_320_KB=0xFA,
	FAT_DOUBLE_SIDED_640_KB=0xFB,
	FAT_SINGLE_SIDED_180_KB=0xFC,
	FAT_DOUBLE_SIDED_360_KB=0xFD,
	FAT_SINGLE_SIDED_160_KB=0xFE,
	FAT_DOUBLE_SIDED_320_KB=0xFF
};

/* Enumeration of FAT types */
enum fat_types {
	FAT_TYPE_12=12,
	FAT_TYPE_16=16,
	FAT_TYPE_32=32
};

/* Enumeration of types of cluster in FAT12 table */
enum fat12_cluster_types {
	FAT12_FREE_CLUSTER=0x000,
	FAT12_RESERVED1_CLUSTER=0x001,
	FAT12_RESERVED2_CLUSTER=0xFF0,
	FAT12_BAD_CLUSTER=0xFF7,
	FAT12_LAST_CLUSTER=0xFF8
};

/* Enumeration of types of cluster in FAT16 table */
enum fat16_cluster_types {
	FAT16_FREE_CLUSTER=0x0000,
	FAT16_RESERVED1_CLUSTER=0x0001,
	FAT16_RESERVED2_CLUSTER=0xFFF0,
	FAT16_BAD_CLUSTER=0xFFF7,
	FAT16_LAST_CLUSTER=0xFFF8
};

/* Enumeration of types of cluster in FAT32 table */
enum fat32_cluster_types {
	FAT32_FREE_CLUSTER=0x00000000,
	FAT32_RESERVED1_CLUSTER=0x00000001,
	FAT32_RESERVED2_CLUSTER=0x0FFFFFF0,
	FAT32_BAD_CLUSTER=0x0FFFFFF7,
	FAT32_LAST_CLUSTER=0x0FFFFFF8
};

/* Extended boot sector information for FAT12/FAT16 */
struct fat_bootsec_ext16 {
	u8 drive_number;
	u8 reserved;
	u8 extended_signature;
	u32 serial_number;
	u8 volume_label[11];
	u8 fs_type[8];
	u8 boot_code[448];
	u16 boot_sector_signature;
} __packed;

/* Extended boot sector information for FAT32 */
struct fat_bootsec_ext32 {
	u32 sectors_per_fat;
	u16 fat_flags;
	u16 version;
	u32 root_directory_cluster;
	u16 fs_info_sector;
	u16 boot_sector_copy;
	u8 reserved1[12];
	u8 drive_number;
	u8 reserved2;
	u8 extended_signature;
	u32 serial_number;
	u8 volume_label[11];
	u8 fs_type[8];
	u8 boot_code[420];
	u16 boot_sector_signature;
} __packed;

/* Boot sector information for FAT12/FAT16/FAT32 */
struct fat_bootsec {
	u8 jump[3];
	u8 oem_name[8];
	u16 bytes_per_sector;
	u8 sectors_per_cluster;
	u16 reserved_sector_count;
	u8 number_of_fat;
	u16 root_entry_count;
	u16 total_sectors_16;
	u8 media_type;
	u16 sectors_per_fat;
	u16 sectors_per_track;
	u16 number_of_heads;
	u32 hidden_sector_count;
	u32 total_sectors_32;
	union {
		struct fat_bootsec_ext16 e16;
		struct fat_bootsec_ext32 e32;
	} ext;
} __packed;

/* Directory entry attributes */
#define	FAT_DIRENT_READONLY	0x01
#define	FAT_DIRENT_HIDDEN	0x02
#define	FAT_DIRENT_SYSTEM	0x04
#define	FAT_DIRENT_VOLLABLE	0x08
#define	FAT_DIRENT_SUBDIR	0x10
#define	FAT_DIRENT_ARCHIVE	0x20
#define	FAT_DIRENT_DEVICE	0x40
#define	FAT_DIRENT_UNUSED	0x80

/* Directory entry information for FAT12/FAT16/FAT32 */
struct fat_dirent {
	u8 dos_file_name[8];
	u8 dos_extension[3];
	u8 file_attributes;
	u8 reserved;
	u8 create_time_millisecs;
	u32 create_time_seconds:5;
	u32 create_time_minutes:6;
	u32 create_time_hours:5;
	u32 create_date_day:5;
	u32 create_date_month:4;
	u32 create_date_year:7;
	u32 laccess_date_day:5;
	u32 laccess_date_month:4;
	u32 laccess_date_year:7;
	u16 first_cluster_hi; /* For FAT16 this is ea_index */
	u32 lmodify_time_seconds:5;
	u32 lmodify_time_minutes:6;
	u32 lmodify_time_hours:5;
	u32 lmodify_date_day:5;
	u32 lmodify_date_month:4;
	u32 lmodify_date_year:7;
	u16 first_cluster_lo; /* For FAT16 first_cluster = first_cluster_lo */
	u32 file_size;
} __packed;

#define FAT_LONGNAME_ATTRIBUTE	0x0F
#define FAT_LONGNAME_SEQNO(s)	((s) & ~0x40)
#define FAT_LONGNAME_LASTSEQ(s) ((s) & 0x40)
#define FAT_LONGNAME_MINSEQ	1
#define FAT_LONGNAME_MAXSEQ	(VFS_MAX_NAME / 13)

/* Directory long filename information for FAT12/FAT16/FAT32 */
struct fat_longname {
	u8 seqno;
	u16 name_utf16_1[5];
	u8 file_attributes;
	u8 type;
	u8 checksum;
	u16 name_utf16_2[6];
	u16 first_cluster;
	u16 name_utf16_3[2];
} __packed;

/* Information for accessing a FAT file/directory. */
struct fatfs_node {
	/* Parent FAT control */
	struct fatfs_control *ctrl;

	/* Parent directory FAT node */
	struct fatfs_node *parent;
	u32 parent_dirent_off;
	u32 parent_dirent_len;

	/* Parent directory entry */
	struct fat_dirent dirent;

	/* First cluster */
	u32 first_cluster;

	/* Cached cluster */
	u8 *cached_data;
	u32 cached_cluster;
	bool cached_dirty;
};

#define FAT_TABLE_CACHE_SIZE		16
#define FAT_TABLE_CACHE_MASK		0x0000000F
#define FAT_TABLE_CACHE_INDEX(num)	((num) & FAT_TABLE_CACHE_MASK)

/* Information about a "mounted" FAT filesystem. */
struct fatfs_control {
	struct vmm_blockdev *bdev;

	/* FAT boot sector */
	struct fat_bootsec bsec;

	/* Frequently required boot sector info */
	u16 bytes_per_sector;
	u8 sectors_per_cluster;
	u8 number_of_fat;
	u32 bytes_per_cluster;
	u32 total_sectors;

	/* Derived FAT info */
	u32 first_fat_sector;
	u32 sectors_per_fat;
	u32 fat_sectors;

	u32 first_root_sector;
	u32 root_sectors;
	u32 first_root_cluster;

	u32 first_data_sector;
	u32 data_sectors;
	u32 data_clusters;

	/* FAT type (i.e. FAT12/FAT16/FAT32) */
	enum fat_types type;

	/* FAT table cache */
	struct vmm_mutex table_sector_lock[FAT_TABLE_CACHE_SIZE];
	bool table_sector_dirty[FAT_TABLE_CACHE_SIZE];
	u32 table_sector_num[FAT_TABLE_CACHE_SIZE];
	u8 *table_sector_buf;
};

/* 
 * Helper routines 
 */

static u64 fatfs_pack_timestamp(u32 year, u32 mon, u32 day, 
				u32 hour, u32 min, u32 sec)
{
	return vmm_wallclock_mktime(1980+year, mon, day, hour, min, sec);
}

static u64 fatfs_control_read_fat(struct fatfs_control *ctrl, 
				  u8 *buf, u64 pos, u32 len)
{
	u64 fat_base, rlen;
	u32 num, ind, i;

	if ((len != 2) && (len != 4)) {
		return 0;
	}

	if ((ctrl->sectors_per_fat * ctrl->bytes_per_sector) <= pos) {
		return 0;
	}

	fat_base = ctrl->first_fat_sector * ctrl->bytes_per_sector;

	num = udiv64(pos, ctrl->bytes_per_sector);
	ind = FAT_TABLE_CACHE_INDEX(num);

	vmm_mutex_lock(&ctrl->table_sector_lock[ind]);

	if (ctrl->table_sector_num[ind] != num) {
		if (ctrl->table_sector_dirty[ind]) {
			/* FIXME: Write back dirty table sector */

			ctrl->table_sector_dirty[ind] = FALSE;
		}

		rlen = vmm_blockdev_read(ctrl->bdev, 
			&ctrl->table_sector_buf[ind * ctrl->bytes_per_sector], 
			fat_base + num * ctrl->bytes_per_sector, 
			ctrl->bytes_per_sector);
		if (rlen != ctrl->bytes_per_sector) {
			vmm_mutex_unlock(&ctrl->table_sector_lock[ind]);
			return 0;
		}
		ctrl->table_sector_num[ind] = num;
	}

	i = (ind * ctrl->bytes_per_sector) + 
				(pos - (num * ctrl->bytes_per_sector));
	memcpy(buf, &ctrl->table_sector_buf[i], len);

	vmm_mutex_unlock(&ctrl->table_sector_lock[ind]);

	return len;
}

static bool fatfs_control_valid_cluster(struct fatfs_control *ctrl, u32 clust)
{
	switch (ctrl->type) {
	case FAT_TYPE_12:
		if ((clust <= FAT12_RESERVED1_CLUSTER) ||
		    (FAT12_RESERVED2_CLUSTER <= clust)) {
			return FALSE;
		}
		break;
	case FAT_TYPE_16:
		if ((clust <= FAT16_RESERVED1_CLUSTER) ||
		    (FAT16_RESERVED2_CLUSTER <= clust)) {
			return FALSE;
		}
		break;
	case FAT_TYPE_32:
		if ((clust <= FAT32_RESERVED1_CLUSTER) ||
		    (FAT32_RESERVED2_CLUSTER <= clust)) {
			return FALSE;
		}
		break;
	};
	return TRUE;
}

static int fatfs_control_next_cluster(struct fatfs_control *ctrl, 
				 	u32 current, u32 *next)
{
	u8 fat_entry_b[4];
	u32 fat_entry;
	u64 fat_offset, fat_rlen, rlen;

	switch (ctrl->type) {
	case FAT_TYPE_12:
		if (current % 2) {
			fat_offset = (current - 1) * 12 / 8 + 1;
		} else {
			fat_offset = current * 12 / 8;
		}
		fat_rlen = 2;
		break;
	case FAT_TYPE_16:
		fat_offset = current * 2;
		fat_rlen = 2;
		break;
	case FAT_TYPE_32:
		fat_offset = current * 4;
		fat_rlen = 4;
		break;
	default:
		return VMM_ENOENT;
	};

	rlen = fat_rlen;
	rlen = fatfs_control_read_fat(ctrl, &fat_entry_b[0], fat_offset, rlen);
	if (rlen != fat_rlen) {
		return VMM_EIO;
	}

	if (fat_rlen == 2) {
		fat_entry = ((u32)fat_entry_b[1] << 8) | 
			    ((u32)fat_entry_b[0]);
	} else {
		fat_entry = ((u32)fat_entry_b[3] << 24) |
			    ((u32)fat_entry_b[2] << 16) | 
			    ((u32)fat_entry_b[1] << 8) | 
			    ((u32)fat_entry_b[0]); 
	}

	if (ctrl->type == FAT_TYPE_12) {
		if (current % 2) { 
			fat_entry >>= 4;
		} else {
			fat_entry &= 0xFFF;
		}
	}

	if (!fatfs_control_valid_cluster(ctrl, fat_entry)) {
		return VMM_ENOENT;
	}

	*next = fat_entry;

	return VMM_OK;
}

static int fatfs_control_sync(struct fatfs_control *ctrl)
{
	u32 ind;

	for (ind = 0; ind < FAT_TABLE_CACHE_SIZE; ind++) {
		vmm_mutex_lock(&ctrl->table_sector_lock[ind]);

		if (ctrl->table_sector_dirty[ind]) {
			/* FIXME: Write back dirty table sector */

			ctrl->table_sector_dirty[ind] = FALSE;
		}

		vmm_mutex_unlock(&ctrl->table_sector_lock[ind]);
	}

	return VMM_OK;
}

static int fatfs_control_init(struct fatfs_control *ctrl,
				struct vmm_blockdev *bdev)
{
	u32 i;
	u64 rlen;
	struct fat_bootsec *bsec = &ctrl->bsec;

	/* Save underlying block device pointer */
	ctrl->bdev = bdev;

	/* Read boot sector from block device */
	rlen = vmm_blockdev_read(ctrl->bdev, (u8 *)bsec, 
				FAT_BOOTSECTOR_OFFSET, 
				sizeof(struct fat_bootsec));
	if (rlen != sizeof(struct fat_bootsec)) {
		return VMM_EIO;
	}

	/* Frequently required info */
	ctrl->bytes_per_sector = __le16(bsec->bytes_per_sector);
	ctrl->sectors_per_cluster = bsec->sectors_per_cluster;
	ctrl->number_of_fat = bsec->number_of_fat;
	ctrl->bytes_per_cluster = 
			ctrl->sectors_per_cluster * ctrl->bytes_per_sector;
	ctrl->total_sectors = __le16(bsec->total_sectors_16);
	if (!ctrl->total_sectors) {
		ctrl->total_sectors = __le32(bsec->total_sectors_32);
	}

	/* Calculate derived info assuming FAT12/FAT16 */
	ctrl->first_fat_sector = __le16(bsec->reserved_sector_count);
	ctrl->sectors_per_fat = __le16(bsec->sectors_per_fat);
	ctrl->fat_sectors = ctrl->number_of_fat * ctrl->sectors_per_fat;

	ctrl->first_root_sector = ctrl->first_fat_sector + ctrl->fat_sectors;
	ctrl->root_sectors = (__le16(bsec->root_entry_count) * 32) + 
						(ctrl->bytes_per_sector - 1);
	ctrl->root_sectors = 
			udiv32(ctrl->root_sectors, ctrl->bytes_per_sector);
	ctrl->first_root_cluster = 0;

	ctrl->first_data_sector = ctrl->first_root_sector + ctrl->root_sectors;
	ctrl->data_sectors = ctrl->total_sectors - ctrl->first_data_sector;
	ctrl->data_clusters = 
			udiv32(ctrl->data_sectors, ctrl->sectors_per_cluster);

	/* Determine FAT type */
	if (ctrl->data_clusters < 4085) {
		ctrl->type = FAT_TYPE_12;
	} else if (ctrl->data_clusters < 65525) {
		ctrl->type = FAT_TYPE_16;
	} else {
		ctrl->type = FAT_TYPE_32;
	}

	/* FAT type sanity check */
	switch (ctrl->type) {
	case FAT_TYPE_12:
		if (memcmp(bsec->ext.e16.fs_type,"FAT12",5)) {
			return VMM_ENOSYS;
		}
		break;
	case FAT_TYPE_16:
		if (memcmp(bsec->ext.e16.fs_type,"FAT16",5)) {
			return VMM_ENOSYS;
		}
		break;
	case FAT_TYPE_32:
		if (memcmp(bsec->ext.e32.fs_type,"FAT32",5)) {
			return VMM_ENOSYS;
		}
		break;
	default:
		return VMM_ENOSYS;
	}

	/* For FAT32, recompute derived info */
	if (ctrl->type == FAT_TYPE_32) {
		ctrl->first_fat_sector = __le16(bsec->reserved_sector_count);
		ctrl->sectors_per_fat = __le32(bsec->ext.e32.sectors_per_fat);
		ctrl->fat_sectors = ctrl->number_of_fat * ctrl->sectors_per_fat;

		ctrl->first_root_sector = 0;
		ctrl->root_sectors = 0;
		ctrl->first_root_cluster = 
				__le32(bsec->ext.e32.root_directory_cluster);

		ctrl->first_data_sector = 
				ctrl->first_fat_sector + ctrl->fat_sectors;
		ctrl->data_sectors = 
				ctrl->total_sectors - ctrl->first_data_sector;
		ctrl->data_clusters = 
			udiv32(ctrl->data_sectors, ctrl->sectors_per_cluster);
	}

	/* Initialize table sector cache */
	for (i = 0; i < FAT_TABLE_CACHE_SIZE; i++) {
		INIT_MUTEX(&ctrl->table_sector_lock[i]);
		ctrl->table_sector_dirty[i] = FALSE;
		ctrl->table_sector_num[i] = i;
	}
	ctrl->table_sector_buf = 
		vmm_zalloc(FAT_TABLE_CACHE_SIZE * ctrl->bytes_per_sector);
	if (!ctrl->table_sector_buf) {
		return VMM_ENOMEM;
	}

	/* Load table sector cache */
	rlen = vmm_blockdev_read(ctrl->bdev, ctrl->table_sector_buf, 
			ctrl->first_fat_sector * ctrl->bytes_per_sector, 
			FAT_TABLE_CACHE_SIZE * ctrl->bytes_per_sector);
	if (rlen != (FAT_TABLE_CACHE_SIZE * ctrl->bytes_per_sector)) {
		vmm_free(ctrl->table_sector_buf);
		return VMM_EIO;
	}

	return VMM_OK;
}

static int fatfs_control_exit(struct fatfs_control *ctrl)
{
	vmm_free(ctrl->table_sector_buf);

	return VMM_OK;
}

static u32 fatfs_node_read(struct fatfs_node *node, 
			   u64 pos, u32 len, char *buf) 
{
	int rc;
	u32 i, j;
	u32 cl_pos, cl_off, cl_num, cl_len;
	u64 rlen, roff;
	struct fatfs_control *ctrl = node->ctrl;

	if (!node->parent && ctrl->type != FAT_TYPE_32) {
		rlen = ctrl->bytes_per_sector * ctrl->fat_sectors;
		if (pos >= rlen) {
			return 0;
		}
		if ((pos + len) > rlen) {
			rlen = rlen - pos;
		} else {
			rlen = len;
		}
		roff = ctrl->first_root_sector * ctrl->bytes_per_sector + pos;
		return vmm_blockdev_read(ctrl->bdev, (u8 *)buf, roff, rlen);
	}

	i = 0;
	while (i < len) {
		/* Get the next cluster */
		if (i == 0) {
			cl_pos = udiv64(pos, ctrl->bytes_per_cluster); 
			cl_off = pos - cl_pos * ctrl->bytes_per_cluster;
			cl_num = node->first_cluster;
			for (j = 0; j < cl_pos; j++) {
				rc = fatfs_control_next_cluster(ctrl, cl_num, &cl_num);
				if (rc) {
					return 0;
				}
			}
			cl_len = ctrl->bytes_per_cluster - cl_off;
			cl_len = (cl_len < len) ? cl_len : len;
		} else {
			cl_pos++;
			cl_off = 0;
			rc = fatfs_control_next_cluster(ctrl, cl_num, &cl_num);
			if (rc) {
				return i;
			}
			cl_len = (ctrl->bytes_per_cluster < len) ? 
						ctrl->bytes_per_cluster : len;
		}

		/* Make sure node cluster cache is updated */
		if (!node->cached_data) {
			node->cached_data = vmm_malloc(ctrl->bytes_per_cluster);
			if (!node->cached_data) {
				return 0;
			}
		}
		if (node->cached_cluster != cl_num) {
			if (node->cached_dirty) {
				/* FIXME: Write back dirty node cluster */

				node->cached_dirty = FALSE;
			}

			node->cached_cluster = cl_num;

			roff = (ctrl->first_data_sector * ctrl->bytes_per_sector) + 
				((cl_num - 2) * ctrl->bytes_per_cluster);
			rlen = vmm_blockdev_read(ctrl->bdev, node->cached_data, 
						roff, ctrl->bytes_per_cluster);
			if (rlen != ctrl->bytes_per_cluster) {
				return i;
			}
		}

		/* Read from node cluster cache */
		memcpy(buf, &node->cached_data[cl_off], cl_len);

		/* Update iteration */
		i += cl_len;
		buf += cl_len;
	}

	return i;
}

static u64 fatfs_node_get_size(struct fatfs_node *node)
{
	if (!node->parent) {
		return 0;
	}

	return __le32(node->dirent.file_size);
}

static int fatfs_node_sync(struct fatfs_node *node)
{
	if (node->cached_dirty) {
		/* FIXME: Write back dirty node cluster */

		node->cached_dirty = FALSE;
	}

	return VMM_OK;
}

static int fatfs_node_init(struct fatfs_node *node)
{
	node->ctrl = NULL;
	node->parent = NULL;
	node->parent_dirent_off = 0;
	node->parent_dirent_len = 0;
	memset(&node->dirent, 0, sizeof(struct fat_dirent));
	node->first_cluster = 0;

	node->cached_cluster = 0;
	node->cached_data = NULL;
	node->cached_dirty = FALSE;

	return VMM_OK;
}

static int fatfs_node_exit(struct fatfs_node *node)
{
	if (node->cached_data) {
		vmm_free(node->cached_data);
		node->cached_data = NULL;
	}

	return VMM_OK;
}

static int fatfs_node_find_dirent(struct fatfs_node *dnode, 
				  const char *name,
				  struct fat_dirent *dent, 
				  u32 *dent_off, u32 *dent_len)
{
	u32 i, rlen, len, lfn_off, lfn_len;
	struct fat_longname lfn;
	char lname[VFS_MAX_NAME];
	u64 off;

	lfn_off = 0;
	lfn_len = 0;
	memset(lname, 0, sizeof(lname));

	off = 0;
	while (1) {
		rlen = fatfs_node_read(dnode, off, 
				sizeof(struct fat_dirent), (char *)dent);
		if (rlen != sizeof(struct fat_dirent)) {
			return VMM_EIO;
		}

		if (dent->dos_file_name[0] == 0x0) {
			return VMM_ENOENT;
		}

		off += sizeof(struct fat_dirent);

		if ((dent->dos_file_name[0] == 0xE5) ||
		    (dent->dos_file_name[0] == 0x2E)) {
			continue;
		}

		if (dent->file_attributes == FAT_LONGNAME_ATTRIBUTE) {
			memcpy(&lfn, dent, sizeof(struct fat_longname));
			if (FAT_LONGNAME_LASTSEQ(lfn.seqno)) {
				lfn.seqno = FAT_LONGNAME_SEQNO(lfn.seqno);
				lfn_off = off - sizeof(struct fat_dirent);
				lfn_len = lfn.seqno * sizeof(struct fat_longname);
				memset(lname, 0, sizeof(lname));
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

		if (dent->file_attributes & FAT_DIRENT_VOLLABLE) {
			continue;
		}

		if (!strlen(lname)) {
			lfn_off = off - sizeof(struct fat_dirent);
			lfn_len = 0;
			i = 8;
			while (i && (dent->dos_file_name[i-1] == ' ')) {
				dent->dos_file_name[i-1] = '\0';
				i--;
			}
			i = 3;
			while (i && (dent->dos_extension[i-1] == ' ')) {
				dent->dos_extension[i-1] = '\0';
				i--;
			}
			memcpy(lname, dent->dos_file_name, 8);
			if (dent->dos_extension[0] != '\0') {
				len = strlen(lname);
				lname[len] = '.';
				lname[len + 1] = dent->dos_extension[0];
				lname[len + 2] = dent->dos_extension[1];
				lname[len + 3] = dent->dos_extension[2];
				lname[len + 4] = '\0';
			}
		}

		if (!strncmp(lname, name, VFS_MAX_NAME)) {
			*dent_off = lfn_off;
			*dent_len = sizeof(struct fat_dirent) + lfn_len;
			break;
		}

		lfn_off = off;
		lfn_len = 0;
		memset(lname, 0, sizeof(lname));
	}

	return VMM_OK;
}

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
