/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file sdhci-xenon.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Support for Xenon SDHC device on Marvell SOCs
 *
 * The source has been largely adapted from u-boot:
 * drivers/mmc/xenon_sdhci.c
 *
 * Driver for Marvell SOC Platform Group Xenon SDHC as a platform device
 *
 * Copyright (C) 2016 Marvell, All Rights Reserved.
 *
 * Author:	Victor Gu <xigu@marvell.com>
 * Date:	2016-8-24
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_delay.h>
#include <vmm_host_io.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_modules.h>
#include <libs/mathlib.h>
#include <drv/clk.h>
#include <drv/mmc/sdhci.h>

#define MODULE_DESC			"Marvell Xenon SDHC Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(SDHCI_IPRIORITY + 1)
#define	MODULE_INIT			xenon_sdhci_driver_init
#define	MODULE_EXIT			xenon_sdhci_driver_exit

/* Register Offset of SD Host Controller SOCP self-defined register */
#define SDHC_SYS_CFG_INFO			0x0104
#define SLOT_TYPE_SDIO_SHIFT			24
#define SLOT_TYPE_EMMC_MASK			0xFF
#define SLOT_TYPE_EMMC_SHIFT			16
#define SLOT_TYPE_SD_SDIO_MMC_MASK		0xFF
#define SLOT_TYPE_SD_SDIO_MMC_SHIFT		8
#define NR_SUPPORTED_SLOT_MASK			0x7

#define SDHC_SYS_OP_CTRL			0x0108
#define AUTO_CLKGATE_DISABLE_MASK		BIT(20)
#define SDCLK_IDLEOFF_ENABLE_SHIFT		8
#define SLOT_ENABLE_SHIFT			0

#define SDHC_SYS_EXT_OP_CTRL			0x010C
#define MASK_CMD_CONFLICT_ERROR			BIT(8)

#define SDHC_SLOT_RETUNING_REQ_CTRL		0x0144
/* retuning compatible */
#define RETUNING_COMPATIBLE			0x1

/* Xenon specific Mode Select value */
#define XENON_SDHCI_CTRL_HS200			0x5
#define XENON_SDHCI_CTRL_HS400			0x6

#define EMMC_PHY_REG_BASE			0x170
#define EMMC_PHY_TIMING_ADJUST			EMMC_PHY_REG_BASE
#define OUTPUT_QSN_PHASE_SELECT			BIT(17)
#define SAMPL_INV_QSP_PHASE_SELECT		BIT(18)
#define SAMPL_INV_QSP_PHASE_SELECT_SHIFT	18
#define EMMC_PHY_SLOW_MODE			BIT(29)
#define PHY_INITIALIZAION			BIT(31)
#define WAIT_CYCLE_BEFORE_USING_MASK		0xf
#define WAIT_CYCLE_BEFORE_USING_SHIFT		12
#define FC_SYNC_EN_DURATION_MASK		0xf
#define FC_SYNC_EN_DURATION_SHIFT		8
#define FC_SYNC_RST_EN_DURATION_MASK		0xf
#define FC_SYNC_RST_EN_DURATION_SHIFT		4
#define FC_SYNC_RST_DURATION_MASK		0xf
#define FC_SYNC_RST_DURATION_SHIFT		0

#define EMMC_PHY_FUNC_CONTROL			(EMMC_PHY_REG_BASE + 0x4)
#define DQ_ASYNC_MODE				BIT(4)
#define DQ_DDR_MODE_SHIFT			8
#define DQ_DDR_MODE_MASK			0xff
#define CMD_DDR_MODE				BIT(16)

#define EMMC_PHY_PAD_CONTROL			(EMMC_PHY_REG_BASE + 0x8)
#define REC_EN_SHIFT				24
#define REC_EN_MASK				0xf
#define FC_DQ_RECEN				BIT(24)
#define FC_CMD_RECEN				BIT(25)
#define FC_QSP_RECEN				BIT(26)
#define FC_QSN_RECEN				BIT(27)
#define OEN_QSN					BIT(28)
#define AUTO_RECEN_CTRL				BIT(30)

