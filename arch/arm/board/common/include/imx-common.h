/*
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * Adapted from Linux Kernel 3.13.6 arch/arm/mach-imx/common.h
 *
 * Copyright 2004-2013 Freescale Semiconductor, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * @file imx-common.h
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Freescale i.MX common header
 */

#ifndef __ASM_ARCH_MXC_COMMON_H__
#define __ASM_ARCH_MXC_COMMON_H__

#include <linux/io.h>
#include <linux/interrupt.h>

#define __raw_readl	readl
#define __raw_writel	writel

#define do_div		udiv64


void mxc_timer_init(void __iomem *, int);
unsigned int imx_get_soc_revision(void);
struct device *imx_soc_device_init(void);

enum mxc_cpu_pwr_mode {
	WAIT_CLOCKED,		/* wfi only */
	WAIT_UNCLOCKED,		/* WAIT */
	WAIT_UNCLOCKED_POWER_OFF,	/* WAIT + SRPG */
	STOP_POWER_ON,		/* just STOP */
	STOP_POWER_OFF,		/* STOP + SRPG */
};

void imx_print_silicon_rev(const char *cpu, int srev);
void imx_gpc_init(void);
void imx_gpc_irq_mask(struct irq_data *d);
void imx_gpc_irq_unmask(struct irq_data *d);
int imx6q_set_lpm(enum mxc_cpu_pwr_mode mode);
void imx6q_pm_set_ccm_base(void __iomem *base);
int imx6_command_setup(void);
struct clk* imx_clk_get(unsigned int clkid);
void imx_gpc_clocks_init(void);

#endif
