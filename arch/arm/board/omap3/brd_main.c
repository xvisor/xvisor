/**
 * Copyright (c) 2011 Pranav Sawargaonkar.
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
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief main source file for board specific code
 */

#include <vmm_error.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>
#include <arch_board.h>
#include <arch_timer.h>
#include <omap/sdrc.h>
#include <omap/gpt.h>
#include <omap/s32k-timer.h>

/* ===== SDRC & SMS ===== */

/** OMAP3/OMAP343X SDRC Base Physical Address */
#define OMAP3_SDRC_BASE				0x6D000000

/** OMAP3/OMAP343X SMS Base Physical Address */
#define OMAP3_SMS_BASE				0x6C000000

/* ===== PRM & CM ===== */

/** OMAP3/OMAP343X PRCM Base Physical Address */
#define OMAP3_PRCM_BASE				0x48004000

/** OMAP3/OMAP343X CM Base Physical Address */
#define OMAP3_CM_BASE				0x48004000
#define OMAP3_CM_SIZE				0x2000

/** OMAP3/OMAP343X PRM Base Physical Address */
#define OMAP3_PRM_BASE				0x48306000
#define OMAP3_PRM_SIZE				0x2000

/* ===== INTC ===== */

/** OMAP3/OMAP343X INTC Base Physical Address */
#define OMAP3_MPU_INTC_BASE			0x48200000

/** OMAP3/OMAP343X INTC IRQ Count */
#define OMAP3_MPU_INTC_NRIRQ			96

