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
 * @file sunxi_mmc.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief MMC driver for allwinner sunxi platform.
 *
 * The source has been largely adapted from u-boot:
 * drivers/mmc/sunxi_mmc.c
 *
 * (C) Copyright 2007-2011
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Aaron <leafy.myeh@allwinnertech.com>
 *
 * MMC driver for allwinner sunxi platform.
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_delay.h>
#include <vmm_cache.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_host_aspace.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_modules.h>
#include <libs/bitops.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <drv/mmc/mmc_core.h>

#define MODULE_DESC			"Sunxi MMC Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			sunxi_mmc_driver_init
#define	MODULE_EXIT			sunxi_mmc_driver_exit

#undef SUNXI_MMCDBG

#ifdef SUNXI_MMCDBG
#define MMCDBG(fmt...)	vmm_printf("[mmc]: "fmt)
#else
#define MMCDBG(fmt...)
#endif

#undef SUNXI_USE_DMA

#define SUNXI_GPIO_A    0
#define SUNXI_GPIO_B    1
#define SUNXI_GPIO_C    2
#define SUNXI_GPIO_D    3
#define SUNXI_GPIO_E    4
#define SUNXI_GPIO_F    5
#define SUNXI_GPIO_G    6
#define SUNXI_GPIO_H    7
#define SUNXI_GPIO_I    8

struct sunxi_gpio {
	u32 cfg[4];
	u32 dat;
	u32 drv[2];
	u32 pull[2];
};

/* gpio interrupt control */
struct sunxi_gpio_int {
	u32 cfg[3];
	u32 ctl;
	u32 sta;
	u32 deb;			/* interrupt debounce */
};

struct sunxi_gpio_reg {
	struct sunxi_gpio gpio_bank[9];
	u8 res[0xbc];
	struct sunxi_gpio_int gpio_int;
};

struct sunxi_mmc_reg {
	u32 gctrl;         /* (0x00) SMC Global Control Register */
	u32 clkcr;         /* (0x04) SMC Clock Control Register */
	u32 timeout;       /* (0x08) SMC Time Out Register */
	u32 width;         /* (0x0C) SMC Bus Width Register */
	u32 blksz;         /* (0x10) SMC Block Size Register */
	u32 bytecnt;       /* (0x14) SMC Byte Count Register */
	u32 cmd;           /* (0x18) SMC Command Register */
	u32 arg;           /* (0x1C) SMC Argument Register */
	u32 resp0;         /* (0x20) SMC Response Register 0 */
	u32 resp1;         /* (0x24) SMC Response Register 1 */
	u32 resp2;         /* (0x28) SMC Response Register 2 */
	u32 resp3;         /* (0x2C) SMC Response Register 3 */
	u32 imask;         /* (0x30) SMC Interrupt Mask Register */
	u32 mint;          /* (0x34) SMC Masked Interrupt Status Register */
	u32 rint;          /* (0x38) SMC Raw Interrupt Status Register */
	u32 status;        /* (0x3C) SMC Status Register */
	u32 ftrglevel;     /* (0x40) SMC FIFO Threshold Watermark Register */
	u32 funcsel;       /* (0x44) SMC Function Select Register */
	u32 cbcr;          /* (0x48) SMC CIU Byte Count Register */
	u32 bbcr;          /* (0x4C) SMC BIU Byte Count Register */
	u32 dbgc;          /* (0x50) SMC Debug Enable Register */
	u32 res0[11];      /* (0x54~0x7c) */
	u32 dmac;          /* (0x80) SMC IDMAC Control Register */
	u32 dlba;          /* (0x84) SMC IDMAC Descriptor List Base Address Register */
	u32 idst;          /* (0x88) SMC IDMAC Status Register */
	u32 idie;          /* (0x8C) SMC IDMAC Interrupt Enable Register */
	u32 chda;          /* (0x90) */
	u32 cbda;          /* (0x94) */
	u32 res1[26];      /* (0x98~0xff) */
	u32 fifo;          /* (0x100) SMC FIFO Access Address */
};

