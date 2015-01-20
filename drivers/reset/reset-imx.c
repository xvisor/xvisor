/*
 * Copyright 2011, 2013 Freescale Semiconductor, Inc.
 * Copyright 2011 Linaro Ltd.
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 * Modified by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 * for Xvisor.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @file reset-imx.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief iMX reset driver source.
 */

#include <linux/module.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/reset.h>
#include <linux/reset-controller.h>
#include <linux/jiffies.h>

#define MODULE_AUTHOR			"Jimmy Durand Wesolowski"
#define MODULE_LICENSE			"GPL"
#define MODULE_DESC			"i.MX Reset driver"
#define MODULE_IPRIORITY		(RESET_CONTROLLER_IPRIORITY + 1)
#define MODULE_INIT			imx_src_init
#define MODULE_EXIT			imx_src_exit

#define SRC_SCR				0x000
#define SRC_GPR1			0x020
#define BP_SRC_SCR_WARM_RESET_ENABLE	0
#define BP_SRC_SCR_SW_GPU_RST		1
#define BP_SRC_SCR_SW_VPU_RST		2
#define BP_SRC_SCR_SW_IPU1_RST		3
#define BP_SRC_SCR_SW_OPEN_VG_RST	4
#define BP_SRC_SCR_SW_IPU2_RST		12
#define BP_SRC_SCR_CORE1_RST		14
#define BP_SRC_SCR_CORE1_ENABLE		22

static void __iomem *src_base;
static DEFINE_SPINLOCK(scr_lock);

static const int sw_reset_bits[5] = {
	BP_SRC_SCR_SW_GPU_RST,
	BP_SRC_SCR_SW_VPU_RST,
	BP_SRC_SCR_SW_IPU1_RST,
	BP_SRC_SCR_SW_OPEN_VG_RST,
	BP_SRC_SCR_SW_IPU2_RST
};

static int imx_src_reset_module(struct reset_controller_dev *rcdev,
		unsigned long sw_reset_idx)
{
	unsigned long timeout;
	unsigned long flags;
	int bit;
	u32 val;

	if (!src_base)
		return -ENODEV;

	if (sw_reset_idx >= ARRAY_SIZE(sw_reset_bits))
		return -EINVAL;

	bit = 1 << sw_reset_bits[sw_reset_idx];

	spin_lock_irqsave(&scr_lock, flags);
	val = readl_relaxed(src_base + SRC_SCR);
	val |= bit;
	writel_relaxed(val, src_base + SRC_SCR);
	spin_unlock_irqrestore(&scr_lock, flags);

	timeout = jiffies + msecs_to_jiffies(1000);
	while (readl(src_base + SRC_SCR) & bit) {
		if (time_after(jiffies, timeout))
			return -ETIME;
		cpu_relax();
	}

	return 0;
}

static struct reset_control_ops imx_src_ops = {
	.reset = imx_src_reset_module,
};

static struct reset_controller_dev imx_reset_controller = {
	.ops = &imx_src_ops,
	.nr_resets = ARRAY_SIZE(sw_reset_bits),
};

void imx_enable_cpu(int cpu, bool enable)
{
	u32 mask, val;

#if 0
	cpu = cpu_logical_map(cpu);
#endif /* 0 */
	mask = 1 << (BP_SRC_SCR_CORE1_ENABLE + cpu - 1);
	spin_lock(&scr_lock);
	val = readl_relaxed(src_base + SRC_SCR);
	val = enable ? val | mask : val & ~mask;
	val |= 1 << (BP_SRC_SCR_CORE1_RST + cpu - 1);
	writel_relaxed(val, src_base + SRC_SCR);
	spin_unlock(&scr_lock);
}

void imx_set_cpu_jump(int cpu, void *jump_addr)
{
	physical_addr_t paddr;

#if 0
	cpu = cpu_logical_map(cpu);
#endif /* 0 */
	if (VMM_OK != vmm_host_va2pa((virtual_addr_t)jump_addr,
				     &paddr)) {
		vmm_printf("Failed to get cpu jump physical address (0x%X)\n",
			   jump_addr);
	}
	writel_relaxed(paddr,
		       src_base + SRC_GPR1 + cpu * 8);
}

u32 imx_get_cpu_arg(int cpu)
{
#if 0
	cpu = cpu_logical_map(cpu);
#endif /* 0 */
	return readl_relaxed(src_base + SRC_GPR1 + cpu * 8 + 4);
}

void imx_set_cpu_arg(int cpu, u32 arg)
{
#if 0
	cpu = cpu_logical_map(cpu);
#endif /* 0 */
	writel_relaxed(arg, src_base + SRC_GPR1 + cpu * 8 + 4);
}

static int imx_src_probe(struct vmm_device *dev,
			 const struct vmm_devtree_nodeid *nodeid)
{
	int ret = VMM_OK;
	struct vmm_devtree_node *np = dev->node;
	u32 val;

	ret = vmm_devtree_request_regmap(np, (virtual_addr_t *)&src_base, 0,
					 "i.MX Reset Control");
	if (VMM_OK != ret) {
		vmm_printf("Failed to retrive %s register mapping\n");
		return ret;
	}

	imx_reset_controller.node = np;
#ifdef CONFIG_RESET_CONTROLLER
	reset_controller_register(&imx_reset_controller);
#endif /* CONFIG_RESET_CONTROLLER */

	/*
	 * force warm reset sources to generate cold reset
	 * for a more reliable restart
	 */
	spin_lock(&scr_lock);
	val = readl_relaxed(src_base + SRC_SCR);
	val &= ~(1 << BP_SRC_SCR_WARM_RESET_ENABLE);
	writel_relaxed(val, src_base + SRC_SCR);
	spin_unlock(&scr_lock);

	return 0;
}

static int imx_src_remove(struct vmm_device *dev)
{
#ifdef CONFIG_RESET_CONTROLLER
	reset_controller_unregister(&imx_reset_controller);
#endif /* CONFIG_RESET_CONTROLLER */

	vmm_devtree_regunmap_release(dev->node, (virtual_addr_t)src_base, 0);
	src_base = NULL;

	return 0;
}

static const struct vmm_devtree_nodeid imx_src_dt_ids[] = {
	{ .compatible = "fsl,imx51-src"},
	{ /* sentinel */ }
};

static struct vmm_driver imx_src_driver = {
	.name = "i.MX reset driver",
	.match_table = imx_src_dt_ids,
	.probe = imx_src_probe,
	.remove = imx_src_remove,
};

static int __init imx_src_init(void)
{
	return vmm_devdrv_register_driver(&imx_src_driver);
}

static void __exit imx_src_exit(void)
{
	vmm_devdrv_unregister_driver(&imx_src_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
