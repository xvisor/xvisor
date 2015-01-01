/**
 * Copyright (c) 2014 Anup Patel.
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
 * @file scsi_disk.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Interface header for SCSI disk library.
 */

#ifndef __SCSI_DISK_H__
#define __SCSI_DISK_H__

#include <vmm_types.h>
#include <libs/scsi.h>
#include <block/vmm_blockdev.h>
#include <block/vmm_blockrq_nop.h>

#define SCSI_DISK_IPRIORITY		(SCSI_IPRIORITY + \
					 VMM_BLOCKDEV_CLASS_IPRIORITY + 1)

struct scsi_disk {
	unsigned short blks_per_xfer;

	struct scsi_transport *tr;
	void *tr_priv;

	struct scsi_info info;

	struct vmm_blockdev *bdev;
	struct vmm_blockrq_nop *rqnop;
};

struct scsi_disk *scsi_create_disk(const char *name,
				   unsigned int lun,
				   unsigned int max_pending,
				   unsigned short blks_per_xfer,
				   struct vmm_device *dev,
				   struct scsi_transport *tr, void *tr_priv);

int scsi_destroy_disk(struct scsi_disk *disk);

#endif /* __SCSI_H__ */
