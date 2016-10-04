/**
 * Copyright (c) 2016 Pramod Kanni.
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
 * @file sdio.c
 * @author Pramod Kanni (kanni.pramod@gmail.com)
 * @brief SDIO card enumeration framework
 *
 * The source has been largely adapted from linux:
 * drivers/mmc/core/sdio.c
 *
 * Copyright 2006-2007 Pierre Ossman
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_delay.h>
#include <vmm_timer.h>
#include <vmm_host_io.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <linux/jiffies.h>

#include <drv/mmc/mmc_core.h>
#include <drv/mmc/sdio.h>
#include <drv/mmc/sdio_func.h>
#include <drv/mmc/sdio_ids.h>

#include "core.h"
#include "sdio_io.h"
#include "sdio_bus.h"

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)			vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

static int cistpl_manfid(struct mmc_card *card, struct sdio_func *func,
			 const unsigned char *buf, unsigned size)
{
	unsigned int vendor, device;

	/* TPLMID_MANF */
	vendor = buf[0] | (buf[1] << 8);

	/* TPLMID_CARD */
	device = buf[2] | (buf[3] << 8);

	if (func) {
		func->vendor = vendor;
		func->device = device;
	} else {
		card->cis.vendor = vendor;
		card->cis.device = device;
	}

	return 0;
}

static const unsigned char speed_val[16] =
	{ 0, 10, 12, 13, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 70, 80 };
static const unsigned int speed_unit[8] =
	{ 10000, 100000, 1000000, 10000000, 0, 0, 0, 0 };


typedef int (tpl_parse_t)(struct mmc_card *, struct sdio_func *,
			   const unsigned char *, unsigned);

struct cis_tpl {
	unsigned char code;
	unsigned char min_size;
	tpl_parse_t *parse;
};

static int cis_tpl_parse(struct mmc_card *card, struct sdio_func *func,
			 const char *tpl_descr,
			 const struct cis_tpl *tpl, int tpl_count,
			 unsigned char code,
			 const unsigned char *buf, unsigned size)
{
	int i, ret;

	/* look for a matching code in the table */
	for (i = 0; i < tpl_count; i++, tpl++) {
		if (tpl->code == code)
			break;
	}
	if (i < tpl_count) {
		if (size >= tpl->min_size) {
			if (tpl->parse)
				ret = tpl->parse(card, func, buf, size);
			else
				ret = VMM_EILSEQ;	/* known tuple, not parsed */
		} else {
			/* invalid tuple */
			ret = VMM_EINVALID;
		}
		if (ret && ret != VMM_EILSEQ && ret != VMM_ENOENT) {
			vmm_lerror("%s: bad %s tuple 0x%02x (%u bytes)\n",
			       mmc_hostname(card->host), tpl_descr, code, size);
		}
	} else {
		/* unknown tuple */
		ret = VMM_ENOENT;
	}

	return ret;
}

static int cistpl_funce_common(struct mmc_card *card, struct sdio_func *func,
			       const unsigned char *buf, unsigned size)
{
	/* Only valid for the common CIS (function 0) */
	if (func)
		return VMM_EINVALID;

	/* TPLFE_FN0_BLK_SIZE */
	card->cis.blksize = buf[1] | (buf[2] << 8);

	/* TPLFE_MAX_TRAN_SPEED */
	card->cis.max_dtr = speed_val[(buf[3] >> 3) & 15] *
			    speed_unit[buf[3] & 7];

	DPRINTF("%s : max transfer speed (%d)\n", card->cis.max_dtr);

	return 0;
}

static int cistpl_funce_func(struct mmc_card *card, struct sdio_func *func,
			     const unsigned char *buf, unsigned size)
{
	unsigned vsn;
	unsigned min_size;

	/* Only valid for the individual function's CIS (1-7) */
	if (!func)
		return VMM_EINVALID;

	/*
	 * This tuple has a different length depending on the SDIO spec
	 * version.
	 */
	vsn = func->card->cccr.sdio_vsn;
	min_size = (vsn == SDIO_SDIO_REV_1_00) ? 28 : 42;

	if (size < min_size)
		return VMM_EINVALID;

	/* TPLFE_MAX_BLK_SIZE */
	func->max_blksize = buf[12] | (buf[13] << 8);

	/* TPLFE_ENABLE_TIMEOUT_VAL, present in ver 1.1 and above */
	if (vsn > SDIO_SDIO_REV_1_00)
		func->enable_timeout = (buf[28] | (buf[29] << 8)) * 10;
	else
		func->enable_timeout = jiffies_to_msecs(HZ);

	return 0;
}

