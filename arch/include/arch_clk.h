/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file arch_clk.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief generic interface for arch specific clocking framework
 */
#ifndef _ARCH_CLK_H__
#define _ARCH_CLK_H__

#include <vmm_types.h>

struct vmm_device;
struct arch_clk;

int arch_clk_prepare(struct arch_clk *clk);

void arch_clk_unprepare(struct arch_clk *clk);

struct arch_clk *arch_clk_get(struct vmm_device *dev, const char *id);

int arch_clk_enable(struct arch_clk *clk);

void arch_clk_disable(struct arch_clk *clk);

unsigned long arch_clk_get_rate(struct arch_clk *clk);

void arch_clk_put(struct arch_clk *clk);

long arch_clk_round_rate(struct arch_clk *clk, unsigned long rate);

int arch_clk_set_rate(struct arch_clk *clk, unsigned long rate);

int arch_clk_set_parent(struct arch_clk *clk, struct arch_clk *parent);

struct arch_clk *arch_clk_get_parent(struct arch_clk *clk);

struct arch_clk *arch_clk_get_sys(const char *dev_id, const char *con_id);

#endif
