/*
 *  linux/include/linux/clk.h
 *
 *  Copyright (C) 2004 ARM Limited.
 *  Written by Deep Blue Solutions Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __LINUX_CLK_H
#define __LINUX_CLK_H

#include <arch_clk.h>

struct clk;

#define clk_prepare(x)		arch_clk_prepare((struct arch_clk *)x)

#define clk_unprepare(x)	arch_clk_unprepare((struct arch_clk *)x)

#define clk_get(dev, id)	((struct clk *)arch_clk_get((struct vmm_device *)dev, id))

#define clk_enable(x)		arch_clk_enable((struct arch_clk *)x)

#define clk_disable(x)		arch_clk_disable((struct arch_clk *)x)

#define clk_get_rate(x)		arch_clk_get_rate((struct arch_clk *)x)

#define clk_put(x)		arch_clk_put((struct arch_clk *)x)

#define clk_round_rate(x, rate)	arch_clk_round_rate((struct arch_clk *)x, rate)

#define clk_set_rate(x, rate)	arch_clk_set_rate((struct arch_clk *)x, rate)

#define clk_set_parent(x, y)	arch_clk_set_parent((struct arch_clk *)x, (struct arch_clk *)y)

#define clk_get_parent(x)	((struct clk *)arch_clk_get_parent((struct arch_clk *)x))

#define clk_get_sys(x, id)	((struct clk *)arch_clk_get_sys((struct arch_clk *)x, id))

#endif
