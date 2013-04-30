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
 * @file bcm2835_pm.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief BCM2835 PM and Watchdog interface
 */

#ifndef __BCM2835_PM_H
#define __BCM2835_PM_H

/** Reset SOC */
void bcm2835_pm_reset(void);

/** Poweroff SOC */
void bcm2835_pm_poweroff(void);

/** Initialize PM and Watchdog interface */
int bcm2835_pm_init(void);

#endif
