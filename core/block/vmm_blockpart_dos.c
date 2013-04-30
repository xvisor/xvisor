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
 * @file vmm_blockpart_dos.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for IBM PC DOS compatible partitions
 *
 * This is the default partition style that is always available with
 * block device partition managment.
 * 
 * Newer partition styles are generally implemented as an extension under
 * IBM PC DOS style primary partitions.
 */

#include <vmm_error.h>
#include <vmm_compiler.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_modules.h>
#include <block/vmm_blockpart.h>

#define MODULE_DESC			"IBM PC DOS Style Partitions"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_BLOCKPART_IPRIORITY+1)
#define	MODULE_INIT			vmm_blockpart_dos_init
#define	MODULE_EXIT			vmm_blockpart_dos_exit

#undef DOS_DEBUG

#ifdef DOS_DEBUG
#define debug(x...)			vmm_printf(x)
#else
#define debug(x...)
#endif

#define DOS_MBR_SIGN_OFFSET		0x1FE
#define DOS_MBR_SIGN_VALUE		0xAA55
#define DOS_MBR_PARTTBL_OFFSET		0x1BE

/* Enumeration of MBR partition status */
enum dos_partition_status {
	DOS_MBR_PARTITON_NONBOOTABLE=0x00,
	DOS_MBR_PARTITION_BOOTABLE=0x80
};

/* Enumeration of MBR partition types */
enum dos_partition_types {
	DOS_MBR_PARTITION_EMPTY=0x00,
	DOS_MBR_PARTITION_FAT12=0x01,
	DOS_MBR_PARTITION_XENIX_ROOT=0x02,
	DOS_MBR_PARTITION_XENIX_USR=0x03,
	DOS_MBR_PARTITION_FAT16_32M=0x04,
	DOS_MBR_PARTITION_EXTENDED=0x05,
	DOS_MBR_PARTITION_FAT16=0x06,
	DOS_MBR_PARTITION_NTFS=0x07,
	DOS_MBR_PARTITION_AIX=0x08,
	DOS_MBR_PARTITION_AIX_BOOTABLE=0x09,
	DOS_MBR_PARTITION_OS2_BOOT_MANAGER=0x0A,
	DOS_MBR_PARTITION_FAT32=0x0B,
	DOS_MBR_PARTITION_FAT32_LBA=0x0C,
	DOS_MBR_PARTITION_FAT16_LBA=0x0E,
	DOS_MBR_PARTITION_FAT16_EXTENDED=0x0F,
	DOS_MBR_PARTITION_OPUS=0x10,
	DOS_MBR_PARTITION_FAT12_HIDDEN=0x11,
	DOS_MBR_PARTITION_COMPAQ_DIAG=0x12,
	DOS_MBR_PARTITION_FAT16_HIDDEN=0x14,
	DOS_MBR_PARTITION_NTFS_HIDDEN=0x17,
	DOS_MBR_PARTITION_FAT32_HIDDEN=0x1B,
	DOS_MBR_PARTITION_FAT32_HIDDEN_LBA=0x1C,
	DOS_MBR_PARTITION_FAT16_HIDDEN_LBA=0x1D,
	DOS_MBR_PARTITION_XOSL_FS=0x78,
	DOS_MBR_PARTITION_LINUX_SWAP=0x82,
	DOS_MBR_PARTITION_LINUX_NATIVE=0x83,
	DOS_MBR_PARTITION_GNU_LINUX_EXTENDED=0x85,
	DOS_MBR_PARTITION_LEGACY_FT_FAT16=0x86,
	DOS_MBR_PARTITION_LEGACY_FT_NTFS=0x87,
	DOS_MBR_PARTITION_GNU_LINUX_PLAINTEXT=0x88,
	DOS_MBR_PARTITION_GNU_LINUX_LVM=0x89,
	DOS_MBR_PARTITION_LEGACY_FT_FAT32=0x8B,
	DOS_MBR_PARTITION_LEGACY_FT_FAT32_LBA=0x8C,
	DOS_MBR_PARTITION_UNKNOWN_LINUX_LVM=0x8E,
	DOS_MBR_PARTITION_BSD_SLICE=0xA5,
	DOS_MBR_PARTITION_RAW=0xDA,
	DOS_MBR_PARTITION_BOOTIT=0xDF,
	DOS_MBR_PARTITION_BFS=0xEB,
	DOS_MBR_PARTITION_EFI_GPT=0xEE,
	DOS_MBR_PARTITION_INTEL_EFI=0xEF,
	DOS_MBR_PARTITION_VMFS=0xFB,
	DOS_MBR_PARTITION_VMKCORE=0xFC,
	DOS_MBR_PARTITION_LINUX_RAID=0xFD
};

/* MBR partition table entry */
struct dos_partition {
	u8 status;
	u8 chs_first[3];
	u8 type;
	u8 chs_last[3];
	u32 lba_start;
	u32 sector_count;
} __packed;

static void dos_process_extended_part(struct vmm_blockdev *bdev,
				      struct dos_partition *parent)
{
	int rc;
	u16 i, sign;
	u64 read, addr, rel = 0;
	struct dos_partition part[2];