/*
 * Known TPLFE_TYPEs table for CISTPL_FUNCE tuples.
 *
 * Note that, unlike PCMCIA, CISTPL_FUNCE tuples are not parsed depending
 * on the TPLFID_FUNCTION value of the previous CISTPL_FUNCID as on SDIO
 * TPLFID_FUNCTION is always hardcoded to 0x0C.
 */
static const struct cis_tpl cis_tpl_funce_list[] = {
	{	0x00,	4,	cistpl_funce_common		},
	{	0x01,	0,	cistpl_funce_func		},
	{	0x04,	1+1+6,	/* CISTPL_FUNCE_LAN_NODE_ID */	},
};

static int cistpl_funce(struct mmc_card *card, struct sdio_func *func,
			const unsigned char *buf, unsigned size)
{
	if (size < 1)
		return VMM_EINVALID;

	return cis_tpl_parse(card, func, "CISTPL_FUNCE",
			     cis_tpl_funce_list,
			     array_size(cis_tpl_funce_list),
			     buf[0], buf, size);
}

/* Known TPL_CODEs table for CIS tuples */
static const struct cis_tpl cis_tpl_list[] = {
	{	0x15,	3,	/* cistpl_vers_1 */	},
	{	0x20,	4,	cistpl_manfid		},
	{	0x21,	2,	/* cistpl_funcid */	},
	{	0x22,	0,	cistpl_funce		},
};

static int sdio_read_cis(struct mmc_card *card, struct sdio_func *func)
{
	int ret;
	struct sdio_func_tuple *this;
	unsigned i, ptr = 0;

	/*
	 * Note that this works for the common CIS (function number 0) as
	 * well as a function's CIS * since SDIO_CCCR_CIS and SDIO_FBR_CIS
	 * have the same offset.
	 */
	for (i = 0; i < 3; i++) {
		unsigned char x, fn;

		if (func)
			fn = func->num;
		else
			fn = 0;

		ret = mmc_io_rw_direct(card, 0, 0,
			SDIO_FBR_BASE(fn) + SDIO_FBR_CIS + i, 0, &x);
		if (ret)
			return ret;
		ptr |= x << (i * 8);
	}

	do {
		unsigned char tpl_code, tpl_link;

		ret = mmc_io_rw_direct(card, 0, 0, ptr++, 0, &tpl_code);
		if (ret)
			break;

		/* 0xff means we're done */
		if (tpl_code == 0xff)
			break;

		/* null entries have no link field or data */
		if (tpl_code == 0x00)
			continue;

		ret = mmc_io_rw_direct(card, 0, 0, ptr++, 0, &tpl_link);
		if (ret)
			break;

		/* a size of 0xff also means we're done */
		if (tpl_link == 0xff)
			break;

		this = vmm_malloc(sizeof(*this) + tpl_link);
		if (!this)
			return VMM_ENOMEM;

		for (i = 0; i < tpl_link; i++) {
			ret = mmc_io_rw_direct(card, 0, 0,
					       ptr + i, 0, &this->data[i]);
			if (ret)
				break;
		}
		if (ret) {
			vmm_free(this);
			break;
		}

		/* Try to parse the CIS tuple */
		ret = cis_tpl_parse(card, func, "CIS",
				    cis_tpl_list, array_size(cis_tpl_list),
				    tpl_code, this->data, tpl_link);
		if (ret == VMM_EILSEQ || ret == VMM_ENOENT) {
			/*
			 * The tuple is unknown or known but not parsed.
			 */
			vmm_free(this);

			/* keep on analyzing tuples */
			ret = 0;
		} else {
			/*
			 * We don't need the tuple anymore if it was
			 * successfully parsed by the SDIO core or if it is
			 * not going to be queued for a driver.
			 */
			vmm_free(this);
		}

		ptr += tpl_link;
	} while (!ret);

	return ret;
}

