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
 * @file sdhci-bcm2835.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Support for SDHCI device on BCM2835
 *
 * The source has been largely adapted from u-boot:
 * drivers/mmc/bcm2835_sdhci.c
 *
 * This u-boot code was extracted from:
 * git://github.com/gonzoua/u-boot-pi.git master
 * and hence presumably (C) 2012 Oleksandr Tymoshenko
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
#include <drv/sdhci.h>

#define MODULE_DESC			"BCM2835 SDHCI Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			bcm2835_sdhci_driver_init
#define	MODULE_EXIT			bcm2835_sdhci_driver_exit

/* 400KHz is max freq for card ID etc. Use that as min */
#define MIN_FREQ 400000

struct bcm2835_sdhci_host {
	struct sdhci_host host;
	u32 irq;
	u32 clock_freq;
	virtual_addr_t base;
};

/*
 * The Arasan has a bugette whereby it may lose the content of successive
 * writes to registers that are within two SD-card clock cycles of each other
 * (a clock domain crossing problem). It seems, however, that the data
 * register does not have this problem, which is just as well - otherwise we'd
 * have to nobble the DMA engine too.
 *
 * This should probably be dynamically calculated based on the actual card
 * frequency. However, this is the longest we'll have to wait, and doesn't
 * seem to slow access down too much, so the added complexity doesn't seem
 * worth it for now.
 *
 * 1/MIN_FREQ is (max) time per tick of eMMC clock.
 * 2/MIN_FREQ is time for two ticks.
 * Multiply by 1000000 to get uS per two ticks.
 * *1000000 for uSecs.
 * +1 for hack rounding.
 */
#define BCM2835_SDHCI_WRITE_DELAY	(((2 * 1000000) / MIN_FREQ) + 1)

static inline void bcm2835_sdhci_raw_writel(struct sdhci_host *host, u32 val,
						int reg)
{
	vmm_writel(val, host->ioaddr + reg);

	vmm_udelay(BCM2835_SDHCI_WRITE_DELAY);
}

static inline u32 bcm2835_sdhci_raw_readl(struct sdhci_host *host, int reg)
{
	u32 val = vmm_readl(host->ioaddr + reg);

	if (reg == SDHCI_CAPABILITIES) {
		val |= SDHCI_CAN_VDD_330;
	}

	return val;
}

static void bcm2835_sdhci_writel(struct sdhci_host *host, u32 val, int reg)
{
	bcm2835_sdhci_raw_writel(host, val, reg);
}

static void bcm2835_sdhci_writew(struct sdhci_host *host, u16 val, int reg)
{
	static u32 shadow;
	u32 oldval = (reg == SDHCI_COMMAND) ? shadow :
		bcm2835_sdhci_raw_readl(host, reg & ~3);
	u32 word_num = (reg >> 1) & 1;
	u32 word_shift = word_num * 16;
	u32 mask = 0xffff << word_shift;
	u32 newval = (oldval & ~mask) | (val << word_shift);

	if (reg == SDHCI_TRANSFER_MODE) {
		shadow = newval;
	} else {
		bcm2835_sdhci_raw_writel(host, newval, reg & ~3);
	}
}

static void bcm2835_sdhci_writeb(struct sdhci_host *host, u8 val, int reg)
{
	u32 oldval = bcm2835_sdhci_raw_readl(host, reg & ~3);
	u32 byte_num = reg & 3;
	u32 byte_shift = byte_num * 8;
	u32 mask = 0xff << byte_shift;
	u32 newval = (oldval & ~mask) | (val << byte_shift);

	bcm2835_sdhci_raw_writel(host, newval, reg & ~3);
}

static u32 bcm2835_sdhci_readl(struct sdhci_host *host, int reg)
{
	return bcm2835_sdhci_raw_readl(host, reg);
}

