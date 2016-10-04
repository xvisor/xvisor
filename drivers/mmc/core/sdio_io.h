/**
 * Copyright (c) 2016 Pramod Kanni.
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
 * @file sdio_io.h
 * @author Pramod Kanni (kanni.pramod@gmail.com)
 * @brief SDIO input-output operations interface
 *
 * The source has been largely adapted from linux:
 * drivers/mmc/core/sdio_ops.h
 *
 * Copyright 2006-2007 Pierre Ossman
 *
 * The original code is licensed under the GPL.
 */

#ifndef __MMC_SDIO_IO_H__
#define __MMC_SDIO_IO_H__

#include <drv/mmc/mmc_core.h>
#include <drv/mmc/sdio.h>

int mmc_io_rw_direct(struct mmc_card *card, int write, unsigned fn,
	unsigned addr, u8 in, u8* out);
int mmc_io_rw_extended(struct mmc_card *card, int write, unsigned fn,
			unsigned addr, int incr_addr, u8 *buf,
			unsigned blocks, unsigned blksz);
int sdio_reset(struct mmc_host *host);

static inline bool mmc_is_io_op(u32 opcode)
{
	return opcode == SD_IO_RW_DIRECT || opcode == SD_IO_RW_EXTENDED;
}

#endif