struct sunxi_mmc_des {
	u32 :1,
	    dic:1,		/* disable interrupt on completion */
	    last_des:1,		/* 1-this data buffer is the last buffer */
	    first_des:1,	/* 1-data buffer is the first buffer,
				   0-data buffer contained in the next descriptor is 1st buffer */
	    des_chain:1,	/* 1-the 2nd address in the descriptor is the next descriptor address */
	    end_of_ring :1,	/* 1-last descriptor flag when using dual data buffer in descriptor */
	    :24,
	    card_err_sum:1,	/* transfer error flag */
	    own:1;		/* des owner:1-idma owns it, 0-host owns it */
	union {
		struct {
			u32 buf1_sz:13,
			    buf2_sz:13,
    			    :6;
		} sun4i;
		struct {
			u32 buf1_sz:16,
			    buf2_sz:16;
		} sun5i;
	} data;
	u32 buf_addr_ptr1;
	u32 buf_addr_ptr2;
};

enum sunxi_mmc_host_types {
	SUNXI_UNKNOWN_MMC = 0,
	SUNXI_SUN4I_MMC = 1,
	SUNXI_SUN5I_MMC = 2,
};

struct sunxi_mmc_host {
	u32 mmc_no;
	u32 host_type;
	u32 des_num_shift;
	u32 des_max_len;

	u32 irq;
	u32 fatal_err;
	u32 mod_clk;

	struct sunxi_mmc_reg *reg;

	void *mclkbase;
	void *hclkbase;
	void *pll5_cfg;
	struct sunxi_gpio_reg *gpio;
	struct sunxi_mmc_des *pdes;
	physical_addr_t pdes_pa;
	u32 pdes_cnt;
};

static int sunxi_mmc_clk_io_on(struct sunxi_mmc_host *host)
{
	u32 rval, pll5_clk, divider, n, k, p;
	struct sunxi_gpio *gpio_c = &host->gpio->gpio_bank[SUNXI_GPIO_C];
	struct sunxi_gpio *gpio_f = &host->gpio->gpio_bank[SUNXI_GPIO_F];
	struct sunxi_gpio *gpio_h = &host->gpio->gpio_bank[SUNXI_GPIO_H];
	struct sunxi_gpio *gpio_i = &host->gpio->gpio_bank[SUNXI_GPIO_I];

	MMCDBG("%s: mmc %d\n", __func__, host->mmc_no);

	/* config gpio */
	switch (host->mmc_no) {
	case 0:
		/* D1-PF0, D0-PF1, CLK-PF2, CMD-PF3, D3-PF4, D4-PF5 */
		vmm_writel(0x222222, &gpio_f->cfg[0]);
		vmm_writel(0x555, &gpio_f->pull[0]);
		vmm_writel(0xaaa, &gpio_f->drv[0]);
		break;
	case 1:
		/* PH22-CMD, PH23-CLK, PH24~27-D0~D3 : 5 */
		vmm_writel(0x55<<24, &gpio_h->cfg[2]);
		vmm_writel(0x5555, &gpio_h->cfg[3]);
		vmm_writel(0x555<<12, &gpio_h->pull[1]);
		vmm_writel(0xaaa<<12, &gpio_h->drv[1]);
		break;
	case 2:
		/* CMD-PC6, CLK-PC7, D0-PC8, D1-PC9, D2-PC10, D3-PC11 */
		vmm_writel(0x33<<24, &gpio_c->cfg[0]);
		vmm_writel(0x3333, &gpio_c->cfg[1]);
		vmm_writel(0x555<<12, &gpio_c->pull[0]);
		vmm_writel(0xaaa<<12, &gpio_c->drv[0]);
		break;
	case 3:
		/* PI4-CMD, PI5-CLK, PI6~9-D0~D3 : 2 */
		vmm_writel(0x2222<<16, &gpio_i->cfg[0]);
		vmm_writel(0x22, &gpio_i->cfg[1]);
		vmm_writel(0x555<<8, &gpio_i->pull[0]);
		vmm_writel(0x555<<8, &gpio_i->drv[0]);
		break;
	default:
		return -1;
	};

	/* config ahb clock */
	rval = vmm_readl(host->hclkbase);
	rval |= (1 << (8 + host->mmc_no));
	vmm_writel(rval, host->hclkbase);
	
	/* config mod clock */
	rval = vmm_readl(host->pll5_cfg);
	n = (rval >> 8) &  0x1f;
	k = ((rval >> 4) & 3) + 1;
	p = 1 << ((rval >> 16) & 3);
	pll5_clk = 24000000 * n * udiv32(k, p);
	if (pll5_clk > 400000000) {
		divider = 4;
	} else {
		divider = 3;
	}
	vmm_writel((1U << 31) | (2U << 24) | divider, host->mclkbase);
	host->mod_clk = udiv32(pll5_clk, (divider + 1));

	return VMM_OK;
}

