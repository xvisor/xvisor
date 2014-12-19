/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
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
 * @file m25p80_blockdev.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief MTD SPI block device driver for ST M25Pxx (and similar) flash
 */

#include <linux/spi/spi.h>
#include <linux/mtd/mtd.h>
#include <linux/device.h>
#include <block/vmm_blockdev.h>

#include "m25p80.h"


static void m25p_erase_callback(struct erase_info *info)
{
}

static int m25p_erase(struct vmm_request_queue *rq,
		      struct vmm_request *r,
		      physical_addr_t off,
		      physical_size_t len,
		      struct mtd_info *mtd)
{
	struct erase_info info;
	unsigned int retlen = 0;

	info.mtd = mtd;
	info.addr = off;
	info.len = len;
	info.callback = m25p_erase_callback;

	if (mtd_erase(mtd, &info)) {
		dev_err(&r->bdev->dev, "Erasing at 0x%08X failed\n", off);
		goto error;
	}

	if (mtd_write(mtd, off, len, &retlen, r->data)) {
		dev_err(&r->bdev->dev, "Writing at 0x%08X failed\n", off);
		goto error;
	}

	if (retlen < len) {
		dev_warn(&r->bdev->dev, "Only 0x%X/0x%X bytes have been "
			 "written at 0x%08X\n", retlen, len, off);
		goto error;
	}
	vmm_blockdev_complete_request(r);
	return VMM_OK;

error:
	vmm_blockdev_fail_request(r);
	return VMM_OK;
}

static int m25p_make_request(struct vmm_request_queue *rq,
			     struct vmm_request *r)
{
	unsigned int retlen = 0;
	physical_addr_t off = 0;
	physical_size_t len = 0;
	struct m25p *flash = rq->priv;

	off = r->lba << flash->mtd.erasesize_shift;
	len = r->bcnt << flash->mtd.erasesize_shift;
	switch (r->type) {
	case VMM_REQUEST_READ:
		mtd_read(&flash->mtd, off, len, &retlen, r->data);

		if (retlen < len) {
			vmm_blockdev_fail_request(r);
		}
		else {
			vmm_blockdev_complete_request(r);
		}
		break;
	case VMM_REQUEST_WRITE:
		while (mtd_block_isbad(&flash->mtd, off)) {
			vmm_printf("Block at 0x%X is bad, skipping...\n",
				   off);
			off += flash->mtd.erasesize;
		}
		return m25p_erase(rq, r, off, len, &flash->mtd);

		break;
	default:
		vmm_blockdev_fail_request(r);
		break;
	};

	return VMM_OK;
}

static int m25p_abort_request(struct vmm_request_queue *rq,
			      struct vmm_request *r)
{
	/* Do nothing to abort */
	return VMM_OK;
}

int m25p_register_blockdev(struct vmm_device *dev)
{
	int			err = 0;
	struct spi_device	*spi = vmm_devdrv_get_data(dev);
	struct vmm_blockdev	*bdev = NULL;
	struct m25p		*flash = NULL;

	if (!spi)
		return VMM_EFAIL;

	flash = spi_get_drvdata(spi);

	if (NULL == (bdev = vmm_blockdev_alloc())) {
		dev_err(dev, "Failed to allocate blockdevice\n");
		return VMM_ENOMEM;
	}

	/* Setup block device instance */
	strncpy(bdev->name, dev->name, VMM_FIELD_NAME_SIZE);
	strncpy(bdev->desc, "MTD m25p80 NOR flash block device",
		VMM_FIELD_DESC_SIZE);
	bdev->dev.parent = dev;
	bdev->flags = VMM_BLOCKDEV_RW;
	bdev->start_lba = 0;
	bdev->num_blocks = flash->mtd.size >> flash->mtd.erasesize_shift;
	bdev->block_size = flash->mtd.erasesize;

	/* Setup request queue for block device instance */
	bdev->rq = vmm_zalloc(sizeof(struct vmm_request_queue));
	if (!bdev->rq) {
		vmm_blockdev_free(flash->blockdev);
		return VMM_ENOMEM;
	}
	INIT_REQUEST_QUEUE(bdev->rq);
	bdev->rq->make_request = m25p_make_request;
	bdev->rq->abort_request = m25p_abort_request;
	bdev->rq->priv = flash;

	/* Register block device instance */
	if (VMM_OK != (err = vmm_blockdev_register(bdev))) {
		vmm_free(flash->blockdev->rq);
		vmm_blockdev_free(flash->blockdev);
		dev_err(dev, "Failed to register blockdev\n");
		return err;
	}
	flash->blockdev = bdev;

	return VMM_OK;
}

int m25p_unregister_blockdev(struct vmm_device *dev)
{
	struct spi_device *spi = vmm_devdrv_get_data(dev);
	struct m25p	*flash = NULL;

	if (!spi)
		return 1;
	flash = spi_get_drvdata(spi);

	if (vmm_blockdev_unregister(flash->blockdev)) {
		return VMM_EFAIL;
	}
	vmm_free(flash->blockdev->rq);
	vmm_blockdev_free(flash->blockdev);

	return VMM_OK;
}