#define EMMC_PHY_PAD_CONTROL1			(EMMC_PHY_REG_BASE + 0xc)
#define EMMC5_1_FC_QSP_PD			BIT(9)
#define EMMC5_1_FC_QSP_PU			BIT(25)
#define EMMC5_1_FC_CMD_PD			BIT(8)
#define EMMC5_1_FC_CMD_PU			BIT(24)
#define EMMC5_1_FC_DQ_PD			0xff
#define EMMC5_1_FC_DQ_PU			(0xff << 16)

#define SDHCI_RETUNE_EVT_INTSIG			0x00001000

/* Hyperion only have one slot 0 */
#define XENON_MMC_SLOT_ID_HYPERION		0

enum soc_pad_ctrl_type {
	SOC_PAD_SD,
	SOC_PAD_FIXED_1_8V,
};

struct xenon_sdhci_priv {
	u8 timing;
	u32 irq;

	struct clk *clk;
	u32 clock_freq;

	virtual_addr_t base;
	virtual_addr_t pad_base;

	void *pad_ctrl_reg;
	int pad_type;
};

static int xenon_mmc_phy_init(struct sdhci_host *host)
{
	struct xenon_sdhci_priv *priv = sdhci_priv(host);
	u32 clock = priv->clock_freq;
	u32 time;
	u32 var;

	/* Enable QSP PHASE SELECT */
	var = sdhci_readl(host, EMMC_PHY_TIMING_ADJUST);
	var |= SAMPL_INV_QSP_PHASE_SELECT;
	if ((priv->timing == MMC_TIMING_UHS_SDR50) ||
	    (priv->timing == MMC_TIMING_UHS_SDR25) ||
	    (priv->timing == MMC_TIMING_UHS_SDR12) ||
	    (priv->timing == MMC_TIMING_SD_HS) ||
	    (priv->timing == MMC_TIMING_LEGACY))
		var |= EMMC_PHY_SLOW_MODE;
	sdhci_writel(host, var, EMMC_PHY_TIMING_ADJUST);

	/* Poll for host MMC PHY clock init to be stable */
	/* Wait up to 10ms */
	time = 100;
	while (time--) {
		var = sdhci_readl(host, SDHCI_CLOCK_CONTROL);
		if (var & SDHCI_CLOCK_INT_STABLE)
			break;

		vmm_udelay(100);
	}

	if (time <= 0) {
		vmm_lerror(host->hw_name,
			   "Failed to enable MMC internal clock in time\n");
		return VMM_ETIMEDOUT;
	}

	/* Init PHY */
	var = sdhci_readl(host, EMMC_PHY_TIMING_ADJUST);
	var |= PHY_INITIALIZAION;
	sdhci_writel(host, var, EMMC_PHY_TIMING_ADJUST);

	if (clock == 0) {
		/* Use the possibly slowest bus frequency value */
		clock = 100000;
	}

	/* Poll for host eMMC PHY init to complete */
	/* Wait up to 10ms */
	time = 100;
	while (time--) {
		var = sdhci_readl(host, EMMC_PHY_TIMING_ADJUST);
		var &= PHY_INITIALIZAION;
		if (!var)
			break;

		/* wait for host eMMC PHY init to complete */
		vmm_udelay(100);
	}

	if (time <= 0) {
		vmm_lerror(host->hw_name, "Failed to init MMC PHY in time\n");
		return VMM_ETIMEDOUT;
	}

	return 0;
}

#define ARMADA_3700_SOC_PAD_1_8V	0x1
#define ARMADA_3700_SOC_PAD_3_3V	0x0

static void armada_3700_soc_pad_voltage_set(struct sdhci_host *host)
{
	struct xenon_sdhci_priv *priv = sdhci_priv(host);

	if (priv->pad_type == SOC_PAD_FIXED_1_8V)
		vmm_writel(ARMADA_3700_SOC_PAD_1_8V, priv->pad_ctrl_reg);
	else if (priv->pad_type == SOC_PAD_SD)
		vmm_writel(ARMADA_3700_SOC_PAD_3_3V, priv->pad_ctrl_reg);
}