int sdio_read_common_cis(struct mmc_card *card)
{
	return sdio_read_cis(card, NULL);
}

int sdio_read_func_cis(struct sdio_func *func)
{
	int ret;

	ret = sdio_read_cis(func->card, func);
	if (ret)
		return ret;

	/*
	 * Vendor/device id is optional for function CIS, so
	 * copy it from the card structure as needed.
	 */
	if (func->vendor == 0) {
		func->vendor = func->card->cis.vendor;
		func->device = func->card->cis.device;
	}

	return 0;
}

static int sdio_send_io_op_cond(struct mmc_host *host, struct mmc_card *card)
{
	int err;
	int timeout = 10;
	struct mmc_cmd cmd;

	/* Some cards seem to need this */
	mmc_go_idle(host);

 	/* Asking to the card its capabilities */
 	cmd.cmdidx = SD_IO_SEND_OP_COND;
 	cmd.resp_type = MMC_RSP_R4;
 	cmd.cmdarg = 0;
	cmd.response[0] = 0;

 	err = mmc_send_cmd(host, &cmd, NULL);
 	if (err) {
 		return err;
	}

	vmm_udelay(1000);

	do {
		cmd.cmdidx = SD_IO_SEND_OP_COND;
		cmd.resp_type = MMC_RSP_R4;
		cmd.cmdarg = (mmc_host_is_spi(host) ? 0 :
				(host->voltages &
				(cmd.response[0] & OCR_VOLTAGE_MASK)) |
				(cmd.response[0] & OCR_ACCESS_MODE));
		if (host->caps & MMC_CAP_MODE_HC) {
			cmd.cmdarg |= OCR_HCS;
		}
		cmd.response[0] = 0;

		err = mmc_send_cmd(host, &cmd, NULL);
		if (err) {
			return err;
		} else {
			break;
		}

		vmm_udelay(1000);
	} while (!(cmd.response[0] & OCR_BUSY) && timeout--);

	if (timeout <= 0) {
		return VMM_ETIMEDOUT;
	}

	if (mmc_host_is_spi(host)) { /* read OCR for spi */
		cmd.cmdidx = MMC_CMD_SPI_READ_OCR;
		cmd.resp_type = MMC_RSP_R3;
		cmd.cmdarg = 0;

		err = mmc_send_cmd(host, &cmd, NULL);
		if (err) {
			return err;
		}
	}

	card->version = MMC_VERSION_UNKNOWN;
	card->ocr = cmd.response[0];
	card->high_capacity = ((card->ocr & OCR_HCS) == OCR_HCS);
	card->rca = 0;

	return VMM_OK;
}

static int sdio_read_fbr(struct sdio_func *func)
{
	int ret;
	unsigned char data;

	ret = mmc_io_rw_direct(func->card, 0, 0,
		SDIO_FBR_BASE(func->num) + SDIO_FBR_STD_IF, 0, &data);
	if (ret)
		goto out;

	data &= 0x0f;

	if (data == 0x0f) {
		ret = mmc_io_rw_direct(func->card, 0, 0,
			SDIO_FBR_BASE(func->num) + SDIO_FBR_STD_IF_EXT, 0, &data);
		if (ret)
			goto out;
	}

	func->class = data;
	DPRINTF("sdio_read_fbr : function class (%x)\n", func->class);

out:
	return ret;
}

