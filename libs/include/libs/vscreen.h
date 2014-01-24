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
 * @file vscreen.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation frame buffer based virtual screen capturing
 */

#ifndef __VSCREEN_H__
#define __VSCREEN_H__

#include <vmm_types.h>
#include <drv/fb.h>
#include <drv/input.h>
#include <vio/vmm_vinput.h>
#include <vio/vmm_vdisplay.h>

#define VSCREEN_IPRIORITY		(INPUT_IPRIORITY + \
					 FB_CLASS_IPRIORITY + \
					 VMM_VDISPLAY_IPRIORITY + \
					 VMM_VINPUT_IPRIORITY + 1)

#define VSCREEN_REFRESH_RATE_MIN	10
#define VSCREEN_REFRESH_RATE_GOOD	25
#define VSCREEN_REFRESH_RATE_MAX	100

/** Generic virtual screen capturing on frame buffer device */
int vscreen_bind(bool is_hard,
		 u32 refresh_rate,
		 u32 esc_key_code0,
		 u32 esc_key_code1,
		 u32 esc_key_code2,
		 struct fb_info *info,
		 struct vmm_vdisplay *vdis,
		 struct vmm_vkeyboard *vkbd,
		 struct vmm_vmouse *vmou);

/** Software emulated virtual screen capturing on frame buffer device */
static inline int vscreen_soft_bind(u32 refresh_rate,
				    u32 esc_key_code0,
				    u32 esc_key_code1,
				    u32 esc_key_code2,
				    struct fb_info *info,
				    struct vmm_vdisplay *vdis,
				    struct vmm_vkeyboard *vkbd,
				    struct vmm_vmouse *vmou)
{
	return vscreen_bind(FALSE, refresh_rate,
			    esc_key_code0, esc_key_code1, esc_key_code2,
			    info, vdis, vkbd, vmou);
}

/** Hardware assisted virtual screen capturing on frame buffer device */
static inline int vscreen_hard_bind(u32 esc_key_code0,
				    u32 esc_key_code1,
				    u32 esc_key_code2,
				    struct fb_info *info,
				    struct vmm_vdisplay *vdis,
				    struct vmm_vkeyboard *vkbd,
				    struct vmm_vmouse *vmou)
{
	return vscreen_bind(TRUE, VSCREEN_REFRESH_RATE_MIN,
			    esc_key_code0, esc_key_code1, esc_key_code2,
			    info, vdis, vkbd, vmou);
}

#endif /* __VSCREEN_H__ */