#define OMAP3_MPU_INTC_EMUINT			0
#define OMAP3_MPU_INTC_COMMTX			1
#define OMAP3_MPU_INTC_COMMRX			2
#define OMAP3_MPU_INTC_BENCH			3
#define OMAP3_MPU_INTC_MCBSP2_ST_IRQ		4
#define OMAP3_MPU_INTC_MCBSP3_ST_IRQ		5
#define OMAP3_MPU_INTC_RESERVED0		6
#define OMAP3_MPU_INTC_SYS_NIRQ			7
#define OMAP3_MPU_INTC_RESERVED1		8
#define OMAP3_MPU_INTC_SMX_DBG_IRQ		9
#define OMAP3_MPU_INTC_SMX_APP_IRQ		10
#define OMAP3_MPU_INTC_PRCM_MPU_IRQ		11
#define OMAP3_MPU_INTC_SDMA_IRQ_0		12
#define OMAP3_MPU_INTC_SDMA_IRQ_1		13
#define OMAP3_MPU_INTC_SDMA_IRQ_2		14
#define OMAP3_MPU_INTC_SDMA_IRQ_3		15
#define OMAP3_MPU_INTC_MCBSP1_IRQ		16
#define OMAP3_MPU_INTC_MCBSP2_IRQ		17
#define OMAP3_MPU_INTC_RESERVED2		18
#define OMAP3_MPU_INTC_RESERVED3		19
#define OMAP3_MPU_INTC_GPMC_IRQ			20
#define OMAP3_MPU_INTC_SGX_IRQ			21
#define OMAP3_MPU_INTC_MCBSP3_IRQ		22
#define OMAP3_MPU_INTC_MCBSP4_IRQ		23
#define OMAP3_MPU_INTC_CAM_IRQ0			24
#define OMAP3_MPU_INTC_DSS_IRQ			25
#define OMAP3_MPU_INTC_MAIL_U0_MPU_IRQ		26
#define OMAP3_MPU_INTC_MCBSP5_IRQ		27
#define OMAP3_MPU_INTC_IVA2_MMU_IRQ		28
#define OMAP3_MPU_INTC_GPIO1_MPU_IRQ		29
#define OMAP3_MPU_INTC_GPIO2_MPU_IRQ		30
#define OMAP3_MPU_INTC_GPIO3_MPU_IRQ		31
#define OMAP3_MPU_INTC_GPIO4_MPU_IRQ		32
#define OMAP3_MPU_INTC_GPIO5_MPU_IRQ		33
#define OMAP3_MPU_INTC_GPIO6_MPU_IRQ		34
#define OMAP3_MPU_INTC_RESERVED4		35
#define OMAP3_MPU_INTC_WDT3_IRQ			36
#define OMAP3_MPU_INTC_GPT1_IRQ			37
#define OMAP3_MPU_INTC_GPT2_IRQ			38
#define OMAP3_MPU_INTC_GPT3_IRQ			39
#define OMAP3_MPU_INTC_GPT4_IRQ			40
#define OMAP3_MPU_INTC_GPT5_IRQ			41
#define OMAP3_MPU_INTC_GPT6_IRQ			42
#define OMAP3_MPU_INTC_GPT7_IRQ			43
#define OMAP3_MPU_INTC_GPT8_IRQ			44
#define OMAP3_MPU_INTC_GPT9_IRQ			45
#define OMAP3_MPU_INTC_GPT10_IRQ		46
#define OMAP3_MPU_INTC_GPT11_IRQ		47
#define OMAP3_MPU_INTC_SPI4_IRQ			48
#define OMAP3_MPU_INTC_RESERVED5		49
#define OMAP3_MPU_INTC_RESERVED6		50
#define OMAP3_MPU_INTC_RESERVED7		51
#define OMAP3_MPU_INTC_RESERVED8		52
#define OMAP3_MPU_INTC_RESERVED9		53
#define OMAP3_MPU_INTC_MCBSP4_IRQ_TX		54
#define OMAP3_MPU_INTC_MCBSP4_IRQ_RX		55
#define OMAP3_MPU_INTC_I2C1_IRQ			56
#define OMAP3_MPU_INTC_I2C2_IRQ			57
#define OMAP3_MPU_INTC_HDQ_IRQ			58
#define OMAP3_MPU_INTC_McBSP1_IRQ_TX		59
#define OMAP3_MPU_INTC_McBSP1_IRQ_RX		60
#define OMAP3_MPU_INTC_I2C3_IRQ			61
#define OMAP3_MPU_INTC_McBSP2_IRQ_TX		62
#define OMAP3_MPU_INTC_McBSP2_IRQ_RX		63
#define OMAP3_MPU_INTC_RESERVED10		64
#define OMAP3_MPU_INTC_SPI1_IRQ			65
#define OMAP3_MPU_INTC_SPI2_IRQ			66
#define OMAP3_MPU_INTC_RESERVED11		67
#define OMAP3_MPU_INTC_RESERVED12		68
#define OMAP3_MPU_INTC_RESERVED13		69
#define OMAP3_MPU_INTC_RESERVED14		70
#define OMAP3_MPU_INTC_RESERVED15		71
#define OMAP3_MPU_INTC_UART1_IRQ		72
#define OMAP3_MPU_INTC_UART2_IRQ		73
#define OMAP3_MPU_INTC_UART3_IRQ		74
#define OMAP3_MPU_INTC_PBIAS_IRQ		75
#define OMAP3_MPU_INTC_OHCI_IRQ			76
#define OMAP3_MPU_INTC_EHCI_IRQ			77
#define OMAP3_MPU_INTC_TLL_IRQ			78
#define OMAP3_MPU_INTC_RESERVED16		79
#define OMAP3_MPU_INTC_RESERVED17		80
#define OMAP3_MPU_INTC_MCBSP5_IRQ_TX		81
#define OMAP3_MPU_INTC_MCBSP5_IRQ_RX		82
#define OMAP3_MPU_INTC_MMC1_IRQ			83
#define OMAP3_MPU_INTC_RESERVED18		84
#define OMAP3_MPU_INTC_RESERVED19		85
#define OMAP3_MPU_INTC_MMC2_IRQ			86
#define OMAP3_MPU_INTC_MPU_ICR_IRQ		87
#define OMAP3_MPU_INTC_D2DFRINT			88
#define OMAP3_MPU_INTC_MCBSP3_IRQ_TX		89
#define OMAP3_MPU_INTC_MCBSP3_IRQ_RX		90
#define OMAP3_MPU_INTC_SPI3_IRQ			91
#define OMAP3_MPU_INTC_HSUSB_MC_NINT		92
#define OMAP3_MPU_INTC_HSUSB_DMA_NINT		93
#define OMAP3_MPU_INTC_MMC3_IRQ			94
#define OMAP3_MPU_INTC_RESERVED20		95

/* ===== Synchronous 32k Timer ===== */

/** OMAP3/OMAP343X S32K Base Physical Address */
#define OMAP3_S32K_BASE 			0x48320000

/* ===== General Purpose Timers ===== */

