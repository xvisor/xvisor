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
#include <libs/bitops.h>
#include <arch_board.h>

#include <hpet.h>

#if defined(CONFIG_VTEMU)
struct vtemu *x86_vt;
#endif

#define KBRD_INTFREG	0x64
#define KBRD_BIT_KDATA	0 /* keyboard data is in buffer */
#define KBRD_BIT_UDATA	1 /* user data is in buffer */
#define KBRD_IO		0x60 /* IO Port */
#define KBRD_RESET	0xfe /* RESET CPU command */

static int generic_reset(void)
{
	volatile unsigned long good;

	arch_cpu_irq_disable();

	/* clear all keyboard buffer (user & keyboard) */
	do {
		good = vmm_inb(KBRD_INTFREG); /* empty user data */
		if (test_bit(KBRD_BIT_KDATA, &good) != 0)
			vmm_inb(KBRD_IO); /* empty keyboard data */

	} while (test_bit(KBRD_BIT_UDATA, &good));

	/* toggle the CPU reset pin */
	vmm_outb(KBRD_RESET, KBRD_INTFREG);

	while (1) {
		arch_cpu_wait_for_irq();
	}

	return VMM_EFAIL;
}

static int generic_shutdown(void)
{
	return VMM_EFAIL;
}

int __init arch_board_early_init(void)
{
	int rv;

	rv = hpet_init();
	BUG_ON(rv != VMM_OK);

	/* Register reset & shutdown callbacks */
	vmm_register_system_reset(generic_reset);
	vmm_register_system_shutdown(generic_shutdown);

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
	struct fb_info *info;
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
	info = fb_get(0);
	if (info) {
		x86_vt = vtemu_create(info->dev->name, info, NULL);
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
