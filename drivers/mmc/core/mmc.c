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
 * @file mmc.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief MMC/SD/SDIO core framework implementation
 *
 * The source has been largely adapted from u-boot:
 * drivers/mmc/mmc.c
 *
 * Copyright 2008,2010 Freescale Semiconductor, Inc
 * Andy Fleming
 *
 * Based (loosely) on the Linux code
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_limits.h>
#include <vmm_heap.h>
#include <vmm_delay.h>
#include <vmm_timer.h>
#include <vmm_host_io.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <drv/mmc/mmc_core.h>

#include "core.h"

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)			vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

struct mode_width_tuning {
	enum mmc_bus_mode mode;
	u32 widths;
	u32 tuning;
};

static bool mmc_is_mode_ddr(enum mmc_bus_mode mode)
{
	if (mode == MMC_DDR_52)
		return TRUE;
	else if (mode == UHS_DDR50)
		return TRUE;
	else if (mode == MMC_HS_400)
		return TRUE;
	else
		return FALSE;
}

static const char *mmc_mode_name(enum mmc_bus_mode mode)
{
	static const char *const names[] = {
	      [MMC_LEGACY]	= "MMC legacy (25MHz)",
	      [SD_LEGACY]	= "SD Legacy (25MHz)",
	      [MMC_HS]		= "MMC High Speed (26MHz)",
	      [SD_HS]		= "SD High Speed (50MHz)",
	      [UHS_SDR12]	= "UHS SDR12 (25MHz)",
	      [UHS_SDR25]	= "UHS SDR25 (50MHz)",
	      [UHS_SDR50]	= "UHS SDR50 (100MHz)",
	      [UHS_SDR104]	= "UHS SDR104 (208MHz)",
	      [UHS_DDR50]	= "UHS DDR50 (50MHz)",
	      [MMC_HS_52]	= "MMC High Speed (52MHz)",
	      [MMC_DDR_52]	= "MMC DDR52 (52MHz)",
	      [MMC_HS_200]	= "HS200 (200MHz)",
	      [MMC_HS_400]	= "HS400 (200MHz)",
	};

	if (mode >= MMC_MODES_END)
		return "Unknown mode";
	else
		return names[mode];
}

static u32 mmc_mode2freq(struct mmc_card *card, enum mmc_bus_mode mode)
{
	static const int freqs[] = {
	      [MMC_LEGACY]	= 25000000,
	      [SD_LEGACY]	= 25000000,
	      [MMC_HS]		= 26000000,
	      [SD_HS]		= 50000000,
	      [MMC_HS_52]	= 52000000,
	      [MMC_DDR_52]	= 52000000,
	      [UHS_SDR12]	= 25000000,
	      [UHS_SDR25]	= 50000000,
	      [UHS_SDR50]	= 100000000,
	      [UHS_DDR50]	= 50000000,
	      [UHS_SDR104]	= 208000000,
	      [MMC_HS_200]	= 200000000,
	      [MMC_HS_400]	= 200000000,
	};

	if (mode == MMC_LEGACY)
		return card->legacy_speed;
	else if (mode >= MMC_MODES_END)
		return 0;
	else
		return freqs[mode];
}

static int mmc_select_mode(struct mmc_card *card, enum mmc_bus_mode mode)
{
	card->selected_mode = mode;
	card->tran_speed = mmc_mode2freq(card, mode);
	card->ddr_mode = mmc_is_mode_ddr(mode);
	card->mode_name = mmc_mode_name(mode);
	DPRINTF("selecting mode %s (freq : %d MHz)\n", mmc_mode_name(mode),
		card->tran_speed / 1000000);
	return 0;
}

static int mmc_bus_width(u32 cap)
{
	if (cap & MMC_CAP_MODE_8BIT)
		return 8;
	if (cap & MMC_CAP_MODE_4BIT)
		return 4;
	if (cap & MMC_CAP_MODE_1BIT)
		return 1;
	DPRINTF("invalid bus width capability 0x%x\n", cap);
	return 0;
}

/* frequency bases */
/* divided by 10 to be nice to platforms without floating point */
static const int fbase[] = {
	10000,
	100000,
	1000000,
	10000000,
};

/* Multiplier values for TRAN_SPEED.  Multiplied by 10 to be nice
 * to platforms without floating point.
 */
static const int multipliers[] = {
	0,	/* reserved */
	10,
	12,
	13,
	15,
	20,
	25,
	30,
	35,
	40,
	45,
	50,
	55,
	60,
	70,
	80,
};

static int __mmc_set_blocklen(struct mmc_host *host, int len)
{
	struct mmc_cmd cmd;

	cmd.cmdidx = MMC_CMD_SET_BLOCKLEN;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = len;

	return mmc_send_cmd(host, &cmd, NULL);
}

static u32 __mmc_write_blocks(struct mmc_host *host, struct mmc_card *card,
			      u64 start, u32 blkcnt, const void *src)
{
	struct mmc_cmd cmd;
	struct mmc_data data;
	int timeout = 1000;

	DPRINTF("%s: start=0x%llx blkcnt=%d\n", __func__,
		(unsigned long long)start, blkcnt);

	if (blkcnt > 1) {
		cmd.cmdidx = MMC_CMD_WRITE_MULTIPLE_BLOCK;
	} else {
		cmd.cmdidx = MMC_CMD_WRITE_SINGLE_BLOCK;
	}

	if (card->high_capacity) {
		cmd.cmdarg = start;
	} else {
		cmd.cmdarg = start * card->write_bl_len;
	}

	cmd.resp_type = MMC_RSP_R1;

	data.src = src;
	data.blocks = blkcnt;
	data.blocksize = card->write_bl_len;
	data.flags = MMC_DATA_WRITE;

	if (mmc_send_cmd(host, &cmd, &data)) {
		return 0;
	}

	/* SPI multiblock writes terminate using a special
	 * token, not a STOP_TRANSMISSION request.
	 */
	if (!(host->caps2 & MMC_CAP2_AUTO_CMD12) && !mmc_host_is_spi(host) &&
	    blkcnt > 1) {
		cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
		cmd.cmdarg = 0;
		cmd.resp_type = MMC_RSP_R1b;
		if (mmc_send_cmd(host, &cmd, NULL)) {
			return 0;
		}
	}

	/* Waiting for the ready status */
	if (mmc_send_status(host, card, timeout)) {
		return 0;
	}

	return blkcnt;
}

u32 __mmc_sd_bwrite(struct mmc_host *host, struct mmc_card *card,
		    u64 start, u32 blkcnt, const void *src)
{
	u32 cur, blocks_todo = blkcnt;

	if (__mmc_set_blocklen(host, card->write_bl_len)) {
		return 0;
	}

	do {
		cur = (blocks_todo > host->b_max) ?  host->b_max : blocks_todo;
		if(__mmc_write_blocks(host, card, start, cur, src) != cur) {
			return 0;
		}
		blocks_todo -= cur;
		start += cur;
		src += cur * card->write_bl_len;
	} while (blocks_todo > 0);

	return blkcnt;
}

