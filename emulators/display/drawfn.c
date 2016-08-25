/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file drawfn.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Generic framebuffer format conversion routines.
 */

#include <vmm_error.h>
#include <vmm_host_io.h>
#include <vio/vmm_pixel_ops.h>
#include <vio/vmm_vdisplay.h>

#include "drawfn.h"

#define SURFACE_BITS 8
#include "drawfn_template.h"
#define SURFACE_BITS 15
#include "drawfn_template.h"
#define SURFACE_BITS 16
#include "drawfn_template.h"
#define SURFACE_BITS 24
#include "drawfn_template.h"
#define SURFACE_BITS 32
#include "drawfn_template.h"
