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
#include <vmm_spinlocks.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <vmm_stdio.h>
#include <vmm_chardev.h>
#include <rtc/vmm_rtcdev.h>
#include <arch_barrier.h>
#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>
#include <versatile/clcd.h>
#include <versatile/clock.h>
#include <libfdt.h>
#include <vtemu.h>
#include <vexpress_plat.h>
#include <ca9x4_board.h>

extern u32 dt_blob_start;
static virtual_addr_t v2m_sys_base;
static vmm_spinlock_t v2m_cfg_lock;
#if defined(CONFIG_VTEMU)
struct vtemu *vea9_vt;
#endif

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
 * Clock handling
 */

static long ct_round(struct versatile_clk *clk, unsigned long rate)
{
	return rate;
}

static int ct_set(struct versatile_clk *clk, unsigned long rate)
{
	return v2m_cfg_write(SYS_CFG_OSC | SYS_CFG_SITE_DB1 | 1, rate);
}

static const struct versatile_clk_ops osc1_clk_ops = {
	.round	= ct_round,
	.set	= ct_set,
};

static struct versatile_clk osc1_clk = {
	.ops	= &osc1_clk_ops,
	.rate	= 24000000,
};

static struct vmm_devclk clcd_clk = {
	.enable = versatile_clk_enable,
	.disable = versatile_clk_disable,
	.get_rate = versatile_clk_get_rate,
	.round_rate = versatile_clk_round_rate,
	.set_rate = versatile_clk_set_rate,
	.priv = &osc1_clk,
};

static struct vmm_devclk *vexpress_getclk(struct vmm_devtree_node *node)
{
	if (strcmp(node->name, "clcd") == 0) {
		return &clcd_clk;
	}

	return NULL;
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
	.name		= "VExpress-A9",
	.caps		= CLCD_CAP_5551 | CLCD_CAP_565,
	.check		= clcdfb_check,
	.decode		= clcdfb_decode,
	.enable		= vexpress_clcd_enable,
	.setup		= vexpress_clcd_setup,
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
#if defined(CONFIG_VTEMU)
	struct vmm_fb_info *info;
#endif

	/* All VMM API's are available here */
	/* We can register a Board specific resource here */

	/* Map control registers */
	v2m_sys_base = vmm_host_iomap(V2M_SYSREGS, 0x1000);
	INIT_SPIN_LOCK(&v2m_cfg_lock);

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

	rc = vmm_devdrv_probe(node, vexpress_getclk, NULL);
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
		vea9_vt = vtemu_create("clcd-vtemu", info, NULL);
	}
#endif

	return VMM_OK;
}
