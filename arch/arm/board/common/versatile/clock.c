/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file clock.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Versatile platform clock managment
 *
 * The source has been largely adapted from Linux 3.x or higher:
 * arch/arm/plat-versatile/clock.c
 *
 *  Copyright (C) 2004 ARM Limited.
 *  Written by Deep Blue Solutions Limited.
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <versatile/clock.h>
#include <drv/clk.h>

int clk_enable(struct clk *clk)
{
	return 0;
}

void clk_disable(struct clk *clk)
{
}

unsigned long clk_get_rate(struct clk *clk)
{
	return clk->rate;
}

long clk_round_rate(struct clk *clk, unsigned long rate)
{
	long ret = VMM_EIO;
	if (clk->ops && clk->ops->round)
		ret = clk->ops->round(clk, rate);
	return ret;
}

int clk_set_rate(struct clk *clk, unsigned long rate)
{
	int ret = VMM_EIO;
	if (clk->ops && clk->ops->set)
		ret = clk->ops->set(clk, rate);
	return ret;
}

long icst_clk_round(struct clk *clk, unsigned long rate)
{
	struct icst_vco vco;
	vco = icst_hz_to_vco(clk->params, rate);
	return icst_hz(clk->params, vco);
}

int icst_clk_set(struct clk *clk, unsigned long rate)
{
	struct icst_vco vco;

	vco = icst_hz_to_vco(clk->params, rate);
	clk->rate = icst_hz(clk->params, vco);
	clk->ops->setvco(clk, vco);

	return 0;
}