	while (1) {
		/* Print debug info */
		debug("%s: extended partition\n", bdev->name);
		debug("%s: status=0x%02x type=0x%02x\n", 
		      bdev->name, part->status, part->type);
		debug("%s: lba_start=0x%08x sector_count=0x%08x\n", 
		      bdev->name, part->lba_start, part->sector_count);

		/* Check for DOS MBR signature */
		addr = (parent->lba_start + rel) * bdev->block_size;
		addr +=	DOS_MBR_SIGN_OFFSET;
		read = vmm_blockdev_read(bdev, (u8 *)&sign, addr, sizeof(u16));
		if (read != sizeof(u16)) {
			break;
		}
		sign = vmm_le16_to_cpu(sign);
		if (sign != DOS_MBR_SIGN_VALUE) {
			break;
		}

		/* Retreive MBR partition table */
		addr = (parent->lba_start + rel) * bdev->block_size;
		addr +=	DOS_MBR_PARTTBL_OFFSET;
		read = vmm_blockdev_read(bdev, (u8 *)&part[0], 
							addr, sizeof(part));
		if (read != sizeof(part)) {
			break;
		}
		for (i = 0; i < 2; i++) {
			part[i].lba_start = vmm_le32_to_cpu(part[i].lba_start);
			part[i].sector_count = 
					vmm_le32_to_cpu(part[i].sector_count);
		}

		/* Sanity check on first entry */
		if (part[0].type == DOS_MBR_PARTITION_EMPTY) {
			break;
		}
		addr = parent->lba_start + rel + part[0].lba_start;
		if (addr < parent->lba_start) {
			break;
		}
		addr += part[0].sector_count;
		if ((parent->lba_start + parent->sector_count) < addr) {
			break;
		}

		/* Process first entry */
		addr = parent->lba_start + rel + part[0].lba_start;
		rc = vmm_blockdev_add_child(bdev, addr, part[0].sector_count);
		if (rc) {
			vmm_printf("%s: failed to add extended partition "
				   "(error %d)\n", bdev->name, rc);
			return;
		}

		/* Sanity check on second entry */
		if (part[1].type == DOS_MBR_PARTITION_EMPTY) {
			break;
		}

		/* Update rel based on second entry */
		rel = part[1].lba_start;
	}
}

static void dos_process_primary_part(struct vmm_blockdev *bdev,
				     struct dos_partition *part)
{
	int rc;

	/* Print debug info */
	debug("%s: primary partition\n", bdev->name);
	debug("%s: status=0x%02x type=0x%02x\n", 
	      bdev->name, part->status, part->type);
	debug("%s: lba_start=0x%08x sector_count=0x%08x\n", 
	      bdev->name, part->lba_start, part->sector_count);

	/* Add primary partition as child block device */
	rc = vmm_blockdev_add_child(bdev, part->lba_start, part->sector_count);
	if (rc) {
		vmm_printf("%s: failed to add primary partition (error %d)\n", 
			   bdev->name, rc);
	}
}

static int dos_parse_part(struct vmm_blockdev *bdev)
{
	u64 read;
	u16 i, sign, process_count;
	struct dos_partition part[4];

	/* Check for DOS MBR signature */
	read = vmm_blockdev_read(bdev, (u8 *)&sign, 
				 DOS_MBR_SIGN_OFFSET, sizeof(u16));
	if (read != sizeof(u16)) {
		return VMM_EIO;
	}
	sign = vmm_le16_to_cpu(sign);
	if (sign != DOS_MBR_SIGN_VALUE) {
		return VMM_ENOSYS;
	}

	/* Retreive MBR partition table */
	read = vmm_blockdev_read(bdev, (u8 *)&part[0], 
				 DOS_MBR_PARTTBL_OFFSET, sizeof(part));
	if (read != sizeof(part)) {
		return VMM_EIO;
	}
	for (i = 0; i < 4; i++) {
		part[i].lba_start = vmm_le32_to_cpu(part[i].lba_start);
		part[i].sector_count = vmm_le32_to_cpu(part[i].sector_count);
	}

	/* Process each entry of MBR partition table */
	process_count = 0;
	for (i = 0; i < 4; i++) {
		/* Skip empty partition */
		if (part[i].type == DOS_MBR_PARTITION_EMPTY) {
			continue;
		}

		/* Skip EFI_GPT and INTEL_EFI partition type because, this
		 * partition style are an extension to the IBM PC DOS style.
		 */
		if ((part[i].type == DOS_MBR_PARTITION_EFI_GPT) ||
		    (part[i].type == DOS_MBR_PARTITION_INTEL_EFI)) {
			continue;
		}
		
		/* Process primary and extended partitions */
		if ((part[i].type == DOS_MBR_PARTITION_EXTENDED) ||
		    (part[i].type == DOS_MBR_PARTITION_FAT16_EXTENDED) ||
		    (part[i].type == DOS_MBR_PARTITION_GNU_LINUX_EXTENDED)) {
			dos_process_extended_part(bdev, &part[i]);
		} else {
			dos_process_primary_part(bdev, &part[i]);
		}

		/* Increment process count */
		process_count++;
	}

	/* Failure if we did not process any MBR partition */
	if (!process_count) {
		return VMM_ENOENT;
	}

	return VMM_OK;
}

static struct vmm_blockpart_manager dos = {
	.sign = 0x1,
	.name = "DOS Partitions",
	.parse_part = dos_parse_part,
};

static int __init vmm_blockpart_dos_init(void)
{
	return vmm_blockpart_manager_register(&dos);
}

static void __exit vmm_blockpart_dos_exit(void)
{
	vmm_blockpart_manager_unregister(&dos);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
