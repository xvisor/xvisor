/**
 * Copyright (c) 2013 Jean-Christophe Dubois.
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
 * @file cpu.h
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief Exynos CPU support header
 *
 * Adapted from linux/arch/arm/plat-samsung/include/plat/cpu.h
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Copyright (c) 2004-2005 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *
 * Header file for Samsung CPU support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __SAMSUNG_PLAT_CPU_H
#define __SAMSUNG_PLAT_CPU_H

#include <vmm_types.h>

extern u32 samsung_cpu_id();

#define S3C24XX_CPU_ID		0x32400000
#define S3C24XX_CPU_MASK	0xFFF00000

#define S3C6400_CPU_ID		0x36400000
#define S3C6410_CPU_ID		0x36410000
#define S3C64XX_CPU_MASK	0xFFFFF000

#define S5P6440_CPU_ID		0x56440000
#define S5P6450_CPU_ID		0x36450000
#define S5P64XX_CPU_MASK	0xFFFFF000

#define S5PC100_CPU_ID		0x43100000
#define S5PC100_CPU_MASK	0xFFFFF000

#define S5PV210_CPU_ID		0x43110000
#define S5PV210_CPU_MASK	0xFFFFF000

#define EXYNOS4210_CPU_ID	0x43210000
#define EXYNOS4212_CPU_ID	0x43220000
#define EXYNOS4412_CPU_ID	0xE4412200
#define EXYNOS4_CPU_MASK	0xFFFE0000

#define EXYNOS5250_SOC_ID	0x43520000
#define EXYNOS5_SOC_MASK	0xFFFFF000

#define IS_SAMSUNG_CPU(name, id, mask)		\
static inline int is_samsung_##name(void)	\
{						\
	return ((samsung_cpu_id() & mask) == (id & mask));	\
}

IS_SAMSUNG_CPU(s3c24xx, S3C24XX_CPU_ID, S3C24XX_CPU_MASK)
IS_SAMSUNG_CPU(s3c6400, S3C6400_CPU_ID, S3C64XX_CPU_MASK)
IS_SAMSUNG_CPU(s3c6410, S3C6410_CPU_ID, S3C64XX_CPU_MASK)
IS_SAMSUNG_CPU(s5p6440, S5P6440_CPU_ID, S5P64XX_CPU_MASK)
IS_SAMSUNG_CPU(s5p6450, S5P6450_CPU_ID, S5P64XX_CPU_MASK)
IS_SAMSUNG_CPU(s5pc100, S5PC100_CPU_ID, S5PC100_CPU_MASK)
IS_SAMSUNG_CPU(s5pv210, S5PV210_CPU_ID, S5PV210_CPU_MASK)
IS_SAMSUNG_CPU(exynos4210, EXYNOS4210_CPU_ID, EXYNOS4_CPU_MASK)
IS_SAMSUNG_CPU(exynos4212, EXYNOS4212_CPU_ID, EXYNOS4_CPU_MASK)
IS_SAMSUNG_CPU(exynos4412, EXYNOS4412_CPU_ID, EXYNOS4_CPU_MASK)
IS_SAMSUNG_CPU(exynos5250, EXYNOS5250_SOC_ID, EXYNOS5_SOC_MASK)

#define soc_is_s3c24xx()	is_samsung_s3c24xx()

#define soc_is_s3c64xx()	(is_samsung_s3c6400() || is_samsung_s3c6410())

#define soc_is_s5p6440()	is_samsung_s5p6440()

#define soc_is_s5p6450()	is_samsung_s5p6450()

#define soc_is_s5pc100()	is_samsung_s5pc100()

#define soc_is_s5pv210()	is_samsung_s5pv210()

#define soc_is_exynos4210()	is_samsung_exynos4210()

#define soc_is_exynos4212()	is_samsung_exynos4212()

#define soc_is_exynos4412()	is_samsung_exynos4412()

#define EXYNOS4210_REV_0	(0x0)
#define EXYNOS4210_REV_1_0	(0x10)
#define EXYNOS4210_REV_1_1	(0x11)

#define soc_is_exynos5250()	is_samsung_exynos5250()

extern void exynos_init_cpu(physical_addr_t cpuid_addr);

extern unsigned int samsung_rev(void);

#endif