static int sdio_read_cccr(struct mmc_card *card, u32 ocr)
{
	int ret;
	int cccr_vsn;
	int uhs = ocr & R4_18V_PRESENT;
	unsigned char data;
	unsigned char speed;

	memset(&card->cccr, 0, sizeof(struct sdio_cccr));

	ret = mmc_io_rw_direct(card, 0, 0, SDIO_CCCR_CCCR, 0, &data);
	if (ret)
		goto out;

	cccr_vsn = data & 0x0f;

	if (cccr_vsn > SDIO_CCCR_REV_3_00) {
		vmm_lerror("%s: unrecognised CCCR structure version %d\n",
			mmc_hostname(card->host), cccr_vsn);
		return VMM_EINVALID;
	}

	card->cccr.sdio_vsn = (data & 0xf0) >> 4;

	ret = mmc_io_rw_direct(card, 0, 0, SDIO_CCCR_CAPS, 0, &data);
	if (ret)
		goto out;

	if (data & SDIO_CCCR_CAP_SMB)
		card->cccr.multi_block = 1;
	if (data & SDIO_CCCR_CAP_LSC)
		card->cccr.low_speed = 1;
	if (data & SDIO_CCCR_CAP_4BLS)
		card->cccr.wide_bus = 1;

	if (cccr_vsn >= SDIO_CCCR_REV_1_10) {
		ret = mmc_io_rw_direct(card, 0, 0, SDIO_CCCR_POWER, 0, &data);
		if (ret)
			goto out;

		if (data & SDIO_POWER_SMPC)
			card->cccr.high_power = 1;
	}

	if (cccr_vsn >= SDIO_CCCR_REV_1_20) {
		ret = mmc_io_rw_direct(card, 0, 0, SDIO_CCCR_SPEED, 0, &speed);
		if (ret)
			goto out;

		card->sda_spec3 = 0;
		card->sw_caps.sd3_bus_mode = 0;
		card->sw_caps.sd3_drv_type = 0;
		if (cccr_vsn >= SDIO_CCCR_REV_3_00 && uhs) {
			card->sda_spec3 = 1;
			ret = mmc_io_rw_direct(card, 0, 0,
				SDIO_CCCR_UHS, 0, &data);
			if (ret)
				goto out;

			ret = mmc_io_rw_direct(card, 0, 0,
				SDIO_CCCR_DRIVE_STRENGTH, 0, &data);
			if (ret)
				goto out;

			if (data & SDIO_DRIVE_SDTA)
				card->sw_caps.sd3_drv_type |= SD_DRIVER_TYPE_A;
			if (data & SDIO_DRIVE_SDTC)
				card->sw_caps.sd3_drv_type |= SD_DRIVER_TYPE_C;
			if (data & SDIO_DRIVE_SDTD)
				card->sw_caps.sd3_drv_type |= SD_DRIVER_TYPE_D;
		}

		/* if no uhs mode ensure we check for high speed */
		if (!card->sw_caps.sd3_bus_mode) {
			if (speed & SDIO_SPEED_SHS) {
				card->cccr.high_speed = 1;
				card->sw_caps.hs_max_dtr = 50000000;
			} else {
				card->cccr.high_speed = 0;
				card->sw_caps.hs_max_dtr = 25000000;
			}
		}
	}

out:
	return ret;
}


static int sdio_enable_wide(struct mmc_card *card)
{
	int ret;
	u8 ctrl;

	if (!(card->host->caps & MMC_CAP_MODE_HS))
		return 0;

	if (card->cccr.low_speed && !card->cccr.wide_bus)
		return 0;

	ret = mmc_io_rw_direct(card, 0, 0, SDIO_CCCR_IF, 0, &ctrl);
	if (ret)
		return ret;

	if ((ctrl & SDIO_BUS_WIDTH_MASK) == SDIO_BUS_WIDTH_RESERVED)
		vmm_lwarning("%s: SDIO_CCCR_IF is invalid: 0x%02x\n",
			mmc_hostname(card->host), ctrl);

	/* set as 4-bit bus width */
	ctrl &= ~SDIO_BUS_WIDTH_MASK;
	ctrl |= SDIO_BUS_WIDTH_4BIT;

	ret = mmc_io_rw_direct(card, 1, 0, SDIO_CCCR_IF, ctrl, NULL);
	if (ret)
		return ret;

	return 1;
}

/*
 * If desired, disconnect the pull-up resistor on CD/DAT[3] (pin 1)
 * of the card. This may be required on certain setups of boards,
 * controllers and embedded sdio device which do not need the card's
 * pull-up. As a result, card detection is disabled and power is saved.
 */
static int sdio_disable_cd(struct mmc_card *card)
{
	int ret;
	u8 ctrl;

	if (!(card->quirks & MMC_QUIRK_DISABLE_CD))
		return 0;

	ret = mmc_io_rw_direct(card, 0, 0, SDIO_CCCR_IF, 0, &ctrl);
	if (ret)
		return ret;

	ctrl |= SDIO_BUS_CD_DISABLE;

	return mmc_io_rw_direct(card, 1, 0, SDIO_CCCR_IF, ctrl, NULL);
}