static int sunxi_mmc_update_clk(struct sunxi_mmc_host *host)
{
	u32 cmd, timeout = 0xfffff;

	cmd = (1U << 31) | (1 << 21) | (1 << 13);
  	vmm_writel(cmd, &host->reg->cmd);

	while ((vmm_readl(&host->reg->cmd) & 0x80000000) && timeout--) ;

	if (!timeout) {
		return VMM_ETIMEDOUT;
	}

	vmm_writel(vmm_readl(&host->reg->rint), &host->reg->rint);

	return VMM_OK;
}

static int sunxi_mmc_config_clock(struct sunxi_mmc_host *host, u32 div)
{
	int rc;
	u32 rval = vmm_readl(&host->reg->clkcr);

	/*
	 * CLKCREG[7:0]: divider
	 * CLKCREG[16]:  on/off
	 * CLKCREG[17]:  power save
	 */

	/* Disable Clock */
	rval &= ~(1 << 16);
	vmm_writel(rval, &host->reg->clkcr);
	rc = sunxi_mmc_update_clk(host);
	if (rc) {
		return rc;
	}

	/* Change Divider Factor */
	rval &= ~(0xFF);
	rval |= div;
	vmm_writel(rval, &host->reg->clkcr);
	rc = sunxi_mmc_update_clk(host);
	if (rc) {
		return rc;
	}

	/* Re-enable Clock */
	rval |= (1 << 16);
	vmm_writel(rval, &host->reg->clkcr);
	rc = sunxi_mmc_update_clk(host);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

static void sunxi_mmc_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	u32 clkdiv = 0;
	struct sunxi_mmc_host* host = mmc_priv(mmc);

	MMCDBG("%s: bus_width: %d, clock: %d, mod_clk=%d\n", __func__,
		ios->bus_width, ios->clock, host->mod_clk);

	/* Change clock first */
	if (ios->clock) {
		clkdiv = udiv32((host->mod_clk + (ios->clock >> 1)), 
				(ios->clock / 2));
		if (sunxi_mmc_config_clock(host, clkdiv)) {
			host->fatal_err = 1;
			return;
		}
	}

	/* Change bus width */
	if (ios->bus_width == 8) {
		vmm_writel(2, &host->reg->width);
	} else if (ios->bus_width == 4) {
		vmm_writel(1, &host->reg->width);
	} else {
		vmm_writel(0, &host->reg->width);
	}
}

static int sunxi_mmc_init_card(struct mmc_host *mmc, struct mmc_card *card)
{
	struct sunxi_mmc_host* host = mmc_priv(mmc);

	/* Reset controller */
	vmm_writel(0x7, &host->reg->gctrl);

	return VMM_OK;
}

static int sunxi_mmc_trans_data_pio(struct sunxi_mmc_host* host, 
				    struct mmc_data *data)
{
	u32 i, timeout = 0xfffff;
	u32 word_cnt = (data->blocksize * data->blocks) >> 2;
	u32 *buff;

	if (data->flags & MMC_DATA_READ) {
		buff = (u32 *)data->dest;
		for (i = 0; i < word_cnt; i++) {
			while (--timeout && 
				(vmm_readl(&host->reg->status) & (1 << 2))) ;
			if (timeout <= 0) {
				goto out;
			}
			buff[i] = vmm_readl(&host->reg->fifo);
			timeout = 0xfffff;
		}
	} else {
		buff = (u32 *)data->src;
		for (i = 0; i < word_cnt; i++) {
			while (--timeout && 
				(vmm_readl(&host->reg->status) & (1 << 3)));
			if (timeout <= 0) {
				goto out;
			}
			vmm_writel(buff[i], &host->reg->fifo);
			timeout = 0xfffff;
		}
	}

out:
	if (timeout <= 0) {
		return VMM_ETIMEDOUT;
	}

	return VMM_OK;
}

static int sunxi_mmc_trans_data_dma(struct sunxi_mmc_host* host, 
				    struct mmc_data *data)
{
	u8 *buff;
	u32 byte_cnt = data->blocksize * data->blocks;
	u32 des_idx = 0, buff_frag_num = 0, remain, i, rval;
	struct sunxi_mmc_des *pdes = host->pdes;

