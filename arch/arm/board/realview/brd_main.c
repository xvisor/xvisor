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
#include <arch_board.h>
#include <arch_timer.h>
#include <libs/stringlib.h>
#include <libs/vtemu.h>
#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>
#include <versatile/clcd.h>
#include <versatile/clock.h>
#include <realview_plat.h>
#include <sp804_timer.h>

/*
 * Global board context
 */

static virtual_addr_t realview_sys_base;
static virtual_addr_t realview_sctl_base;
static virtual_addr_t realview_sp804_base;
static u32 realview_sp804_irq;

#if defined(CONFIG_VTEMU)
struct vtemu *realview_vt;
#endif

/*
 * Reset & Shutdown
 */

int arch_board_reset(void)
{
	void *sys_lock = (void *)realview_sys_base + REALVIEW_SYS_LOCK_OFFSET;

	vmm_writel(REALVIEW_SYS_LOCKVAL, sys_lock);
	vmm_writel(0x0, 
		   (void *)(realview_sys_base + REALVIEW_SYS_RESETCTL_OFFSET));
	vmm_writel(REALVIEW_SYS_CTRL_RESET_PLLRESET, 
		   (void *)(realview_sys_base + REALVIEW_SYS_RESETCTL_OFFSET));
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

static void realview_oscvco_set(struct arch_clk *clk, struct icst_vco vco)
{
	void *sys_lock = (void *)realview_sys_base + REALVIEW_SYS_LOCK_OFFSET;
	u32 val;

	val = vmm_readl(clk->vcoreg) & ~0x7ffff;
	val |= vco.v | (vco.r << 9) | (vco.s << 16);

	vmm_writel(REALVIEW_SYS_LOCKVAL, sys_lock);
	vmm_writel(val, clk->vcoreg);
	vmm_writel(0, sys_lock);
}

static const struct arch_clk_ops oscvco_clk_ops = {
	.round	= icst_clk_round,
	.set	= icst_clk_set,
	.setvco	= realview_oscvco_set,
};

static struct arch_clk oscvco_clk = {
	.ops	= &oscvco_clk_ops,
	.params	= &realview_oscvco_params,
};

static struct arch_clk clk24mhz = {
	.rate	= 24000000,
};

int arch_clk_prepare(struct arch_clk *clk)
{
	/* Ignore it. */
	return 0;
}

void arch_clk_unprepare(struct arch_clk *clk)
{
	/* Ignore it. */
}

struct arch_clk *arch_clk_get(struct vmm_device *dev, const char *id)
{
	if (strcmp(dev->node->name, "clcd") == 0) {
		return &oscvco_clk;
	}

	if (strcmp(id, "KMIREFCLK") == 0) {
		return &clk24mhz;
	}

	return NULL;
}

void arch_clk_put(struct arch_clk *clk)
{
	/* Ignore it. */
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
	void *sys_clcd = (void *)realview_sys_base + REALVIEW_SYS_CLCD_OFFSET;
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
	void *sys_clcd = (void *)realview_sys_base + REALVIEW_SYS_CLCD_OFFSET;
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
	void *sys_clcd = (void *)realview_sys_base + REALVIEW_SYS_CLCD_OFFSET;
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
	int rc;
	u32 val, *valp;
	struct vmm_devtree_node *hnode, *node;

	/* Host aspace, Heap, Device tree, and Host IRQ available.
	 *
	 * Do necessary early stuff like:
	 * iomapping devices, 
	 * SOC clocking init, 
	 * Setting-up system data in device tree nodes,
	 * ....
	 */

	/* Get host node */
	hnode = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				    VMM_DEVTREE_HOSTINFO_NODE_NAME);

	/* Map sysreg */
	node = vmm_devtree_find_compatible(hnode, NULL, "arm,realview-sysreg");
	if (!node) {
		return VMM_ENODEV;
	}
	rc = vmm_devtree_regmap(node, &realview_sys_base, 0);
	if (rc) {
		return rc;
	}

	/* Map sysctl */
	node = vmm_devtree_find_compatible(hnode, NULL, "arm,sp810");
	if (!node) {
		return VMM_ENODEV;
	}
	rc = vmm_devtree_regmap(node, &realview_sctl_base, 0);
	if (rc) {
		return rc;
	}

	/* Select reference clock for sp804 timers: 
	 *      REFCLK is 32KHz
	 *      TIMCLK is 1MHz
	 */
	val = vmm_readl((void *)realview_sctl_base) | 
			(REALVIEW_TIMCLK << REALVIEW_TIMER1_EnSel) |
			(REALVIEW_TIMCLK << REALVIEW_TIMER2_EnSel) |
			(REALVIEW_TIMCLK << REALVIEW_TIMER3_EnSel) |
			(REALVIEW_TIMCLK << REALVIEW_TIMER4_EnSel);
	vmm_writel(val, (void *)realview_sctl_base);

	/* Map sp804 registers */
	node = vmm_devtree_find_compatible(hnode, NULL, "arm,sp804");
	if (!node) {
		return VMM_ENODEV;
	}
	rc = vmm_devtree_regmap(node, &realview_sp804_base, 0);
	if (rc) {
		return rc;
	}

	/* Get sp804 irq */
	valp = vmm_devtree_attrval(node, "irq");
	if (!valp) {
		return VMM_EFAIL;
	}
	realview_sp804_irq = *valp;

	/* Setup Clocks (before probing) */
	oscvco_clk.vcoreg = (void *)realview_sys_base + REALVIEW_SYS_OSC4_OFFSET;

	/* Setup CLCD (before probing) */
	node = vmm_devtree_find_compatible(hnode, NULL, "arm,pl111");
	if (node) {
		node->system_data = &clcd_system_data;
	}

	return VMM_OK;
}

