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

#define clk 			arch_clk

#define clk_prepare		arch_clk_prepare

#define clk_unprepare		arch_clk_unprepare

#define clk_get			arch_clk_get

#define clk_enable		arch_clk_enable

#define clk_disable		arch_clk_disable

#define clk_get_rate		arch_clk_get_rate

#define clk_put			arch_clk_put

#define clk_round_rate		arch_clk_round_rate

#define clk_set_rate		arch_clk_set_rate

#define clk_set_parent		arch_clk_set_parent

#define clk_get_parent		arch_clk_get_parent

#define clk_get_sys		arch_clk_get_sys

#endif
