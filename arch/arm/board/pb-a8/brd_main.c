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
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <vmm_stdio.h>
#include <vmm_chardev.h>
#include <rtc/vmm_rtcdev.h>
#include <stringlib.h>
#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>
#include <versatile/clcd.h>
#include <versatile/clock.h>
#include <libfdt.h>
#include <vtemu.h>
#include <realview_plat.h>
#include <pba8_board.h>
#include <sp804_timer.h>

/*
 * Global board context
 */

virtual_addr_t pba8_sys_base;
#if defined(CONFIG_VTEMU)
struct vtemu *pba8_vt;
#endif

/*
 * Device Tree support
 */

extern u32 dt_blob_start;

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

/*
 * Reset & Shutdown
 */

int arch_board_reset(void)
{
	void *sys_lock = (void *)pba8_sys_base + REALVIEW_SYS_LOCK_OFFSET;

	vmm_writel(REALVIEW_SYS_LOCKVAL, sys_lock);
	vmm_writel(0x0, 
		   (void *)(pba8_sys_base + REALVIEW_SYS_RESETCTL_OFFSET));
	vmm_writel(REALVIEW_SYS_CTRL_RESET_PLLRESET, 
		   (void *)(pba8_sys_base + REALVIEW_SYS_RESETCTL_OFFSET));
	vmm_writel(0, sys_lock);

	return VMM_OK;
}

int arch_board_shutdown(void)
{
	/* FIXME: TBD */
	return VMM_OK;
}

/*
 * Clocking support
 */

static const struct icst_params realview_oscvco_params = {
	.ref		= 24000000,
	.vco_max	= ICST307_VCO_MAX,
	.vco_min	= ICST307_VCO_MIN,
	.vd_min		= 4 + 8,
	.vd_max		= 511 + 8,
	.rd_min		= 1 + 2,
	.rd_max		= 127 + 2,
	.s2div		= icst307_s2div,
	.idx2s		= icst307_idx2s,
};

static void realview_oscvco_set(struct versatile_clk *vclk, struct icst_vco vco)
{
	void *sys_lock = (void *)pba8_sys_base + REALVIEW_SYS_LOCK_OFFSET;
	u32 val;

	val = vmm_readl(vclk->vcoreg) & ~0x7ffff;
	val |= vco.v | (vco.r << 9) | (vco.s << 16);

	vmm_writel(REALVIEW_SYS_LOCKVAL, sys_lock);
	vmm_writel(val, vclk->vcoreg);
	vmm_writel(0, sys_lock);
}

static const struct versatile_clk_ops oscvco_clk_ops = {
	.round	= icst_clk_round,
	.set	= icst_clk_set,
	.setvco	= realview_oscvco_set,
};

static struct versatile_clk oscvco_clk = {
	.ops	= &oscvco_clk_ops,
	.params	= &realview_oscvco_params,
};

static struct vmm_devclk clcd_clk = {
	.enable = versatile_clk_enable,
	.disable = versatile_clk_disable,
	.get_rate = versatile_clk_get_rate,
	.round_rate = versatile_clk_round_rate,
	.set_rate = versatile_clk_set_rate,
	.priv = &oscvco_clk,
};

static struct vmm_devclk *realview_getclk(struct vmm_devtree_node *node)
{
	if (strcmp(node->name, "clcd") == 0) {
		return &clcd_clk;
	}

	return NULL;
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
	return 0;
}

static virtual_addr_t pba8_timer0_base;
static virtual_addr_t pba8_timer1_base;

int __init arch_clocksource_init(void)
{
	int rc;
	u32 val;
	virtual_addr_t sctl_base;

	/* Map control registers */
	sctl_base = vmm_host_iomap(REALVIEW_SCTL_BASE, 0x1000);

	/* 
	 * set clock frequency: 
	 *      REALVIEW_REFCLK is 32KHz
	 *      REALVIEW_TIMCLK is 1MHz
	 */
	val = vmm_readl((void *)sctl_base) | 
			(REALVIEW_TIMCLK << REALVIEW_TIMER2_EnSel);
	vmm_writel(val, (void *)sctl_base);

	/* Unmap control register */
	rc = vmm_host_iounmap(sctl_base, 0x1000);
	if (rc) {
		return rc;
	}

	/* Map timer1 registers */
	pba8_timer1_base = vmm_host_iomap(REALVIEW_PBA8_TIMER0_1_BASE, 0x1000);
	pba8_timer1_base += 0x20;

	/* Initialize timer1 as clocksource */
	rc = sp804_clocksource_init(pba8_timer1_base, 
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
	sctl_base = vmm_host_iomap(REALVIEW_SCTL_BASE, 0x1000);

	/* 
	 * set clock frequency: 
	 *      REALVIEW_REFCLK is 32KHz
	 *      REALVIEW_TIMCLK is 1MHz
	 */
	val = vmm_readl((void *)sctl_base) | 
			(REALVIEW_TIMCLK << REALVIEW_TIMER1_EnSel);
	vmm_writel(val, (void *)sctl_base);

	/* Unmap control register */
	rc = vmm_host_iounmap(sctl_base, 0x1000);
	if (rc) {
		return rc;
	}

	/* Map timer0 registers */
	pba8_timer0_base = vmm_host_iomap(REALVIEW_PBA8_TIMER0_1_BASE, 0x1000);

	/* Initialize timer0 as clockchip */
	rc = sp804_clockchip_init(pba8_timer0_base, IRQ_PBA8_TIMER0_1, 
				  "sp804_timer0", 300, 1000000, 0);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

int __init arch_board_final_init(void)
{
	int rc;
	struct vmm_devtree_node *node;
	struct vmm_chardev *cdev;
#if defined(CONFIG_RTC)
	struct vmm_rtcdev *rdev;
#endif
#if defined(CONFIG_VTEMU)
	struct vmm_fb_info *info;
#endif

	/* All VMM API's are available here */
	/* We can register a Board specific resource here */

	/* Map control registers */
	pba8_sys_base = vmm_host_iomap(REALVIEW_SYS_BASE, 0x1000);

	/* Setup Clocks (before probing) */
	oscvco_clk.vcoreg = (void *)pba8_sys_base + REALVIEW_SYS_OSC4_OFFSET;

	/* Setup CLCD (before probing) */
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

	rc = vmm_devdrv_probe(node, realview_getclk, NULL);
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

	/* Create VTEMU instace if available*/
#if defined(CONFIG_VTEMU)
	info = vmm_fb_find("clcd");
	if (info) {
		pba8_vt = vtemu_create("clcd-vtemu", info, NULL);
	}
#endif

	return VMM_OK;
}
