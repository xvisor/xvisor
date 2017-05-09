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
 * @file core.h
 * @author Pramod Kanni (kanni.pramod@gmail.com)
 * @brief MMC/SD/SDIO core header
 *
 * The original code is licensed under the GPL.
 */

#ifndef __MMC_CORE_H__
#define __MMC_CORE_H__

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_delay.h>
#include <vmm_timer.h>
#include <vmm_host_io.h>
#include <vmm_modules.h>
#include <drv/mmc/mmc_core.h>
#include <drv/mmc/sdio.h>

/*
 * Core internal functions.
 */
int mmc_send_cmd(struct mmc_host *host,
		 struct mmc_cmd *cmd,
		 struct mmc_data *data);

unsigned int mmc_align_data_size(struct mmc_card *card,
				 unsigned int sz);

void mmc_set_clock(struct mmc_host *host, u32 clock);
void mmc_set_bus_width(struct mmc_host *host, u32 width);

int mmc_init_card(struct mmc_host *host, struct mmc_card *card);
int mmc_getcd(struct mmc_host *host);
int mmc_send_status(struct mmc_host *host,
		    struct mmc_card *card,
		    int timeout);
int mmc_go_idle(struct mmc_host *host);

/*
 * SDIO internal functions.
 * Note: Must be called with host->lock held.
 */
extern struct vmm_bus sdio_bus_type;
struct vmm_device_type sdio_func_type;

int __sdio_attach(struct mmc_host *host);

/*
 * MMC/SD internal functions.
 * Note: Must be called with host->lock held.
 */
u32 __mmc_sd_bwrite(struct mmc_host *host, struct mmc_card *card,
		    u64 start, u32 blkcnt, const void *src);
u32 __mmc_sd_bread(struct mmc_host *host, struct mmc_card *card,
		   u64 start, u32 blkcnt, void *dst);
int __mmc_sd_attach(struct mmc_host *host);

#endif