static u32 __mmc_read_blocks(struct mmc_host *host, struct mmc_card *card,
			     void *dst, u64 start, u32 blkcnt)
{
	struct mmc_cmd cmd;
	struct mmc_data data;

	DPRINTF("%s: start=0x%llx blkcnt=%d\n", __func__,
		(unsigned long long)start, blkcnt);

	if (blkcnt > 1) {
		cmd.cmdidx = MMC_CMD_READ_MULTIPLE_BLOCK;
	} else {
		cmd.cmdidx = MMC_CMD_READ_SINGLE_BLOCK;
	}

	if (card->high_capacity) {
		cmd.cmdarg = start;
	} else {
		cmd.cmdarg = start * card->read_bl_len;
	}

	cmd.resp_type = MMC_RSP_R1;

	data.dest = dst;
	data.blocks = blkcnt;
	data.blocksize = card->read_bl_len;
	data.flags = MMC_DATA_READ;

	if (mmc_send_cmd(host, &cmd, &data)) {
		return 0;
	}

	if (!(host->caps2 & MMC_CAP2_AUTO_CMD12) && (blkcnt > 1)) {
		cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
		cmd.cmdarg = 0;
		cmd.resp_type = MMC_RSP_R1b;
		if (mmc_send_cmd(host, &cmd, NULL)) {
			return 0;
		}
	}

	return blkcnt;
}

u32 __mmc_sd_bread(struct mmc_host *host, struct mmc_card *card,
		   u64 start, u32 blkcnt, void *dst)
{
	u32 cur, blocks_todo = blkcnt;

	if (blkcnt == 0) {
		return 0;
	}

	if (__mmc_set_blocklen(host, card->read_bl_len)) {
		return 0;
	}

	do {
		cur = (blocks_todo > host->b_max) ?  host->b_max : blocks_todo;
		if (__mmc_read_blocks(host, card, dst, start, cur) != cur) {
			return 0;
		}
		blocks_todo -= cur;
		start += cur;
		dst += cur * card->read_bl_len;
	} while (blocks_todo > 0);

	return blkcnt;
}

static int __sd_switch(struct mmc_host *host,
		       int mode, int group, u8 value, u8 *resp)
{
	struct mmc_cmd cmd;
	struct mmc_data data;

	/* Switch the frequency */
	cmd.cmdidx = SD_CMD_SWITCH_FUNC;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = (mode << 31) | 0xffffff;
	cmd.cmdarg &= ~(0xf << (group * 4));
	cmd.cmdarg |= value << (group * 4);

	data.dest = resp;
	data.blocksize = 64;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;

	return mmc_send_cmd(host, &cmd, &data);
}

static int __sd_get_capabilities(struct mmc_host *host, struct mmc_card *card)
{
	int err;
	struct mmc_cmd cmd;
	u32 scr[2];
	u32 switch_status[16];
	u32 sd3_bus_mode;
	struct mmc_data data;
	int timeout;

	card->caps = MMC_CAP_MODE(SD_LEGACY);

	if (mmc_host_is_spi(host)) {
		return VMM_OK;
	}

	/* Read the SCR to find out if this card supports higher speeds */
	cmd.cmdidx = MMC_CMD_APP_CMD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = card->rca << 16;

	err = mmc_send_cmd(host, &cmd, NULL);
	if (err) {
		return err;
	}

	cmd.cmdidx = SD_CMD_APP_SEND_SCR;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = 0;

	timeout = 3;

retry_scr:
	data.dest = (u8 *)scr;
	data.blocksize = 8;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;

	err = mmc_send_cmd(host, &cmd, &data);
	if (err) {
		if (timeout--) {
			goto retry_scr;
		}
		return err;
	}

	card->scr[0] = vmm_be32_to_cpu(scr[0]);
	card->scr[1] = vmm_be32_to_cpu(scr[1]);

	switch ((card->scr[0] >> 24) & 0xf) {
	case 0:
		card->version = SD_VERSION_1_0;
		break;
	case 1:
		card->version = SD_VERSION_1_10;
		break;
	case 2:
		card->version = SD_VERSION_2;
		if ((card->scr[0] >> 15) & 0x1) {
			card->version = SD_VERSION_3;
		}
		break;
	default:
		card->version = SD_VERSION_1_0;
		break;
	};

	if (card->scr[0] & SD_DATA_4BIT) {
		card->caps |= MMC_CAP_MODE_4BIT;
	}

	/* Version 1.0 doesn't support switching */
	if (card->version == SD_VERSION_1_0) {
		return VMM_OK;
	}

	timeout = 4;
	while (timeout--) {
		err = __sd_switch(host, SD_SWITCH_CHECK, 0, 1,
				  (u8 *)switch_status);
		if (err) {
			return err;
		}

		/* The high-speed function is busy.  Try again */
		if (!(vmm_be32_to_cpu(switch_status[7]) & SD_HIGHSPEED_BUSY)) {
			break;
		}
	}

	/* If high-speed isn't supported, we return */
	if (vmm_be32_to_cpu(switch_status[3]) & SD_HIGHSPEED_SUPPORTED) {
		card->caps |= MMC_CAP_MODE(SD_HS);
	}

	/* Version before 3.0 don't support UHS modes */
	if (card->version < SD_VERSION_3)
		return 0;

	sd3_bus_mode = vmm_be32_to_cpu(switch_status[3]) >> 16 & 0x1f;
	if (sd3_bus_mode & SD_MODE_UHS_SDR104)
		card->caps |= MMC_CAP_MODE(UHS_SDR104);
	if (sd3_bus_mode & SD_MODE_UHS_SDR50)
		card->caps |= MMC_CAP_MODE(UHS_SDR50);
	if (sd3_bus_mode & SD_MODE_UHS_SDR25)
		card->caps |= MMC_CAP_MODE(UHS_SDR25);
	if (sd3_bus_mode & SD_MODE_UHS_SDR12)
		card->caps |= MMC_CAP_MODE(UHS_SDR12);
	if (sd3_bus_mode & SD_MODE_UHS_DDR50)
		card->caps |= MMC_CAP_MODE(UHS_DDR50);

	return 0;
}

static int __sd_select_bus_width(struct mmc_host *host,
				 struct mmc_card *card, int w)
{
	int err;
	struct mmc_cmd cmd;

	if ((w != 4) && (w != 1))
		return VMM_EINVALID;

	cmd.cmdidx = MMC_CMD_APP_CMD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = card->rca << 16;

	err = mmc_send_cmd(host, &cmd, NULL);
	if (err)
		return err;

	cmd.cmdidx = SD_CMD_APP_SET_BUS_WIDTH;
	cmd.resp_type = MMC_RSP_R1;
	if (w == 4)
		cmd.cmdarg = 2;
	else if (w == 1)
		cmd.cmdarg = 0;
	err = mmc_send_cmd(host, &cmd, NULL);
	if (err) {
		return err;
	}

	return 0;
}

static int __sd_set_card_speed(struct mmc_host *host,
			       struct mmc_card *card,
			       enum mmc_bus_mode mode)
{
	int err, speed;
	u32 switch_status[16];

	switch (mode) {
	case SD_LEGACY:
		speed = UHS_SDR12_BUS_SPEED;
		break;
	case SD_HS:
		speed = HIGH_SPEED_BUS_SPEED;
		break;
	case UHS_SDR12:
		speed = UHS_SDR12_BUS_SPEED;
		break;
	case UHS_SDR25:
		speed = UHS_SDR25_BUS_SPEED;
		break;
	case UHS_SDR50:
		speed = UHS_SDR50_BUS_SPEED;
		break;
	case UHS_DDR50:
		speed = UHS_DDR50_BUS_SPEED;
		break;
	case UHS_SDR104:
		speed = UHS_SDR104_BUS_SPEED;
		break;
	default:
		return VMM_EINVALID;
	}

