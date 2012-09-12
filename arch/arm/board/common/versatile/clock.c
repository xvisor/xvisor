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

int versatile_clk_enable(struct vmm_devclk *clk)
{
	return 0;
}

void versatile_clk_disable(struct vmm_devclk *clk)
{
}

unsigned long versatile_clk_get_rate(struct vmm_devclk *clk)
{
	struct versatile_clk *vclk = clk->priv;
	return vclk->rate;
}

long versatile_clk_round_rate(struct vmm_devclk *clk, unsigned long rate)
{
	long ret = VMM_EIO;
	struct versatile_clk *vclk = clk->priv;
	if (vclk->ops && vclk->ops->round)
		ret = vclk->ops->round(vclk, rate);
	return ret;
}

int versatile_clk_set_rate(struct vmm_devclk *clk, unsigned long rate)
{
	int ret = VMM_EIO;
	struct versatile_clk *vclk = clk->priv;
	if (vclk->ops && vclk->ops->set)
		ret = vclk->ops->set(vclk, rate);
	return ret;
}

long icst_clk_round(struct versatile_clk *vclk, unsigned long rate)
{
	struct icst_vco vco;
	vco = icst_hz_to_vco(vclk->params, rate);
	return icst_hz(vclk->params, vco);
}

int icst_clk_set(struct versatile_clk *vclk, unsigned long rate)
{
	struct icst_vco vco;

	vco = icst_hz_to_vco(vclk->params, rate);
	vclk->rate = icst_hz(vclk->params, vco);
	vclk->ops->setvco(vclk, vco);

	return 0;
}