static int sdio_enable_4bit_bus(struct mmc_card *card)
{
	int err;

	if (card->type == MMC_TYPE_SDIO) {
		err = sdio_enable_wide(card);
	} else {
		return 0;
	}

	if (err > 0) {
		mmc_set_bus_width(card->host, MMC_BUS_WIDTH_4);
		err = 0;
	}

	return err;
}


/*
 * Test if the card supports high-speed mode and, if so, switch to it.
 */
static int sdio_switch_hs(struct mmc_card *card, int enable)
{
	int ret;
	u8 speed;

	if (!(card->host->caps & MMC_CAP_MODE_HS))
		return 0;

	if (!card->cccr.high_speed)
		return 0;

	ret = mmc_io_rw_direct(card, 0, 0, SDIO_CCCR_SPEED, 0, &speed);
	if (ret)
		return ret;

	if (enable)
		speed |= SDIO_SPEED_EHS;
	else
		speed &= ~SDIO_SPEED_EHS;

	ret = mmc_io_rw_direct(card, 1, 0, SDIO_CCCR_SPEED, speed, NULL);
	if (ret)
		return ret;

	return 1;
}

/*
 * Enable SDIO/combo card's high-speed mode. Return 0/1 if [not]supported.
 */
static int sdio_enable_hs(struct mmc_card *card)
{
	int ret;

	ret = sdio_switch_hs(card, true);
	if (ret <= 0 || card->type == MMC_TYPE_SDIO)
		return ret;

	return ret;
}

static unsigned sdio_get_max_clock(struct mmc_card *card)
{
	return card->cis.max_dtr;
}

/*
 * Host is being removed. Free up the current card.
 */
static void sdio_remove(struct mmc_host *host)
{
	int i;

	BUG_ON(!host);
	BUG_ON(!host->card);

	for (i = 0;i < host->card->sdio_funcs;i++) {
		if (host->card->sdio_func[i]) {
			sdio_remove_func(host->card->sdio_func[i]);
			host->card->sdio_func[i] = NULL;
		}
	}

	host->card = NULL;
}

static int sdio_init_func(struct mmc_card *card, unsigned int fn)
{
	int ret;
	struct sdio_func *func;

	BUG_ON(fn > SDIO_MAX_FUNCS);

	func = sdio_alloc_func(card);
	if (VMM_IS_ERR(func)) {
		return VMM_PTR_ERR(func);
	}

	func->num = fn;

	if (!(card->quirks & MMC_QUIRK_NONSTD_SDIO)) {
		ret = sdio_read_fbr(func);
		if (ret)
			goto fail;

		ret = sdio_read_func_cis(func);
		if (ret)
			goto fail;
	} else {
		func->vendor = func->card->cis.vendor;
		func->device = func->card->cis.device;
		func->max_blksize = func->card->cis.blksize;
	}

	card->sdio_func[fn - 1] = func;

	return 0;

fail:
	/*
	 * It is okay to remove the function here even though we hold
	 * the host lock as we haven't registered the device yet.
	 */
	sdio_remove_func(func);
	return ret;
}