/** OMAP3/OMAP343X GPT Base Physical Addresses */
#define OMAP3_GPT1_BASE 			0x48318000
#define OMAP3_GPT2_BASE 			0x49032000
#define OMAP3_GPT3_BASE 			0x49034000
#define OMAP3_GPT4_BASE 			0x49036000
#define OMAP3_GPT5_BASE 			0x49038000
#define OMAP3_GPT6_BASE 			0x4903A000
#define OMAP3_GPT7_BASE 			0x4903C000
#define OMAP3_GPT8_BASE 			0x4903E000
#define OMAP3_GPT9_BASE 			0x49040000
#define OMAP3_GPT10_BASE 			0x48086000
#define OMAP3_GPT11_BASE 			0x48088000
#define OMAP3_GPT12_BASE 			0x48304000

/* ===== UART (or Serial Port) ===== */

#define OMAP3_COM_FREQ   	48000000L

/** OMAP3/OMAP343X UART Base Physical Address */
#define OMAP3_UART_BASE 	0x49020000
#define OMAP3_UART_BAUD 	115200
#define OMAP3_UART_INCLK 	OMAP3_COM_FREQ

/*
 * OMAP3 Power, Reset, and Clock Managment
 */

#define OMAP3_SYSCLK_S12M		12000000
#define OMAP3_SYSCLK_S13M		13000000
#define OMAP3_SYSCLK_S19_2M		19200000
#define OMAP3_SYSCLK_S24M		24000000
#define OMAP3_SYSCLK_S26M		26000000
#define OMAP3_SYSCLK_S38_4M		38400000

#define OMAP3_IVA2_CM			0x0000
#define OMAP3_OCP_SYS_REG_CM		0x0800
#define OMAP3_MPU_CM			0x0900
#define OMAP3_CORE_CM			0x0A00
#define OMAP3_SGX_CM			0x0B00
#define OMAP3_WKUP_CM			0x0C00
#define OMAP3_CLOCK_CTRL_REG_CM		0x0D00
#define OMAP3_DSS_CM			0x0E00
#define OMAP3_CAM_CM			0x0F00
#define OMAP3_PER_CM			0x1000
#define OMAP3_EMU_CM			0x1100
#define OMAP3_GLOBAL_REG_CM		0x1200
#define OMAP3_NEON_CM			0x1300
#define OMAP3_USBHOST_CM		0x1400

#define OMAP3_IVA2_PRM			0x0000
#define OMAP3_OCP_SYS_REG_PRM		0x0800
#define OMAP3_MPU_PRM			0x0900
#define OMAP3_CORE_PRM			0x0A00
#define OMAP3_SGX_PRM			0x0B00
#define OMAP3_WKUP_PRM			0x0C00
#define OMAP3_CLOCK_CTRL_REG_PRM	0x0D00
#define OMAP3_DSS_PRM			0x0E00
#define OMAP3_CAM_PRM			0x0F00
#define OMAP3_PER_PRM			0x1000
#define OMAP3_EMU_PRM			0x1100
#define OMAP3_GLOBAL_REG_PRM		0x1200
#define OMAP3_NEON_PRM			0x1300
#define OMAP3_USBHOST_PRM		0x1400

#define OMAP3_PRM_CLKSRC_CTRL			0x70
#define OMAP3_PRM_CLKSRC_CTRL_SYSCLKDIV_S	6
#define OMAP3_PRM_CLKSRC_CTRL_SYSCLKDIV_M	(0x3 << 6)
#define OMAP3_PRM_CLKSRC_CTRL_AUTOEXTCLK_S	3
#define OMAP3_PRM_CLKSRC_CTRL_AUTOEXTCLK_M	(0x3 << 3)
#define OMAP3_PRM_CLKSRC_CTRL_SYSCLKSEL_S	0
#define OMAP3_PRM_CLKSRC_CTRL_SYSCLKSEL_M	(0x3 << 0)

#define OMAP3_CM_FCLKEN_WKUP			0x00
#define OMAP3_CM_FCLKEN_WKUP_EN_WDT2_S		5
#define OMAP3_CM_FCLKEN_WKUP_EN_WDT2_M		(1 << 5)
#define OMAP3_CM_FCLKEN_WKUP_EN_GPIO1_S		3
#define OMAP3_CM_FCLKEN_WKUP_EN_GPIO1_M		(1 << 3)
#define OMAP3_CM_FCLKEN_WKUP_EN_GPT1_S		0
#define OMAP3_CM_FCLKEN_WKUP_EN_GPT1_M		(1 << 0)

