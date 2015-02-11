/*
 * Generic GPIO card-detect helper header
 *
 * Copyright (C) 2011, Guennadi Liakhovetski <g.liakhovetski@gmx.de>
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 *
 * Adapted from the 3.13.6 Linux kernel for Xvisor.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @file slot-gpio.h
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Generic GPIO card-detect helper header
 */

#ifndef __DRV_MMC_SLOT_GPIO_H__
#define __DRV_MMC_SLOT_GPIO_H__

struct mmc_host;

int mmc_gpio_get_ro(struct mmc_host *host);
int mmc_gpio_request_ro(struct mmc_host *host, unsigned int gpio);
void mmc_gpio_free_ro(struct mmc_host *host);

int mmc_gpio_get_cd(struct mmc_host *host);
int mmc_gpio_request_cd(struct mmc_host *host, unsigned int gpio,
			unsigned int debounce);
void mmc_gpio_free_cd(struct mmc_host *host);

#endif
