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
 * @file mmci.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM PrimeCell MultiMedia Card Interface - PL180
 *
 * The source has been largely adapted from u-boot:
 * drivers/mmc/arm_pl180_mmci.c
 *
 * ARM PrimeCell MultiMedia Card Interface - PL180
 *
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Ulf Hansson <ulf.hansson@stericsson.com>
 * Author: Martin Lundholm <martin.xa.lundholm@stericsson.com>
 * Ported to drivers/mmc/ by: Matt Waddel <matt.waddel@linaro.org>
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_delay.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_modules.h>
#include <libs/bitops.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <drv/mmc/mmc_core.h>

#include <linux/amba/bus.h>

#include "mmci.h"

#define MODULE_DESC			"PL180 MMCI Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			mmci_driver_init
#define	MODULE_EXIT			mmci_driver_exit

#undef MMCI_DEBUG

#ifdef MMCI_DEBUG
#define debug(x...)			vmm_printf(x)
#else
#define debug(x...)
#endif

static int mmci_wait_for_command_end(struct mmc_host *mmc, struct mmc_cmd *cmd)
{
	u32 hoststatus, statusmask;
	struct mmci_host *host = mmc_priv(mmc);

	statusmask = SDI_STA_CTIMEOUT | SDI_STA_CCRCFAIL;
	if ((cmd->resp_type & MMC_RSP_PRESENT)) {
		statusmask |= SDI_STA_CMDREND;
	} else {
		statusmask |= SDI_STA_CMDSENT;
	}

	do {
		hoststatus = vmm_readl(&host->base->status) & statusmask;
	} while (!hoststatus);

	vmm_writel(statusmask, &host->base->status_clear);
	if (hoststatus & SDI_STA_CTIMEOUT) {
		debug("%s: CMD%d time out\n", __func__, cmd->cmdidx);
		return VMM_ETIMEDOUT;
	} else if ((hoststatus & SDI_STA_CCRCFAIL) &&
		   (cmd->resp_type & MMC_RSP_CRC)) {
		vmm_printf("%s: CMD%d CRC error\n", __func__, cmd->cmdidx);
		return VMM_EILSEQ;
	}

	if (cmd->resp_type & MMC_RSP_PRESENT) {
		cmd->response[0] = vmm_readl(&host->base->response0);
		cmd->response[1] = vmm_readl(&host->base->response1);
		cmd->response[2] = vmm_readl(&host->base->response2);
		cmd->response[3] = vmm_readl(&host->base->response3);
		debug("%s: CMD%d response[0]:0x%08X, response[1]:0x%08X, "
		      "response[2]:0x%08X, response[3]:0x%08X\n", __func__,
		      cmd->cmdidx, cmd->response[0], cmd->response[1],
		      cmd->response[2], cmd->response[3]);
	}

	return VMM_OK;
}

static int mmci_command(struct mmc_host *mmc, struct mmc_cmd *cmd)
{
	int result;
	u32 sdi_cmd, sdi_pwr;
	struct mmci_host *host = mmc_priv(mmc);

	sdi_cmd = ((cmd->cmdidx & SDI_CMD_CMDINDEX_MASK) | SDI_CMD_CPSMEN);

	if (cmd->resp_type) {
		sdi_cmd |= SDI_CMD_WAITRESP;
		if (cmd->resp_type & MMC_RSP_136) {
			sdi_cmd |= SDI_CMD_LONGRESP;
		}
	}

	vmm_writel((u32)cmd->cmdarg, &host->base->argument);
	vmm_udelay(COMMAND_REG_DELAY);
	vmm_writel(sdi_cmd, &host->base->command);
	result = mmci_wait_for_command_end(mmc, cmd);

	/* After CMD2 set RCA to a none zero value. */
	if ((result == 0) && (cmd->cmdidx == MMC_CMD_ALL_SEND_CID)) {
		mmc->card->rca = 10;
	}

	/* After CMD3 open drain is switched off and push pull is used. */
	if ((result == 0) && (cmd->cmdidx == MMC_CMD_SET_RELATIVE_ADDR)) {
		sdi_pwr = vmm_readl(&host->base->power) & ~SDI_PWR_OPD;
		vmm_writel(sdi_pwr, &host->base->power);
	}

	return result;
}

