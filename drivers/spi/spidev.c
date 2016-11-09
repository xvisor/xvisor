/**
 * Copyright (c) 2016 Chaitanya Dhere.
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
 * @file spidev.c
 * @author Chaitanya Dhere (chaitanyadhere1@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Generic SPIDEV driver source
 *
 * The source has been largely adapted from Linux
 * include/linux/spi/spidev.h
 *
 * The original code is licensed under the GPL.
 *
 * Copyright (C) 2006 SWAPP
 *	Andrea Paterniani <a.paterniani@swapp-eng.it>
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_completion.h>
#include <vmm_mutex.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>
#include <libs/list.h>
#include <drv/spi/spidev.h>

#include <linux/spi/spi.h>

#define MODULE_DESC             "SPIDEV driver"
#define MODULE_AUTHOR           "Chaitanya Dhere"
#define MODULE_LICENSE          "GPL"
#define MODULE_IPRIORITY        (SPIDEV_IPRIORITY)
#define MODULE_INIT             spidev_init
#define MODULE_EXIT             spidev_exit

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);

#define SPI_MODE_MASK           (SPI_CPHA | SPI_CPOL | SPI_CS_HIGH \
                                | SPI_LSB_FIRST | SPI_3WIRE | SPI_LOOP \
                                | SPI_NO_CS | SPI_READY | SPI_TX_DUAL \
                                | SPI_TX_QUAD | SPI_RX_DUAL | SPI_RX_QUAD)

int spidev_count(void)
{
	int num = 0;
	struct spidev *spidev;

	list_for_each_entry(spidev, &device_list, device_entry)
		num++;

	return num;
}
VMM_EXPORT_SYMBOL(spidev_count);

struct spidev *spidev_get(int id)
{
	int num = 0;
	struct spidev *spidev, *ptr = NULL;

	list_for_each_entry(spidev, &device_list, device_entry) {
		if (id == num) {
			ptr = spidev;
			break;
		}
		num++;
	}

	return ptr;
}
VMM_EXPORT_SYMBOL(spidev_get);

static void spidev_complete(void *arg)
{
	vmm_completion_complete(arg);
}

static ssize_t spidev_sync(struct spidev *spidev, struct spi_message *msg)
{
	int status;
	unsigned long flags;
	struct vmm_completion done;

	INIT_COMPLETION(&done);
	msg->complete = spidev_complete;
	msg->context = &done;

	vmm_spin_lock_irqsave(&spidev->spi_lock, flags);
	if (spidev->spi == NULL) {
		status = VMM_ENOTAVAIL;
	} else if (spidev->busy) {
		status = VMM_EBUSY;
	} else {
		spidev->busy = 1;
		status = spi_async(spidev->spi, msg);
	}
	vmm_spin_unlock_irqrestore(&spidev->spi_lock, flags);

	if (status == 0) {
		vmm_completion_wait(&done);
		vmm_spin_lock_irqsave(&spidev->spi_lock, flags);
		spidev->busy = 0;
		vmm_spin_unlock_irqrestore(&spidev->spi_lock, flags);
		status = msg->status;
		if (status == 0)
			status = msg->actual_length;
	}

	return status;
}

int spidev_xfer(struct spidev *spidev, struct spidev_xfer_data *xdata)
{
	int mask, ret = 0;
	struct spi_transfer t;
	struct spi_message m;

	if (!spidev || !xdata)
		return VMM_EINVALID;

	if (xdata->mode == -1) {
		spidev->spi->mode = SPI_MODE_0;
		spidev->spi->bits_per_word = 8;
		spidev->spi->max_speed_hz = 500000;
		mask = spidev->spi->mode & ~SPI_MODE_MASK;
		spidev->spi->mode = (u16)mask;
	} else {
		switch (xdata->mode) {
		case 0:
			spidev->spi->mode = SPI_MODE_0;
			break;
		case 1:
			spidev->spi->mode = SPI_MODE_1;
			break;
		case 2:
			spidev->spi->mode = SPI_MODE_2;
			break;
		case 3:
			spidev->spi->mode = SPI_MODE_3;
			break;
		};
		spidev->spi->bits_per_word = xdata->bits_per_word;
		spidev->spi->max_speed_hz = xdata->out_frequency;
		mask = spidev->spi->mode & ~SPI_MODE_MASK;
		spidev->spi->mode = (u16)mask;
	}

	ret = spi_setup(spidev->spi);
	if (ret < 0) {
		vmm_lerror("SPIDEV", "Setting up SPI failed\n");
		return VMM_EINVALID;
	}

	t.tx_buf = xdata->tx_buf;
	t.rx_buf = xdata->rx_buf;
	t.len = xdata->len;
	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	ret = spidev_sync(spidev, &m);
	if (ret < 0) {
		vmm_lerror("SPIDEV", "Submitting data to SPI failed\n");
		return VMM_EIO;
	}

	return ret;
}
EXPORT_SYMBOL(spidev_xfer);

static int spidev_probe(struct spi_device *spi)
{
	struct spidev *spidev;

	spidev = vmm_zalloc(sizeof(*spidev));
	if (!spidev)
		return VMM_ENOMEM;

	spidev->spi = spi;
	spin_lock_init(&spidev->spi_lock);
	spidev->busy = 0;
	INIT_LIST_HEAD(&spidev->device_entry);

	vmm_mutex_lock(&device_list_lock);
	list_add_tail(&spidev->device_entry, &device_list);
	vmm_mutex_unlock(&device_list_lock);

	spi_set_drvdata(spi, spidev);

	return 0;
}

static int spidev_remove(struct spi_device *spi)
{
	struct spidev *spidev = spi_get_drvdata(spi);

	spidev->spi = NULL;

	vmm_mutex_lock(&device_list_lock);
	list_del(&spidev->device_entry);
	vmm_mutex_unlock(&device_list_lock);

	vmm_free(spidev);

	return 0;
}

static const struct of_device_id spidev_match[] = {
	{ .compatible = "spidev", },
	{}
};

static struct spi_driver spidev_spi_driver = {
	.driver = {
		.match_table   = spidev_match,
	},
	.probe         = spidev_probe,
	.remove        = spidev_remove,
};

static int __init spidev_init(void)
{
	return spi_register_driver(&spidev_spi_driver);
}

static void __exit spidev_exit(void)
{
	spi_unregister_driver(&spidev_spi_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