#define OMAP3_CM_ICLKEN_WKUP			0x10
#define OMAP3_CM_ICLKEN_WKUP_EN_WDT2_S		5
#define OMAP3_CM_ICLKEN_WKUP_EN_WDT2_M		(1 << 5)
#define OMAP3_CM_ICLKEN_WKUP_EN_GPIO1_S		3
#define OMAP3_CM_ICLKEN_WKUP_EN_GPIO1_M		(1 << 3)
#define OMAP3_CM_ICLKEN_WKUP_EN_32KSYNC_S	2
#define OMAP3_CM_ICLKEN_WKUP_EN_32KSYNC_M	(1 << 2)
#define OMAP3_CM_ICLKEN_WKUP_EN_GPT1_S		0
#define OMAP3_CM_ICLKEN_WKUP_EN_GPT1_M		(1 << 0)

#define OMAP3_CM_IDLEST_WKUP			0x20
#define OMAP3_CM_IDLEST_WKUP_ST_WDT2_S		5
#define OMAP3_CM_IDLEST_WKUP_ST_WDT2_M		(1 << 5)
#define OMAP3_CM_IDLEST_WKUP_ST_GPIO1_S		3
#define OMAP3_CM_IDLEST_WKUP_ST_GPIO1_M		(1 << 3)
#define OMAP3_CM_IDLEST_WKUP_ST_32KSYNC_S	2
#define OMAP3_CM_IDLEST_WKUP_ST_32KSYNC_M	(1 << 2)
#define OMAP3_CM_IDLEST_WKUP_ST_GPT1_S		0
#define OMAP3_CM_IDLEST_WKUP_ST_GPT1_M		(1 << 0)

#define OMAP3_CM_AUTOIDLE_WKUP			0x30
#define OMAP3_CM_AUTOIDLE_WKUP_AUTO_WDT2_S	5
#define OMAP3_CM_AUTOIDLE_WKUP_AUTO_WDT2_M	(1 << 5)
#define OMAP3_CM_AUTOIDLE_WKUP_AUTO_GPIO1_S	3
#define OMAP3_CM_AUTOIDLE_WKUP_AUTO_GPIO1_M	(1 << 3)
#define OMAP3_CM_AUTOIDLE_WKUP_AUTO_32KSYNC_S	2
#define OMAP3_CM_AUTOIDLE_WKUP_AUTO_32KSYNC_M	(1 << 2)
#define OMAP3_CM_AUTOIDLE_WKUP_AUTO_GPT1_S	0
#define OMAP3_CM_AUTOIDLE_WKUP_AUTO_GPT1_M	(1 << 0)

#define OMAP3_CM_CLKSEL_WKUP			0x40
#define OMAP3_CM_CLKSEL_WKUP_CLKSEL_RM_S	1
#define OMAP3_CM_CLKSEL_WKUP_CLKSEL_RM_M	(0x3 << 1)
#define OMAP3_CM_CLKSEL_WKUP_CLKSEL_GPT1_S	0
#define OMAP3_CM_CLKSEL_WKUP_CLKSEL_GPT1_M	(1 << 0)


#define OMAP3_CM_FCLKEN_PER			0x00
#define OMAP3_CM_FCLKEN_PER_EN_GPT2_S		3
#define OMAP3_CM_FCLKEN_PER_EN_GPT2_M		(1 << 3)

#define OMAP3_CM_ICLKEN_PER			0x10
#define OMAP3_CM_ICLKEN_PER_EN_GPT2_S		3
#define OMAP3_CM_ICLKEN_PER_EN_GPT2_M		(1 << 3)

#define OMAP3_CM_CLKSEL_PER			0x40
#define OMAP3_CM_CLKSEL_PER_CLKSEL_GPT2_S	0
#define OMAP3_CM_CLKSEL_PER_CLKSEL_GPT2_M	(1 << 0)

#define OMAP3_CM_ICLKEN			0x00
#define OMAP3_CM_FCLKEN			0x10
#define OMAP3_CM_CLKSEL			0x40

static virtual_addr_t cm_base = 0;