	err = __sd_switch(host, SD_SWITCH_SWITCH, 0, speed,
			  (u8 *)switch_status);
	if (err)
		return err;

	if (((vmm_be32_to_cpu(switch_status[4]) >> 24) & 0xF) != speed)
		return VMM_ENOTSUPP;

	return 0;
}

static int __sd_read_ssr(struct mmc_host *host, struct mmc_card *card)
{
	static const unsigned int sd_au_size[] = {
		0,		SZ_16K / 512,		SZ_32K / 512,
		SZ_64K / 512,	SZ_128K / 512,		SZ_256K / 512,
		SZ_512K / 512,	SZ_1M / 512,		SZ_2M / 512,
		SZ_4M / 512,	SZ_8M / 512,		(SZ_8M + SZ_4M) / 512,
		SZ_16M / 512,	(SZ_16M + SZ_8M) / 512,	SZ_32M / 512,
		SZ_64M / 512,
	};
	int err, i;
	struct mmc_cmd cmd;
	u32 ssr[16];
	struct mmc_data data;
	int timeout = 3;
	unsigned int au, eo, et, es;

	cmd.cmdidx = MMC_CMD_APP_CMD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = card->rca << 16;

	err = mmc_send_cmd(host, &cmd, NULL);
	if (err)
		return err;

	cmd.cmdidx = SD_CMD_APP_SD_STATUS;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = 0;

retry_ssr:
	data.dest = (u8 *)ssr;
	data.blocksize = 64;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;

	err = mmc_send_cmd(host, &cmd, &data);
	if (err) {
		if (timeout--)
			goto retry_ssr;

		return err;
	}

	for (i = 0; i < 16; i++)
		ssr[i] = vmm_be32_to_cpu(ssr[i]);

	au = (ssr[2] >> 12) & 0xF;
	if ((au <= 9) || (card->version == SD_VERSION_3)) {
		card->ssr.au = sd_au_size[au];
		es = (ssr[3] >> 24) & 0xFF;
		es |= (ssr[2] & 0xFF) << 8;
		et = (ssr[3] >> 18) & 0x3F;
		if (es && et) {
			eo = (ssr[3] >> 16) & 0x3;
			card->ssr.erase_timeout = udiv32((et * 1000), es);
			card->ssr.erase_offset = eo * 1000;
		}
	} else {
		DPRINTF("Invalid Allocation Unit Size.\n");
	}

	return 0;
}

static const struct mode_width_tuning sd_modes_by_pref[] = {
	{
		.mode = UHS_SDR104,
		.widths = MMC_CAP_MODE_4BIT | MMC_CAP_MODE_1BIT,
		.tuning = MMC_CMD_SEND_TUNING_BLOCK
	},
	{
		.mode = UHS_SDR50,
		.widths = MMC_CAP_MODE_4BIT | MMC_CAP_MODE_1BIT,
	},
	{
		.mode = UHS_DDR50,
		.widths = MMC_CAP_MODE_4BIT | MMC_CAP_MODE_1BIT,
	},
	{
		.mode = UHS_SDR25,
		.widths = MMC_CAP_MODE_4BIT | MMC_CAP_MODE_1BIT,
	},
	{
		.mode = SD_HS,
		.widths = MMC_CAP_MODE_4BIT | MMC_CAP_MODE_1BIT,
	},
	{
		.mode = UHS_SDR12,
		.widths = MMC_CAP_MODE_4BIT | MMC_CAP_MODE_1BIT,
	},
	{
		.mode = SD_LEGACY,
		.widths = MMC_CAP_MODE_4BIT | MMC_CAP_MODE_1BIT,
	}
};

#define for_each_sd_mode_by_pref(caps, mwt) \
	for (mwt = sd_modes_by_pref;\
	     mwt < sd_modes_by_pref + array_size(sd_modes_by_pref);\
	     mwt++) \
		if (caps & MMC_CAP_MODE(mwt->mode))

static int __sd_select_mode_and_width(struct mmc_host *host,
				      struct mmc_card *card)
{
	int err;
	u32 widths[] = {MMC_CAP_MODE_4BIT, MMC_CAP_MODE_1BIT};
	const struct mode_width_tuning *mwt;
	bool uhs_en = (card->ocr & OCR_S18R) ? TRUE : FALSE;
	u32 caps;

	DPRINTF("Host capabilities = 0x%08x\n", host->caps);
	DPRINTF("Card capabilities = 0x%08x\n", card->caps);

	/* Restrict card's capabilities by what the host can do */
	caps = card->caps & host->caps;

	if (!uhs_en)
		caps &= ~MMC_CAP_MODE_UHS;

	for_each_sd_mode_by_pref(caps, mwt) {
		u32 *w;

		DPRINTF("Trying mode %s (at %d MHz)\n",
			mmc_mode_name(mwt->mode),
			mmc_mode2freq(card, mwt->mode) / 1000000);

		for (w = widths; w < widths + array_size(widths); w++) {
			if (!(*w & caps & mwt->widths)) {
				continue;
			}
			DPRINTF("Trying width %d\n", mmc_bus_width(*w));

			/* configure the bus width (card + host) */
			err = __sd_select_bus_width(host, card,
						    mmc_bus_width(*w));
			if (err)
				goto error;
			mmc_set_bus_width(host, mmc_bus_width(*w));

			/* configure the bus mode (card) */
			err = __sd_set_card_speed(host, card, mwt->mode);
			if (err)
				goto error;

			/* configure the bus mode (host) */
			mmc_select_mode(card, mwt->mode);
			mmc_set_clock(host, card->tran_speed, FALSE);

			/* execute tuning if needed */
			if (mwt->tuning && !mmc_host_is_spi(host)) {
				err = mmc_execute_tuning(host, mwt->tuning);
				if (err) {
					DPRINTF("tuning failed\n");
					goto error;
				}
			}

			err = __sd_read_ssr(host, card);
			if (err)
				DPRINTF("unable to read ssr\n");
			if (!err)
				return 0;

error:
			/* revert to a safer bus speed */
			mmc_select_mode(card, SD_LEGACY);
			mmc_set_clock(host, card->tran_speed, FALSE);
		}
	}

	DPRINTF("unable to select a mode\n");
	return VMM_ENOTSUPP;
}

static int __mmc_switch(struct mmc_host *host, struct mmc_card *card,
			u8 set, u8 index, u8 value)
{
	struct mmc_cmd cmd;
	int timeout = 1000;
	int ret;

	cmd.cmdidx = MMC_CMD_SWITCH;
	cmd.resp_type = MMC_RSP_R1b;
	cmd.cmdarg = (MMC_SWITCH_MODE_WRITE_BYTE << 24) |
				 (index << 16) |
				 (value << 8);

	ret = mmc_send_cmd(host, &cmd, NULL);

	/* Waiting for the ready status */
	if (!ret) {
		ret = mmc_send_status(host, card, timeout);
	}

	return ret;

}

