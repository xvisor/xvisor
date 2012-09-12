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
 * @file icst.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief calculate clocks/divisors for the ICST307 clock generator
 *
 * The source has been largely adapted from Linux 3.x or higher:
 * arch/arm/include/asm/hardware/icst.h
 *
 *  Copyright (C) 2003 Deep Blue Solutions, Ltd, All Rights Reserved.
 *
 *  Support functions for calculating clocks/divisors for the ICST
 *  clock generators.  See http://www.idt.com/ for more information
 *  on these devices.
 *
 * The original code is licensed under the GPL.
 */
#ifndef __ICST_H__
#define __ICST_H__

struct icst_params {
	unsigned long	ref;
	unsigned long	vco_max;	/* inclusive */
	unsigned long	vco_min;	/* exclusive */
	unsigned short	vd_min;		/* inclusive */
	unsigned short	vd_max;		/* inclusive */
	unsigned char	rd_min;		/* inclusive */
	unsigned char	rd_max;		/* inclusive */
	const unsigned char *s2div;	/* chip specific s2div array */
	const unsigned char *idx2s;	/* chip specific idx2s array */
};

struct icst_vco {
	unsigned short	v;
	unsigned char	r;
	unsigned char	s;
};

unsigned long icst_hz(const struct icst_params *p, struct icst_vco vco);
struct icst_vco icst_hz_to_vco(const struct icst_params *p, unsigned long freq);

/*
 * ICST307 VCO frequency must be between 6MHz and 200MHz (3.3 or 5V).
 * This frequency is pre-output divider.
 */
#define ICST307_VCO_MIN	6000000
#define ICST307_VCO_MAX	200000000

extern const unsigned char icst307_s2div[];
extern const unsigned char icst307_idx2s[];

/*
 * ICST525 VCO frequency must be between 10MHz and 200MHz (3V) or 320MHz (5V).
 * This frequency is pre-output divider.
 */
#define ICST525_VCO_MIN		10000000
#define ICST525_VCO_MAX_3V	200000000
#define ICST525_VCO_MAX_5V	320000000

extern const unsigned char icst525_s2div[];
extern const unsigned char icst525_idx2s[];

#endif /* __ICST_H__ */
