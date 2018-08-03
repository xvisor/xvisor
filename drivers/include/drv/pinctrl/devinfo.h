/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file devinfo.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Per-device information from the pin control system
 *
 * Adapted from linux/include/linux/pinctrl/devinfo.h
 *
 * Per-device information from the pin control system.
 * This is the stuff that get included into the device
 * core.
 *
 * Copyright (C) 2012 ST-Ericsson SA
 * Written on behalf of Linaro for ST-Ericsson
 * This interface is used in the core to keep track of pins.
 *
 * Author: Linus Walleij <linus.walleij@linaro.org>
 *
 * The original source is licensed under GPL.
 */

#ifndef __PINCTRL_DEVINFO_H__
#define __PINCTRL_DEVINFO_H__

#ifdef CONFIG_PINCTRL

/* The device core acts as a consumer toward pinctrl */
#include <drv/pinctrl/consumer.h>

/**
 * struct dev_pin_info - pin state container for devices
 * @p: pinctrl handle for the containing device
 * @default_state: the default state for the handle, if found
 * @init_state: the state at probe time, if found
 * @sleep_state: the state at suspend time, if found
 * @idle_state: the state at idle (runtime suspend) time, if found
 */
struct dev_pin_info {
	struct pinctrl *p;
	struct pinctrl_state *default_state;
	struct pinctrl_state *init_state;
#ifdef CONFIG_PM
	struct pinctrl_state *sleep_state;
	struct pinctrl_state *idle_state;
#endif
};

extern int pinctrl_bind_pins(struct vmm_device *dev);
extern int pinctrl_init_done(struct vmm_device *dev);

#else

struct device;

/* Stubs if we're not using pinctrl */

static inline int pinctrl_bind_pins(struct vmm_device *dev)
{
	return 0;
}

static inline int pinctrl_init_done(struct vmm_device *dev)
{
	return 0;
}

#endif /* CONFIG_PINCTRL */
#endif /* __PINCTRL_DEVINFO_H__ */
