/*
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 * Modified by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * @file mtdblock.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief A very simple version of MTD block device part.
 */

#include <vmm_stdio.h>
#include <block/vmm_blockdev.h>
#include <block/vmm_blockrq.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include "mtdcore.h"

static void mtd_blockdev_erase_callback(struct erase_info *info)
{
	/* Nothing to do here. */
}

static int mtd_blockdev_erase_write(struct vmm_request *r,
				    physical_addr_t off,
				    physical_size_t len,
				    struct mtd_info *mtd)
{
	struct erase_info info;
	unsigned int retlen = 0;

	info.mtd = mtd;
	info.addr = off;
	info.len = len;
	info.callback = mtd_blockdev_erase_callback;

	if (mtd_erase(mtd, &info)) {
		dev_err(&r->bdev->dev, "Erasing at 0x%08X failed\n", off);
		return VMM_EIO;
	}

	if (mtd_write(mtd, off, len, &retlen, r->data)) {
		dev_err(&r->bdev->dev, "Writing at 0x%08X failed\n", off);
		return VMM_EIO;
	}

	if (retlen < len) {
		dev_warn(&r->bdev->dev, "Only 0x%X/0x%X bytes have been "
			 "written at 0x%08X\n", retlen, len, off);
		return VMM_EIO;
	}
	return VMM_OK;
}

int mtd_blockdev_read(struct vmm_blockrq *brq,
	      struct vmm_request *r, void *priv)
{
	struct mtd_info *mtd = priv;
	unsigned int retlen = 0;

	physical_addr_t off = r->lba << mtd->erasesize_shift;
	physical_size_t len = r->bcnt << mtd->erasesize_shift;

	mtd_read(mtd, off, len, &retlen, r->data);
	if (retlen < len) {
		return VMM_EIO;
	}

	return VMM_OK;
}

int mtd_blockdev_write(struct vmm_blockrq *brq,
		       struct vmm_request *r, void *priv)
{
	struct mtd_info *mtd = priv;
	physical_addr_t off = r->lba << mtd->erasesize_shift;
	physical_size_t len = r->bcnt << mtd->erasesize_shift;

	while (mtd_block_isbad(mtd, off)) {
		vmm_printf("%s: block at 0x%X is bad, skipping...\n",
			   __func__, off);
		off += mtd->erasesize;
	}

	return mtd_blockdev_erase_write(r, off, len, mtd);
}

void mtd_blockdev_flush(struct vmm_blockrq *brq, void *priv)
{
	/* Nothing to do here. */
}

void mtdblock_add(struct mtd_info *mtd)
{
	int			err = 0;
	struct vmm_blockdev	*bdev = NULL;
	struct vmm_blockrq	*brq = NULL;

	if (NULL == (bdev = vmm_blockdev_alloc())) {
		dev_err(&mtd->dev, "Failed to allocate MTD block device\n");
		return;
	}

	/* Setup block device instance */
	strncpy(bdev->name, mtd->name, sizeof (bdev->name));
	strncpy(bdev->desc, "MTD m25p80 NOR flash block device",
		VMM_FIELD_DESC_SIZE);
	bdev->dev.priv = mtd;
	bdev->flags = VMM_BLOCKDEV_RW;
	bdev->start_lba = 0;
	bdev->num_blocks = mtd->size >> mtd->erasesize_shift;
	bdev->block_size = mtd->erasesize;

	/* Setup request queue for block device instance */
	brq = vmm_blockrq_create(mtd->name, 128, FALSE,
				 mtd_blockdev_read,
				 mtd_blockdev_write,
				 NULL,
				 mtd_blockdev_flush,
				 mtd);
	if (!brq) {
		vmm_blockdev_free(bdev);
		return;
	}
	bdev->rq = vmm_blockrq_to_rq(brq);

	/* Register block device instance */
	if (VMM_OK != (err = vmm_blockdev_register(bdev))) {
		vmm_blockrq_destroy(brq);
		vmm_blockdev_free(bdev);
		dev_err(&mtd->dev, "Failed to register MTD block device\n");
	}
}

void mtdblock_remove(struct mtd_info *mtd)
{
	struct vmm_blockdev	*bdev = NULL;
	struct vmm_blockrq	*brq = NULL;

	if (NULL == (bdev = vmm_blockdev_find(mtd->name)))
		return;
	brq = vmm_blockrq_from_rq(bdev->rq);

	vmm_blockdev_unregister(bdev);
	if (brq)
		vmm_blockrq_destroy(brq);
	vmm_blockdev_free(bdev);
}

static struct mtd_notifier mtdblock_notify = {
	.add	 = mtdblock_add,
	.remove	 = mtdblock_remove,
};

int __init init_mtdblock(void)
{
	register_mtd_user(&mtdblock_notify);

	return VMM_OK;
}

void __exit cleanup_mtdblock(void)
{
	unregister_mtd_user(&mtdblock_notify);
}

VMM_DECLARE_MODULE("MTD Core",
		   "Jimmy Durand Wesolowski",
		   "GPL",
		   MTD_IPRIORITY + 1,
		   init_mtdblock,
		   cleanup_mtdblock);