static int __mmc_send_ext_csd(struct mmc_host *host, u8 *ext_csd)
{
	struct mmc_cmd cmd;
	struct mmc_data data;

	/* Get the Card Status Register */
	cmd.cmdidx = MMC_CMD_SEND_EXT_CSD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = 0;

	data.dest = ext_csd;
	data.blocks = 1;
	data.blocksize = 512;
	data.flags = MMC_DATA_READ;

	return mmc_send_cmd(host, &cmd, &data);
}

static int __mmc_get_capabilities(struct mmc_host *host,
				  struct mmc_card *card)
{
	u8 *ext_csd = &card->ext_csd[0];
	u8 cardtype;

	card->caps = MMC_CAP_MODE_1BIT | MMC_CAP_MODE(MMC_LEGACY);

	if (mmc_host_is_spi(host)) {
		return VMM_OK;
	}

	/* Only version 4 supports high-speed */
	if (card->version < MMC_VERSION_4) {
		return VMM_OK;
	}

	if (!ext_csd) {
		DPRINTF("No ext_csd found!\n"); /* this should enver happen */
		return VMM_ENOTSUPP;
	}

	card->caps |= MMC_CAP_MODE_4BIT | MMC_CAP_MODE_8BIT;

	cardtype = ext_csd[EXT_CSD_CARD_TYPE];
	card->ext_csd_cardtype = cardtype;

	if (cardtype & (EXT_CSD_CARD_TYPE_HS200_1_2V |
			EXT_CSD_CARD_TYPE_HS200_1_8V)) {
		card->caps |= MMC_CAP_MODE(MMC_HS_200);
	}

	if (cardtype & (EXT_CSD_CARD_TYPE_HS400_1_2V |
			EXT_CSD_CARD_TYPE_HS400_1_8V)) {
		card->caps |= MMC_CAP_MODE(MMC_HS_400);
	}

	if (cardtype & EXT_CSD_CARD_TYPE_52) {
		if (cardtype & EXT_CSD_CARD_TYPE_DDR_52)
			card->caps |= MMC_CAP_MODE(MMC_DDR_52);
		card->caps |= MMC_CAP_MODE(MMC_HS_52);
	}
	if (cardtype & EXT_CSD_CARD_TYPE_26)
		card->caps |= MMC_CAP_MODE(MMC_HS);

	return VMM_OK;
}

static int __mmc_set_card_speed(struct mmc_host *host,
				struct mmc_card *card,
				enum mmc_bus_mode mode)
{
	int err;
	int speed_bits;
	u8 test_csd[MMC_MAX_BLOCK_LEN];

	memset(test_csd, 0, sizeof(test_csd));

	switch (mode) {
	case MMC_HS:
	case MMC_HS_52:
	case MMC_DDR_52:
		speed_bits = EXT_CSD_TIMING_HS;
		break;
	case MMC_HS_200:
		speed_bits = EXT_CSD_TIMING_HS200;
		break;
	case MMC_HS_400:
		speed_bits = EXT_CSD_TIMING_HS400;
		break;
	case MMC_LEGACY:
		speed_bits = EXT_CSD_TIMING_LEGACY;
		break;
	default:
		return VMM_EINVALID;
	}
	err = __mmc_switch(host, card, EXT_CSD_CMD_SET_NORMAL,
			   EXT_CSD_HS_TIMING, speed_bits);
	if (err)
		return err;

	if ((mode == MMC_HS) || (mode == MMC_HS_52)) {
		/* Now check to see that it worked */
		err = __mmc_send_ext_csd(host, &test_csd[0]);
		if (err)
			return err;

		/* No high-speed support */
		if (!test_csd[EXT_CSD_HS_TIMING])
			return VMM_ENOTSUPP;
	}

	return 0;
}

/*
 * read the compare the part of ext csd that is constant.
 * This can be used to check that the transfer is working
 * as expected.
 */
static int __mmc_read_and_compare_ext_csd(struct mmc_host *host,
					  struct mmc_card *card)
{
	int err;
	const u8 *ext_csd = &card->ext_csd[0];
	u8 test_csd[MMC_MAX_BLOCK_LEN];

	if (card->version < MMC_VERSION_4)
		return 0;

	memset(test_csd, 0, sizeof(test_csd));

	err = __mmc_send_ext_csd(host, &test_csd[0]);
	if (err)
		return err;

	/* Only compare read only fields */
	if (ext_csd[EXT_CSD_PARTITIONING_SUPPORT]
		== test_csd[EXT_CSD_PARTITIONING_SUPPORT] &&
	    ext_csd[EXT_CSD_HC_WP_GRP_SIZE]
		== test_csd[EXT_CSD_HC_WP_GRP_SIZE] &&
	    ext_csd[EXT_CSD_REV]
		== test_csd[EXT_CSD_REV] &&
	    ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE]
		== test_csd[EXT_CSD_HC_ERASE_GRP_SIZE] &&
	    memcmp(&ext_csd[EXT_CSD_SEC_CNT],
		   &test_csd[EXT_CSD_SEC_CNT], 4) == 0)
		return 0;

	return VMM_EIO;
}

static int __mmc_set_lowest_voltage(struct mmc_host *host,
				    u8 ext_csd_cardtype,
				    enum mmc_bus_mode mode,
				    u32 allowed_mask)
{
	u32 card_mask = 0;

	switch (mode) {
	case MMC_HS_400:
	case MMC_HS_200:
		if (ext_csd_cardtype & (EXT_CSD_CARD_TYPE_HS200_1_8V |
		    EXT_CSD_CARD_TYPE_HS400_1_8V))
			card_mask |= MMC_SIGNAL_VOLTAGE_180;
		if (ext_csd_cardtype & (EXT_CSD_CARD_TYPE_HS200_1_2V |
		    EXT_CSD_CARD_TYPE_HS400_1_2V))
			card_mask |= MMC_SIGNAL_VOLTAGE_120;
		break;
	case MMC_DDR_52:
		if (ext_csd_cardtype & EXT_CSD_CARD_TYPE_DDR_1_8V)
			card_mask |= MMC_SIGNAL_VOLTAGE_330 |
				     MMC_SIGNAL_VOLTAGE_180;
		if (ext_csd_cardtype & EXT_CSD_CARD_TYPE_DDR_1_2V)
			card_mask |= MMC_SIGNAL_VOLTAGE_120;
		break;
	default:
		card_mask |= MMC_SIGNAL_VOLTAGE_330;
		break;
	}

	while (card_mask & allowed_mask) {
		enum mmc_voltage best_match;

		best_match = 1 << (ffs(card_mask & allowed_mask) - 1);
		if (!mmc_set_signal_voltage(host, best_match))
			return 0;

		allowed_mask &= ~best_match;
	}

	return VMM_ENOTSUPP;
}

static const struct mode_width_tuning mmc_modes_by_pref[] = {
	{
		.mode = MMC_HS_400,
		.widths = MMC_CAP_MODE_8BIT,
		.tuning = MMC_CMD_SEND_TUNING_BLOCK_HS200
	},
	{
		.mode = MMC_HS_200,
		.widths = MMC_CAP_MODE_8BIT | MMC_CAP_MODE_4BIT,
		.tuning = MMC_CMD_SEND_TUNING_BLOCK_HS200
	},
	{
		.mode = MMC_DDR_52,
		.widths = MMC_CAP_MODE_8BIT | MMC_CAP_MODE_4BIT,
	},
	{
		.mode = MMC_HS_52,
		.widths = MMC_CAP_MODE_8BIT | MMC_CAP_MODE_4BIT |
			  MMC_CAP_MODE_1BIT,
	},
	{
		.mode = MMC_HS,
		.widths = MMC_CAP_MODE_8BIT | MMC_CAP_MODE_4BIT |
			  MMC_CAP_MODE_1BIT,
	},
	{
		.mode = MMC_LEGACY,
		.widths = MMC_CAP_MODE_8BIT | MMC_CAP_MODE_4BIT |
			  MMC_CAP_MODE_1BIT,
	}
};