static int mmci_read_bytes(struct mmc_host *mmc, 
			   u32 *dest, u32 blkcount, u32 blksize)
{
	u32 *tempbuff = dest;
	u64 xfercount = blkcount * blksize;
	struct mmci_host *host = mmc_priv(mmc);
	u32 status, status_err;

	debug("%s: read_bytes: blkcount=%u blksize=%u\n", 
	      __func__, blkcount, blksize);

	status = vmm_readl(&host->base->status);
	status_err = status & (SDI_STA_DCRCFAIL | SDI_STA_DTIMEOUT |
			       SDI_STA_RXOVERR);
	while ((!status_err) && (xfercount >= sizeof(u32))) {
		if (status & SDI_STA_RXDAVL) {
			*(tempbuff) = vmm_readl(&host->base->fifo);
			tempbuff++;
			xfercount -= sizeof(u32);
		}
		status = vmm_readl(&host->base->status);
		status_err = status & (SDI_STA_DCRCFAIL | SDI_STA_DTIMEOUT |
				       SDI_STA_RXOVERR);
	}

	status_err = status &
		(SDI_STA_DCRCFAIL | SDI_STA_DTIMEOUT | SDI_STA_DBCKEND |
		 SDI_STA_RXOVERR);
	while (!status_err) {
		status = vmm_readl(&host->base->status);
		status_err = status &
			(SDI_STA_DCRCFAIL | SDI_STA_DTIMEOUT | SDI_STA_DBCKEND |
			 SDI_STA_RXOVERR);
	}

	if (status & SDI_STA_DTIMEOUT) {
		vmm_printf("%s: Read data timed out, "
			   "xfercount: %llu, status: 0x%08X\n",
			   __func__, xfercount, status);
		return VMM_ETIMEDOUT;
	} else if (status & SDI_STA_DCRCFAIL) {
		vmm_printf("%s: Read data bytes CRC error: 0x%x\n", 
			   __func__, status);
		return VMM_EILSEQ;
	} else if (status & SDI_STA_RXOVERR) {
		vmm_printf("%s: Read data RX overflow error\n", __func__);
		return VMM_EIO;
	}

	vmm_writel(SDI_ICR_MASK, &host->base->status_clear);

	if (xfercount) {
		vmm_printf("%s: Read data error, xfercount: %llu\n", 
			   __func__, xfercount);
		return VMM_EIO;
	}

	return VMM_OK;
}

static int mmci_write_bytes(struct mmc_host *mmc, 
			    u32 *src, u32 blkcount, u32 blksize)
{
	int i;
	u32 *tempbuff = src;
	u64 xfercount = blkcount * blksize;
	struct mmci_host *host = mmc_priv(mmc);
	u32 status, status_err;

	debug("%s: write_bytes: blkcount=%u blksize=%u\n", 
	      __func__, blkcount, blksize);

	status = vmm_readl(&host->base->status);
	status_err = status & (SDI_STA_DCRCFAIL | SDI_STA_DTIMEOUT);
	while (!status_err && xfercount) {
		if (status & SDI_STA_TXFIFOBW) {
			if (xfercount >= SDI_FIFO_BURST_SIZE * sizeof(u32)) {
				for (i = 0; i < SDI_FIFO_BURST_SIZE; i++) {
					vmm_writel(*(tempbuff + i),
						   &host->base->fifo);
				}
				tempbuff += SDI_FIFO_BURST_SIZE;
				xfercount -= SDI_FIFO_BURST_SIZE * sizeof(u32);
			} else {
				while (xfercount >= sizeof(u32)) {
					vmm_writel(*(tempbuff), 
						   &host->base->fifo);
					tempbuff++;
					xfercount -= sizeof(u32);
				}
			}
		}
		status = vmm_readl(&host->base->status);
		status_err = status & (SDI_STA_DCRCFAIL | SDI_STA_DTIMEOUT);
	}

	status_err = status &
		(SDI_STA_DCRCFAIL | SDI_STA_DTIMEOUT | SDI_STA_DBCKEND);
	while (!status_err) {
		status = vmm_readl(&host->base->status);
		status_err = status &
			(SDI_STA_DCRCFAIL | SDI_STA_DTIMEOUT | SDI_STA_DBCKEND);
	}

	if (status & SDI_STA_DTIMEOUT) {
		vmm_printf("%s: Write data timed out, "
			   "xfercount:%llu,status:0x%08X\n",
		           __func__, xfercount, status);
		return VMM_ETIMEDOUT;
	} else if (status & SDI_STA_DCRCFAIL) {
		vmm_printf("%s: Write data CRC error\n", __func__);
		return VMM_EILSEQ;
	}

	vmm_writel(SDI_ICR_MASK, &host->base->status_clear);

	if (xfercount) {
		vmm_printf("%s: Write data error, xfercount:%llu", 
			   __func__, xfercount);
		return VMM_EIO;
	}

	return VMM_OK;
}

