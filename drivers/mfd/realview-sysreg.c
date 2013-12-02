/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file realview-sysreg.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM Realview sysreg driver
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_devdrv.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <drv/realview.h>

#define MODULE_DESC			"Realview Sysreg Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			realview_sysreg_init
#define	MODULE_EXIT			realview_sysreg_exit

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)	vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

static void *realview_sysreg_base;

u32 realview_board_id(void)
{
	void *sys_id;

	if (!realview_sysreg_base) {
		return 0;
	}

	sys_id = realview_sysreg_base + REALVIEW_SYS_ID_OFFSET;

	return (vmm_readl(sys_id) & REALVIEW_SYS_ID_BOARD_MASK) >> 
						REALVIEW_SYS_ID_BOARD_SHIFT;
}

const char *realview_clcd_panel_name(void)
{
	u32 val;
	const char *panel_name, *vga_panel_name;
	void *sys_clcd;

	if (!realview_sysreg_base) {
		return NULL;
	}

	sys_clcd = realview_sysreg_base + REALVIEW_SYS_CLCD_OFFSET;
	val = vmm_readl(sys_clcd) & REALVIEW_SYS_CLCD_ID_MASK;

	/* XVGA, 16bpp 
	 * (Assuming machine is always realview-pb-a8 and not realview-eb)
	 */
	vga_panel_name = "XVGA";

	if (val == REALVIEW_SYS_CLCD_ID_SANYO_3_8)
		panel_name = "Sanyo TM38QV67A02A";
	else if (val == REALVIEW_SYS_CLCD_ID_SANYO_2_5)
		panel_name = "Sanyo QVGA Portrait";
	else if (val == REALVIEW_SYS_CLCD_ID_EPSON_2_2)
		panel_name = "Epson L2F50113T00";
	else if (val == REALVIEW_SYS_CLCD_ID_VGA)
		panel_name = vga_panel_name;
	else {
		vmm_printf("CLCD: unknown LCD panel ID 0x%08x, using VGA\n", val);
		panel_name = vga_panel_name;
	}

	return panel_name;
}

void realview_clcd_disable_power(void)
{
	u32 val;
	void *sys_clcd;

	if (!realview_sysreg_base) {
		return;
	}

	sys_clcd = realview_sysreg_base + REALVIEW_SYS_CLCD_OFFSET;

	val = vmm_readl(sys_clcd);
	val &= ~REALVIEW_SYS_CLCD_NLCDIOON | REALVIEW_SYS_CLCD_PWR3V5SWITCH;
	vmm_writel(val, sys_clcd);
}

void realview_clcd_enable_power(void)
{
	u32 val;
	void *sys_clcd;

	if (!realview_sysreg_base) {
		return;
	}

	sys_clcd = realview_sysreg_base + REALVIEW_SYS_CLCD_OFFSET;

	val = vmm_readl(sys_clcd);
	val |= REALVIEW_SYS_CLCD_NLCDIOON | REALVIEW_SYS_CLCD_PWR3V5SWITCH;
	vmm_writel(val, sys_clcd);
}

void realview_flags_set(u32 data)
{
	vmm_writel(~0, realview_sysreg_base + REALVIEW_SYS_FLAGSCLR_OFFSET);
	vmm_writel(data, realview_sysreg_base + REALVIEW_SYS_FLAGSSET_OFFSET);
}

int realview_system_reset(void)
{
	u32 board_id;
	void *sys_id, *sys_lock, *sys_resetctl;

	if (!realview_sysreg_base) {
		return VMM_ENODEV;
	}

	sys_id = realview_sysreg_base + REALVIEW_SYS_ID_OFFSET;
	sys_lock = realview_sysreg_base + REALVIEW_SYS_LOCK_OFFSET;
	sys_resetctl = realview_sysreg_base + REALVIEW_SYS_RESETCTL_OFFSET;
	board_id = (vmm_readl(sys_id) & REALVIEW_SYS_ID_BOARD_MASK) >> 
						REALVIEW_SYS_ID_BOARD_SHIFT;

	vmm_writel(REALVIEW_SYS_LOCKVAL, sys_lock);

	switch (board_id) {
	case REALVIEW_SYS_ID_EB:
		vmm_writel(0x0, sys_resetctl);
		vmm_writel(0x08, sys_resetctl);
		break;
	case REALVIEW_SYS_ID_PBA8:
		vmm_writel(0x0, sys_resetctl);
		vmm_writel(0x04, sys_resetctl);
		break;
	default:
		break;
	};

	vmm_writel(0, sys_lock);

	return VMM_OK;
}

void *realview_get_24mhz_clock_base(void)
{
	return realview_sysreg_base + REALVIEW_SYS_24MHz_OFFSET;
}

void *realview_system_base(void)
{
	return realview_sysreg_base;
}

void __init realview_sysreg_early_init(void *base)
{
	realview_sysreg_base = base;
}

void __init realview_sysreg_of_early_init(void)
{
	int err;
	virtual_addr_t base_va;
	struct vmm_devtree_node *node;

	if (realview_sysreg_base)
		return;

	node = vmm_devtree_find_compatible(NULL, NULL, "arm,realview-sysreg");
	if (node) {
		err = vmm_devtree_regmap(node, &base_va, 0);
		if (err) {
			vmm_printf("%s: Faild to map registers (err %d)\n",
				   __func__, err);
			return;
		}		
		realview_sysreg_base = (void *)base_va;
	}
}

static int realview_sysreg_probe(struct vmm_device *dev, 
				 const struct vmm_devtree_nodeid *devid)
{
	int err;
	virtual_addr_t base_va;

	if (!realview_sysreg_base) {
		err = vmm_devtree_regmap(dev->node, &base_va, 0);
		if (err) {
			return err;
		}
		realview_sysreg_base = (void *)base_va;
	}

	if (!realview_sysreg_base) {
		vmm_printf("%s: Failed to obtain base address!\n", __func__);
		return VMM_EFAULT;
	}

	return VMM_OK;
}

static int realview_sysreg_remove(struct vmm_device *dev)
{
	/* Nothing to do here for now. */
	return VMM_OK;
}

static const struct vmm_devtree_nodeid realview_sysreg_match[] = {
	{ .compatible = "arm,realview-sysreg", },
	{},
};

static struct vmm_driver realview_sysreg_driver = {
	.name = "realview_sysreg",
	.match_table = realview_sysreg_match,
	.probe = realview_sysreg_probe,
	.remove = realview_sysreg_remove,
};

static int __init realview_sysreg_init(void)
{
	/* Note: realview_sysreg_of_early_init() must be called
	 * from arch_board_early_init() or arch_cpu_early_init()
	 * before we reach here.
	 */
	return vmm_devdrv_register_driver(&realview_sysreg_driver);
}

static void __exit realview_sysreg_exit(void)
{
	vmm_devdrv_unregister_driver(&realview_sysreg_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