int __init cm_init(void)
{
	if(!cm_base) {
		cm_base = vmm_host_iomap(OMAP3_CM_BASE, OMAP3_CM_SIZE);
		if(!cm_base)
			return VMM_EFAIL;
	}
	return VMM_OK;
}

u32 cm_read(u32 domain, u32 offset)
{
	return vmm_readl((void *)(cm_base + domain + offset));
}

void cm_write(u32 domain, u32 offset, u32 val)
{
	vmm_writel(val, (void *)(cm_base + domain + offset));
}

void cm_setbits(u32 domain, u32 offset, u32 mask)
{
	vmm_writel(vmm_readl((void *)(cm_base + domain + offset)) | mask,
		(void *)(cm_base + domain + offset));
}

void cm_clrbits(u32 domain, u32 offset, u32 mask)
{
	vmm_writel(vmm_readl((void *)(cm_base + domain + offset)) & (~mask),
		(void *)(cm_base + domain + offset));
}

static virtual_addr_t prm_base = 0;

int __init prm_init(void)
{
	if(!prm_base) {
		prm_base = vmm_host_iomap(OMAP3_PRM_BASE, OMAP3_PRM_SIZE);
		if(!prm_base)
			return VMM_EFAIL;
	}
	return VMM_OK;
}

u32 prm_read(u32 domain, u32 offset)
{
	return vmm_readl((void *)(prm_base + domain + offset));
}

void prm_write(u32 domain, u32 offset, u32 val)
{
	vmm_writel(val, (void *)(prm_base + domain + offset));
}

void prm_setbits(u32 domain, u32 offset, u32 mask)
{
	vmm_writel(vmm_readl((void *)(prm_base + domain + offset)) | mask,
		(void *)(prm_base + domain + offset));
}

void prm_clrbits(u32 domain, u32 offset, u32 mask)
{
	vmm_writel(vmm_readl((void *)(prm_base + domain + offset)) & (~mask),
		(void *)(prm_base + domain + offset));
}

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

/* Micron MT46H32M32LF-6 */
/* XXX Using ARE = 0x1 (no autorefresh burst) -- can this be changed? */
static struct sdrc_params mt46h32m32lf6_sdrc_params[] = {
	[0] = {
		.rate	     = 166000000,
		.actim_ctrla = 0x9a9db4c6,
		.actim_ctrlb = 0x00011217,
		.rfr_ctrl    = 0x0004dc01,
		.mr	     = 0x00000032,
	},
	[1] = {
		.rate	     = 165941176,
		.actim_ctrla = 0x9a9db4c6,
		.actim_ctrlb = 0x00011217,
		.rfr_ctrl    = 0x0004dc01,
		.mr	     = 0x00000032,
	},
	[2] = {
		.rate	     = 83000000,
		.actim_ctrla = 0x51512283,
		.actim_ctrlb = 0x0001120c,
		.rfr_ctrl    = 0x00025501,
		.mr	     = 0x00000032,
	},
	[3] = {
		.rate	     = 82970588,
		.actim_ctrla = 0x51512283,
		.actim_ctrlb = 0x0001120c,
		.rfr_ctrl    = 0x00025501,
		.mr	     = 0x00000032,
	},
	[4] = {
		.rate	     = 0
	},
};

int __init arch_board_early_init(void)
{
	int rc;

	/* Host virtual memory, device tree, heap is up.
	 * Do necessary early stuff like iomapping devices
	 * memory or boot time memory reservation here.
	 */

	/* The function omap3_beagle_init_early() of 
	 * <linux>/arch/arm/mach-omap2/board-omap3beagle.c
	 * does the following:
	 *   1. Initialize Clock & Power Domains using function 
	 *      omap2_init_common_infrastructure() of
	 *      <linux>/arch/arm/mach-omap2/io.c
	 *   2. Initialize & Reprogram Clock of SDRC using function
	 *      omap2_sdrc_init() of <linux>/arch/arm/mach-omap2/sdrc.c
	 */

	/* Initialize Clock Mamagment */
	if ((rc = cm_init())) {
		return rc;
	}

	/* Initialize Power & Reset Mamagment */
	if ((rc = prm_init())) {
		return rc;
	}

	/* Enable I-clock for S32K timer 
	 * Note: S32K is our reference clocksource and also used
	 * as clock reference for GPTs
	 */
	cm_setbits(OMAP3_WKUP_CM, 
		   OMAP3_CM_ICLKEN_WKUP, 
		   OMAP3_CM_ICLKEN_WKUP_EN_32KSYNC_M);

	/* Initialize SDRAM Controller (SDRC) */
	if ((rc = sdrc_init(OMAP3_SDRC_BASE,
			    OMAP3_SMS_BASE,
			    mt46h32m32lf6_sdrc_params, 
			    mt46h32m32lf6_sdrc_params))) {
		return rc;
	}

	return 0;
}