#define for_each_mmc_mode_by_pref(caps, mwt) \
	for (mwt = mmc_modes_by_pref;\
	    mwt < mmc_modes_by_pref + array_size(mmc_modes_by_pref);\
	    mwt++) \
		if (caps & MMC_CAP_MODE(mwt->mode))

static const struct ext_csd_bus_width {
	u32 cap;
	bool is_ddr;
	u32 ext_csd_bits;
} ext_csd_bus_width[] = {
	{MMC_CAP_MODE_8BIT, TRUE, EXT_CSD_DDR_BUS_WIDTH_8},
	{MMC_CAP_MODE_4BIT, TRUE, EXT_CSD_DDR_BUS_WIDTH_4},
	{MMC_CAP_MODE_8BIT, FALSE, EXT_CSD_BUS_WIDTH_8},
	{MMC_CAP_MODE_4BIT, FALSE, EXT_CSD_BUS_WIDTH_4},
	{MMC_CAP_MODE_1BIT, FALSE, EXT_CSD_BUS_WIDTH_1},
};

static int __mmc_select_hs400(struct mmc_host *host, struct mmc_card *card)
{
	int err;

	/* Set timing to HS200 for tuning */
	err = __mmc_set_card_speed(host, card, MMC_HS_200);
	if (err)
		return err;

	/* configure the bus mode (host) */
	mmc_select_mode(card, MMC_HS_200);
	mmc_set_clock(host, card->tran_speed, FALSE);

	/* execute tuning if needed */
	err = mmc_execute_tuning(host, MMC_CMD_SEND_TUNING_BLOCK_HS200);
	if (err) {
		DPRINTF("tuning failed\n");
		return err;
	}

	/* Set back to HS */
	__mmc_set_card_speed(host, card, MMC_HS);
	mmc_set_clock(host, mmc_mode2freq(card, MMC_HS), FALSE);

	err = __mmc_switch(host, card, EXT_CSD_CMD_SET_NORMAL,
			   EXT_CSD_BUS_WIDTH,
			   EXT_CSD_BUS_WIDTH_8 | EXT_CSD_DDR_FLAG);
	if (err)
		return err;

	err = __mmc_set_card_speed(host, card, MMC_HS_400);
	if (err)
		return err;

	mmc_select_mode(card, MMC_HS_400);
	err = mmc_set_clock(host, card->tran_speed, FALSE);
	if (err)
		return err;

	return 0;
}

#define for_each_supported_width(caps, ddr, ecbv) \
	for (ecbv = ext_csd_bus_width;\
	    ecbv < ext_csd_bus_width + array_size(ext_csd_bus_width);\
	    ecbv++) \
		if ((ddr == ecbv->is_ddr) && (caps & ecbv->cap))

static int __mmc_select_mode_and_width(struct mmc_host *host,
				       struct mmc_card *card)
{
	int err;
	u32 card_caps;
	u8 *ext_csd = &card->ext_csd[0];
	const struct mode_width_tuning *mwt;
	const struct ext_csd_bus_width *ecbw;

	DPRINTF("Host capabilities = 0x%08x\n", host->caps);
	DPRINTF("Card capabilities = 0x%08x\n", card->caps);

	/* Restrict card's capabilities by what the host can do */
	card_caps = card->caps & host->caps;

	/* Only version 4 of MMC supports wider bus widths */
	if (card->version < MMC_VERSION_4)
		return 0;

	if (!ext_csd) {
		DPRINTF("No ext_csd found!\n"); /* this should enver happen */
		return VMM_ENOTSUPP;
	}

	mmc_set_clock(host, card->legacy_speed, FALSE);

	for_each_mmc_mode_by_pref(card_caps, mwt) {
		for_each_supported_width(card_caps & mwt->widths,
					 mmc_is_mode_ddr(mwt->mode), ecbw) {
			enum mmc_voltage old_voltage;
			DPRINTF("Trying mode %s width %d (at %d MHz)\n",
				mmc_mode_name(mwt->mode),
				mmc_bus_width(ecbw->cap),
				mmc_mode2freq(card, mwt->mode) / 1000000);

			old_voltage = host->ios.signal_voltage;
			err = __mmc_set_lowest_voltage(host,
						       card->ext_csd_cardtype,
						       mwt->mode,
						       MMC_ALL_SIGNAL_VOLTAGE);
			if (err)
				continue;

			/* configure the bus width (card + host) */
			err = __mmc_switch(host, card, EXT_CSD_CMD_SET_NORMAL,
					   EXT_CSD_BUS_WIDTH,
					   ecbw->ext_csd_bits & ~EXT_CSD_DDR_FLAG);
			if (err)
				goto error;
			mmc_set_bus_width(host, mmc_bus_width(ecbw->cap));

			if (mwt->mode == MMC_HS_400) {
				err = __mmc_select_hs400(host, card);
				if (err) {
					DPRINTF("Select HS400 failed %d\n", err);
					goto error;
				}
			} else {
				/* configure the bus speed (card) */
				err = __mmc_set_card_speed(host, card, mwt->mode);
				if (err)
					goto error;

				/*
				 * configure the bus width AND the ddr mode
				 * (card). The host side will be taken care
				 * of in the next step
				 */
				if (ecbw->ext_csd_bits & EXT_CSD_DDR_FLAG) {
					err = __mmc_switch(host, card,
							 EXT_CSD_CMD_SET_NORMAL,
							 EXT_CSD_BUS_WIDTH,
							 ecbw->ext_csd_bits);
					if (err)
						goto error;
				}

				/* configure the bus mode (host) */
				mmc_select_mode(card, mwt->mode);
				mmc_set_clock(host, card->tran_speed, FALSE);

				/* execute tuning if needed */
				if (mwt->tuning) {
					err = mmc_execute_tuning(host,
								 mwt->tuning);
					if (err) {
						DPRINTF("tuning failed\n");
						goto error;
					}
				}
			}

			/* do a transfer to check the configuration */
			err = __mmc_read_and_compare_ext_csd(host, card);
			if (!err)
				return 0;

error:
			mmc_set_signal_voltage(host, old_voltage);
			/* if an error occured, revert to a safer bus mode */
			__mmc_switch(host, card, EXT_CSD_CMD_SET_NORMAL,
				     EXT_CSD_BUS_WIDTH, EXT_CSD_BUS_WIDTH_1);
			mmc_select_mode(card, MMC_LEGACY);
			mmc_set_bus_width(host, 1);
		}
	}

	DPRINTF("unable to select a mode\n");

	return VMM_ENOTSUPP;
}

