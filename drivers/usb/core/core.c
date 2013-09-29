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
 * @file core.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief main source file for USB core framework
 */

#include <vmm_error.h>
#include <vmm_modules.h>
#include <drv/usb.h>
#include <drv/usb/hcd.h>
#include <drv/usb/hub.h>

#define MODULE_DESC			"USB Core Framework"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		USB_CORE_IPRIORITY
#define	MODULE_INIT			usb_core_init
#define	MODULE_EXIT			usb_core_exit

static int __init usb_core_init(void)
{
	int rc;

	rc = usb_hcd_init();
	if (rc) {
		return rc;
	}

	rc = usb_hub_init();
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

static void __exit usb_core_exit(void)
{
	usb_hub_exit();

	usb_hcd_exit();
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
