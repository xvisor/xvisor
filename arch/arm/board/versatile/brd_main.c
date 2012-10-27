/**
 * Copyright (c) 2012 Jean-Christophe Dubois.
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
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief main source file for board specific code
 */

#include <vmm_error.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <vmm_stdio.h>
#include <vmm_chardev.h>
#include <rtc/vmm_rtcdev.h>
#include <libs/libfdt.h>
#include <versatile_plat.h>
#include <versatile_board.h>
#include <sp804_timer.h>

/*
 * Device Tree support
 */

extern u32 dt_blob_start;
static virtual_addr_t versatile_sys_base;

int arch_board_ram_start(physical_addr_t *addr)
{
	int rc = VMM_OK;
	struct fdt_fileinfo fdt;
	struct fdt_node_header *fdt_node;

	*addr = 0xffffffff;

	if (libfdt_parse_fileinfo((virtual_addr_t) & dt_blob_start, &fdt)) {
		return VMM_EFAIL;
	}

	fdt_node = libfdt_find_node(&fdt,
				    VMM_DEVTREE_PATH_SEPARATOR_STRING
				    VMM_DEVTREE_HOSTINFO_NODE_NAME
				    VMM_DEVTREE_PATH_SEPARATOR_STRING
				    VMM_DEVTREE_MEMORY_NODE_NAME);
	if (!fdt_node) {
		return VMM_EFAIL;
	}

	rc = libfdt_get_property(&fdt, fdt_node,
				 VMM_DEVTREE_MEMORY_PHYS_ADDR_ATTR_NAME, addr);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

int arch_board_ram_size(physical_size_t *size)
{
	int rc = VMM_OK;
	struct fdt_fileinfo fdt;
	struct fdt_node_header *fdt_node;

	*size = 0;

	if (libfdt_parse_fileinfo((virtual_addr_t) & dt_blob_start, &fdt)) {
		return VMM_EFAIL;
	}

	fdt_node = libfdt_find_node(&fdt,
				    VMM_DEVTREE_PATH_SEPARATOR_STRING
				    VMM_DEVTREE_HOSTINFO_NODE_NAME
				    VMM_DEVTREE_PATH_SEPARATOR_STRING
				    VMM_DEVTREE_MEMORY_NODE_NAME);
	if (!fdt_node) {
		return VMM_EFAIL;
	}

	rc = libfdt_get_property(&fdt, fdt_node,
				 VMM_DEVTREE_MEMORY_PHYS_SIZE_ATTR_NAME, size);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

int arch_board_devtree_populate(struct vmm_devtree_node **root)
{
	struct fdt_fileinfo fdt;

	if (libfdt_parse_fileinfo((virtual_addr_t) & dt_blob_start, &fdt)) {
		return VMM_EFAIL;
	}

	return libfdt_parse_devtree(&fdt, root);
}

/*
 * Reset & Shutdown
 */

int arch_board_reset(void)
{
	vmm_writel(0x101,
		   (void *)(versatile_sys_base +
			    VERSATILE_SYS_RESETCTL_OFFSET));

	return VMM_OK;
}

int arch_board_shutdown(void)
{
	/* FIXME: TBD */
	return VMM_OK;
}

/*
 * Initialization functions
 */

int __init arch_board_early_init(void)
{
	/*
	 * TODO:
	 * Host virtual memory, device tree, heap is up.
	 * Do necessary early stuff like iomapping devices
	 * memory or boot time memory reservation here.
	 */
	return VMM_OK;
}

static virtual_addr_t sp804_timer0_base;
static virtual_addr_t sp804_timer1_base;

int __init arch_clocksource_init(void)
{
	int rc;
	u32 val;
	virtual_addr_t sctl_base;

	/* Map control registers */
	sctl_base = vmm_host_iomap(VERSATILE_SCTL_BASE, 0x1000);

        /*
         * set clock frequency:
         *      REALVIEW_REFCLK is 32KHz
         *      REALVIEW_TIMCLK is 1MHz
         */
        val = vmm_readl((void *)sctl_base) |
                        (VERSATILE_TIMCLK << VERSATILE_TIMER2_EnSel);
        vmm_writel(val, (void *)sctl_base);

	/* Unmap control register */
	rc = vmm_host_iounmap(sctl_base, 0x1000);
	if (rc) {
		return rc;
	}

	/* Configure timer1 as free running source */
	/* Map timer registers */
	sp804_timer1_base = vmm_host_iomap(VERSATILE_TIMER0_1_BASE, 0x1000);
	sp804_timer1_base += 0x20;

	/* Initialize timer1 as clocksource */
	rc = sp804_clocksource_init(sp804_timer1_base, 
				    "sp804_timer1", 300, 1000000, 20);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

int __init arch_clockchip_init(void)
{
	int rc;
	u32 val;
	virtual_addr_t sctl_base;

	/* Map control registers */
	sctl_base = vmm_host_iomap(VERSATILE_SCTL_BASE, 0x1000);

        /*
         * set clock frequency:
         *      REALVIEW_REFCLK is 32KHz
         *      REALVIEW_TIMCLK is 1MHz
         */
        val = vmm_readl((void *)sctl_base) |
                        (VERSATILE_TIMCLK << VERSATILE_TIMER1_EnSel);
        vmm_writel(val, (void *)sctl_base);

	/* Unmap control register */
	rc = vmm_host_iounmap(sctl_base, 0x1000);
	if (rc) {
		return rc;
	}

	/* Map timer0 registers */
	sp804_timer0_base = vmm_host_iomap(VERSATILE_TIMER0_1_BASE, 0x1000);

	/* Initialize timer0 as clockchip */
	rc = sp804_clockchip_init(sp804_timer0_base, INT_TIMERINT0_1, 
				  "sp804_timer0", 300, 1000000, 0);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

int __init arch_board_final_init(void)
{
	struct vmm_devtree_node *node;
	struct vmm_chardev *cdev;
#if defined(CONFIG_RTC)
	struct vmm_rtcdev * rdev;
#endif

	/* All VMM API's are available here */
	/* We can register a Board specific resource here */

	/* Map control registers */
	versatile_sys_base = vmm_host_iomap(VERSATILE_SYS_BASE, 0x1000);

	/* Unlock Lockable registers */
	vmm_writel(VERSATILE_SYS_LOCKVAL,
		   (void *)(versatile_sys_base + VERSATILE_SYS_LOCK_OFFSET));

	/* Do Probing using device driver framework */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_HOSTINFO_NODE_NAME
				   VMM_DEVTREE_PATH_SEPARATOR_STRING "nbridge");

	if (!node) {
		return VMM_ENOTAVAIL;
	}

	if (vmm_devdrv_probe(node, NULL, NULL)) {
		return VMM_EFAIL;
	}

	/* Find uart0 character device and 
	 * set it as vmm_stdio character device */
	if ((cdev = vmm_chardev_find("uart0"))) {
		vmm_stdio_change_device(cdev);
	}

	/* Syncup wall-clock time from rtc0 */
#if defined(CONFIG_RTC)
	if ((rdev = vmm_rtcdev_find("rtc0"))) {
		int rc;
		if ((rc = vmm_rtcdev_sync_wallclock(rdev))) {
			return rc;
		}
	}
#endif
	return VMM_OK;
}
