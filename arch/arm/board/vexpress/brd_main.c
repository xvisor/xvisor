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
#include <vmm_smp.h>
#include <vmm_spinlocks.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <libs/libfdt.h>
#include <libs/vtemu.h>
#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>
#include <versatile/clcd.h>
#include <versatile/clock.h>
#include <motherboard.h>
#include <sp810.h>
#include <sp804_timer.h>
#include <smp_twd.h>
#include <generic_timer.h>

/*
 * Global board context
 */

static virtual_addr_t v2m_sys_base;
static vmm_spinlock_t v2m_cfg_lock;
#if defined(CONFIG_VTEMU)
struct vtemu *v2m_vt;
#endif

void v2m_flags_set(u32 addr)
{
	struct vmm_devtree_node *node;
	int rc;
	if (v2m_sys_base == 0) {
		/* Map control registers */
		node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
					   VMM_DEVTREE_HOSTINFO_NODE_NAME
					   VMM_DEVTREE_PATH_SEPARATOR_STRING "motherboard"
					   VMM_DEVTREE_PATH_SEPARATOR_STRING "iofpga"
					   VMM_DEVTREE_PATH_SEPARATOR_STRING "sysreg");
		if (!node) {
			return;
		}
		rc = vmm_devtree_regmap(node, &v2m_sys_base, 0);
		if (rc) {
			return;
		}
	}
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
	if (v2m_cfg_write(SYS_CFG_REBOOT | SYS_CFG_SITE_MB, 0)) {
		vmm_panic("Unable to reboot\n");
	}
	return VMM_OK;
}

int arch_board_shutdown(void)
{
	if (v2m_cfg_write(SYS_CFG_SHUTDOWN | SYS_CFG_SITE_MB, 0)) {
		vmm_panic("Unable to shutdown\n");
	}
	return VMM_OK;
}

/*
 * Clocking support
 */

static long ct_round(struct arch_clk *clk, unsigned long rate)
{
	return rate;
}

static int ct_set(struct arch_clk *clk, unsigned long rate)
{
	return v2m_cfg_write(SYS_CFG_OSC | SYS_CFG_SITE_DB1 | 1, rate);
}

static const struct arch_clk_ops osc1_clk_ops = {
	.round	= ct_round,
	.set	= ct_set,
};

static struct arch_clk osc1_clk = {
	.ops	= &osc1_clk_ops,
	.rate	= 24000000,
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
		return &osc1_clk;
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

static virtual_addr_t v2m_timer0_base;

int __init arch_clocksource_init(void)
{
	int rc;
	u32 val;
	struct vmm_devtree_node *node;
	virtual_addr_t sctl_base;

	/* Map control registers */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_HOSTINFO_NODE_NAME
				   VMM_DEVTREE_PATH_SEPARATOR_STRING "motherboard"
				   VMM_DEVTREE_PATH_SEPARATOR_STRING "iofpga"
				   VMM_DEVTREE_PATH_SEPARATOR_STRING "sysctl");
	if (!node) {
		goto skip_sp804_init;
	}
	rc = vmm_devtree_regmap(node, &sctl_base, 0);
	if (rc) {
		return rc;
	}

	/* Select 1MHz TIMCLK as the reference clock for SP804 timers */
	val = vmm_readl((void *)sctl_base) | SCCTRL_TIMEREN0SEL_TIMCLK;
	vmm_writel(val, (void *)sctl_base);

	/* Unmap control register */
	rc = vmm_devtree_regunmap(node, sctl_base, 0);
	if (rc) {
		return rc;
	}

	/* Map timer0 registers */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_HOSTINFO_NODE_NAME
				   VMM_DEVTREE_PATH_SEPARATOR_STRING "motherboard"
				   VMM_DEVTREE_PATH_SEPARATOR_STRING "iofpga"
				   VMM_DEVTREE_PATH_SEPARATOR_STRING "timer01");
	if (!node) {
		goto skip_sp804_init;
	}
	rc = vmm_devtree_regmap(node, &v2m_timer0_base, 0);
	if (rc) {
		return rc;
	}

	/* Initialize timer0 as clocksource */
	rc = sp804_clocksource_init(v2m_timer0_base, 
				    node->name, 300, 1000000, 20);
	if (rc) {
		return rc;
	}
skip_sp804_init:

#if defined(CONFIG_ARM_GENERIC_TIMER)
	/* Initialize generic timer as clock source */
	rc = generic_timer_clocksource_init();
	if (rc) {
		return rc;
	}
skip_gen_init:
#endif

	return VMM_OK;
}

static virtual_addr_t v2m_timer1_base;
#if defined(CONFIG_ARM_TWD)
static virtual_addr_t v2m_sys_24mhz;
static virtual_addr_t v2m_twd_base;
#endif