	buff = data->flags & MMC_DATA_READ ? 
				(u8 *)data->dest : (u8 *)data->src;
	buff_frag_num = byte_cnt >> host->des_num_shift;
	remain = byte_cnt & (host->des_max_len - 1);
	if (remain) {
		buff_frag_num ++;
	} else {
		remain = host->des_max_len;
	}

	vmm_flush_cache_range((virtual_addr_t)&buff[0], 
					(virtual_addr_t)&buff[byte_cnt]);

	for (i = 0; i < buff_frag_num; i++, des_idx++) {
		memset(&pdes[des_idx], 0, sizeof(struct sunxi_mmc_des));
		pdes[des_idx].des_chain = 1;
		pdes[des_idx].own = 1;
		pdes[des_idx].dic = 1;
		if (buff_frag_num > 1 && i != (buff_frag_num - 1)) {
			if (host->host_type == SUNXI_SUN4I_MMC) {
				pdes[des_idx].data.sun4i.buf1_sz = 
				(host->des_max_len - 1) & host->des_max_len;
			} else {
				pdes[des_idx].data.sun5i.buf1_sz = 
				(host->des_max_len - 1) & host->des_max_len;
			}
		} else {
			if (host->host_type == SUNXI_SUN4I_MMC) {
				pdes[des_idx].data.sun4i.buf1_sz = remain;
			} else {
				pdes[des_idx].data.sun5i.buf1_sz = remain;
			}
		}

		pdes[des_idx].buf_addr_ptr1 = 
					(u32)buff + i * host->des_max_len;
		if (i == 0) {
			pdes[des_idx].first_des = 1;
		}

		if (i == (buff_frag_num - 1)) {
			pdes[des_idx].dic = 0;
			pdes[des_idx].last_des = 1;
			pdes[des_idx].end_of_ring = 1;
			pdes[des_idx].buf_addr_ptr2 = 0;
		} else {
			pdes[des_idx].buf_addr_ptr2 = (u32)&pdes[des_idx + 1];
		}
	}

	vmm_flush_cache_range((virtual_addr_t)&pdes[0], 
					(virtual_addr_t)&pdes[des_idx + 1]);

	/*
	 * GCTRLREG
	 * GCTRL[2]	: DMA reset
	 * GCTRL[5]	: DMA enable
	 *
	 * IDMACREG
	 * IDMAC[0]	: IDMA soft reset
	 * IDMAC[1]	: IDMA fix burst flag
	 * IDMAC[7]	: IDMA on
	 *
	 * IDIECREG
	 * IDIE[0]	: IDMA transmit interrupt flag
	 * IDIE[1]	: IDMA receive interrupt flag
	 */
	rval = vmm_readl(&host->reg->gctrl);
	vmm_writel(rval | (1 << 5) | (1 << 2), &host->reg->gctrl);	/* dma enable */
	vmm_writel((1 << 0), &host->reg->dmac); /* idma reset */
	vmm_writel((1 << 1) | (1 << 7), &host->reg->dmac); /* idma on */
	rval = vmm_readl(&host->reg->idie) & (~3);
	if (data->flags & MMC_DATA_WRITE) {
		rval |= (1 << 0);
	} else {
		rval |= (1 << 1);
	}
	vmm_writel(rval, &host->reg->idie);
	vmm_writel((u32)pdes, &host->reg->dlba);
	vmm_writel((2U<<28)|(7<<16)|8, &host->reg->ftrglevel);

	return VMM_OK;
}

static int sunxi_mmc_send_cmd(struct mmc_host *mmc, 
			      struct mmc_cmd *cmd, 
			      struct mmc_data *data)
{
	struct sunxi_mmc_host* host = mmc_priv(mmc);
	int timeout = 0, error = 0;
	u32 cmdval = 0x80000000;
	u32 status = 0;
	u32 usedma = 0;
	u32 bytecnt = 0;

	if (host->fatal_err) {
		return VMM_EIO;
	}
	if (cmd->resp_type & MMC_RSP_BUSY) {
		MMCDBG("%s: cmd %d check rsp busy\n", __func__, cmd->cmdidx);
	}
	if (cmd->cmdidx == 12) {
		return VMM_OK;
	}

