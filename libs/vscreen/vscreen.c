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
 * @file vscreen.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation frame buffer based virtual screen capturing
 */

#include <vmm_error.h>
#include <vmm_modules.h>
#include <libs/vscreen.h>

#define MODULE_DESC			"VTEMU library"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VSCREEN_IPRIORITY)
#define	MODULE_INIT			NULL
#define	MODULE_EXIT			NULL

int vscreen_soft_bind(u32 refresh_rate,
		      u32 esc_key_code0,
		      u32 esc_key_code1,
		      u32 esc_key_code2,
		      struct fb_info *info,
		      struct vmm_vdisplay *vdis,
		      struct vmm_vkeyboard *vkbd,
		      struct vmm_vmouse *vmou)
{
	/* FIXME: */
	return VMM_EFAIL;
}
VMM_EXPORT_SYMBOL(vscreen_soft_bind);

int vscreen_hard_bind(u32 esc_key_code0,
		      u32 esc_key_code1,
		      u32 esc_key_code2,
		      struct fb_info *info,
		      struct vmm_vdisplay *vdis,
		      struct vmm_vkeyboard *vkbd,
		      struct vmm_vmouse *vmou)
{
	/* FIXME: */
	return VMM_EFAIL;
}
VMM_EXPORT_SYMBOL(vscreen_hard_bind);

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
