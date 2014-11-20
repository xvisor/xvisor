/*
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * Adapted from Linux Kernel 3.13.6 arch/arm/mach-imx/pm-imx6q.c
 *
 * Copyright 2011-2013 Freescale Semiconductor, Inc.
 * Copyright 2011 Linaro Ltd.
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 *
 * http://www.opensource.org/licenses/gpl-license.html
 * http://www.gnu.org/copyleft/gpl.html
 *
 * @file pm-imx6q.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Freescale i.MX6Q minimal PM management
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <imx-common.h>
#include <imx-hardware.h>

#define CCR				0x0
#define BM_CCR_WB_COUNT			(0x7 << 16)
#define BM_CCR_RBC_BYPASS_COUNT		(0x3f << 21)
#define BM_CCR_RBC_EN			(0x1 << 27)

#define CLPCR				0x54
#define BP_CLPCR_LPM			0
#define BM_CLPCR_LPM			(0x3 << 0)
#define BM_CLPCR_BYPASS_PMIC_READY	(0x1 << 2)
#define BM_CLPCR_ARM_CLK_DIS_ON_LPM	(0x1 << 5)
#define BM_CLPCR_SBYOS			(0x1 << 6)
#define BM_CLPCR_DIS_REF_OSC		(0x1 << 7)
#define BM_CLPCR_VSTBY			(0x1 << 8)
#define BP_CLPCR_STBY_COUNT		9
#define BM_CLPCR_STBY_COUNT		(0x3 << 9)
#define BM_CLPCR_COSC_PWRDOWN		(0x1 << 11)
#define BM_CLPCR_WB_PER_AT_LPM		(0x1 << 16)
#define BM_CLPCR_WB_CORE_AT_LPM		(0x1 << 17)
#define BM_CLPCR_BYP_MMDC_CH0_LPM_HS	(0x1 << 19)
#define BM_CLPCR_BYP_MMDC_CH1_LPM_HS	(0x1 << 21)
#define BM_CLPCR_MASK_CORE0_WFI		(0x1 << 22)
#define BM_CLPCR_MASK_CORE1_WFI		(0x1 << 23)
#define BM_CLPCR_MASK_CORE2_WFI		(0x1 << 24)
#define BM_CLPCR_MASK_CORE3_WFI		(0x1 << 25)
#define BM_CLPCR_MASK_SCU_IDLE		(0x1 << 26)
#define BM_CLPCR_MASK_L2CC_IDLE		(0x1 << 27)

#define CGPR				0x64
#define BM_CGPR_CHICKEN_BIT		(0x1 << 17)

static void __iomem *ccm_base;

int imx6q_set_lpm(enum mxc_cpu_pwr_mode mode)
{
	struct vmm_host_irq *iomuxc_irq_desc;
	u32 val = readl_relaxed(ccm_base + CLPCR);
	struct irq_data mxc_gpc_irq_data;

	val &= ~BM_CLPCR_LPM;
	switch (mode) {
	case WAIT_CLOCKED:
		break;
	case WAIT_UNCLOCKED:
		val |= 0x1 << BP_CLPCR_LPM;
		val |= BM_CLPCR_ARM_CLK_DIS_ON_LPM;
		break;
	case STOP_POWER_ON:
		val |= 0x2 << BP_CLPCR_LPM;
		break;
	case WAIT_UNCLOCKED_POWER_OFF:
		val |= 0x1 << BP_CLPCR_LPM;
		val &= ~BM_CLPCR_VSTBY;
		val &= ~BM_CLPCR_SBYOS;
		break;
	case STOP_POWER_OFF:
		val |= 0x2 << BP_CLPCR_LPM;
		val |= 0x3 << BP_CLPCR_STBY_COUNT;
		val |= BM_CLPCR_VSTBY;
		val |= BM_CLPCR_SBYOS;
		if (cpu_is_imx6sl()) {
			val |= BM_CLPCR_BYPASS_PMIC_READY;
			val |= BM_CLPCR_BYP_MMDC_CH0_LPM_HS;
		} else {
			val |= BM_CLPCR_BYP_MMDC_CH1_LPM_HS;
		}
		break;
	default:
		return -EINVAL;
	}

	/*
	 * Unmask the always pending IOMUXC interrupt #32 as wakeup source to
	 * deassert dsm_request signal, so that we can ensure dsm_request
	 * is not asserted when we're going to write CLPCR register to set LPM.
	 * After setting up LPM bits, we need to mask this wakeup source.
	 */
	/* FIXME: This can be avoided as we already have the IRQ number */
	iomuxc_irq_desc = vmm_host_irq_get(32);
	mxc_gpc_irq_data.num = iomuxc_irq_desc->num;
	imx_gpc_irq_unmask(&mxc_gpc_irq_data);
	writel_relaxed(val, ccm_base + CLPCR);
	imx_gpc_irq_mask(&mxc_gpc_irq_data);

	return 0;
}

void __init imx6q_pm_set_ccm_base(void __iomem *base)
{
	ccm_base = base;
}
