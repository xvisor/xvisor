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
 * @file spidev.h
 * @author Chaitanya Dhere (chaitanyadhere1@gmail.com)
 * @author Anup Patel (anup@brainfault.org)
 * @brief Generic SPIDEV driver interface
 *
 * The source has been largely adapted from Linux
 * include/linux/spi/spidev.h
 *
 * The original code is licensed under the GPL.
 *
 * Copyright (C) 2006 SWAPP
 *	Andrea Paterniani <a.paterniani@swapp-eng.it>
 */

#ifndef __SPIDEV_H__
#define __SPIDEV_H__

#include <vmm_types.h>
#include <vmm_spinlocks.h>

/*
 * SPIDEV module init priority level
 * Note: Ideally this should be (SPI_IPRIORITY + 1)
 * but to make spidev.h independent of <linux/spi/spi.h>
 * we directly define 
 */
#define SPIDEV_IPRIORITY			(2)

struct spi_device;

/** Opaque structure representing SPIDEV */
struct spidev {
	struct spi_device *spi;
	vmm_spinlock_t spi_lock;
	int busy;
	struct dlist device_entry;
};

/*
 * SPIDEV_xxx defines which are excatly same as
 * SPI_xxx defines provided in <linux/spi/spi.h>
 * so that users of SPIDEV don't have to depend
 * on <linux/spi/spi.h>
 */

#define SPIDEV_CPHA		0x01
#define SPIDEV_CPOL		0x02

#define SPIDEV_MODE_0		(0|0)
#define SPIDEV_MODE_1		(0|SPIDEV_CPHA)
#define SPIDEV_MODE_2		(SPIDEV_CPOL|0)
#define SPIDEV_MODE_3		(SPIDEV_CPOL|SPIDEV_CPHA)

#define SPIDEV_CS_HIGH		0x04
#define SPIDEV_LSB_FIRST	0x08
#define SPIDEV_3WIRE		0x10
#define SPIDEV_LOOP		0x20
#define SPIDEV_NO_CS		0x40
#define SPIDEV_READY		0x80
#define SPIDEV_TX_DUAL		0x100
#define SPIDEV_TX_QUAD		0x200
#define SPIDEV_RX_DUAL		0x400
#define SPIDEV_RX_QUAD		0x800

/** Structure representing xfer on SPIDEV */
struct spidev_xfer_data {
	int mode;
	int out_frequency;
	int bits_per_word;
	u8 *tx_buf;
	u8 *rx_buf;
	size_t len;
};

/** Get count of available SPIDEV instances */
int spidev_count(void);

/** Get SPIDEV instance */
struct spidev *spidev_get(int id);

/** Do Xfer on SPIDEV instances */
int spidev_xfer(struct spidev *spidev, struct spidev_xfer_data *xdata);

#endif /* __SPIDEV_H__ */