int sdio_attach(struct mmc_host *host)
{
	int rc = VMM_OK;
	struct mmc_card *card;
	int i, funcs;
	struct mmc_cmd cmd;

	if (!host) {
		return VMM_EFAIL;
	}

	/* If card instance available then do nothing */
	if (host->card) {
		rc = VMM_OK;
		goto detect_done;
	}

	sdio_reset(host);
	/* Reset mmc card */
	rc = mmc_go_idle(host);
	if (rc) {
		DPRINTF("sdio_attach: mmc_go_idle failed:%d\n", rc);
		goto detect_done;
	}

	/* Allocate new card instance */
	host->card = vmm_zalloc(sizeof(struct mmc_card));
	if (!host->card) {
		rc = VMM_ENOMEM;
		goto detect_done;
	}
	card = host->card;
	card->host = host;
	card->version = MMC_VERSION_UNKNOWN;

	/* send io op cond */
	rc = sdio_send_io_op_cond(host, host->card);
	if (rc) {
		goto removecard;
	}

	/* init card, also take care of voltage selection */
	/* Attempt to detect sdio card */
	if (!mmc_getcd(host)) {
		rc = VMM_ENOTAVAIL;
		goto removecard;
	}

	/* Set minimum bus_width and minimum clock */
	mmc_set_bus_width(host, 1);
	mmc_set_clock(host, 1);

	if (!(card->ocr & R4_MEMORY_PRESENT)) {
		card->type = MMC_TYPE_SDIO;
	} else {
		/* This is not a SDIO card */
		DPRINTF("sdio_attach: R4_MEMORY_PRESENT, may not be SDIO card\n");
		rc = VMM_EIO;
		goto removecard;
	}

	/* host specific card init */
	rc = mmc_init_card(host, host->card);
	if (rc) {
		DPRINTF("sdio_attach: host contrl drvr init_card failed:%d\n", rc);
		goto removecard;
	}

	if (!mmc_host_is_spi(host)) { /* cmd not supported in spi */
		cmd.cmdidx = SD_CMD_SEND_RELATIVE_ADDR;
		cmd.cmdarg = card->rca << 16;
		cmd.resp_type = MMC_RSP_R6;
		rc = mmc_send_cmd(host, &cmd, NULL);
		if (rc) {
			DPRINTF("sdio_attach: send relative address failed:%d\n", rc);
			goto removecard;
		}
		card->rca = (cmd.response[0] >> 16) & 0xffff;
	}

	/*
	 * Read the common registers.
	 */
	rc = sdio_read_cccr(card, card->ocr);
	if (rc) {
		DPRINTF("sdio_attach: read cccr failed:%d\n", rc);
		goto removecard;
	}

	/*
	 * Read the common CIS tuples.
	 */
	rc = sdio_read_common_cis(card);
	if (rc) {
		DPRINTF("sdio_attach: read common cis failed:%d\n", rc);
		goto removecard;
	}

	/*
	 * If needed, disconnect card detection pull-up resistor.
	 */
	rc = sdio_disable_cd(card);
	if (rc) {
		DPRINTF("sdio_attach: sdio disable card-detect failed:%d\n", rc);
		goto removecard;
	}

	/* Initialization sequence for UHS-I cards */
	/* Only if card supports 1.8v and UHS signaling */
	if ((card->ocr & R4_18V_PRESENT) && card->sw_caps.sd3_bus_mode) {
		DPRINTF("sdio_attach: UHS-I mode is not supported yet.\n");
	}

	/*
	 * Switch to high-speed by default.
	 */
	rc = sdio_enable_hs(card);
	if (rc) {
		DPRINTF("sdio_attach: sdio enable high-speed failed:%d\n", rc);
		goto removecard;
	}

	/*
	 * Change to the card's maximum speed.
	 */
	mmc_set_clock(host, sdio_get_max_clock(card));

	/*
	 * Switch to wider bus (if supported).
	 */
	rc = sdio_enable_4bit_bus(card);
	if (rc) {
		DPRINTF("sdio_attach: sdio enable high-speed failed:%d\n", rc);
		goto removecard;
	}

	/* init sdio funcs */
	/*
	 * The number of functions on the card is encoded inside
	 * the ocr.
	 */
	funcs = (card->ocr & 0x70000000) >> 28;
	card->sdio_funcs = 0;

	/* Initialize (but don't add) all present functions. */
	for (i = 0; i < funcs; i++, card->sdio_funcs++) {
		rc = sdio_init_func(host->card, i + 1);
		if (rc) {
			DPRINTF("sdio_attach: sdio init func:%d failed:%d\n", i, rc);
			goto removecard;
		}
	}

	/* add sdio funcs */
	for (i = 0; i < funcs; i++) {
		rc = sdio_add_func(host->card->sdio_func[i]);
		if (rc) {
			DPRINTF("sdio_attach: sdio add func:%d failed:%d\n", i, rc);
			goto removefunc;
		}
	}

	rc = VMM_OK;
	goto detect_done;

removefunc:
	sdio_remove(host);

removecard:
	vmm_free(host->card);
	host->card = NULL;

detect_done:
	return rc;
}
