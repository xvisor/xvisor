/*
 * Interface the pinctrl subsystem
 *
 * Copyright (C) 2011 ST-Ericsson SA
 * Written on behalf of Linaro for ST-Ericsson
 * This interface is used in the core to keep track of pins.
 *
 * Author: Linus Walleij <linus.walleij@linaro.org>
 *
 * License terms: GNU General Public License (GPL) version 2
 */
#ifndef __LINUX_PINCTRL_PINCTRL_H
#define __LINUX_PINCTRL_PINCTRL_H

#include <drv/pinctrl/pinctrl.h>

#ifdef CONFIG_PINCTRL

#include <linux/radix-tree.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/seq_file.h>
#include <linux/pinctrl/pinctrl-state.h>

#endif /* !CONFIG_PINCTRL */

/* For now this is just place holder header. */

#endif /* __LINUX_PINCTRL_PINCTRL_H */
