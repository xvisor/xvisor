/**
 * Copyright (c) 2012 Anup Patel.
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
#include <vmm_main.h>
#include <vmm_smp.h>
#include <vmm_spinlocks.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <arch_board.h>
#include <arch_timer.h>
#include <drv/clkdev.h>
#include <libs/vtemu.h>

#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>

#include <motherboard.h>

#include <gic.h>
#include <sp810.h>
#include <sp804_timer.h>
#include <smp_twd.h>
#include <generic_timer.h>
#include <versatile/clcd.h>
#include <versatile/clock.h>

/*
 * Global board context
 */

static vmm_spinlock_t v2m_cfg_lock;
static virtual_addr_t v2m_sys_base;
static virtual_addr_t v2m_sys_24mhz;
static virtual_addr_t v2m_sctl_base;
static virtual_addr_t v2m_sp804_base;
static u32 v2m_sp804_irq;

#if defined(CONFIG_VTEMU)
struct vtemu *v2m_vt;
#endif

void v2m_flags_set(u32 addr)
{
	vmm_writel(~0x0, (void *)(v2m_sys_base + V2M_SYS_FLAGSCLR));
	vmm_writel(addr, (void *)(v2m_sys_base + V2M_SYS_FLAGSSET));

	arch_mb();
}

int v2m_cfg_write(u32 devfn, u32 data)
{
	u32 val;
	irq_flags_t flags;

	devfn |= SYS_CFG_START | SYS_CFG_WRITE;

	vmm_spin_lock_irqsave(&v2m_cfg_lock, flags);
	val = vmm_readl((void *)(v2m_sys_base + V2M_SYS_CFGSTAT));
	vmm_writel(val & ~SYS_CFG_COMPLETE, (void *)(v2m_sys_base + V2M_SYS_CFGSTAT));

	vmm_writel(data, (void *)(v2m_sys_base + V2M_SYS_CFGDATA));
	vmm_writel(devfn, (void *)(v2m_sys_base + V2M_SYS_CFGCTRL));

	do {
		val = vmm_readl((void *)(v2m_sys_base + V2M_SYS_CFGSTAT));
	} while (val == 0);
	vmm_spin_unlock_irqrestore(&v2m_cfg_lock, flags);

	return !!(val & SYS_CFG_ERR);
}

int v2m_cfg_read(u32 devfn, u32 *data)
{
	u32 val;
	irq_flags_t flags;

	devfn |= SYS_CFG_START;

	vmm_spin_lock_irqsave(&v2m_cfg_lock, flags);
	vmm_writel(0, (void *)(v2m_sys_base + V2M_SYS_CFGSTAT));
	vmm_writel(devfn, (void *)(v2m_sys_base + V2M_SYS_CFGCTRL));

	arch_mb();

	do {
		/* FIXME: cpu_relax() */
		val = vmm_readl((void *)(v2m_sys_base + V2M_SYS_CFGSTAT));
	} while (val == 0);

	*data = vmm_readl((void *)(v2m_sys_base + V2M_SYS_CFGDATA));
	vmm_spin_unlock_irqrestore(&v2m_cfg_lock, flags);

	return !!(val & SYS_CFG_ERR);
}

/*
 * Reset & Shutdown
 */

static int v2m_reset(void)
{
	if (v2m_cfg_write(SYS_CFG_REBOOT | SYS_CFG_SITE_MB, 0)) {
		vmm_panic("Unable to reboot\n");
	}
	return VMM_OK;
}

static int v2m_shutdown(void)
{
	if (v2m_cfg_write(SYS_CFG_SHUTDOWN | SYS_CFG_SITE_MB, 0)) {
		vmm_panic("Unable to shutdown\n");
	}
	return VMM_OK;
}

/*
 * Clocking support
 */

static long ct_round(struct clk *clk, unsigned long rate)
{
	return rate;
}

static int ct_set(struct clk *clk, unsigned long rate)
{
	return v2m_cfg_write(SYS_CFG_OSC | SYS_CFG_SITE_DB1 | 1, rate);
}

static const struct clk_ops osc1_clk_ops = {
	.round	= ct_round,
	.set	= ct_set,
};

static struct clk osc1_clk = {
	.ops	= &osc1_clk_ops,
	.rate	= 24000000,
};

static struct clk clk24mhz = {
	.rate	= 24000000,
};

int clk_prepare(struct clk *clk)
{
	/* Ignore it. */
	return 0;
}

void clk_unprepare(struct clk *clk)
{
	/* Ignore it. */
}

static struct clk_lookup clcd_lookup = {
	.dev_id = "clcd",
	.con_id = NULL,
	.clk = &osc1_clk,
};

static struct clk_lookup kmi_lookup = {
	.dev_id = NULL,
	.con_id = "KMIREFCLK",
	.clk = &clk24mhz,
};

/*
 * CLCD support.
 */

static void vexpress_clcd_enable(struct clcd_fb *fb)
{
	v2m_cfg_write(SYS_CFG_MUXFPGA | SYS_CFG_SITE_DB1, 0);
	v2m_cfg_write(SYS_CFG_DVIMODE | SYS_CFG_SITE_DB1, 2);
}

static int vexpress_clcd_setup(struct clcd_fb *fb)
{
	unsigned long framesize = 1024 * 768 * 2;

	fb->panel = versatile_clcd_get_panel("XVGA");
	if (!fb->panel)
		return VMM_EINVALID;

	return versatile_clcd_setup(fb, framesize);
}

static struct clcd_board clcd_system_data = {
	.name		= "VExpress",
	.caps		= CLCD_CAP_5551 | CLCD_CAP_565,
	.check		= clcdfb_check,
	.decode		= clcdfb_decode,
	.enable		= vexpress_clcd_enable,
	.setup		= vexpress_clcd_setup,
	.remove		= versatile_clcd_remove,
};

