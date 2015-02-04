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
 * @file mtdchar.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief A very simple version of MTD character device part.
 */

#include <vmm_stdio.h>
#include <vmm_chardev.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include "mtdcore.h"

#define MTD_IOCTL_CMD_ERASE	0x1

static void mtd_chardev_erase_cb(__maybe_unused struct erase_info *info)
{
	struct vmm_completion *compl = (struct vmm_completion *)info->priv;

	complete(compl);
}

int mtd_chardev_ioctl(struct vmm_chardev *cdev,
		      int cmd, void *arg)
{
	struct mtd_info		*mtd = cdev->priv;
	struct erase_info	info;
	size_t			off = 0;
	struct vmm_completion	compl;
	u64			timeout = 100000;

	switch (cmd) {
	case MTD_IOCTL_CMD_ERASE:
		INIT_COMPLETION(&compl);
		info.mtd = mtd;
		info.addr = (u32)arg;
		info.len = mtd->erasesize;
		info.callback = mtd_chardev_erase_cb;
		info.priv = (u_long)&compl;

		if (mtd_erase(mtd, &info)) {
			dev_err(&cdev->dev, "Erasing at 0x%08X failed\n", off);
			return VMM_EFAIL;
		}
		vmm_completion_wait_timeout(&compl, &timeout);
		break;
	default:
		dev_err(&cdev->dev, "Unknown command 0x%X\n", cmd);
		return VMM_EFAIL;
	}
	return VMM_OK;
}

u32 mtd_chardev_read(struct vmm_chardev *cdev,
		     u8 *dest, size_t len, off_t *off, bool sleep)
{
	struct mtd_info		*mtd = cdev->priv;
	unsigned int		retlen = 0;

	if (mtd_read(mtd, *off, len, &retlen, dest)) {
		dev_err(&cdev->dev, "Writing at 0x%08X failed\n", off);
		return VMM_EFAIL;
	}
	*off += retlen;

	return VMM_OK;
}

u32 mtd_chardev_write(struct vmm_chardev *cdev,
		      u8 *src, size_t len, off_t *off, bool sleep)
{
	struct mtd_info		*mtd = cdev->priv;
	unsigned int		retlen = 0;
	u32			block = 0;

	block = *off & ~mtd->erasesize_mask;
	if (mtd_block_isbad(mtd, block)) {
		dev_err(&cdev->dev, "Block at 0x%08X failed\n", block);
		return VMM_EFAIL;
	}
	if (mtd_write(mtd, *off, len, &retlen, src)) {
		dev_err(&cdev->dev, "Writing at 0x%08X failed\n", off);
		return VMM_EFAIL;
	}
	*off += retlen;

	return VMM_OK;
}

void mtdchar_add(struct mtd_info *mtd)
{
	struct vmm_chardev *cdev = NULL;

	if (NULL == (cdev = vmm_zalloc(sizeof (struct vmm_chardev)))) {
		dev_err(&mtd->dev, "Failed to allocate MTD character "
			"device\n");
		return;
	}
	strncpy(cdev->name, mtd->name, sizeof (cdev->name));
	cdev->ioctl = mtd_chardev_ioctl;
	cdev->read = mtd_chardev_read;
	cdev->write = mtd_chardev_write;
	cdev->priv = mtd;

	if (VMM_OK != vmm_chardev_register(cdev)) {
		vmm_free(cdev);
		dev_err(&mtd->dev, "Failed to register MTD char device\n");
	}
}

void mtdchar_remove(struct mtd_info *mtd)
{
	struct vmm_chardev *cdev = NULL;

	if (NULL == (cdev = vmm_chardev_find(mtd->name)))
		return;
	vmm_chardev_unregister(cdev);
}

static struct mtd_notifier mtdchar_notify = {
	.add	 = mtdchar_add,
	.remove	 = mtdchar_remove,
};

int __init init_mtdchar(void)
{
	register_mtd_user(&mtdchar_notify);

	return VMM_OK;
}

void __exit cleanup_mtdchar(void)
{
	unregister_mtd_user(&mtdchar_notify);
}