static void xenon_mmc_phy_set(struct sdhci_host *host)
{
	struct xenon_sdhci_priv *priv = sdhci_priv(host);
	u32 var;

	/* Setup pad, set bit[30], bit[28] and bits[26:24] */
	var = sdhci_readl(host, EMMC_PHY_PAD_CONTROL);
	var |= AUTO_RECEN_CTRL | OEN_QSN | FC_QSP_RECEN |
		FC_CMD_RECEN | FC_DQ_RECEN;
	sdhci_writel(host, var, EMMC_PHY_PAD_CONTROL);

	/* Set CMD and DQ Pull Up */
	var = sdhci_readl(host, EMMC_PHY_PAD_CONTROL1);
	var |= (EMMC5_1_FC_CMD_PU | EMMC5_1_FC_DQ_PU);
	var &= ~(EMMC5_1_FC_CMD_PD | EMMC5_1_FC_DQ_PD);
	sdhci_writel(host, var, EMMC_PHY_PAD_CONTROL1);

	/*
	 * If timing belongs to high speed, set bit[17] of
	 * EMMC_PHY_TIMING_ADJUST register
	 */
	if ((priv->timing == MMC_TIMING_MMC_HS400) ||
	    (priv->timing == MMC_TIMING_MMC_HS200) ||
	    (priv->timing == MMC_TIMING_UHS_SDR50) ||
	    (priv->timing == MMC_TIMING_UHS_SDR104) ||
	    (priv->timing == MMC_TIMING_UHS_DDR50) ||
	    (priv->timing == MMC_TIMING_UHS_SDR25) ||
	    (priv->timing == MMC_TIMING_MMC_DDR52)) {
		var = sdhci_readl(host, EMMC_PHY_TIMING_ADJUST);
		var |= OUTPUT_QSN_PHASE_SELECT;
		sdhci_writel(host, var, EMMC_PHY_TIMING_ADJUST);
	}

	/*
	 * When setting EMMC_PHY_FUNC_CONTROL register,
	 * SD clock should be disabled
	 */
	var = sdhci_readl(host, SDHCI_CLOCK_CONTROL);
	var &= ~SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, var, SDHCI_CLOCK_CONTROL);

	var = sdhci_readl(host, EMMC_PHY_FUNC_CONTROL);
	if (host->mmc->card->ddr_mode) {
		var |= (DQ_DDR_MODE_MASK << DQ_DDR_MODE_SHIFT) | CMD_DDR_MODE;
	} else {
		var &= ~((DQ_DDR_MODE_MASK << DQ_DDR_MODE_SHIFT) |
			 CMD_DDR_MODE);
	}
	sdhci_writel(host, var, EMMC_PHY_FUNC_CONTROL);

	/* Enable bus clock */
	var = sdhci_readl(host, SDHCI_CLOCK_CONTROL);
	var |= SDHCI_CLOCK_CARD_EN;
	sdhci_writew(host, var, SDHCI_CLOCK_CONTROL);

	xenon_mmc_phy_init(host);
}

/* Enable/Disable the Auto Clock Gating function of this slot */
static void xenon_mmc_set_acg(struct sdhci_host *host, bool enable)
{
	u32 var;

	var = sdhci_readl(host, SDHC_SYS_OP_CTRL);
	if (enable)
		var &= ~AUTO_CLKGATE_DISABLE_MASK;
	else
		var |= AUTO_CLKGATE_DISABLE_MASK;

	sdhci_writel(host, var, SDHC_SYS_OP_CTRL);
}

#define SLOT_MASK(slot)		BIT(slot)

/* Enable specific slot */
static void xenon_mmc_enable_slot(struct sdhci_host *host, u8 slot)
{
	u32 var;

	var = sdhci_readl(host, SDHC_SYS_OP_CTRL);
	var |= SLOT_MASK(slot) << SLOT_ENABLE_SHIFT;
	sdhci_writel(host, var, SDHC_SYS_OP_CTRL);
}

/* Enable Parallel Transfer Mode */
static void xenon_mmc_enable_parallel_tran(struct sdhci_host *host, u8 slot)
{
	u32 var;

	var = sdhci_readl(host, SDHC_SYS_EXT_OP_CTRL);
	var |= SLOT_MASK(slot);
	sdhci_writel(host, var, SDHC_SYS_EXT_OP_CTRL);
}