/*
 * Print board information
 */

void arch_board_print_info(struct vmm_chardev *cdev)
{
	/* FIXME: To be implemented. */
}

/*
 * Initialization functions
 */

int __cpuinit arch_host_irq_init(void)
{
	int rc;
	u32 cpu = vmm_smp_processor_id();
	struct vmm_devtree_node *node;

	if (!cpu) {
		node = vmm_devtree_find_compatible(NULL, NULL, 
						   "arm,cortex-a9-gic");
		if (!node) {
			return VMM_ENODEV;
		}

		rc = gic_devtree_init(node, NULL);
	} else {
		gic_secondary_init(0);
		rc = VMM_OK;
	}

	return rc;
}

int __init arch_board_early_init(void)
{
	int rc;
	u32 val;
	struct vmm_devtree_node *node;

	/* Host aspace, Heap, Device tree, and Host IRQ available.
	 *
	 * Do necessary early stuff like:
	 * iomapping devices, 
	 * SOC clocking init, 
	 * Setting-up system data in device tree nodes,
	 * ....
	 */

	/* Register CLCD and KMI clocks */
	clkdev_add(&clcd_lookup);
	clkdev_add(&kmi_lookup);

	/* Init config lock */
	INIT_SPIN_LOCK(&v2m_cfg_lock);

	/* Map sysreg */
	node = vmm_devtree_find_compatible(NULL, NULL, "arm,vexpress-sysreg");
	if (!node) {
		return VMM_ENODEV;
	}
	rc = vmm_devtree_regmap(node, &v2m_sys_base, 0);
	if (rc) {
		return rc;
	}

	/* Register reset & shutdown callbacks */
	vmm_register_system_reset(v2m_reset);
	vmm_register_system_shutdown(v2m_shutdown);

	/* Get address of 24mhz counter */
	v2m_sys_24mhz = v2m_sys_base + V2M_SYS_24MHZ;

	/* Map sysctl */
	node = vmm_devtree_find_compatible(NULL, NULL, "arm,sp810");
	if (!node) {
		return VMM_ENODEV;
	}
	rc = vmm_devtree_regmap(node, &v2m_sctl_base, 0);
	if (rc) {
		return rc;
	}

	/* Select reference clock for sp804 timers: 
	 *      REFCLK is 32KHz
	 *      TIMCLK is 1MHz
	 */
	val = vmm_readl((void *)v2m_sctl_base) | 
				SCCTRL_TIMEREN0SEL_TIMCLK |
				SCCTRL_TIMEREN1SEL_TIMCLK;
	vmm_writel(val, (void *)v2m_sctl_base);

	/* Map sp804 registers */
	node = vmm_devtree_find_compatible(NULL, NULL, "arm,sp804");
	if (!node) {
		return VMM_ENODEV;
	}
	rc = vmm_devtree_regmap(node, &v2m_sp804_base, 0);
	if (rc) {
		return rc;
	}

	/* Get sp804 irq */
	rc = vmm_devtree_irq_get(node, &v2m_sp804_irq, 0);
	if (rc) {
		return rc;
	}

	/* Setup CLCD (before probing) */
	node = vmm_devtree_find_compatible(NULL, NULL, "arm,pl111");
	if (node) {
		node->system_data = &clcd_system_data;
	}

	return 0;
}

int __init arch_clocksource_init(void)
{
	int rc;

	/* Initialize sp804 timer0 as clocksource */
	rc = sp804_clocksource_init(v2m_sp804_base, 
				    "sp804_timer0", 1000000);
	if (rc) {
		vmm_printf("%s: sp804 clocksource init failed (error %d)\n", 
			   __func__, rc);
	}

#if defined(CONFIG_ARM_GENERIC_TIMER)
	/* Initialize generic timer as clock source */
	rc = generic_timer_clocksource_init();
	if (rc) {
		vmm_printf("%s: generic clocksource init failed (error %d)\n",
			   __func__, rc);
	}
#endif

	return VMM_OK;
}

int __cpuinit arch_clockchip_init(void)
{
	int rc;
	u32 cpu = vmm_smp_processor_id();

	if (!cpu) {
		/* Initialize sp804 timer1 as clockchip */
		rc = sp804_clockchip_init(v2m_sp804_base + 0x20, 
					  "sp804_timer1", v2m_sp804_irq, 
					  1000000, 0);
		if (rc) {
			vmm_printf("%s: sp804 clockchip init failed "
				   "(error %d)\n", __func__, rc);
		}
	}

#if defined(CONFIG_ARM_TWD)
	/* Initialize SMP twd local timer as clockchip */
	rc = twd_clockchip_init(v2m_sys_24mhz, 24000000);
	if (rc) {
		vmm_printf("%s: local timer init failed (error %d)\n", 
			   __func__, rc);
	}
#endif

#if defined(CONFIG_ARM_GENERIC_TIMER)
	/* Initialize generic timer as clock source */
	rc = generic_timer_clockchip_init();
	if (rc) {
		vmm_printf("%s: generic clockchip init failed (error %d)\n", 
			   __func__, rc);
	}
#endif

	return VMM_OK;
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

	/* Find simple-bus node */
	node = vmm_devtree_find_compatible(NULL, NULL, "simple-bus");
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
	node = vmm_devtree_find_compatible(NULL, NULL, "arm,pl111");
	if (!node) {
		return VMM_ENODEV;
	}
	info = vmm_fb_find(node->name);
	if (info) {
		v2m_vt = vtemu_create(node->name, info, NULL);
	}
#endif

	return VMM_OK;
}
