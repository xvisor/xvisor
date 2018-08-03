/*
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
 * License terms: GNU General Public License (GPL) version 2
 */

#ifndef PINCTRL_DEVINFO_H
#define PINCTRL_DEVINFO_H

#include <drv/pinctrl/devinfo.h>

#ifdef CONFIG_PINCTRL

/* The device core acts as a consumer toward pinctrl */
#include <linux/pinctrl/consumer.h>

#endif /* CONFIG_PINCTRL */

/* For now this is just place holder header. */

#endif /* PINCTRL_DEVINFO_H */
