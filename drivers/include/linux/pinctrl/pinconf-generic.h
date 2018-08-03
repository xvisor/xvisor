/*
 * Interface the generic pinconfig portions of the pinctrl subsystem
 *
 * Copyright (C) 2011 ST-Ericsson SA
 * Written on behalf of Linaro for ST-Ericsson
 * This interface is used in the core to keep track of pins.
 *
 * Author: Linus Walleij <linus.walleij@linaro.org>
 *
 * License terms: GNU General Public License (GPL) version 2
 */
#ifndef __LINUX_PINCTRL_PINCONF_GENERIC_H
#define __LINUX_PINCTRL_PINCONF_GENERIC_H

#include <drv/pinctrl/pinconf-generic.h>

/*
 * You shouldn't even be able to compile with these enums etc unless you're
 * using generic pin config. That is why this is defined out.
 */
#ifdef CONFIG_GENERIC_PINCONF

#ifdef CONFIG_OF

#include <linux/device.h>
#include <linux/pinctrl/machine.h>

#endif

#endif /* CONFIG_GENERIC_PINCONF */

/* For now this is just place holder header. */

#endif /* __LINUX_PINCTRL_PINCONF_GENERIC_H */
