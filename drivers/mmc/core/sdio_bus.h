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
 * @file sdio_bus.h
 * @author Pramod Kanni (kanni.pramod@gmail.com)
 * @brief SDIO functions driver model interface
 *
 * The source has been largely adapted from linux:
 * drivers/mmc/core/sdio_bus.h
 *
 * Copyright 2007 Pierre Ossman
 *
 * The original code is licensed under the GPL.
 */

#ifndef __MMC_CORE_SDIO_BUS_H__
#define __MMC_CORE_SDIO_BUS_H__

struct sdio_func *sdio_alloc_func(struct mmc_card *card);
int sdio_add_func(struct sdio_func *func);
void sdio_remove_func(struct sdio_func *func);

int sdio_register_bus(void);
void sdio_unregister_bus(void);

#endif


