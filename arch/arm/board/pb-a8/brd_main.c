/**
 * Copyright (c) 2011 Anup Patel.
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
 * @author Anup Patel (anup@brainfault.org)
 * @brief main source file for board specific code
 */

#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <vmm_stdio.h>
#include <vmm_chardev.h>
#include <rtc/vmm_rtcdev.h>
#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>
#include <versatile/clcd.h>
#include <libfdt.h>
#include <realview_plat.h>
#include <pba8_board.h>

extern u32 dt_blob_start;
virtual_addr_t pba8_sys_base;

int arch_board_ram_start(physical_addr_t *addr)
{
	int rc = VMM_OK;
	struct fdt_fileinfo fdt;
	struct fdt_node_header *fdt_node;
	
	rc = libfdt_parse_fileinfo((virtual_addr_t) & dt_blob_start, &fdt);
	if (rc) {
		return rc;
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
	
	rc = libfdt_parse_fileinfo((virtual_addr_t) & dt_blob_start, &fdt);
	if (rc) {
		return rc;
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

int arch_board_devtree_populate(struct vmm_devtree_node ** root)
{
	int rc = VMM_OK;
	struct fdt_fileinfo fdt;
	
	rc = libfdt_parse_fileinfo((virtual_addr_t) & dt_blob_start, &fdt);
	if (rc) {
		return rc;
	}

	return libfdt_parse_devtree(&fdt, root);
}

int arch_board_reset(void)
{
	vmm_writel(0x0, 
		   (void *)(pba8_sys_base + REALVIEW_SYS_RESETCTL_OFFSET));
	vmm_writel(REALVIEW_SYS_CTRL_RESET_PLLRESET, 
		   (void *)(pba8_sys_base + REALVIEW_SYS_RESETCTL_OFFSET));
	return VMM_OK;
}

int arch_board_shutdown(void)
{
	/* FIXME: TBD */
	return VMM_OK;
}

int __init arch_board_early_init(void)
{
	/*
	 * TODO:
	 * Host virtual memory, device tree, heap is up.
	 * Do necessary early stuff like iomapping devices
	 * memory or boot time memory reservation here.
	 */
	return 0;
}

/*
 * CLCD support.
 */
#define SYS_CLCD_NLCDIOON	(1 << 2)
#define SYS_CLCD_VDDPOSSWITCH	(1 << 3)
#define SYS_CLCD_PWR3V5SWITCH	(1 << 4)
#define SYS_CLCD_ID_MASK	(0x1f << 8)
#define SYS_CLCD_ID_SANYO_3_8	(0x00 << 8)
#define SYS_CLCD_ID_UNKNOWN_8_4	(0x01 << 8)
#define SYS_CLCD_ID_EPSON_2_2	(0x02 << 8)
#define SYS_CLCD_ID_SANYO_2_5	(0x07 << 8)
#define SYS_CLCD_ID_VGA		(0x1f << 8)

/*
 * Disable all display connectors on the interface module.
 */
static void realview_clcd_disable(struct clcd_fb *fb)
{
	void *sys_clcd = (void *)pba8_sys_base + REALVIEW_SYS_CLCD_OFFSET;
	u32 val;

	val = vmm_readl(sys_clcd);
	val &= ~SYS_CLCD_NLCDIOON | SYS_CLCD_PWR3V5SWITCH;
	vmm_writel(val, sys_clcd);
}

/*
 * Enable the relevant connector on the interface module.
 */
static void realview_clcd_enable(struct clcd_fb *fb)
{
	void *sys_clcd = (void *)pba8_sys_base + REALVIEW_SYS_CLCD_OFFSET;
	u32 val;

	/*
	 * Enable the PSUs
	 */
	val = vmm_readl(sys_clcd);
	val |= SYS_CLCD_NLCDIOON | SYS_CLCD_PWR3V5SWITCH;
	vmm_writel(val, sys_clcd);
}

/*
 * Detect which LCD panel is connected, and return the appropriate
 * clcd_panel structure.  Note: we do not have any information on
 * the required timings for the 8.4in panel, so we presently assume
 * VGA timings.
 */
static int realview_clcd_setup(struct clcd_fb *fb)
{
	void *sys_clcd = (void *)pba8_sys_base + REALVIEW_SYS_CLCD_OFFSET;
	const char *panel_name, *vga_panel_name;
	unsigned long framesize;
	u32 val;

	/* XVGA, 16bpp 
	 * (Assuming machine is always realview-pb-a8 and not realview-eb)
	 */
	framesize = 1024 * 768 * 2;
	vga_panel_name = "XVGA";

	val = vmm_readl(sys_clcd) & SYS_CLCD_ID_MASK;
	if (val == SYS_CLCD_ID_SANYO_3_8)
		panel_name = "Sanyo TM38QV67A02A";
	else if (val == SYS_CLCD_ID_SANYO_2_5)
		panel_name = "Sanyo QVGA Portrait";
	else if (val == SYS_CLCD_ID_EPSON_2_2)
		panel_name = "Epson L2F50113T00";
	else if (val == SYS_CLCD_ID_VGA)
		panel_name = vga_panel_name;
	else {
		vmm_printf("CLCD: unknown LCD panel ID 0x%08x, using VGA\n", val);
		panel_name = vga_panel_name;
	}

	fb->panel = versatile_clcd_get_panel(panel_name);
	if (!fb->panel)
		return VMM_EINVALID;

	return versatile_clcd_setup(fb, framesize);
}

struct clcd_board clcd_system_data = {
	.name		= "PB-A8",
	.caps		= CLCD_CAP_ALL,
	.check		= clcdfb_check,
	.decode		= clcdfb_decode,
	.disable	= realview_clcd_disable,
	.enable		= realview_clcd_enable,
	.setup		= realview_clcd_setup,
	.remove		= versatile_clcd_remove,
};

int __init arch_board_final_init(void)
{
	int rc;
	struct vmm_devtree_node *node;
	struct vmm_chardev * cdev;
#if defined(CONFIG_RTC)
	struct vmm_rtcdev * rdev;
#endif

	/* All VMM API's are available here */
	/* We can register a Board specific resource here */

	/* Map control registers */
	pba8_sys_base = vmm_host_iomap(REALVIEW_SYS_BASE, 0x1000);

	/* Unlock Lockable registers */
	vmm_writel(REALVIEW_SYS_LOCKVAL, 
		   (void *)(pba8_sys_base + REALVIEW_SYS_LOCK_OFFSET));

	/* Setup CLCD system data (before probing) */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_HOSTINFO_NODE_NAME
				   VMM_DEVTREE_PATH_SEPARATOR_STRING "nbridge"
				   VMM_DEVTREE_PATH_SEPARATOR_STRING "sbridge"
				   VMM_DEVTREE_PATH_SEPARATOR_STRING "clcd");
	if (node) {
		node->system_data = &clcd_system_data;
	}

	/* Do Probing using device driver framework */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_HOSTINFO_NODE_NAME
				   VMM_DEVTREE_PATH_SEPARATOR_STRING "nbridge");
	if (!node) {
		return VMM_ENOTAVAIL;
	}

	rc = vmm_devdrv_probe(node, NULL, NULL);
	if (rc) {
		return rc;
	}

	/* Find uart0 character device and 
	 * set it as vmm_stdio character device */
	if ((cdev = vmm_chardev_find("uart0"))) {
		vmm_stdio_change_device(cdev);
	}

	/* Syncup wall-clock time from rtc0 */
#if defined(CONFIG_RTC)
	if ((rdev = vmm_rtcdev_find("rtc0"))) {
		if ((rc = vmm_rtcdev_sync_wallclock(rdev))) {
			return rc;
		}
	}
#endif

	return VMM_OK;
}