int __cpuinit arch_clockchip_init(void)
{
	int rc;
	struct vmm_devtree_node *node;
	u32 val, *valp, cpu = vmm_smp_processor_id();

	if (!cpu) {
		virtual_addr_t sctl_base;

		/* Map control registers */
		node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
					   VMM_DEVTREE_HOSTINFO_NODE_NAME
					   VMM_DEVTREE_PATH_SEPARATOR_STRING "motherboard"
					   VMM_DEVTREE_PATH_SEPARATOR_STRING "iofpga"
					   VMM_DEVTREE_PATH_SEPARATOR_STRING "sysctl");
		if (!node) {
			goto skip_sp804_init;
		}
		rc = vmm_devtree_regmap(node, &sctl_base, 0);
		if (rc) {
			return rc;
		}

		/* Select 1MHz TIMCLK as the reference clock for SP804 timers */
		val = vmm_readl((void *)sctl_base) | SCCTRL_TIMEREN1SEL_TIMCLK;
		vmm_writel(val, (void *)sctl_base);

		/* Unmap control register */
		rc = vmm_devtree_regunmap(node, sctl_base, 0);
		if (rc) {
			return rc;
		}

		/* Map timer1 registers */
		node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
					   VMM_DEVTREE_HOSTINFO_NODE_NAME
					   VMM_DEVTREE_PATH_SEPARATOR_STRING "motherboard"
					   VMM_DEVTREE_PATH_SEPARATOR_STRING "iofpga"
					   VMM_DEVTREE_PATH_SEPARATOR_STRING "timer01");
		if (!node) {
			goto skip_sp804_init;
		}
		rc = vmm_devtree_regmap(node, &v2m_timer1_base, 0);
		if (rc) {
			return rc;
		}
		v2m_timer1_base += 0x20;

		/* Get timer1 irq */
		valp = vmm_devtree_attrval(node, "irq");
		if (!valp) {
			return VMM_EFAIL;
		}
		val = *valp; 

		/* Initialize timer1 as clockchip */
		rc = sp804_clockchip_init(v2m_timer1_base, val, 
					  node->name, 300, 1000000, 0);
		if (rc) {
			return rc;
		}
	}
skip_sp804_init:

#if defined(CONFIG_ARM_TWD)
	/* Map 24mhz reference counter register */
	if (!v2m_sys_24mhz) {
		node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
					   VMM_DEVTREE_HOSTINFO_NODE_NAME
					   VMM_DEVTREE_PATH_SEPARATOR_STRING "motherboard"
					   VMM_DEVTREE_PATH_SEPARATOR_STRING "iofpga"
					   VMM_DEVTREE_PATH_SEPARATOR_STRING "sysreg");
		if (!node) {
			goto skip_twd_init;
		}
		rc = vmm_devtree_regmap(node, &v2m_sys_24mhz, 0);
		if (rc) {
			return rc;
		}
		v2m_sys_24mhz += V2M_SYS_24MHZ;
	}

	/* Find local timer node */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_HOSTINFO_NODE_NAME
				   VMM_DEVTREE_PATH_SEPARATOR_STRING "twd-timer");
	if (!node) {
		goto skip_twd_init;
	}

	/* Get twd-timer irq */
	valp = vmm_devtree_attrval(node, "irq");
	if (!valp) {
		return VMM_EFAIL;
	}
	val = *valp; 

	/* Map SMP twd local timer registers */
	if (!v2m_twd_base) {
		rc = vmm_devtree_regmap(node, &v2m_twd_base, 0);
		if (rc) {
			return rc;
		}
	}

	/* Initialize SMP twd local timer as clockchip */
	rc = twd_clockchip_init(v2m_twd_base, v2m_sys_24mhz, 24000000, val);
	if (rc) {
		return rc;
	}
skip_twd_init:
#endif

#if defined(CONFIG_ARM_GENERIC_TIMER)
	/* Initialize generic timer as clock source */
	rc = generic_timer_clockchip_init();
	if (rc) {
		return rc;
	}
skip_gen_init:
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

	if(v2m_sys_base == 0) {
		/* Map control registers */
		node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
					   VMM_DEVTREE_HOSTINFO_NODE_NAME
					   VMM_DEVTREE_PATH_SEPARATOR_STRING "motherboard"
					   VMM_DEVTREE_PATH_SEPARATOR_STRING "iofpga"
					   VMM_DEVTREE_PATH_SEPARATOR_STRING "sysreg");
		if (!node) {
			return VMM_ENODEV;
		}
		rc = vmm_devtree_regmap(node, &v2m_sys_base, 0);
		if (rc) {
			return rc;
		}
	}
	INIT_SPIN_LOCK(&v2m_cfg_lock);

	/* Setup CLCD (before probing) */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_HOSTINFO_NODE_NAME
				   VMM_DEVTREE_PATH_SEPARATOR_STRING "motherboard"
				   VMM_DEVTREE_PATH_SEPARATOR_STRING "iofpga"
				   VMM_DEVTREE_PATH_SEPARATOR_STRING "clcd");
	if (node) {
		node->system_data = &clcd_system_data;
	}

	/* Do Probing using device driver framework */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_HOSTINFO_NODE_NAME
				   VMM_DEVTREE_PATH_SEPARATOR_STRING "motherboard");

	if (!node) {
		return VMM_ENOTAVAIL;
	}

	rc = vmm_devdrv_probe(node);
	if (rc) {
		return rc;
	}

	/* Create VTEMU instace if available*/
#if defined(CONFIG_VTEMU)
	info = vmm_fb_find("clcd");
	if (info) {
		v2m_vt = vtemu_create("clcd-vtemu", info, NULL);
	}
#endif

	return VMM_OK;
}
