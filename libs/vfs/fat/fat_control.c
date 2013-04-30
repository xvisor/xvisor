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
 * @file fat_control.c
 * @author Anup Patel. (anup@brainfault.org)
 * @brief source file for FAT control functions
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_wallclock.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>

#include "fat_control.h"

u64 fatfs_pack_timestamp(u32 year, u32 mon, u32 day, 
			 u32 hour, u32 min, u32 sec)
{
	return vmm_wallclock_mktime(1980+year, mon, day, hour, min, sec);
}

u64 fatfs_control_read_fat(struct fatfs_control *ctrl, 
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

bool fatfs_control_valid_cluster(struct fatfs_control *ctrl, u32 clust)
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

int fatfs_control_next_cluster(struct fatfs_control *ctrl, 
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

int fatfs_control_sync(struct fatfs_control *ctrl)
{
	int rc;
	u32 ind;

	for (ind = 0; ind < FAT_TABLE_CACHE_SIZE; ind++) {
		vmm_mutex_lock(&ctrl->table_sector_lock[ind]);

		if (ctrl->table_sector_dirty[ind]) {
			/* FIXME: Write back dirty table sector */

			ctrl->table_sector_dirty[ind] = FALSE;
		}

		vmm_mutex_unlock(&ctrl->table_sector_lock[ind]);
	}

	/* Flush cached data in device request queue */
	rc = vmm_blockdev_flush_cache(ctrl->bdev);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

int fatfs_control_init(struct fatfs_control *ctrl, struct vmm_blockdev *bdev)
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

	/* Get bytes_per_sector and sector_per_cluster */
	ctrl->bytes_per_sector = __le16(bsec->bytes_per_sector);
	ctrl->sectors_per_cluster = bsec->sectors_per_cluster;

	/* Sanity check bytes_per_sector and sector_per_cluster */
	if (!ctrl->bytes_per_sector || !ctrl->sectors_per_cluster) {
		return VMM_ENOSYS;
	}

	/* Frequently required info */
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

int fatfs_control_exit(struct fatfs_control *ctrl)
{
	vmm_free(ctrl->table_sector_buf);

	return VMM_OK;
}