#define OMAP3_CLK_EVENT_GPT	0 

#ifndef CONFIG_OMAP3_CLKSRC_S32KT
#define OMAP3_CLK_SRC_GPT	1 
#endif

struct gpt_cfg {
	const char *name;
	physical_addr_t base_pa;
	u32 cm_domain;
	u32 clksel_offset;
	u32 clksel_mask;
	u32 iclken_offset;
	u32 iclken_mask;
	u32 fclken_offset;
	u32 fclken_mask;
	bool src_sys_clk;
	u32 clk_hz;
	u32 irq_no;
};

struct gpt_cfg omap3_gpt[] = {
	{
		.name		= "gpt1",
		.base_pa	= OMAP3_GPT1_BASE,
		.cm_domain	= OMAP3_WKUP_CM,
		.clksel_offset	= OMAP3_CM_CLKSEL,
		.clksel_mask	= OMAP3_CM_CLKSEL_WKUP_CLKSEL_GPT1_M,
		.iclken_offset	= OMAP3_CM_ICLKEN,
		.iclken_mask	= OMAP3_CM_ICLKEN_WKUP_EN_GPT1_M,
		.fclken_offset	= OMAP3_CM_FCLKEN,
		.fclken_mask	= OMAP3_CM_FCLKEN_WKUP_EN_GPT1_M,	
		.src_sys_clk	= TRUE,
		.irq_no	=	OMAP3_MPU_INTC_GPT1_IRQ
	},
	{
		.name		= "gpt2",
		.base_pa	= OMAP3_GPT2_BASE,
		.cm_domain	= OMAP3_PER_CM,
		.clksel_offset	= OMAP3_CM_CLKSEL,
		.clksel_mask	= OMAP3_CM_CLKSEL_PER_CLKSEL_GPT2_M,
		.iclken_offset	= OMAP3_CM_ICLKEN,
		.iclken_mask	= OMAP3_CM_ICLKEN_PER_EN_GPT2_M,
		.fclken_offset	= OMAP3_CM_FCLKEN,
		.fclken_mask	= OMAP3_CM_FCLKEN_PER_EN_GPT2_M,	
		.src_sys_clk	= TRUE,
		.irq_no		= OMAP3_MPU_INTC_GPT2_IRQ
	}
};

static u32 get_osc_clk_speed(u32 gpt_num, u32 sys_clk_div)
{
	u32 osc_clk_hz = 0, regval;
	u32 start, cstart, cend, cdiff;
	virtual_addr_t gpt_va, s32k_va;

	/* Map gpt registers */
	gpt_va = vmm_host_iomap(omap3_gpt[gpt_num].base_pa, 0x1000);

	/* Map s32k registers */
	s32k_va = vmm_host_iomap(OMAP3_S32K_BASE, 0x1000);

	/* Start counting at 0 */
	vmm_writel(0, (void *)(gpt_va + GPT_TLDR));

	/* Enable GPT */
	vmm_writel(GPT_TCLR_ST_M, (void *)(gpt_va + GPT_TCLR));

	/* Start time in 20 cycles */
	start = 20 + vmm_readl((void *)(s32k_va + S32K_CR));

	/* Dead loop till start time */
	while (vmm_readl((void *)(s32k_va + S32K_CR)) < start);

	/* Get start sys_clk count */
	cstart = vmm_readl((void *)(gpt_va + GPT_TCRR));

	/* Wait for 20 cycles */
	while (vmm_readl((void *)(s32k_va + S32K_CR)) < (start + 20)) ;
	cend = vmm_readl((void *)(gpt_va + GPT_TCRR));
	cdiff = cend - cstart;	/* get elapsed ticks */
	cdiff *= sys_clk_div;

	/* Stop Timer (TCLR[ST] = 0) */
	regval = vmm_readl((void *)(gpt_va + GPT_TCLR));
	regval &= ~GPT_TCLR_ST_M;
	vmm_writel(regval, (void *)(gpt_va + GPT_TCLR));

	/* Based on number of ticks assign speed */
	if (cdiff > 19000) {
		osc_clk_hz = OMAP3_SYSCLK_S38_4M;
	} else if (cdiff > 15200) {
		osc_clk_hz = OMAP3_SYSCLK_S26M;
	} else if (cdiff > 13000) {
		osc_clk_hz = OMAP3_SYSCLK_S24M;
	} else if (cdiff > 9000) {
		osc_clk_hz = OMAP3_SYSCLK_S19_2M;
	} else if (cdiff > 7600) {
		osc_clk_hz = OMAP3_SYSCLK_S13M;
	} else {
		osc_clk_hz = OMAP3_SYSCLK_S12M;
	}

	/* Unmap s32k registers */
	vmm_host_iounmap(s32k_va);

	/* Unmap gpt registers */
	vmm_host_iounmap(gpt_va);

	return osc_clk_hz >> (sys_clk_div - 1);
}