static int __mmc_set_capacity(struct mmc_card *card, int part_num)
{
	switch (part_num) {
	case 0:
		card->capacity = card->capacity_user;
		break;
	case 1:
	case 2:
		card->capacity = card->capacity_boot;
		break;
	case 3:
		card->capacity = card->capacity_rpmb;
		break;
	case 4:
	case 5:
	case 6:
	case 7:
		card->capacity = card->capacity_gp[part_num - 4];
		break;
	default:
		return VMM_EINVALID;
	}

	return VMM_OK;
}

static int __mmc_startup_v4(struct mmc_host *host, struct mmc_card *card)
{
	int err, i;
	u64 capacity;
	bool has_parts = FALSE;
	bool part_completed;
	u8 *ext_csd = &card->ext_csd[0];
	static const u32 mmc_versions[] = {
		MMC_VERSION_4,
		MMC_VERSION_4_1,
		MMC_VERSION_4_2,
		MMC_VERSION_4_3,
		MMC_VERSION_4_4,
		MMC_VERSION_4_41,
		MMC_VERSION_4_5,
		MMC_VERSION_5_0,
		MMC_VERSION_5_1
	};

	if (IS_SD(card) || (card->version < MMC_VERSION_4)) {
		return 0;
	}

	memset(card->ext_csd, 0, sizeof(card->ext_csd));

	/* check ext_csd version and capacity */
	err = __mmc_send_ext_csd(host, ext_csd);
	if (err)
		return err;

	if (ext_csd[EXT_CSD_REV] >= array_size(mmc_versions))
		return VMM_EINVALID;

	card->version = mmc_versions[ext_csd[EXT_CSD_REV]];

	if (card->version >= MMC_VERSION_4_2) {
		/*
		 * According to the JEDEC Standard, the value of
		 * ext_csd's capacity is valid if the value is more
		 * than 2GB
		 */
		capacity = ext_csd[EXT_CSD_SEC_CNT] << 0
				| ext_csd[EXT_CSD_SEC_CNT + 1] << 8
				| ext_csd[EXT_CSD_SEC_CNT + 2] << 16
				| ext_csd[EXT_CSD_SEC_CNT + 3] << 24;
		capacity *= MMC_MAX_BLOCK_LEN;
		if ((capacity >> 20) > 2 * 1024)
			card->capacity_user = capacity;
	}

	/* The partition data may be non-zero but it is only
	 * effective if PARTITION_SETTING_COMPLETED is set in
	 * EXT_CSD, so ignore any data if this bit is not set,
	 * except for enabling the high-capacity group size
	 * definition (see below).
	 */
	part_completed = !!(ext_csd[EXT_CSD_PARTITION_SETTING] &
			    EXT_CSD_PARTITION_SETTING_COMPLETED);

	/* store the partition info of emmc */
	card->part_support = ext_csd[EXT_CSD_PARTITIONING_SUPPORT];
	if ((ext_csd[EXT_CSD_PARTITIONING_SUPPORT] & PART_SUPPORT) ||
	    ext_csd[EXT_CSD_BOOT_MULT])
		card->part_config = ext_csd[EXT_CSD_PART_CONF];
	if (part_completed &&
	    (ext_csd[EXT_CSD_PARTITIONING_SUPPORT] & ENHNCD_SUPPORT))
		card->part_attr = ext_csd[EXT_CSD_PARTITIONS_ATTRIBUTE];

	card->capacity_boot = ext_csd[EXT_CSD_BOOT_MULT] << 17;

	card->capacity_rpmb = ext_csd[EXT_CSD_RPMB_MULT] << 17;

	for (i = 0; i < 4; i++) {
		int idx = EXT_CSD_GP_SIZE_MULT + i * 3;
		u32 mult = (ext_csd[idx + 2] << 16) +
			(ext_csd[idx + 1] << 8) + ext_csd[idx];
		if (mult)
			has_parts = true;
		if (!part_completed)
			continue;
		card->capacity_gp[i] = mult;
		card->capacity_gp[i] *=
			ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE];
		card->capacity_gp[i] *= ext_csd[EXT_CSD_HC_WP_GRP_SIZE];
		card->capacity_gp[i] <<= 19;
	}

	if (part_completed) {
		card->enh_user_size =
			(ext_csd[EXT_CSD_ENH_SIZE_MULT + 2] << 16) +
			(ext_csd[EXT_CSD_ENH_SIZE_MULT + 1] << 8) +
			ext_csd[EXT_CSD_ENH_SIZE_MULT];
		card->enh_user_size *= ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE];
		card->enh_user_size *= ext_csd[EXT_CSD_HC_WP_GRP_SIZE];
		card->enh_user_size <<= 19;
		card->enh_user_start =
			(ext_csd[EXT_CSD_ENH_START_ADDR + 3] << 24) +
			(ext_csd[EXT_CSD_ENH_START_ADDR + 2] << 16) +
			(ext_csd[EXT_CSD_ENH_START_ADDR + 1] << 8) +
			ext_csd[EXT_CSD_ENH_START_ADDR];
		if (card->high_capacity)
			card->enh_user_start <<= 9;
	}

	/*
	 * Host needs to enable ERASE_GRP_DEF bit if device is
	 * partitioned. This bit will be lost every time after a reset
	 * or power off. This will affect erase size.
	 */
	if (part_completed)
		has_parts = TRUE;
	if ((ext_csd[EXT_CSD_PARTITIONING_SUPPORT] & PART_SUPPORT) &&
	    (ext_csd[EXT_CSD_PARTITIONS_ATTRIBUTE] & PART_ENH_ATTRIB))
		has_parts = TRUE;
	if (has_parts) {
		err = __mmc_switch(host, card, EXT_CSD_CMD_SET_NORMAL,
				 EXT_CSD_ERASE_GROUP_DEF, 1);
		if (err)
			return err;

		ext_csd[EXT_CSD_ERASE_GROUP_DEF] = 1;
	}

	if (ext_csd[EXT_CSD_ERASE_GROUP_DEF] & 0x01) {
		/* Read out group size from ext_csd */
		card->erase_grp_size =
			ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE] * 1024;
		/*
		 * if high capacity and partition setting completed
		 * SEC_COUNT is valid even if it is smaller than 2 GiB
		 * JEDEC Standard JESD84-B45, 6.2.4
		 */
		if (card->high_capacity && part_completed) {
			capacity = (ext_csd[EXT_CSD_SEC_CNT]) |
				(ext_csd[EXT_CSD_SEC_CNT + 1] << 8) |
				(ext_csd[EXT_CSD_SEC_CNT + 2] << 16) |
				(ext_csd[EXT_CSD_SEC_CNT + 3] << 24);
			capacity *= MMC_MAX_BLOCK_LEN;
			card->capacity_user = capacity;
		}
	} else {
		/* Calculate the group size from the csd value. */
		int erase_gsz, erase_gmul;

		erase_gsz = (card->csd[2] & 0x00007c00) >> 10;
		erase_gmul = (card->csd[2] & 0x000003e0) >> 5;
		card->erase_grp_size = (erase_gsz + 1) * (erase_gmul + 1);
	}
	card->hc_wp_grp_size = 1024 *
			       ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE] *
			       ext_csd[EXT_CSD_HC_WP_GRP_SIZE];

	card->wr_rel_set = ext_csd[EXT_CSD_WR_REL_SET];

	return 0;
}