	/*
	 * CMDREG
	 * CMD[5:0]	: Command index
	 * CMD[6]	: Has response
	 * CMD[7]	: Long response
	 * CMD[8]	: Check response CRC
	 * CMD[9]	: Has data
	 * CMD[10]	: Write
	 * CMD[11]	: Steam mode
	 * CMD[12]	: Auto stop
	 * CMD[13]	: Wait previous over
	 * CMD[14]	: About cmd
	 * CMD[15]	: Send initialization
	 * CMD[21]	: Update clock
	 * CMD[31]	: Load cmd
	 */
	if (!cmd->cmdidx) {
		cmdval |= (1 << 15);
	}
	if (cmd->resp_type & MMC_RSP_PRESENT) {
		cmdval |= (1 << 6);
	}
	if (cmd->resp_type & MMC_RSP_136) {
		cmdval |= (1 << 7);
	}
	if (cmd->resp_type & MMC_RSP_CRC) {
		cmdval |= (1 << 8);
	}
	if (data) {
		if ((u32)data->dest & 0x3) {
			error = VMM_EINVALID;
			goto out;
		}

		cmdval |= (1 << 9) | (1 << 13);
		if (data->flags & MMC_DATA_WRITE) {
			cmdval |= (1 << 10);
		}
		if (data->blocks > 1) {
			cmdval |= (1 << 12);
		}

		vmm_writel(data->blocksize, &host->reg->blksz);
		vmm_writel(data->blocks * data->blocksize, 
						&host->reg->bytecnt);
	}

	MMCDBG("%s: mmc %d, cmd %d(0x%08x), arg 0x%08x\n", __func__,
		host->mmc_no, cmd->cmdidx, cmdval|cmd->cmdidx, cmd->cmdarg);

	vmm_writel(cmd->cmdarg, &host->reg->arg);
	if (!data) {
		vmm_writel(cmdval|cmd->cmdidx, &host->reg->cmd);
	}