static void omap3_gpt_clock_enable(u32 gpt_num)
{
	u32 sys_div;

	/* select clock source (1=sys_clk; 0=32K) for GPT */
	if (omap3_gpt[gpt_num].src_sys_clk) {
		sys_div = prm_read(OMAP3_GLOBAL_REG_PRM, OMAP3_PRM_CLKSRC_CTRL);
		sys_div = (sys_div & OMAP3_PRM_CLKSRC_CTRL_SYSCLKDIV_M) 
			>> OMAP3_PRM_CLKSRC_CTRL_SYSCLKDIV_S;
		cm_setbits(omap3_gpt[gpt_num].cm_domain, 
			   omap3_gpt[gpt_num].clksel_offset,
			   omap3_gpt[gpt_num].clksel_mask);
		omap3_gpt[gpt_num].clk_hz = 
					get_osc_clk_speed(gpt_num, sys_div);
	} else {
		cm_clrbits(omap3_gpt[gpt_num].cm_domain, 
			   omap3_gpt[gpt_num].clksel_offset,
			   omap3_gpt[gpt_num].clksel_mask);
		omap3_gpt[gpt_num].clk_hz = S32K_FREQ_HZ;
	}

	/* Enable I Clock for GPT */
	cm_setbits(omap3_gpt[gpt_num].cm_domain, 
		   omap3_gpt[gpt_num].iclken_offset, 
		   omap3_gpt[gpt_num].iclken_mask);

	/* Enable F Clock for GPT */
	cm_setbits(omap3_gpt[gpt_num].cm_domain, 
		   omap3_gpt[gpt_num].fclken_offset, 
		   omap3_gpt[gpt_num].fclken_mask);
}

int __init arch_clocksource_init(void)
{
#ifdef CONFIG_OMAP3_CLKSRC_S32KT
	return s32k_clocksource_init(OMAP3_S32K_BASE);
#else
	u32 gpt_num = OMAP3_CLK_SRC_GPT;

	omap3_gpt_clock_enable(gpt_num);

	return gpt_clocksource_init(omap3_gpt[gpt_num].name, 
				    omap3_gpt[gpt_num].base_pa, 
				    omap3_gpt[gpt_num].clk_hz);
#endif
}

int __cpuinit arch_clockchip_init(void)
{
	u32 gpt_num = OMAP3_CLK_EVENT_GPT;

	omap3_gpt_clock_enable(gpt_num);

	return gpt_clockchip_init(omap3_gpt[gpt_num].name, 
				  omap3_gpt[gpt_num].base_pa, 
				  omap3_gpt[gpt_num].clk_hz,
				  omap3_gpt[gpt_num].irq_no);
}

int __init arch_board_final_init(void)
{
	int rc;
	struct vmm_devtree_node *node;

	/* All VMM API's are available here */
	/* We can register a Board specific resource here */

	/* Find simple-bus node */
	node = vmm_devtree_find_compatible(NULL, NULL, "simple-bus");
	if (!node) {
		return VMM_ENODEV;
	}

	/* Do probing using device driver framework */
	rc = vmm_devdrv_probe(node);
	vmm_devtree_dref_node(node);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}
