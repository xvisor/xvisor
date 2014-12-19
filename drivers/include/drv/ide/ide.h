/**
 * Copyright (c) 2014 Himanshu Chauhan.
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
 * @file ide.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief IDE/ATA maintenance related defines.
 */
#ifndef	_IDE_H
#define _IDE_H

#include <vmm_completion.h>
#include <vmm_threads.h>
#include <vmm_mutex.h>

#define MAX_IDE_DRIVES		4
#define MAX_IDE_CHANNELS	2
#define MAX_IDE_DRIVES_PER_CHAN	(MAX_IDE_DRIVES / MAX_IDE_CHANNELS)

#define IDE_ATA			0x00
#define IDE_ATAPI		0x01

#define PRIMARY_ATA_CHANNEL_IRQ		14
#define SECONDARY_ATA_CHANNEL_IRQ	15

struct ide_drive;

struct ide_channel {
	u16 base;	/* i/o base */
	u16 ctrl;	/* control base */
	u16 bmide;	/* bus master ide */
	u8  int_en;	/* no interrupt */
	u8  id;		/* channel number */
};

struct ide_drive_ops {
        u32 (*block_read)(struct ide_drive *drive, u64 start_lba,
			  u32 blkcnt, void *buffer);
        u32 (*block_write)(struct ide_drive *drive, u64 start_lba,
			   u32 blkcnt, const void *buffer);
        u32 (*block_erase)(struct ide_drive *drive, u64 start_lba,
			   u32 blkcnt);
};

struct ide_drive {
	struct dlist link;		/* For the list of detected drives */
	struct vmm_device *dev;
	u8 present;			/* If this drive is present */
	u8 drive;			/* drive number on the channel */
        u8 type;			/* device type ATA/ATAPI*/
	u16 signature;			/* drive signature */
	u16 capabilities;		/* drive capabilities */
	u32 cmd_set;			/* command sets supported */
	u32 size;			/* size in sectors */
	u32 blk_size;			/* Block size of drive CDROM: 2048 */
	struct ide_channel *channel;	/* channel on which drive is connected. */

        u8 lba48_enabled;		/* device can use 48bit addr (ATA/ATAPI v7) */

        u32 lba;			/* number of blocks */
        u32 blksz;			/* block size */
        u8 model[41];			/* IDE model, SCSI Vendor */

        struct dlist io_list;		/* IO request list */
        vmm_spinlock_t io_list_lock;	/* IO list lock */
	struct vmm_mutex lock;

        struct vmm_thread *io_thread;	/* IO thread */
        struct vmm_completion io_avail;	/* To wake up I/O thread */
	struct vmm_completion dev_intr; /* Device reported interrupt */
	struct vmm_blockdev *bdev;	/* Block device associated to this drive */

	struct ide_drive_ops io_ops;	/* Host operations */
        void            *priv;		/* driver private struct pointer */
};

struct ide_host_controller {
	u32 vendor_id;
	u32 device_id;
	u32 class_id;
	u32 subclass_id;
	u64 bar0;
	u64 bar1;
	u64 bar2;
	u64 bar3;
	u64 bar4;
	struct ide_drive ide_drives[MAX_IDE_DRIVES];
	struct ide_channel ide_channels[MAX_IDE_CHANNELS];
	u32 nr_drives_present;
};

/*
 * Function Prototypes
 */
extern u32 ide_write_sectors(struct ide_drive *drive,
			     u64 lba, u32 numsects, const void *buffer);
extern u32 ide_read_sectors(struct ide_drive *drive,
			    u64 lba, u32 numsects, void *buffer);
extern int ide_initialize(struct ide_host_controller *controller);
extern int ide_add_drive(struct ide_drive *drive);

#endif /* _IDE_H */