static int mmci_data_transfer(struct mmc_host *mmc,
			      struct mmc_cmd *cmd,
			      struct mmc_data *data)
{
	int error = VMM_ETIMEDOUT;
	struct mmci_host *host = mmc_priv(mmc);
	u32 blksz = 0;
	u32 data_ctrl = 0;
	u32 data_len = (u32)(data->blocks * data->blocksize);

	if (!host->version2) {
		blksz = (ffs(data->blocksize) - 1);
		data_ctrl |= ((blksz << 4) & SDI_DCTRL_DBLKSIZE_MASK);
	} else {
		blksz = data->blocksize;
		data_ctrl |= (blksz << SDI_DCTRL_DBLOCKSIZE_V2_SHIFT);
	}
	data_ctrl |= SDI_DCTRL_DTEN | SDI_DCTRL_BUSYMODE;

	vmm_writel(SDI_DTIMER_DEFAULT, &host->base->datatimer);
	vmm_writel(data_len, &host->base->datalength);
	vmm_udelay(DATA_REG_DELAY);

	if (data->flags & MMC_DATA_READ) {
		data_ctrl |= SDI_DCTRL_DTDIR_IN;
		vmm_writel(data_ctrl, &host->base->datactrl);

		error = mmci_command(mmc, cmd);
		if (error) {
			return error;
		}

		error = mmci_read_bytes(mmc, (u32 *)data->dest, 
					     (u32)data->blocks,
					     (u32)data->blocksize);
	} else if (data->flags & MMC_DATA_WRITE) {
		error = mmci_command(mmc, cmd);
		if (error) {
			return error;
		}

		vmm_writel(data_ctrl, &host->base->datactrl);
		error = mmci_write_bytes(mmc, (u32 *)data->src, 
					      (u32)data->blocks,
					      (u32)data->blocksize);
	}

	return error;
}

static int mmci_request(struct mmc_host *mmc, 
			struct mmc_cmd *cmd, 
			struct mmc_data *data)
{
	if (data) {
		return mmci_data_transfer(mmc, cmd, data);
	} else {
		return mmci_command(mmc, cmd);
	}
}