	/*
	 * transfer data and check status
	 * STATREG[2] : FIFO empty
	 * STATREG[3] : FIFO full
	 */
	if (data) {
		bytecnt = data->blocksize * data->blocks;
		MMCDBG("%s: trans data %d bytes\n", __func__, bytecnt);
#if defined(SUNXI_USE_DMA)
		if (bytecnt > 64) {
#else
		if (0) {
#endif
			usedma = 1;
			vmm_writel(vmm_readl(&host->reg->gctrl) & ~0x80000000,
				   &host->reg->gctrl);
			error = sunxi_mmc_trans_data_dma(host, data);
			vmm_writel(cmdval|cmd->cmdidx, &host->reg->cmd);
		} else {
			vmm_writel(vmm_readl(&host->reg->gctrl) | 0x80000000, 
				   &host->reg->gctrl);
			vmm_writel(cmdval|cmd->cmdidx, &host->reg->cmd);
			error = sunxi_mmc_trans_data_pio(host, data);
		}
		if (error) {
			goto out;
		}
	}

	timeout = 0xfffff;
	do {
		status = vmm_readl(&host->reg->rint);
		if (!timeout-- || (status & 0xbfc2)) {
			error = VMM_EIO;
			MMCDBG("%s: cmd timeout %x\n", __func__, status);
			goto out;
		}
	} while (!(status & 0x4));

	if (data) {
		u32 done = 0;
		timeout = usedma ? (0xffff * bytecnt) : 0xffff;

		MMCDBG("%s: calc timeout %x\n", __func__, timeout);

		do {
			status = vmm_readl(&host->reg->rint);
			if (!timeout-- || (status & 0xbfc2)) {
				error = VMM_EIO;
				MMCDBG("%s: data timeout %x\n", 
					__func__, status);
				goto out;
			}
			if (data->blocks > 1) {
				done = status & (1 << 14);
			} else {
				done = status & (1 << 3);
			}
		} while (!done);
	}

	if (cmd->resp_type & MMC_RSP_BUSY) {
		timeout = 0xfffff;

		do {
			status = vmm_readl(&host->reg->status);
			if (!timeout--) {
				error = VMM_EIO;
				MMCDBG("%s: busy timeout\n", __func__);
				goto out;
			}
		} while (status & (1 << 9));
	}

	if (cmd->resp_type & MMC_RSP_136) {
		cmd->response[0] = vmm_readl(&host->reg->resp3);
		cmd->response[1] = vmm_readl(&host->reg->resp2);
		cmd->response[2] = vmm_readl(&host->reg->resp1);
		cmd->response[3] = vmm_readl(&host->reg->resp0);
		MMCDBG("%s: mmc resp 0x%08x 0x%08x 0x%08x 0x%08x\n", __func__,
			cmd->response[3], cmd->response[2],
			cmd->response[1], cmd->response[0]);
	} else {
		cmd->response[0] = vmm_readl(&host->reg->resp0);
		MMCDBG("%s: mmc resp 0x%08x\n", __func__, cmd->response[0]);
	}

out:
	if (data && usedma) {
		/* IDMASTAREG
		 * IDST[0] : idma tx int
		 * IDST[1] : idma rx int
		 * IDST[2] : idma fatal bus error
		 * IDST[4] : idma descriptor invalid
		 * IDST[5] : idma error summary
		 * IDST[8] : idma normal interrupt sumary
		 * IDST[9] : idma abnormal interrupt sumary
		 */
		status = vmm_readl(&host->reg->idst);
		vmm_writel(status, &host->reg->idst);
	        vmm_writel(0, &host->reg->idie);
	        vmm_writel(0, &host->reg->dmac);
	        vmm_writel(vmm_readl(&host->reg->gctrl) & ~(1 << 5), 
							&host->reg->gctrl);
	}

	if (error) {
		vmm_writel(0x7, &host->reg->gctrl);
		sunxi_mmc_update_clk(host);
		MMCDBG("%s: mmc cmd %d err %d\n", __func__, 
			cmd->cmdidx, error);
	}

	vmm_writel(0xffffffff, &host->reg->rint);
	vmm_writel(vmm_readl(&host->reg->gctrl) | (1 << 1), 
							&host->reg->gctrl);

	return error;
}

static vmm_irq_return_t sunxi_mmc_irq_handler(int irq_no, void *dev)
{
	/* FIXME: For now, we don't use interrupt */

	return VMM_IRQ_HANDLED;
}

static int sunxi_mmc_driver_probe(struct vmm_device *dev,
				  const struct vmm_devtree_nodeid *devid)
{
	int rc;
	virtual_addr_t base;
	physical_addr_t basepa;
	struct mmc_host *mmc;
	struct sunxi_mmc_host *host;

	/* Allocate MMC host */
	mmc = mmc_alloc_host(sizeof(struct sunxi_mmc_host), dev);
	if (!mmc) {
		rc = VMM_ENOMEM;
		goto free_nothing;
	}
	host = mmc_priv(mmc);

	/* Setup host type specific info */
	if (vmm_devtree_read_u32(dev->node, "mmc_no", &host->mmc_no)) {
		host->mmc_no = 0;
	}

	if (vmm_devtree_is_compatible(dev->node, "allwinner,sun4i-a10-mmc")) {
		host->host_type = SUNXI_SUN4I_MMC;
		host->des_num_shift = 13;
		host->des_max_len = (1 << 13);
	} else {
		host->host_type = SUNXI_SUN5I_MMC;
		host->des_num_shift = 16;
		host->des_max_len = (1 << 16);
	}

	/* Acquire resources */
	rc = vmm_devtree_regmap(dev->node, &base, 0);
	if (rc) {
		goto free_host;
	}
	host->reg = (struct sunxi_mmc_reg *)base;

	rc = vmm_devtree_regmap(dev->node, &base, 1);
	if (rc) {
		goto free_reg;
	}
	host->mclkbase = (void *)base;

	rc = vmm_devtree_regmap(dev->node, &base, 2);
	if (rc) {
		goto free_mclkbase;
	}
	host->hclkbase = (void *)base;

	rc = vmm_devtree_regmap(dev->node, &base, 3);
	if (rc) {
		goto free_hclkbase;
	}
	host->pll5_cfg = (void *)base;

	rc = vmm_devtree_regmap(dev->node, &base, 4);
	if (rc) {
		goto free_pll5_cfg;
	}
	host->gpio = (struct sunxi_gpio_reg *)base;

	base = vmm_host_alloc_pages(1, VMM_MEMORY_FLAGS_NORMAL);
	if (!base) {
		rc = VMM_ENOMEM;
		goto free_gpio;
	}
	host->pdes = (struct sunxi_mmc_des *)base;
	rc = vmm_host_va2pa(base, &host->pdes_pa);
	if (rc) {
		goto free_pdes;
	}
	host->pdes_cnt = VMM_PAGE_SIZE / sizeof(struct sunxi_mmc_des);

	/* Setup interrupt handler */
	rc = vmm_devtree_irq_get(dev->node, &host->irq, 0);
	if (rc) {
		goto free_pdes;
	}
	if ((rc = vmm_host_irq_register(host->irq, dev->name, 
					sunxi_mmc_irq_handler, mmc))) {
		goto free_pdes;
	}

	/* Setup mmc host configuration */
	mmc->caps = MMC_CAP_MODE_4BIT | 
		    MMC_CAP_MODE_HS_52MHz | 
		    MMC_CAP_MODE_HS |
		    MMC_CAP_NEEDS_POLL;
	mmc->voltages = MMC_VDD_32_33 | MMC_VDD_33_34;
	mmc->f_min = 400000;
	mmc->f_max = 52000000;

	/* Setup mmc host operations */
	mmc->ops.send_cmd = sunxi_mmc_send_cmd;
	mmc->ops.set_ios = sunxi_mmc_set_ios;
	mmc->ops.init_card = sunxi_mmc_init_card;
	mmc->ops.get_cd = NULL;
	mmc->ops.get_wp = NULL;

	/* Initialize mmc host */
	rc = sunxi_mmc_clk_io_on(host);
	if (rc) {
		goto free_irq;
	}

	/* Add MMC host */
	rc = mmc_add_host(mmc);
	if (rc) {
		goto free_irq;
	}

	dev->priv = mmc;

	vmm_devtree_regaddr(dev->node, &basepa, 0);
	vmm_printf("%s: Sunxi MMC at 0x%08llx irq %d (%s)\n",
		   dev->name, (unsigned long long)basepa, host->irq,
#ifdef SUNXI_USE_DMA
		   "dma");
#else
		   "pio");
#endif