static void xenon_mmc_disable_tuning(struct sdhci_host *host, u8 slot)
{
	u32 var;

	/* Clear the Re-Tuning Request functionality */
	var = sdhci_readl(host, SDHC_SLOT_RETUNING_REQ_CTRL);
	var &= ~RETUNING_COMPATIBLE;
	sdhci_writel(host, var, SDHC_SLOT_RETUNING_REQ_CTRL);

	/* Clear the Re-tuning Event Signal Enable */
	var = sdhci_readl(host, SDHCI_SIGNAL_ENABLE);
	var &= ~SDHCI_RETUNE_EVT_INTSIG;
	sdhci_writel(host, var, SDHCI_SIGNAL_ENABLE);
}

/* Mask command conflict error */
static void xenon_mask_cmd_conflict_err(struct sdhci_host *host)
{
	u32  reg;

	reg = sdhci_readl(host, SDHC_SYS_EXT_OP_CTRL);
	reg |= MASK_CMD_CONFLICT_ERROR;
	sdhci_writel(host, reg, SDHC_SYS_EXT_OP_CTRL);
}

/* Platform specific function for post set_ios configuration */
static void xenon_sdhci_set_ios_post(struct sdhci_host *host)
{
	struct xenon_sdhci_priv *priv = sdhci_priv(host);
	u32 speed = host->mmc->card->tran_speed;
	int pwr_18v = 0;

	if ((sdhci_readb(host, SDHCI_POWER_CONTROL) & ~SDHCI_POWER_ON) ==
	    SDHCI_POWER_180)
		pwr_18v = 1;

	/* Set timing variable according to the configured speed */
	if (IS_SD(host->mmc->card)) {
		/* SD/SDIO */
		if (pwr_18v) {
			if (host->mmc->card->ddr_mode)
				priv->timing = MMC_TIMING_UHS_DDR50;
			else if (speed <= 25000000)
				priv->timing = MMC_TIMING_UHS_SDR25;
			else
				priv->timing = MMC_TIMING_UHS_SDR50;
		} else {
			if (speed <= 25000000)
				priv->timing = MMC_TIMING_LEGACY;
			else
				priv->timing = MMC_TIMING_SD_HS;
		}
	} else {
		/* eMMC */
		if (host->mmc->card->ddr_mode)
			priv->timing = MMC_TIMING_MMC_DDR52;
		else if (speed <= 26000000)
			priv->timing = MMC_TIMING_LEGACY;
		else
			priv->timing = MMC_TIMING_MMC_HS;
	}

	/* Re-init the PHY */
	xenon_mmc_phy_set(host);
}

