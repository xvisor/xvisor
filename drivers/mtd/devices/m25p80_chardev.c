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
 * @file m25p80_chardev.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief MTD SPI character device driver for ST M25Pxx (and similar) flash
 */

#include <linux/mtd/mtd.h>
#include <vmm_chardev.h>
#include "m25p80.h"


static inline struct m25p *vmm_chardev_to_flash(struct vmm_chardev *cdev)
{
	struct spi_device	*spi = 0;

	spi = vmm_devdrv_get_data(cdev->dev.parent);
	if (!spi) {
		return NULL;
	}

	return spi_get_drvdata(spi);
}

#define FLASH_IOCTL_CMD_ERASE	0x1

static void m25p_chardev_erase_cb(__maybe_unused struct erase_info *info)
{
}

int m25p_chardev_ioctl(struct vmm_chardev *cdev,
		       int cmd, void *arg)
{
	struct m25p		*flash = NULL;
	struct erase_info	info;
	/* FIXME */
	size_t			off = 0;

	if (NULL == (flash = vmm_chardev_to_flash(cdev))) {
		return VMM_EFAIL;
	}

	switch (cmd) {
	case FLASH_IOCTL_CMD_ERASE:
		info.mtd = &flash->mtd;
		info.addr = (u32)arg;
		info.len = flash->mtd.erasesize;
		info.callback = m25p_chardev_erase_cb;

		if (mtd_erase(&flash->mtd, &info)) {
			dev_err(&cdev->dev, "Erasing at 0x%08X failed\n", off);
			return VMM_EFAIL;
		}
		break;
	default:
		dev_err(&cdev->dev, "Unknown command 0x%X\n", cmd);
		return VMM_EFAIL;
	}
	return VMM_OK;
}

u32 m25p_chardev_read(struct vmm_chardev *cdev,
		      u8 *dest, size_t len, off_t *off, bool sleep)
{
	unsigned int		retlen = 0;
	struct m25p		*flash = NULL;

	if (NULL == (flash = vmm_chardev_to_flash(cdev))) {
		return VMM_EFAIL;
	}

	if (mtd_read(&flash->mtd, *off, len, &retlen, dest)) {
		dev_err(&cdev->dev, "Writing at 0x%08X failed\n", off);
		return VMM_EFAIL;
	}
	*off += retlen;

	return VMM_OK;
}

u32 m25p_chardev_write(struct vmm_chardev *cdev,
		       u8 *src, size_t len, off_t *off, bool sleep)
{
	unsigned int		retlen = 0;
	u32			block = 0;
	struct m25p		*flash = NULL;

	if (NULL == (flash = vmm_chardev_to_flash(cdev))) {
		return VMM_EFAIL;
	}

	block = *off & ~flash->mtd.erasesize_mask;
	if (mtd_block_isbad(&flash->mtd, block)) {
		dev_err(&cdev->dev, "Block at 0x%08X failed\n", block);
		return VMM_EFAIL;
	}
	if (mtd_write(&flash->mtd, *off, len, &retlen, src)) {
		dev_err(&cdev->dev, "Writing at 0x%08X failed\n", off);
		return VMM_EFAIL;
	}
	*off += retlen;

	return VMM_OK;
}

struct vmm_chardev m25p_chardev = {
	.ioctl = m25p_chardev_ioctl,
	.read = m25p_chardev_read,
	.write = m25p_chardev_write,
};

int m25p_register_chardev(struct vmm_device *dev)
{
	int			err = VMM_OK;
	struct spi_device	*spi = to_spi_device(dev);
	struct m25p		*flash = NULL;

	if (!spi)
		return VMM_EFAIL;

	flash = spi_get_drvdata(spi);

	strncpy(m25p_chardev.name, dev->name, VMM_FIELD_NAME_SIZE);
	m25p_chardev.dev.parent = dev;
	m25p_chardev.priv = flash;

	if (VMM_OK != (err = vmm_chardev_register(&m25p_chardev))) {
		dev_warn(dev, "Failed to register MTD chardev\n");
		return err;
	}
	flash->chardev = &m25p_chardev;

	return VMM_OK;
}

int m25p_unregister_chardev(struct vmm_device *dev)
{
	struct spi_device	*spi = vmm_devdrv_get_data(dev);
	struct m25p		*flash = NULL;

	if (!spi)
		return VMM_EFAIL;

	flash = spi_get_drvdata(spi);
	flash->chardev = NULL;

	return vmm_chardev_unregister(&m25p_chardev);
}