static u16 bcm2835_sdhci_readw(struct sdhci_host *host, int reg)
{
	u32 val = bcm2835_sdhci_raw_readl(host, (reg & ~3));
	u32 word_num = (reg >> 1) & 1;
	u32 word_shift = word_num * 16;
	u32 word = (val >> word_shift) & 0xffff;

	return word;
}

static u8 bcm2835_sdhci_readb(struct sdhci_host *host, int reg)
{
	u32 val = bcm2835_sdhci_raw_readl(host, (reg & ~3));
	u32 byte_num = reg & 3;
	u32 byte_shift = byte_num * 8;
	u32 byte = (val >> byte_shift) & 0xff;

	return byte;
}

static int bcm2835_sdhci_driver_probe(struct vmm_device *dev,
				      const struct vmm_devtree_nodeid *devid)
{
	int rc;
	struct sdhci_host *host;
	struct bcm2835_sdhci_host *bcm_host;

	host = sdhci_alloc_host(dev, sizeof(struct bcm2835_sdhci_host));
	if (!host) {
		rc = VMM_ENOMEM;
		goto free_nothing;
	}
	bcm_host = sdhci_priv(host);

	rc = vmm_devtree_regmap(dev->node, &bcm_host->base, 0);
	if (rc) {
		goto free_host;
	}

	bcm_host->irq = 0;
	vmm_devtree_irq_get(dev->node, &bcm_host->irq, 0);

	rc = vmm_devtree_clock_frequency(dev->node, &bcm_host->clock_freq);
	if (rc) {
		goto free_reg;
	}

	host->hw_name = dev->node->name;
	host->irq = (bcm_host->irq) ? bcm_host->irq : -1;
	host->ioaddr = (void *)bcm_host->base;
	host->quirks = SDHCI_QUIRK_BROKEN_VOLTAGE | \
			SDHCI_QUIRK_BROKEN_R1B | \
			SDHCI_QUIRK_WAIT_SEND_CMD;
	host->voltages = MMC_VDD_32_33 | MMC_VDD_33_34 | MMC_VDD_165_195;
	host->max_clk = bcm_host->clock_freq;
	host->min_clk = MIN_FREQ;

	host->ops.write_l = bcm2835_sdhci_writel;
	host->ops.write_w = bcm2835_sdhci_writew;
	host->ops.write_b = bcm2835_sdhci_writeb;
	host->ops.read_l = bcm2835_sdhci_readl;
	host->ops.read_w = bcm2835_sdhci_readw;
	host->ops.read_b = bcm2835_sdhci_readb;

	rc = sdhci_add_host(host);
	if (rc) {
		goto free_reg;
	}

	dev->priv = host;

	return VMM_OK;

free_reg:
	vmm_devtree_regunmap(dev->node, bcm_host->base, 0);
free_host:
	sdhci_free_host(host);
free_nothing:
	return rc;
}

static int bcm2835_sdhci_driver_remove(struct vmm_device *dev)
{
	struct sdhci_host *host = dev->priv;
	struct bcm2835_sdhci_host *bcm_host = sdhci_priv(host);

	if (host && bcm_host) {
		sdhci_remove_host(host, 1);

		vmm_devtree_regunmap(dev->node, bcm_host->base, 0);

		sdhci_free_host(host);

		dev->priv = NULL;
	}

	return VMM_OK;
}

static struct vmm_devtree_nodeid bcm2835_sdhci_devid_table[] = {
	{.type = "mmc",.compatible = "brcm,bcm2835-sdhci"},
	{ /* end of list */ },
};

static struct vmm_driver bcm2835_sdhci_driver = {
	.name = "bcm2835_sdhci",
	.match_table = bcm2835_sdhci_devid_table,
	.probe = bcm2835_sdhci_driver_probe,
	.remove = bcm2835_sdhci_driver_remove,
};

static int __init bcm2835_sdhci_driver_init(void)
{
	return vmm_devdrv_register_driver(&bcm2835_sdhci_driver);
}

static void __exit bcm2835_sdhci_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&bcm2835_sdhci_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