int __init arch_clocksource_init(void)
{
	int rc;

	/* Initialize sp804 timer0 as clocksource */
	rc = sp804_clocksource_init(realview_sp804_base, 
				    "sp804_timer0", 300, 1000000, 20);
	if (rc) {
		vmm_printf("%s: sp804 clocksource init failed (error %d)\n", 
			   __func__, rc);
	}

	return VMM_OK;
}

int __cpuinit arch_clockchip_init(void)
{
	int rc;

	/* Initialize sp804 timer1 as clockchip */
	rc = sp804_clockchip_init(realview_sp804_base + 0x20, 
				  realview_sp804_irq, 
				  "sp804_timer1", 300, 1000000, 0);
	if (rc) {
		vmm_printf("%s: sp804 clockchip init failed (error %d)\n", 
			   __func__, rc);
	}

	return VMM_OK;
}

int __init arch_board_final_init(void)
{
	int rc;
	struct vmm_devtree_node *hnode, *node;
#if defined(CONFIG_VTEMU)
	struct vmm_fb_info *info;
#endif

	/* All VMM API's are available here */
	/* We can register a Board specific resource here */

	/* Get host node */
	hnode = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				    VMM_DEVTREE_HOSTINFO_NODE_NAME);

	/* Find simple-bus node */
	node = vmm_devtree_find_compatible(hnode, NULL, "simple-bus");
	if (!node) {
		return VMM_ENODEV;
	}

	/* Do probing using device driver framework */
	rc = vmm_devdrv_probe(node);
	if (rc) {
		return rc;
	}

	/* Create VTEMU instace if available*/
#if defined(CONFIG_VTEMU)
	node = vmm_devtree_find_compatible(hnode, NULL, "arm,pl111");
	if (!node) {
		return VMM_ENODEV;
	}
	info = vmm_fb_find(node->name);
	if (info) {
		realview_vt = vtemu_create(node->name, info, NULL);
	}
#endif

	return VMM_OK;
}