static void mmci_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct mmci_host *host = mmc_priv(mmc);
	u32 sdi_clkcr;

	sdi_clkcr = vmm_readl(&host->base->clock);

	/* Ramp up the clock rate */
	if (ios->clock) {
		u32 clkdiv = 0;
		u32 tmp_clock;

		if (ios->clock >= mmc->f_max) {
			clkdiv = 0;
			ios->clock = mmc->f_max;
		} else {
			clkdiv = udiv32(host->clock_in, ios->clock) - 2;
		}

		tmp_clock = udiv32(host->clock_in, (clkdiv + 2));
		while (tmp_clock > ios->clock) {
			clkdiv++;
			tmp_clock = udiv32(host->clock_in, (clkdiv + 2));
		}

		if (clkdiv > SDI_CLKCR_CLKDIV_MASK)
			clkdiv = SDI_CLKCR_CLKDIV_MASK;

		tmp_clock = udiv32(host->clock_in, (clkdiv + 2));
		ios->clock = tmp_clock;
		sdi_clkcr &= ~(SDI_CLKCR_CLKDIV_MASK);
		sdi_clkcr |= clkdiv;
	}

	/* Set the bus width */
	if (ios->bus_width) {
		u32 buswidth = 0;

		switch (ios->bus_width) {
		case 1:
			buswidth |= SDI_CLKCR_WIDBUS_1;
			break;
		case 4:
			buswidth |= SDI_CLKCR_WIDBUS_4;
			break;
		case 8:
			buswidth |= SDI_CLKCR_WIDBUS_8;
			break;
		default:
			vmm_printf("%s: Invalid bus width: %d\n", 
				   __func__, ios->bus_width);
			break;
		}
		sdi_clkcr &= ~(SDI_CLKCR_WIDBUS_MASK);
		sdi_clkcr |= buswidth;
	}

	vmm_writel(sdi_clkcr, &host->base->clock);
	vmm_udelay(CLK_CHANGE_DELAY);
}

int mmci_init_card(struct mmc_host *mmc, struct mmc_card *card)
{
	struct mmci_host *host = mmc_priv(mmc);

	/* MMCI uses open drain drivers in the enumeration phase */
	vmm_writel(host->pwr_init, &host->base->power);

	return VMM_OK;
}

static vmm_irq_return_t mmci_cmd_irq_handler(int irq_no, void *dev)
{
	/* FIXME: For now, we don't use interrupt */

	return VMM_IRQ_HANDLED;
}

static vmm_irq_return_t mmci_pio_irq_handler(int irq_no, void *dev)
{
	/* FIXME: For now, we don't use interrupt */

	return VMM_IRQ_HANDLED;
}

static int mmci_driver_probe(struct vmm_device *dev,
				   const struct vmm_devtree_nodeid *devid)
{
	int rc;
	u32 sdi;
	virtual_addr_t base;
	physical_addr_t basepa;
	struct mmc_host *mmc;
	struct mmci_host *host;

	mmc = mmc_alloc_host(sizeof(struct mmci_host), dev);
	if (!mmc) {
		rc = VMM_ENOMEM;
		goto free_nothing;
	}
	host = mmc_priv(mmc);

	rc = vmm_devtree_regmap(dev->node, &base, 0);
	if (rc) {
		goto free_host;
	}
	host->base = (struct sdi_registers *)base;

	rc = vmm_devtree_irq_get(dev->node, &host->irq0, 0);
	if (rc) {
		goto free_reg;
	}
	if ((rc = vmm_host_irq_register(host->irq0, dev->node->name, 
					mmci_cmd_irq_handler, mmc))) {
		goto free_reg;
	}

	rc = vmm_devtree_irq_get(dev->node, &host->irq1, 1);
	if (!rc) {
		if ((rc = vmm_host_irq_register(host->irq1, dev->node->name, 
						mmci_pio_irq_handler, mmc))) {
			goto free_irq0;
		}
		host->singleirq = 0;
	} else {
		host->singleirq = 1;
	}

	/* Retrive matching data */
	host->pwr_init = ((const u32 *)devid->data)[0];
	host->clkdiv_init = ((const u32 *)devid->data)[1];
	host->voltages = ((const u32 *)devid->data)[2];
	host->caps = ((const u32 *)devid->data)[3];
	host->clock_in = ((const u32 *)devid->data)[4];
	host->clock_min = ((const u32 *)devid->data)[5];
	host->clock_max = ((const u32 *)devid->data)[6];
	host->version2 = ((const u32 *)devid->data)[7];

	/* Initialize power and clock divider */
	vmm_writel(host->pwr_init, &host->base->power);
	vmm_writel(host->clkdiv_init, &host->base->clock);
	vmm_udelay(CLK_CHANGE_DELAY);

	/* Disable interrupts */
	sdi = vmm_readl(&host->base->mask0) & ~SDI_MASK0_MASK;
	vmm_writel(sdi, &host->base->mask0);

	/* Setup mmc host configuration */
	mmc->caps = host->caps;
	mmc->voltages = host->voltages;
	mmc->f_min = host->clock_min;
	mmc->f_max = host->clock_max;
	mmc->b_max = host->b_max;

	/* Setup mmc host operations */
	mmc->ops.send_cmd = mmci_request;
	mmc->ops.set_ios = mmci_set_ios;
	mmc->ops.init_card = mmci_init_card;
	mmc->ops.get_cd = NULL;
	mmc->ops.get_wp = NULL;

	rc = mmc_add_host(mmc);
	if (rc) {
		goto free_irq1;
	}

	dev->priv = mmc;

	vmm_devtree_regaddr(dev->node, &basepa, 0);
	vmm_printf("%s: PL%03x manf %x rev%u at 0x%08llx irq %d,%d (pio)\n",
		   dev->node->name, amba_part(dev), amba_manf(dev),
		   amba_rev(dev), (unsigned long long)basepa,
		   host->irq0, host->irq1);

	return VMM_OK;

free_irq1:
	if (!host->singleirq) {
		vmm_host_irq_unregister(host->irq1, mmc);
	}
free_irq0:
	vmm_host_irq_unregister(host->irq0, mmc);
free_reg:
	vmm_devtree_regunmap(dev->node, (virtual_addr_t)host->base, 0);
free_host:
	mmc_free_host(mmc);
free_nothing:
	return rc;
}

