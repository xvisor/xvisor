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
 * @file icst.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief calculate clocks/divisors for the ICST307 clock generator
 *
 * The source has been largely adapted from Linux 3.x or higher:
 * arch/arm/common/icst307.c
 *
 *  Copyright (C) 2003 Deep Blue Solutions, Ltd, All Rights Reserved.
 *
 *  Support functions for calculating clocks/divisors for the ICST
 *  clock generators.  See http://www.idt.com/ for more information
 *  on these devices.
 *
 * The original code is licensed under the GPL.
 */

#include <libs/mathlib.h>
#include <icst.h>

/*
 * Divisors for each OD setting.
 */
const unsigned char icst307_s2div[8] = { 10, 2, 8, 4, 5, 7, 3, 6 };
const unsigned char icst525_s2div[8] = { 10, 2, 8, 4, 5, 7, 9, 6 };

unsigned long icst_hz(const struct icst_params *p, struct icst_vco vco)
{
	return udiv32(p->ref * 2 * (vco.v + 8), ((vco.r + 2) * p->s2div[vco.s]));
}

/*
 * Ascending divisor S values.
 */
const unsigned char icst307_idx2s[8] = { 1, 6, 3, 4, 7, 5, 2, 0 };
const unsigned char icst525_idx2s[8] = { 1, 3, 4, 7, 5, 2, 6, 0 };

struct icst_vco
icst_hz_to_vco(const struct icst_params *p, unsigned long freq)
{
	struct icst_vco vco = { .s = 1, .v = p->vd_max, .r = p->rd_max };
	unsigned long f;
	unsigned int i = 0, rd, best = (unsigned int)-1;

	/*
	 * First, find the PLL output divisor such
	 * that the PLL output is within spec.
	 */
	do {
		f = freq * p->s2div[p->idx2s[i]];

		if (f > p->vco_min && f <= p->vco_max)
			break;
	} while (i < 8);

	if (i >= 8)
		return vco;

	vco.s = p->idx2s[i];

	/*
	 * Now find the closest divisor combination
	 * which gives a PLL output of 'f'.
	 */
	for (rd = p->rd_min; rd <= p->rd_max; rd++) {
		unsigned long fref_div, f_pll;
		unsigned int vd;
		int f_diff;

		fref_div = udiv32((2 * p->ref), rd);

		vd = udiv32((f + fref_div / 2), fref_div);
		if (vd < p->vd_min || vd > p->vd_max)
			continue;

		f_pll = fref_div * vd;
		f_diff = f_pll - f;
		if (f_diff < 0)
			f_diff = -f_diff;

		if ((unsigned)f_diff < best) {
			vco.v = vd - 8;
			vco.r = rd - 2;
			if (f_diff == 0)
				break;
			best = f_diff;
		}
	}

	return vco;
}

