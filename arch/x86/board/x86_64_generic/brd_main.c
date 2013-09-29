/**
 * Copyright (c) 2010-20 Himanshu Chauhan.
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
 * @file brd_main.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief main source file for board specific code
 */

#include <vmm_main.h>
#include <vmm_devtree.h>
#include <vmm_error.h>
#include <vmm_host_aspace.h>
#include <vmm_devdrv.h>
#include <vmm_stdio.h>
#include <libs/vtemu.h>
#include <arch_board.h>

#include <hpet.h>

#if defined(CONFIG_VTEMU)
struct vtemu *x86_vt;
#endif

int arch_board_reset(void)
{
	return VMM_EFAIL;
}

int arch_board_shutdown(void)
{
	return VMM_EFAIL;
}

int __init arch_board_early_init(void)
{
	int rv;

	rv = hpet_init();
	BUG_ON(rv != VMM_OK);

        return VMM_OK;
}

int __init arch_clocksource_init(void)
{
	return hpet_clocksource_init(DEFAULT_HPET_SYS_TIMER,
				     "hpet_clksrc");
}

int __cpuinit arch_clockchip_init(void)
{
	return hpet_clockchip_init(DEFAULT_HPET_SYS_TIMER, 
				"hpet_clkchip", 0);
}

int __init arch_board_final_init(void)
{
	int rc;
	struct vmm_devtree_node *node;
#if defined(CONFIG_VTEMU)
	struct vmm_fb_info *info;
#endif

	/* All VMM API's are available here */
	/* We can register a Board specific resource here */

	/* Find simple bus node */
	node = vmm_devtree_find_compatible(NULL, NULL, "simple-bus");
	if (!node) {
		return VMM_ENODEV;
	}

	/* Do probing using device driver framework */
	rc = vmm_devdrv_probe(node);
	if (rc) {
		return rc;
	}

	/* Create VTEMU instace from first available frame buffer 
	 * and make it our stdio device.
	 */
#if defined(CONFIG_VTEMU)
	info = vmm_fb_get(0);
	if (info) {
		x86_vt = vtemu_create(info->dev->node->name, info, NULL);
		if (x86_vt) {
			vmm_stdio_change_device(&x86_vt->cdev);
		}
	}
#endif

	return VMM_OK;
}

void arch_board_print_info(struct vmm_chardev *cdev)
{
}
