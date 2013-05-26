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
 * @file bcm2835_timer.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief BCM2835 timer interface
 */

#ifndef __BCM2835_TIMER_H
#define __BCM2835_TIMER_H

/** Initialize bcm2835 clocksource */
int bcm2835_clocksource_init(void);

/** Initialize bcm2835 clockchip */
int bcm2835_clockchip_init(void);

#endif
