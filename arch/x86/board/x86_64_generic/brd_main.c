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
#include <vmm_host_ram.h>
#include <vmm_platform.h>
#include <vmm_stdio.h>
#include <multiboot.h>
#include <libs/vtemu.h>
#include <libs/bitops.h>
#include <arch_board.h>

#include <hpet.h>

#ifdef CONFIG_PCI
extern int __init pci_arch_init(void);
extern int __init pci_subsys_init(void);
#endif

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
		if (good & (1 << KBRD_BIT_KDATA))
			vmm_inb(KBRD_IO); /* empty keyboard data */

	} while (good & (1 << KBRD_BIT_UDATA));

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

static int __init boot_module_initrd(physical_addr_t start, physical_addr_t end)
{
	int rc;
	struct vmm_devtree_node *node;

	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_CHOSEN_NODE_NAME);

	/* There should be a /chosen node */
	if (!node) {
		vmm_printf("%s: No chosen node\n", __func__);
		rc = VMM_ENODEV;
		goto _done;
	}

	/*
	 * Assumption here is that start and end physical addresses
	 * will be marked as reserved by the RBD driver.
	 */
	rc = vmm_devtree_setattr(node, "linux,initrd-start",
				 &start, VMM_DEVTREE_ATTRTYPE_UINT64,
				 sizeof(start), FALSE);
	if (rc != VMM_OK)
		goto _done;

	rc = vmm_devtree_setattr(node, "linux,initrd-end",
				 &end, VMM_DEVTREE_ATTRTYPE_UINT64,
				 sizeof(end), FALSE);

 _done:
	return rc;
}

static int __init boot_modules_init(void)
{
	extern struct multiboot_info boot_info;
	int i, rc;
	struct multiboot_mod_list *modlist = NULL;

	if (!boot_info.mods_count)
		return VMM_OK;

	modlist = (struct multiboot_mod_list *)vmm_host_memmap(boot_info.mods_addr,
							       (boot_info.mods_count * (4 * 1024)),
							       VMM_MEMORY_FLAGS_NORMAL);
	if (modlist == NULL) {
		vmm_printf("Boot info module address mapping failed!\n");
		return VMM_EFAIL;
	}

	for (i = 0; i < boot_info.mods_count; i++) {
		switch(i) {
		case 0:
			rc = boot_module_initrd(modlist->mod_start,
						modlist->mod_end);
			if (rc != VMM_OK)
				goto _done;
			break;

		default:
			rc = VMM_ENODEV;
			vmm_printf("Unknown Mod Start: 0x%"PRIx32
				   " Mod End: 0x%"PRIx32"\n",
				   modlist->mod_start, modlist->mod_end);
		}
	}

 _done:
	if (modlist)
		vmm_host_memunmap((virtual_addr_t)modlist);

	return rc;
}

int __init arch_board_early_init(void)
{
	int rv;

	rv = hpet_init();
	BUG_ON(rv != VMM_OK);

	/* Register reset & shutdown callbacks */
	vmm_register_system_reset(generic_reset);
	vmm_register_system_shutdown(generic_shutdown);

	if ((rv = boot_modules_init()) != VMM_OK) {
		vmm_printf("Initializing boot modules failed!\n");
	}

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

	/* Do platform device probing using device driver framework */
	rc = vmm_platform_probe(node);
	vmm_devtree_dref_node(node);
	if (rc) {
		return rc;
	}

	/* Create VTEMU instace from first available frame buffer 
	 * and make it our stdio device.
	 */
#if defined(CONFIG_VTEMU)
	info = fb_find("fb0");
	if (info) {
		x86_vt = vtemu_create(info->dev.name, info, NULL);
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
