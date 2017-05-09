/**
 * Copyright (C) 2014 Anup Patel.
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
 * @file scsi_disk.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief SCSI disk library
 */

#include <vmm_error.h>
#include <vmm_cache.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_devdrv.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/scsi_disk.h>

#define MODULE_DESC			"SCSI Disk Library"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(SCSI_DISK_IPRIORITY)
#define	MODULE_INIT			NULL
#define	MODULE_EXIT			NULL

#undef _DEBUG
#ifdef _DEBUG
#define DPRINTF(msg...)			vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

static int scsi_disk_rq_read(struct vmm_blockrq *brq,
			     struct vmm_request *r, void *priv)
{
	void *data;
	int rc, retry;
	unsigned long lba, bcnt, blksz;
	unsigned short blks;
	struct scsi_request srb;
	struct scsi_disk *disk = priv;

	bcnt = (unsigned long)r->bcnt;
	data = r->data;
	lba = (unsigned long)r->lba;
	blksz = disk->info.blksz;

	while (bcnt) {
		blks = (disk->blks_per_xfer < bcnt) ?
					disk->blks_per_xfer : bcnt;

		retry = 3;
		rc = VMM_OK;
		while (retry) {
			INIT_SCSI_REQUEST(&srb, disk->info.lun,
					 data, blks * blksz);
			rc = scsi_read10(&srb, lba, blks,
					 disk->tr, disk->tr_priv);
			if (rc == VMM_OK) {
				break;
			}

			rc = scsi_request_sense(&srb,
						disk->tr, disk->tr_priv);
			if (rc) {
				return rc;
			}
			if ((srb.sense_buf[2] == 0x02) &&
			    (srb.sense_buf[12] == 0x3a)) {
				return VMM_ENODEV;
			}

			retry--;
		}
		if (!retry && rc != VMM_OK) {
			return rc;
		}

		lba += blks;
		bcnt -= blks;
		data += blks * blksz;
	}

	return VMM_OK;
}

static int scsi_disk_rq_write(struct vmm_blockrq *brq,
			      struct vmm_request *r, void *priv)
{
	void *data;
	int rc, retry;
	unsigned long lba, bcnt, blksz;
	unsigned short blks;
	struct scsi_request srb;
	struct scsi_disk *disk = priv;

	bcnt = (unsigned long)r->bcnt;
	data = r->data;
	lba = (unsigned long)r->lba;
	blksz = disk->info.blksz;

	while (bcnt) {
		blks = (disk->blks_per_xfer < bcnt) ?
					disk->blks_per_xfer : bcnt;

		retry = 3;
		rc = VMM_OK;
		while (retry) {
			INIT_SCSI_REQUEST(&srb, disk->info.lun,
					  data, blks * blksz);
			rc = scsi_write10(&srb, lba, blks,
					  disk->tr, disk->tr_priv);
			if (rc == VMM_OK) {
				break;
			}

			rc = scsi_request_sense(&srb,
						disk->tr, disk->tr_priv);
			if (rc) {
				return rc;
			}
			if ((srb.sense_buf[2] == 0x02) &&
			    (srb.sense_buf[12] == 0x3a)) {
				return VMM_ENODEV;
			}

			retry--;
		}
		if (!retry && rc != VMM_OK) {
			return rc;
		}

		lba += blks;
		bcnt -= blks;
		data += blks * blksz;
	}

	return VMM_OK;
}

static void scsi_disk_rq_flush(struct vmm_blockrq *brq, void *priv)
{
	/* Nothing to do here. */
}

struct scsi_disk *scsi_create_disk(const char *name,
				   unsigned int lun,
				   unsigned int max_pending,
				   unsigned short blks_per_xfer,
				   struct vmm_device *dev,
				   struct scsi_transport *tr, void *tr_priv)
{
	int err = 0;
	struct scsi_disk *disk = NULL;

	if (!name || !max_pending || !blks_per_xfer ||
	    !tr || !tr->transport || !tr->reset) {
		return VMM_ERR_PTR(VMM_EINVALID);
	}

	/* Reset SCSI transport */
	err = scsi_reset(tr, tr_priv);
	if (err) {
		return VMM_ERR_PTR(err);
	}

	/* Alloc SCSI disk */
	disk = vmm_zalloc(sizeof(*disk));
	if (!disk) {
		return VMM_ERR_PTR(VMM_ENOMEM);
	}
	disk->blks_per_xfer = blks_per_xfer;
	disk->tr = tr;
	disk->tr_priv = tr_priv;

	/* Get SCSI info */
	err = scsi_get_info(&disk->info, lun, disk->tr, disk->tr_priv);
	if (err) {
		vmm_free(disk);
		return VMM_ERR_PTR(err);
	}

	/* Alloc block device instance */
	if (!(disk->bdev = vmm_blockdev_alloc())) {
		vmm_free(disk);
		return VMM_ERR_PTR(VMM_ENOMEM);
	}

	/* Setup block device instance */
	strncpy(disk->bdev->name, name, sizeof(disk->bdev->name));
	vmm_snprintf(disk->bdev->desc, sizeof(disk->bdev->desc),
		     "%s %s %s", disk->info.vendor,
		     disk->info.product, disk->info.revision);
	disk->bdev->dev.parent = dev;
	disk->bdev->flags = (disk->info.readonly) ?
				VMM_BLOCKDEV_RW : VMM_BLOCKDEV_RDONLY;
	disk->bdev->start_lba = 0;
	disk->bdev->num_blocks = disk->info.capacity;
	disk->bdev->block_size = disk->info.blksz;

	/* Setup request queue for block device instance */
	disk->brq = vmm_blockrq_create(name, max_pending, FALSE,
				       scsi_disk_rq_read,
				       scsi_disk_rq_write, NULL,
				       scsi_disk_rq_flush, disk);
	if (!disk->brq) {
		vmm_blockdev_free(disk->bdev);
		vmm_free(disk);
		return VMM_ERR_PTR(VMM_ENOMEM);
	}
	disk->bdev->rq = vmm_blockrq_to_rq(disk->brq);

	/* Register block device instance */
	if ((err = vmm_blockdev_register(disk->bdev))) {
		vmm_blockrq_destroy(disk->brq);
		vmm_blockdev_free(disk->bdev);
		vmm_free(disk);
		return VMM_ERR_PTR(err);
	}

	return disk;
}
VMM_EXPORT_SYMBOL(scsi_create_disk);

int scsi_destroy_disk(struct scsi_disk *disk)
{
	if (!disk) {
		return VMM_EINVALID;
	}

	vmm_blockdev_unregister(disk->bdev);
	vmm_blockrq_destroy(disk->brq);
	vmm_blockdev_free(disk->bdev);
	vmm_free(disk);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(scsi_destroy_disk);

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
