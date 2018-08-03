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
 * @file consumer.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Consumer interface the pin control subsystem
 *
 * Adapted from linux/include/linux/pinctrl/consumer.h
 *
 * Copyright (C) 2012 ST-Ericsson SA
 * Written on behalf of Linaro for ST-Ericsson
 * Based on bits of regulator core, gpio core and clk core
 *
 * Author: Linus Walleij <linus.walleij@linaro.org>
 *
 * The original source is licensed under GPL.
 */
#ifndef __PINCTRL_CONSUMER_H__
#define __PINCTRL_CONSUMER_H__

#include <vmm_error.h>
#include <vmm_devdrv.h>
#include <libs/list.h>
#include <drv/pinctrl/pinctrl-state.h>

/* This struct is private to the core and should be regarded as a cookie */
struct pinctrl;
struct pinctrl_state;
struct vmm_device;

#ifdef CONFIG_PINCTRL

/* External interface to pin control */
extern int pinctrl_gpio_request(unsigned gpio);
extern void pinctrl_gpio_free(unsigned gpio);
extern int pinctrl_gpio_direction_input(unsigned gpio);
extern int pinctrl_gpio_direction_output(unsigned gpio);
extern int pinctrl_gpio_set_config(unsigned gpio, unsigned long config);

extern struct pinctrl *pinctrl_get(struct vmm_device *dev);
extern void pinctrl_put(struct pinctrl *p);
extern struct pinctrl_state *pinctrl_lookup_state(struct pinctrl *p,
						  const char *name);
extern int pinctrl_select_state(struct pinctrl *p, struct pinctrl_state *s);

extern struct pinctrl *devm_pinctrl_get(struct vmm_device *dev);
extern void devm_pinctrl_put(struct pinctrl *p);

#ifdef CONFIG_PM
extern int pinctrl_pm_select_default_state(struct vmm_device *dev);
extern int pinctrl_pm_select_sleep_state(struct vmm_device *dev);
extern int pinctrl_pm_select_idle_state(struct vmm_device *dev);
#else
static inline int pinctrl_pm_select_default_state(struct vmm_device *dev)
{
	return 0;
}
static inline int pinctrl_pm_select_sleep_state(struct vmm_device *dev)
{
	return 0;
}
static inline int pinctrl_pm_select_idle_state(struct vmm_device *dev)
{
	return 0;
}
#endif

#else /* !CONFIG_PINCTRL */

static inline int pinctrl_gpio_request(unsigned gpio)
{
	return 0;
}

static inline void pinctrl_gpio_free(unsigned gpio)
{
}

static inline int pinctrl_gpio_direction_input(unsigned gpio)
{
	return 0;
}

static inline int pinctrl_gpio_direction_output(unsigned gpio)
{
	return 0;
}

static inline int pinctrl_gpio_set_config(unsigned gpio, unsigned long config)
{
	return 0;
}

static inline struct pinctrl *pinctrl_get(struct vmm_device *dev)
{
	return NULL;
}

static inline void pinctrl_put(struct pinctrl *p)
{
}

static inline struct pinctrl_state *pinctrl_lookup_state(
							struct pinctrl *p,
							const char *name)
{
	return NULL;
}

static inline int pinctrl_select_state(struct pinctrl *p,
				       struct pinctrl_state *s)
{
	return 0;
}

static inline struct pinctrl *devm_pinctrl_get(struct vmm_device *dev)
{
	return NULL;
}

static inline void devm_pinctrl_put(struct pinctrl *p)
{
}

static inline int pinctrl_pm_select_default_state(struct vmm_device *dev)
{
	return 0;
}

static inline int pinctrl_pm_select_sleep_state(struct vmm_device *dev)
{
	return 0;
}

static inline int pinctrl_pm_select_idle_state(struct vmm_device *dev)
{
	return 0;
}

#endif /* CONFIG_PINCTRL */

static inline struct pinctrl *pinctrl_get_select(
				struct vmm_device *dev, const char *name)
{
	struct pinctrl *p;
	struct pinctrl_state *s;
	int ret;

	p = pinctrl_get(dev);
	if (VMM_IS_ERR(p))
		return p;

	s = pinctrl_lookup_state(p, name);
	if (VMM_IS_ERR(s)) {
		pinctrl_put(p);
		return VMM_ERR_CAST(s);
	}

	ret = pinctrl_select_state(p, s);
	if (ret < 0) {
		pinctrl_put(p);
		return VMM_ERR_PTR(ret);
	}

	return p;
}

static inline struct pinctrl *pinctrl_get_select_default(
					struct vmm_device *dev)
{
	return pinctrl_get_select(dev, PINCTRL_STATE_DEFAULT);
}

static inline struct pinctrl *devm_pinctrl_get_select(
					struct vmm_device *dev, const char *name)
{
	struct pinctrl *p;
	struct pinctrl_state *s;
	int ret;

	p = devm_pinctrl_get(dev);
	if (VMM_IS_ERR(p))
		return p;

	s = pinctrl_lookup_state(p, name);
	if (VMM_IS_ERR(s)) {
		devm_pinctrl_put(p);
		return VMM_ERR_CAST(s);
	}

	ret = pinctrl_select_state(p, s);
	if (ret < 0) {
		devm_pinctrl_put(p);
		return VMM_ERR_PTR(ret);
	}

	return p;
}

static inline struct pinctrl *devm_pinctrl_get_select_default(
					struct vmm_device *dev)
{
	return devm_pinctrl_get_select(dev, PINCTRL_STATE_DEFAULT);
}

#endif /* __PINCTRL_CONSUMER_H__ */