static int xenon_sdhci_driver_probe(struct vmm_device *dev)
{
	int rc;
	u32 tmp;
	struct sdhci_host *host;
	struct xenon_sdhci_priv *priv;

	host = sdhci_alloc_host(dev, sizeof(struct xenon_sdhci_priv));
	if (!host) {
		rc = VMM_ENOMEM;
		goto free_nothing;
	}
	priv = sdhci_priv(host);

	rc = vmm_devtree_request_regmap(dev->of_node, &priv->base, 0,
					"XENON SDHCI");
	if (rc) {
		goto free_host;
	}

	rc = vmm_devtree_request_regmap(dev->of_node, &priv->pad_base, 1,
					"XENON SDHCI PAD");
	if (!rc) {
		priv->pad_ctrl_reg = (void *)priv->pad_base;
	} else {
		priv->pad_ctrl_reg = NULL;
		rc = VMM_OK;
	}

	priv->irq = vmm_devtree_irq_parse_map(dev->of_node, 0);
	if (!priv->irq) {
		rc = VMM_ENODEV;
		goto free_reg;
	}

	priv->clk = clk_get(dev, NULL);
	if (VMM_IS_ERR_OR_NULL(priv->clk)) {
		rc = VMM_PTR_ERR(priv->clk);
		goto free_reg;
	}
	priv->clock_freq = clk_get_rate(priv->clk);

	/* Set name, irq, and register base */
	host->hw_name = dev->name;
	host->irq = (priv->irq) ? priv->irq : -1;
	host->ioaddr = (void *)priv->base;

	/* Set quirks */
	host->quirks = SDHCI_QUIRK_WAIT_SEND_CMD | SDHCI_QUIRK_32BIT_DMA_ADDR;

	/* Set default timing */
	priv->timing = MMC_TIMING_LEGACY;

	/* Disable auto clock gating during init */
	xenon_mmc_set_acg(host, false);

	/* Enable slot */
	xenon_mmc_enable_slot(host, XENON_MMC_SLOT_ID_HYPERION);

	/*
	 * Set default power on SoC PHY PAD register (currently only
	 * available on the Armada 3700)
	 */
	if (priv->pad_ctrl_reg)
		armada_3700_soc_pad_voltage_set(host);

	/* Set MMC capabilities of SDHCI host */
	host->mmc_caps = MMC_CAP_MODE_HS |
			 MMC_CAP_MODE_HS_52MHz |
			 MMC_CAP_MODE_DDR_52MHz;
	rc = vmm_devtree_read_u32(dev->of_node, "bus-width", &tmp);
	if (rc) {
		goto free_clk;
	}
	switch (tmp) {
	case 8:
		host->mmc_caps |= MMC_CAP_MODE_8BIT;
		break;
	case 4:
		host->mmc_caps |= MMC_CAP_MODE_4BIT;
		break;
	case 1:
		break;
	default:
		vmm_lerror(host->hw_name, "Invalid \"bus-width\" value\n");
		rc = VMM_EINVALID;
		goto free_clk;
	};

	/* Set ops */
	host->ops.set_ios_post = xenon_sdhci_set_ios_post;

	/* Set max clock */
	host->max_clk = priv->clock_freq;

	/* Register SDHCI host */
	rc = sdhci_add_host(host);
	if (rc) {
		goto free_clk;
	}
	dev->priv = host;

	/* Enable parallel transfer */
	xenon_mmc_enable_parallel_tran(host, XENON_MMC_SLOT_ID_HYPERION);

	/* Disable tuning functionality of this slot */
	xenon_mmc_disable_tuning(host, XENON_MMC_SLOT_ID_HYPERION);

	/* Enable auto clock gating after init */
	xenon_mmc_set_acg(host, TRUE);

	xenon_mask_cmd_conflict_err(host);

	return VMM_OK;

free_clk:
	clk_put(priv->clk);
free_reg:
	if (priv->pad_ctrl_reg) {
		vmm_devtree_regunmap_release(dev->of_node, priv->pad_base, 1);
	}
	vmm_devtree_regunmap_release(dev->of_node, priv->base, 0);
free_host:
	sdhci_free_host(host);
free_nothing:
	return rc;
}

static int xenon_sdhci_driver_remove(struct vmm_device *dev)
{
	struct sdhci_host *host = dev->priv;
	struct xenon_sdhci_priv *priv = sdhci_priv(host);

	if (!host || !priv) {
		return VMM_ENODEV;
	}

	sdhci_remove_host(host, 1);

	clk_put(priv->clk);

	if (priv->pad_ctrl_reg) {
		vmm_devtree_regunmap_release(dev->of_node, priv->pad_base, 1);
	}
	vmm_devtree_regunmap_release(dev->of_node, priv->base, 0);

	sdhci_free_host(host);

	dev->priv = NULL;

	return VMM_OK;
}

static struct vmm_devtree_nodeid xenon_sdhci_devid_table[] = {
	{ .compatible = "marvell,armada-ap806-sdhci",},
	{ .compatible = "marvell,armada-cp110-sdhci",},
	{ .compatible = "marvell,armada-3700-sdhci",},
	{ /* end of list */ },
};

static struct vmm_driver xenon_sdhci_driver = {
	.name = "xenon_sdhci",
	.match_table = xenon_sdhci_devid_table,
	.probe = xenon_sdhci_driver_probe,
	.remove = xenon_sdhci_driver_remove,
};

static int __init xenon_sdhci_driver_init(void)
{
	return vmm_devdrv_register_driver(&xenon_sdhci_driver);
}

static void __exit xenon_sdhci_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&xenon_sdhci_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
