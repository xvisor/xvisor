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
 * @file fbnotify.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief frame buffer notification APIs
 * 
 * The source has been largely adapted from Linux 3.x or higher:
 * drivers/video/fb_notify.c
 *
 *  Copyright (C) 2006 Antonino Daplas <adaplas@pol.net>
 *
 *	2001 - Documented with DocBook
 *	- Brad Douglas <brad@neruo.com>
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_modules.h>
#include <drv/fb.h>

static BLOCKING_NOTIFIER_CHAIN(fb_notifier_list);

/**
 *  Register a client notifier
 *  @nb notifier block to callback on events
 */
int fb_register_client(struct vmm_notifier_block *nb)
{
	return vmm_blocking_notifier_register(&fb_notifier_list, nb);
}
VMM_EXPORT_SYMBOL(fb_register_client);

/**
 *  Unregister a client notifier
 *  @nb notifier block to callback on events
 */
int fb_unregister_client(struct vmm_notifier_block *nb)
{
	return vmm_blocking_notifier_unregister(&fb_notifier_list, nb);
}
VMM_EXPORT_SYMBOL(fb_unregister_client);

/**
 * Notify clients of fb_events
 */
int fb_notifier_call_chain(unsigned long val, void *v)
{
	return vmm_blocking_notifier_call(&fb_notifier_list, val, v);
}
VMM_EXPORT_SYMBOL(fb_notifier_call_chain);