static int __mmc_startup(struct mmc_host *host, struct mmc_card *card)
{
	int err, i;
	u32 mult, freq;
	u64 cmult, csize;
	struct mmc_cmd cmd;
	int timeout = 1000;

	if (!host || !card) {
		return VMM_EFAIL;
	}

#ifdef CONFIG_MMC_SPI_CRC_ON
	if (mmc_host_is_spi(host)) { /* enable CRC check for spi */
		cmd.cmdidx = MMC_CMD_SPI_CRC_ON_OFF;
		cmd.resp_type = MMC_RSP_R1;
		cmd.cmdarg = 1;
		err = mmc_send_cmd(host, &cmd, NULL);
		if (err) {
			return err;
		}
	}
#endif

	/* Put the Card in Identify Mode */
	cmd.cmdidx = mmc_host_is_spi(host) ? MMC_CMD_SEND_CID :
		MMC_CMD_ALL_SEND_CID; /* cmd not supported in spi */
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = 0;

	err = mmc_send_cmd(host, &cmd, NULL);
	if (err) {
		return err;
	}
	memcpy(card->cid, cmd.response, 16);

	/*
	 * For MMC cards, set the Relative Address.
	 * For SD cards, get the Relatvie Address.
	 * This also puts the cards into Standby State
	 */
	if (!mmc_host_is_spi(host)) { /* cmd not supported in spi */
		cmd.cmdidx = SD_CMD_SEND_RELATIVE_ADDR;
		cmd.cmdarg = card->rca << 16;
		cmd.resp_type = MMC_RSP_R6;
		err = mmc_send_cmd(host, &cmd, NULL);
		if (err) {
			return err;
		}
		if (IS_SD(card)) {
			card->rca = (cmd.response[0] >> 16) & 0xffff;
		}
	}

	/* Get the card-specific data */
	cmd.cmdidx = MMC_CMD_SEND_CSD;
	cmd.resp_type = MMC_RSP_R2;
	cmd.cmdarg = card->rca << 16;
	err = mmc_send_cmd(host, &cmd, NULL);
	if (!err) {
		err = mmc_send_status(host, card, timeout);
	}
	if (err) {
		return err;
	}

	/* Save card-specific data */
	card->csd[0] = cmd.response[0];
	card->csd[1] = cmd.response[1];
	card->csd[2] = cmd.response[2];
	card->csd[3] = cmd.response[3];

	if (card->version == MMC_VERSION_UNKNOWN) {
		int version = (cmd.response[0] >> 26) & 0xf;

		switch (version) {
		case 0:
			card->version = MMC_VERSION_1_2;
			break;
		case 1:
			card->version = MMC_VERSION_1_4;
			break;
		case 2:
			card->version = MMC_VERSION_2_2;
			break;
		case 3:
			card->version = MMC_VERSION_3;
			break;
		case 4:
			card->version = MMC_VERSION_4;
			break;
		default:
			card->version = MMC_VERSION_1_2;
			break;
		};
	}

	/* Determine card parameters */
	freq = fbase[(cmd.response[0] & 0x7)];
	mult = multipliers[((cmd.response[0] >> 3) & 0xf)];

	card->legacy_speed = freq * mult;
	mmc_select_mode(card, MMC_LEGACY);

	card->dsr_imp = ((cmd.response[1] >> 12) & 0x1);
	card->read_bl_len = 1 << ((cmd.response[1] >> 16) & 0xf);
	if (IS_SD(card)) {
		card->write_bl_len = card->read_bl_len;
	} else {
		card->write_bl_len = 1 << ((cmd.response[3] >> 22) & 0xf);
	}
	if (card->high_capacity) {
		csize = (card->csd[1] & 0x3f) << 16
			| (card->csd[2] & 0xffff0000) >> 16;
		cmult = 8;
	} else {
		csize = (card->csd[1] & 0x3ff) << 2
			| (card->csd[2] & 0xc0000000) >> 30;
		cmult = (card->csd[2] & 0x00038000) >> 15;
	}
	card->capacity_user = (csize + 1) << (cmult + 2);
	card->capacity_user *= card->read_bl_len;
	card->capacity_boot = 0;
	card->capacity_rpmb = 0;
	for (i = 0; i < 4; i++) {
		card->capacity_gp[i] = 0;
	}
	if (card->read_bl_len > 512) {
		card->read_bl_len = 512;
	}
	if (card->write_bl_len > 512) {
		card->write_bl_len = 512;
	}

	if ((card->dsr_imp) && (0xffffffff != card->dsr)) {
		cmd.cmdidx = MMC_CMD_SET_DSR;
		cmd.cmdarg = (card->dsr & 0xffff) << 16;
		cmd.resp_type = MMC_RSP_NONE;
		if (mmc_send_cmd(host, &cmd, NULL))
			vmm_printf("MMC: SET_DSR failed\n");
	}

	/* Select the card, and put it into Transfer Mode */
	if (!mmc_host_is_spi(host)) { /* cmd not supported in spi */
		cmd.cmdidx = MMC_CMD_SELECT_CARD;
		cmd.resp_type = MMC_RSP_R1;
		cmd.cmdarg = card->rca << 16;
		err = mmc_send_cmd(host, &cmd, NULL);
		if (err) {
			return err;
		}
	}

	/* For SD, its erase group is always one sector */
	card->erase_grp_size = 1;
	card->part_config = MMCPART_NOAVAILABLE;

	/* Startup MMCv4 card */
	err = __mmc_startup_v4(host, card);
	if (err) {
		return err;
	}

	/* Set card capacity based on current partition */
	err = __mmc_set_capacity(card, card->part_num);
	if (err) {
		return err;
	}

	/* Change card frequency and update capablities */
	if (IS_SD(card)) {
		err = __sd_get_capabilities(host, card);
		if (err) {
			return err;
		}
		err = __sd_select_mode_and_width(host, card);
	} else {
		err = __mmc_get_capabilities(host, card);
		if (err) {
			return err;
		}
		err = __mmc_select_mode_and_width(host, card);
	}
	if (err) {
		return err;
	}

	card->best_mode = card->selected_mode;

	/* Fix the block length for DDR mode */
	if (card->ddr_mode) {
		card->read_bl_len = 512;
		card->write_bl_len = 512;
	}

	return VMM_OK;
}

