/*
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * Adapted from Linux Kernel 3.13.6 arch/arm/mach-imx/cpu.c
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
 * @file cpu.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Freescale i.MX CPU information function helpers
 */
#include <linux/err.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/sys_soc.h>
#include <linux/device.h>

#include <imx-hardware.h>
#include <imx-common.h>

unsigned int __mxc_cpu_type;
EXPORT_SYMBOL(__mxc_cpu_type);

static unsigned int imx_soc_revision;

void mxc_set_cpu_type(unsigned int type)
{
	__mxc_cpu_type = type;
}

void imx_set_soc_revision(unsigned int rev)
{
	imx_soc_revision = rev;
}

unsigned int imx_get_soc_revision(void)
{
	return imx_soc_revision;
}

void imx_print_silicon_rev(const char *cpu, int srev)
{
	if (srev == IMX_CHIP_REVISION_UNKNOWN)
		pr_info("CPU identified as %s, unknown revision\n", cpu);
	else
		pr_info("CPU identified as %s, silicon rev %d.%d\n",
				cpu, (srev >> 4) & 0xf, srev & 0xf);
}

void __init imx_set_aips(void __iomem *base)
{
	unsigned int reg;
/*
 * Set all MPROTx to be non-bufferable, trusted for R/W,
 * not forced to user-mode.
 */
	vmm_writel(0x77777777, base + 0x0);
	vmm_writel(0x77777777, base + 0x4);

/*
 * Set all OPACRx to be non-bufferable, to not require
 * supervisor privilege level for access, allow for
 * write access and untrusted master access.
 */
	vmm_writel(0x0, base + 0x40);
	vmm_writel(0x0, base + 0x44);
	vmm_writel(0x0, base + 0x48);
	vmm_writel(0x0, base + 0x4C);
	reg = vmm_readl(base + 0x50) & 0x00FFFFFF;
	vmm_writel(reg, base + 0x50);
}

struct device * __init imx_soc_device_init(void)
{
	struct soc_device_attribute *soc_dev_attr;
	/* struct soc_device *soc_dev; */
	struct device_node *root;
	const char *soc_id;
	char* revision = NULL;

	soc_dev_attr = kzalloc(sizeof(*soc_dev_attr), GFP_KERNEL);
	if (!soc_dev_attr)
		return NULL;

	soc_dev_attr->family = "Freescale i.MX";

	root = vmm_devtree_getnode("/");
	soc_dev_attr->machine = vmm_devtree_attrval(root, "model");
	vmm_devtree_dref_node(root);
	if (!soc_dev_attr->machine) {
		vmm_printf("Error: SOC model not found in device tree\n");
		goto free_soc;
	}

	switch (__mxc_cpu_type) {
	case MXC_CPU_MX1:
		soc_id = "i.MX1";
		break;
	case MXC_CPU_MX21:
		soc_id = "i.MX21";
		break;
	case MXC_CPU_MX25:
		soc_id = "i.MX25";
		break;
	case MXC_CPU_MX27:
		soc_id = "i.MX27";
		break;
	case MXC_CPU_MX31:
		soc_id = "i.MX31";
		break;
	case MXC_CPU_MX35:
		soc_id = "i.MX35";
		break;
	case MXC_CPU_MX51:
		soc_id = "i.MX51";
		break;
	case MXC_CPU_MX53:
		soc_id = "i.MX53";
		break;
	case MXC_CPU_IMX6SL:
		soc_id = "i.MX6SL";
		break;
	case MXC_CPU_IMX6DL:
		soc_id = "i.MX6DL";
		break;
	case MXC_CPU_IMX6Q:
		soc_id = "i.MX6Q";
		break;
	default:
		soc_id = "Unknown";
	}
	soc_dev_attr->soc_id = soc_id;

	/* Allocate a string that can contain 2 digits (up to 0xf), a dot,
	   2 other digits (also up to 0xf), and the 0 terminating character */
	if (NULL == (revision = kmalloc(6, GFP_KERNEL))) {
		vmm_printf("Failed to allocate SOC revision string space\n");
		goto free_soc;
	}

	snprintf(revision, 6, "%d.%d", (imx_soc_revision >> 4) & 0xf,
		 imx_soc_revision & 0xf);

	soc_dev_attr->revision = revision;

	/* soc_dev = soc_device_register(soc_dev_attr); */
	/* if (IS_ERR(soc_dev)) */
	/* 	goto free_rev; */

	/* return soc_device_to_device(soc_dev); */
	vmm_printf("soc_device_register not implemented yet\n");
	return NULL;

free_soc:
	kfree(soc_dev_attr);
	return NULL;
}