static int mmci_driver_remove(struct vmm_device *dev)
{
	struct mmc_host *mmc = dev->priv;
	struct mmci_host *host = mmc_priv(mmc);

	if (mmc && host) {
		mmc_remove_host(mmc);

		vmm_writel(0, &host->base->mask0);
		vmm_writel(0, &host->base->mask1);
		vmm_writel(0, &host->base->command);
		vmm_writel(0, &host->base->datactrl);

		if (!host->singleirq) {
			vmm_host_irq_unregister(host->irq1, mmc);
		}
		vmm_host_irq_unregister(host->irq0, mmc);
		vmm_devtree_regunmap(dev->node, (virtual_addr_t)host->base, 0);
		mmc_free_host(mmc);
		dev->priv = NULL;
	}

	return VMM_OK;
}

static u32 mmci_v1[]= {
	INIT_PWR, /* pwr_init */
	SDI_CLKCR_CLKDIV_INIT_V1 | SDI_CLKCR_CLKEN, /* clkdiv_init */
	VOLTAGE_WINDOW_MMC, /* voltages */
	0, /* caps */
	ARM_MCLK, /* clock_in */
	ARM_MCLK / (2 * (SDI_CLKCR_CLKDIV_INIT_V1 + 1)), /* clock_min */
	6250000, /* clock_max */
	0, /* version2 */
};

static u32 mmci_v2[]= {
	SDI_PWR_OPD | SDI_PWR_PWRCTRL_ON, /* pwr_init */
	SDI_CLKCR_CLKDIV_INIT_V2 | SDI_CLKCR_CLKEN | SDI_CLKCR_HWFC_EN, /* clkdiv_init */
	VOLTAGE_WINDOW_MMC, /* voltages */
	MMC_CAP_MODE_8BIT | MMC_CAP_MODE_HS | MMC_CAP_MODE_HS_52MHz, /* caps */
	ARM_MCLK, /* clock_in */
	ARM_MCLK / (2 + SDI_CLKCR_CLKDIV_INIT_V2), /* clock_min */
	ARM_MCLK / 2, /* clock_max */
	1, /* version2 */
};

static struct vmm_devtree_nodeid mmci_devid_table[] = {
	{.type = "mmc",.compatible = "arm,pl180", .data = &mmci_v1},
	{.type = "mmc",.compatible = "arm,pl180v2", .data = &mmci_v2},
	{ /* end of list */ },
};

static struct vmm_driver mmci_driver = {
	.name = "pl180_mmci",
	.match_table = mmci_devid_table,
	.probe = mmci_driver_probe,
	.remove = mmci_driver_remove,
};

static int __init mmci_driver_init(void)
{
	return vmm_devdrv_register_driver(&mmci_driver);
}

static void __exit mmci_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&mmci_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
