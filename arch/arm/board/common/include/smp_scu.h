/**
 * Copyright (c) 2012 Jean-Christophe Dubois.
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
 * @file smp_scu.h
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief SCU header file
 *
 * Adapted from linux/arch/arm/include/asm/smp_scu.h
 */
#ifndef __SMP_SCU_H
#define __SMP_SCU_H

#define SCU_PM_NORMAL	0
#define SCU_PM_EINVAL	1
#define SCU_PM_DORMANT	2
#define SCU_PM_POWEROFF	3

#ifdef CONFIG_SMP
u32 scu_get_core_count(void *);
bool scu_cpu_core_is_smp(void *, u32);
void scu_enable(void *);
#endif

int scu_power_mode(void *, u32);

#endif