static int __sd_send_op_cond(struct mmc_host *host, struct mmc_card *card)
{
	int err;
	int timeout = 10;
	struct mmc_cmd cmd;

	do {
		cmd.cmdidx = MMC_CMD_APP_CMD;
		cmd.resp_type = MMC_RSP_R1;
		cmd.cmdarg = 0;

		err = mmc_send_cmd(host, &cmd, NULL);
		if (err) {
			return err;
		}

		cmd.cmdidx = SD_CMD_APP_SEND_OP_COND;
		cmd.resp_type = MMC_RSP_R3;
		cmd.response[0] = 0;

		/*
		 * Most cards do not answer if some reserved bits
		 * in the ocr are set. However, Some controller
		 * can set bit 7 (reserved for low voltages), but
		 * how to manage low voltages SD card is not yet
		 * specified.
		 */
		cmd.cmdarg = mmc_host_is_spi(host) ? 0 :
			(host->voltages & 0xff8000);

		if (card->version == SD_VERSION_2) {
			cmd.cmdarg |= OCR_HCS;
		}

		err = mmc_send_cmd(host, &cmd, NULL);
		if (err) {
			return err;
		}

		/* If card is powered-up then check whether
		 * it has any valid voltages as-per SD spec
		 */
		if (!mmc_host_is_spi(host) &&
		    (cmd.response[0] & OCR_BUSY) &&
		    !(cmd.response[0] & OCR_VOLTAGE_MASK)) {
			/* No valid voltages hence this is not a SD card */
			return VMM_ENODEV;
		}

		vmm_udelay(10000);
	} while ((!(cmd.response[0] & OCR_BUSY)) && timeout--);

	if (timeout <= 0) {
		return VMM_ETIMEDOUT;
	}

	if (card->version != SD_VERSION_2) {
		card->version = SD_VERSION_1_0;
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

	card->ocr = cmd.response[0];
	card->high_capacity = ((card->ocr & OCR_HCS) == OCR_HCS);
	card->rca = 0;

	return VMM_OK;
}

int mmc_send_op_cond(struct mmc_host *host, struct mmc_card *card)
{
	int err;
	int timeout = 10;
	struct mmc_cmd cmd;

	/* Some cards seem to need this */
	mmc_go_idle(host);

 	/* Asking to the card its capabilities */
 	cmd.cmdidx = MMC_CMD_SEND_OP_COND;
 	cmd.resp_type = MMC_RSP_R3;
 	cmd.cmdarg = 0;
	cmd.response[0] = 0;

 	err = mmc_send_cmd(host, &cmd, NULL);
 	if (err) {
 		return err;
	}

	vmm_udelay(1000);

	do {
		cmd.cmdidx = MMC_CMD_SEND_OP_COND;
		cmd.resp_type = MMC_RSP_R3;
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

int mmc_send_if_cond(struct mmc_host *host, struct mmc_card *card)
{
	int err;
	struct mmc_cmd cmd;

	cmd.cmdidx = SD_CMD_SEND_IF_COND;
	/* We set the bit if the host supports voltages between 2.7 and 3.6 V */
	cmd.cmdarg = ((host->voltages & 0xff8000) != 0) << 8 | 0xaa;
	cmd.resp_type = MMC_RSP_R7;

	err = mmc_send_cmd(host, &cmd, NULL);
	if (err) {
		return err;
	}

	if ((cmd.response[0] & 0xff) != 0xaa) {
		return VMM_EIO;
	} else {
		card->version = SD_VERSION_2;
	}

	return VMM_OK;
}

int __mmc_sd_attach(struct mmc_host *host)
{
	int i, rc = VMM_OK;
	char str[16];
	struct mmc_card *card;
	struct vmm_blockdev *bdev;

	if (!host) {
		return VMM_EFAIL;
	}

	/* If mmc card instance available then do nothing */
	if (host->card) {
		rc = VMM_OK;
		goto detect_done;
	}

	/* Allocate new mmc card instance */
	host->card = vmm_zalloc(sizeof(struct mmc_card));
	if (!host->card) {
		rc = VMM_ENOMEM;
		goto detect_done;
	}
	card = host->card;
	card->version = MMC_VERSION_UNKNOWN;
	card->legacy_speed = host->f_min;

	/* Attempt to detect mmc card */
	if (!mmc_getcd(host)) {
		rc = VMM_ENOTAVAIL;
		goto detect_freecard_fail;
	}

	/* Do mmc host specific mmc card init */
	rc = mmc_init_card(host, host->card);
	if (rc) {
		goto detect_freecard_fail;
	}

	/* Set initial mmc host and mmc card state */
	mmc_select_mode(host->card, MMC_LEGACY);
	mmc_set_initial_state(host);

	/* Reset card */
	rc = mmc_go_idle(host);
	if (rc) {
		goto detect_freecard_fail;
	}

	/* The internal partition reset to user partition(0) at every CMD0 */
	host->card->part_num = 0;

	/* Test for SD version 2 */
	rc = mmc_send_if_cond(host, host->card);

	/* Now try to get the SD card's operating condition */
	rc = __sd_send_op_cond(host, host->card);

	/* If the command timed out, we check for MMC card */
	if ((rc == VMM_ETIMEDOUT) || (rc == VMM_ENODEV)) {
		rc = mmc_send_op_cond(host, host->card);
		if (rc) {
			goto detect_freecard_fail;
		}
	} else if (rc) {
		goto detect_freecard_fail;
	}

	/* Startup mmc/sd card */
	rc = __mmc_startup(host, host->card);
	if (rc) {
		goto detect_freecard_fail;
	}

	/* Allocate new block device instance */
	card->bdev = vmm_blockdev_alloc();
	if (!card->bdev) {
		rc = VMM_ENOMEM;
		goto detect_freecard_fail;
	}
	bdev = card->bdev;

	/* Setup block device instance */
	memset(str, 0, sizeof(str));
	str[0] = (card->cid[0] & 0xff);
	str[1] = ((card->cid[1] >> 24) & 0xff);
	str[2] = ((card->cid[1] >> 16) & 0xff);
	str[3] = ((card->cid[1] >> 8) & 0xff);
	str[4] = (card->cid[1] & 0xff);
	str[5] = ((card->cid[2] >> 24) & 0xff);
	for (i = 0; i < 6; i++) {
		if (!vmm_isprintable(str[i]))
			str[i] = '\0';
	}
	vmm_snprintf(bdev->name, sizeof(bdev->name), "mmc%d", host->host_num);
	vmm_snprintf(bdev->desc, sizeof(bdev->desc),
		     "%s-%d.%d Manufacturer=%06x Serial=%04x%04x "
		     "Product=%c%c%c%c%c%c Rev=%d.%d",
		     IS_SD(host->card) ? "SD" : "MMC",
		     EXTRACT_SDMMC_MAJOR_VERSION(host->card->version),
		     EXTRACT_SDMMC_MINOR_VERSION(host->card->version),
		     (card->cid[0] >> 24),
		     (card->cid[2] & 0xffff),
		     ((card->cid[3] >> 16) & 0xffff),
		     str[0], str[1], str[2], str[3], str[4], str[5],
		     ((card->cid[2] >> 20) & 0xf),
		     ((card->cid[2] >> 16) & 0xf));
	bdev->dev.parent = host->dev;
	bdev->flags = VMM_BLOCKDEV_RW;
	if (card->read_bl_len < card->write_bl_len) {
		bdev->block_size = card->write_bl_len;
	} else {
		bdev->block_size = card->read_bl_len;
	}
	bdev->start_lba = 0;
	bdev->num_blocks = udiv64(card->capacity, bdev->block_size);

	/* Setup request queue for block device instance */
	bdev->rq = vmm_blockrq_to_rq(host->brq);

	/* Register block device instance */
	rc = vmm_blockdev_register(card->bdev);
	if (rc) {
		goto detect_freebdev_fail;
	}

	/* Print banner */
	vmm_linfo(bdev->name, "using %s mode and %dbit bus-width\n",
		  host->card->mode_name, mmc_bus_width(host->card->caps));
	vmm_linfo(bdev->name, "capacity %"PRIu64" blocks of %d bytes\n",
		  bdev->num_blocks, bdev->block_size);
	vmm_linfo(bdev->name, "%s\n", bdev->desc);

	rc = VMM_OK;
	goto detect_done;

detect_freebdev_fail:
	vmm_blockdev_free(host->card->bdev);
detect_freecard_fail:
	vmm_free(host->card);
	host->card = NULL;
detect_done:
	return rc;
}
