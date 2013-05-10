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
 * @file cpu_hacks.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief unimplemented hacks or hacky stubs.
 */

#include <vmm_error.h>

#include <linux/screen_info.h>

/* FIXME:
 * Populate screen_info for proper working of framebuffer drivers
 *
 * To populate this we need to be in real-mode and use bios int 0x10
 * routines.
 */
struct screen_info screen_info;

/* FIXME: 
 * For now, no delay in accessing IO ports
 */
void native_io_delay(void)
{
}