	return VMM_OK;

free_irq:
	vmm_host_irq_unregister(host->irq, mmc);
free_pdes:
	vmm_host_free_pages((virtual_addr_t)host->pdes, 1);
free_gpio:
	vmm_devtree_regunmap(dev->node, (virtual_addr_t)host->gpio, 4);
free_pll5_cfg:
	vmm_devtree_regunmap(dev->node, (virtual_addr_t)host->pll5_cfg, 3);
free_hclkbase:
	vmm_devtree_regunmap(dev->node, (virtual_addr_t)host->hclkbase, 2);
free_mclkbase:
	vmm_devtree_regunmap(dev->node, (virtual_addr_t)host->mclkbase, 1);
free_reg:
	vmm_devtree_regunmap(dev->node, (virtual_addr_t)host->reg, 0);
free_host:
	mmc_free_host(mmc);
free_nothing:
	return rc;
}

static int sunxi_mmc_driver_remove(struct vmm_device *dev)
{
	virtual_addr_t base;
	struct mmc_host *mmc = dev->priv;
	struct sunxi_mmc_host *host = mmc_priv(mmc);

	if (mmc && host) {
		/* Remove MMC host */
		mmc_remove_host(mmc);

		/* Reset controller */
		vmm_writel(0x7, &host->reg->gctrl);

		/* Free resources */
		vmm_host_irq_unregister(host->irq, mmc);
		base = (virtual_addr_t)host->pdes;
		vmm_host_free_pages(base, 1);
		base = (virtual_addr_t)host->gpio;
		vmm_devtree_regunmap(dev->node, base, 4);
		base = (virtual_addr_t)host->pll5_cfg;
		vmm_devtree_regunmap(dev->node, base, 3);
		base = (virtual_addr_t)host->hclkbase;
		vmm_devtree_regunmap(dev->node, base, 2);
		base = (virtual_addr_t)host->mclkbase;
		vmm_devtree_regunmap(dev->node, base, 1);
		base = (virtual_addr_t)host->reg;
		vmm_devtree_regunmap(dev->node, base, 0);

		/* Free MMC host */
		mmc_free_host(mmc);
		dev->priv = NULL;
	}

	return VMM_OK;
}

static const struct vmm_devtree_nodeid sunxi_mmc_devid_table[] = {
	{ .compatible = "allwinner,sun4i-a10-mmc", },
	{ .compatible = "allwinner,sun5i-a13-mmc", },
	{ /* sentinel */ }
};

static struct vmm_driver sunxi_mmc_driver = {
	.name = "sunxi_mmc",
	.match_table = sunxi_mmc_devid_table,
	.probe = sunxi_mmc_driver_probe,
	.remove = sunxi_mmc_driver_remove,
};

static int __init sunxi_mmc_driver_init(void)
{
	return vmm_devdrv_register_driver(&sunxi_mmc_driver);
}

static void __exit sunxi_mmc_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&sunxi_mmc_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
