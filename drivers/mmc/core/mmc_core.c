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
 * @file mmc_core.c
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
#include <vmm_heap.h>
#include <vmm_delay.h>
#include <vmm_timer.h>
#include <vmm_host_io.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <drv/mmc/mmc_core.h>

#define MODULE_DESC			"MMC/SD/SDIO Framework"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		MMC_CORE_IPRIORITY
#define	MODULE_INIT			mmc_core_init
#define	MODULE_EXIT			mmc_core_exit

/*
 *  Debugging related defines
 */
#undef CONFIG_MMC_TRACE

/* 
 * Set block count limit because of 16 bit register limit on some hardware
 */
#ifndef CONFIG_SYS_MMC_MAX_BLK_COUNT
#define CONFIG_SYS_MMC_MAX_BLK_COUNT 65535
#endif

/*
 * Protected list of mmc hosts.
 */
static DEFINE_MUTEX(mmc_host_list_mutex);
static LIST_HEAD(mmc_host_list);
static u32 mmc_host_count;

/*
 * IO types for mmc hosts.
 */
enum mmc_host_io_type {
	MMC_HOST_IO_DETECT_CARD_CHANGE=1,
	MMC_HOST_IO_BLOCKDEV_REQUEST=2
};

/*
 * IO instance for mmc hosts.
 */
struct mmc_host_io {
	struct dlist head;
	enum mmc_host_io_type type;
	struct vmm_request *r;
	struct vmm_request_queue *rq;
	u64 card_change_tstamp;
};

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

static void __mmc_set_ios(struct mmc_host *host)
{
	if (host->ops.set_ios) {
		host->ops.set_ios(host, &host->ios);
	}
}

static void __mmc_set_clock(struct mmc_host *host, u32 clock)
{
	if (clock > host->f_max)
		clock = host->f_max;

	if (clock < host->f_min)
		clock = host->f_min;

	host->ios.clock = clock;

	__mmc_set_ios(host);
}

static void __mmc_set_bus_width(struct mmc_host *host, u32 width)
{
	host->ios.bus_width = width;

	__mmc_set_ios(host);
}

static int __mmc_init_card(struct mmc_host *host, struct mmc_card *card)
{
	if (host->ops.init_card) {
		return host->ops.init_card(host, card);
	}

	return VMM_OK;
}

static int __mmc_getcd(struct mmc_host *host)
{
	if (host->ops.get_cd) {
		return host->ops.get_cd(host);
	}

	return 1;
}

static int __mmc_send_cmd(struct mmc_host *host, 
			  struct mmc_cmd *cmd,
			  struct mmc_data *data)
{
	int ret;
	struct mmc_data backup;

	if (!host->ops.send_cmd) {
		return VMM_EFAIL;
	}

	memset(&backup, 0, sizeof(backup));

#ifdef CONFIG_MMC_TRACE
	int i;
	u8 *ptr;

	vmm_printf("CMD_SEND:%d\n", cmd->cmdidx);
	vmm_printf("\t\tARG\t\t\t 0x%08X\n", cmd->cmdarg);
	ret = host->ops.send_cmd(host, cmd, data);
	switch (cmd->resp_type) {
		case MMC_RSP_NONE:
			vmm_printf("\t\tMMC_RSP_NONE\n");
			break;
		case MMC_RSP_R1:
			vmm_printf("\t\tMMC_RSP_R1,5,6,7 \t 0x%08X \n",
				cmd->response[0]);
			break;
		case MMC_RSP_R1b:
			vmm_printf("\t\tMMC_RSP_R1b\t\t 0x%08X \n",
				cmd->response[0]);
			break;
		case MMC_RSP_R2:
			vmm_printf("\t\tMMC_RSP_R2\t\t 0x%08X \n",
				cmd->response[0]);
			vmm_printf("\t\t          \t\t 0x%08X \n",
				cmd->response[1]);
			vmm_printf("\t\t          \t\t 0x%08X \n",
				cmd->response[2]);
			vmm_printf("\t\t          \t\t 0x%08X \n",
				cmd->response[3]);
			vmm_printf("\n");
			vmm_printf("\t\t\t\t\tDUMPING DATA\n");
			for (i = 0; i < 4; i++) {
				int j;
				vmm_printf("\t\t\t\t\t%03d - ", i*4);
				ptr = (u8 *)&cmd->response[i];
				ptr += 3;
				for (j = 0; j < 4; j++)
					vmm_printf("%02X ", *ptr--);
				vmm_printf("\n");
			}
			break;
		case MMC_RSP_R3:
			vmm_printf("\t\tMMC_RSP_R3,4\t\t 0x%08X \n",
				cmd->response[0]);
			break;
		default:
			vmm_printf("\t\tERROR MMC rsp not supported\n");
			break;
	}
	vmm_printf("CMD_RET:%d\n", ret);
#else
	ret = host->ops.send_cmd(host, cmd, data);
#endif
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

	data.dest = (char *)ext_csd;
	data.blocks = 1;
	data.blocksize = 512;
	data.flags = MMC_DATA_READ;

	return __mmc_send_cmd(host, &cmd, &data);
}

static int __mmc_send_status(struct mmc_host *host, 
			     struct mmc_card *card, 
			     int timeout)
{
	struct mmc_cmd cmd;
	int err, retries = 5;
#ifdef CONFIG_MMC_TRACE
	u32 status;
#endif

	cmd.cmdidx = MMC_CMD_SEND_STATUS;
	cmd.resp_type = MMC_RSP_R1;
	if (!mmc_host_is_spi(host)) {
		cmd.cmdarg = card->rca << 16;
	}

	do {
		err = __mmc_send_cmd(host, &cmd, NULL);
		if (!err) {
			if ((cmd.response[0] & MMC_STATUS_RDY_FOR_DATA) &&
			    (cmd.response[0] & MMC_STATUS_CURR_STATE) !=
			     MMC_STATE_PRG) {
				break;
			} else if (cmd.response[0] & MMC_STATUS_MASK) {
				vmm_printf("Status Error: 0x%08X\n",
					cmd.response[0]);
				return VMM_EFAIL;
			}
		} else if (--retries < 0) {
			return err;
		}

		vmm_udelay(1000);
	} while (timeout--);

#ifdef CONFIG_MMC_TRACE
	status = (cmd.response[0] & MMC_STATUS_CURR_STATE) >> 9;
	vmm_printf("CURR STATE:%d\n", status);
#endif

	if (timeout <= 0) {
		vmm_printf("Timeout waiting card ready\n");
		return VMM_ETIMEDOUT;
	}

	return VMM_OK;
}

static int __mmc_set_blocklen(struct mmc_host *host, int len)
{
	struct mmc_cmd cmd;

	cmd.cmdidx = MMC_CMD_SET_BLOCKLEN;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = len;

	return __mmc_send_cmd(host, &cmd, NULL);
}

static int __mmc_go_idle(struct mmc_host *host)
{
	int err;
	struct mmc_cmd cmd;

	vmm_udelay(1000);

	cmd.cmdidx = MMC_CMD_GO_IDLE_STATE;
	cmd.cmdarg = 0;
	cmd.resp_type = MMC_RSP_NONE;

	err = __mmc_send_cmd(host, &cmd, NULL);
	if (err) {
		return err;
	}

	vmm_udelay(2000);

	return VMM_OK;
}

static int __mmc_send_if_cond(struct mmc_host *host, struct mmc_card *card)
{
	int err;
	struct mmc_cmd cmd;

	cmd.cmdidx = SD_CMD_SEND_IF_COND;
	/* We set the bit if the host supports voltages between 2.7 and 3.6 V */
	cmd.cmdarg = ((host->voltages & 0xff8000) != 0) << 8 | 0xaa;
	cmd.resp_type = MMC_RSP_R7;

	err = __mmc_send_cmd(host, &cmd, NULL);
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

static int __sd_send_op_cond(struct mmc_host *host, struct mmc_card *card)
{
	int err;
	int timeout = 1000;
	struct mmc_cmd cmd;

	do {
		cmd.cmdidx = MMC_CMD_APP_CMD;
		cmd.resp_type = MMC_RSP_R1;
		cmd.cmdarg = 0;

		err = __mmc_send_cmd(host, &cmd, NULL);
		if (err) {
			return err;
		}

		cmd.cmdidx = SD_CMD_APP_SEND_OP_COND;
		cmd.resp_type = MMC_RSP_R3;

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

		err = __mmc_send_cmd(host, &cmd, NULL);
		if (err) {
			return err;
		}

		vmm_udelay(1000);
	} while ((!(cmd.response[0] & OCR_BUSY)) && timeout--);

	if (timeout <= 0) {
		return VMM_EIO;
	}

	if (card->version != SD_VERSION_2) {
		card->version = SD_VERSION_1_0;
	}

	if (mmc_host_is_spi(host)) { /* read OCR for spi */
		cmd.cmdidx = MMC_CMD_SPI_READ_OCR;
		cmd.resp_type = MMC_RSP_R3;
		cmd.cmdarg = 0;

		err = __mmc_send_cmd(host, &cmd, NULL);
		if (err) {
			return err;
		}
	}

	card->ocr = cmd.response[0];
	card->high_capacity = ((card->ocr & OCR_HCS) == OCR_HCS);
	card->rca = 0;

	return VMM_OK;
}

static int __mmc_send_op_cond(struct mmc_host *host, struct mmc_card *card)
{
	int err;
	int timeout = 10000;
	struct mmc_cmd cmd;

	/* Some cards seem to need this */
	__mmc_go_idle(host);

 	/* Asking to the card its capabilities */
 	cmd.cmdidx = MMC_CMD_SEND_OP_COND;
 	cmd.resp_type = MMC_RSP_R3;
 	cmd.cmdarg = 0;

 	err = __mmc_send_cmd(host, &cmd, NULL);
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

		err = __mmc_send_cmd(host, &cmd, NULL);
		if (err) {
			return err;
		}

		vmm_udelay(1000);
	} while (!(cmd.response[0] & OCR_BUSY) && timeout--);

	if (timeout <= 0) {
		return VMM_EIO;
	}

	if (mmc_host_is_spi(host)) { /* read OCR for spi */
		cmd.cmdidx = MMC_CMD_SPI_READ_OCR;
		cmd.resp_type = MMC_RSP_R3;
		cmd.cmdarg = 0;

		err = __mmc_send_cmd(host, &cmd, NULL);
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

	data.dest = (char *)resp;
	data.blocksize = 64;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;

	return __mmc_send_cmd(host, &cmd, &data);
}

static int __sd_change_freq(struct mmc_host *host, struct mmc_card *card)
{
	int err;
	struct mmc_cmd cmd;
	u32 scr[2];
	u32 switch_status[16];
	struct mmc_data data;
	int timeout;

	card->caps = 0;

	if (mmc_host_is_spi(host)) {
		return VMM_OK;
	}

	/* Read the SCR to find out if this card supports higher speeds */
	cmd.cmdidx = MMC_CMD_APP_CMD;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = card->rca << 16;

	err = __mmc_send_cmd(host, &cmd, NULL);
	if (err) {
		return err;
	}

	cmd.cmdidx = SD_CMD_APP_SEND_SCR;
	cmd.resp_type = MMC_RSP_R1;
	cmd.cmdarg = 0;

	timeout = 3;

retry_scr:
	data.dest = (char *)scr;
	data.blocksize = 8;
	data.blocks = 1;
	data.flags = MMC_DATA_READ;

	err = __mmc_send_cmd(host, &cmd, &data);
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
	if (!(vmm_be32_to_cpu(switch_status[3]) & SD_HIGHSPEED_SUPPORTED)) {
		return VMM_OK;
	}

	/*
	 * If the host doesn't support SD_HIGHSPEED, do not switch card to
	 * HIGHSPEED mode even if the card support SD_HIGHSPPED.
	 * This can avoid furthur problem when the card runs in different
	 * mode between the host.
	 */
	if (!((host->caps & MMC_CAP_MODE_HS_52MHz) &&
		(host->caps & MMC_CAP_MODE_HS))) {
		return VMM_OK;
	}

	err = __sd_switch(host, SD_SWITCH_SWITCH, 0, 1, (u8 *)switch_status);
	if (err) {
		return err;
	}

	if ((vmm_be32_to_cpu(switch_status[4]) & 0x0f000000) == 0x01000000) {
		card->caps |= MMC_CAP_MODE_HS;
	}

	return VMM_OK;
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

	ret = __mmc_send_cmd(host, &cmd, NULL);

	/* Waiting for the ready status */
	if (!ret) {
		ret = __mmc_send_status(host, card, timeout);
	}

	return ret;

}

static int __mmc_change_freq(struct mmc_host *host, struct mmc_card *card)
{
	u8 ext_csd[512];
	char cardtype;
	int err;

	card->caps = 0;

	if (mmc_host_is_spi(host)) {
		return VMM_OK;
	}

	/* Only version 4 supports high-speed */
	if (card->version < MMC_VERSION_4) {
		return VMM_OK;
	}

	err = __mmc_send_ext_csd(host, ext_csd);
	if (err) {
		return err;
	}

	cardtype = ext_csd[EXT_CSD_CARD_TYPE] & 0xf;

	err = __mmc_switch(host, card, 
			   EXT_CSD_CMD_SET_NORMAL, EXT_CSD_HS_TIMING, 1);
	if (err) {
		return err;
	}

	/* Now check to see that it worked */
	err = __mmc_send_ext_csd(host, ext_csd);
	if (err) {
		return err;
	}

	/* No high-speed support */
	if (!ext_csd[EXT_CSD_HS_TIMING]) {
		return VMM_OK;
	}

	/* High Speed is set, there are two types: 52MHz and 26MHz */
	if (cardtype & MMC_HS_52MHZ) {
		card->caps |= MMC_CAP_MODE_HS_52MHz | MMC_CAP_MODE_HS;
	} else {
		card->caps |= MMC_CAP_MODE_HS;
	}

	return VMM_OK;
}

static int __mmc_startup(struct mmc_host *host, struct mmc_card *card)
{
	int err;
	u32 mult, freq;
	u64 cmult, csize, capacity;
	struct mmc_cmd cmd;
	u8 ext_csd[512];
	u8 test_csd[512];
	int timeout = 1000;

	if (!host || !card) {
		return VMM_EFAIL;
	}

	memset(ext_csd, 0, sizeof(ext_csd));
	memset(test_csd, 0, sizeof(test_csd));

#ifdef CONFIG_MMC_SPI_CRC_ON
	if (mmc_host_is_spi(host)) { /* enable CRC check for spi */
		cmd.cmdidx = MMC_CMD_SPI_CRC_ON_OFF;
		cmd.resp_type = MMC_RSP_R1;
		cmd.cmdarg = 1;
		err = __mmc_send_cmd(host, &cmd, NULL);
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

	err = __mmc_send_cmd(host, &cmd, NULL);
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
		err = __mmc_send_cmd(host, &cmd, NULL);
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
	err = __mmc_send_cmd(host, &cmd, NULL);
	if (!err) {
		err = __mmc_send_status(host, card, timeout);
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
		}
	}

	/* Determine card parameters */
	freq = fbase[(cmd.response[0] & 0x7)];
	mult = multipliers[((cmd.response[0] >> 3) & 0xf)];
	card->tran_speed = freq * mult;
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
	card->capacity = (csize + 1) << (cmult + 2);
	card->capacity *= card->read_bl_len;
	if (card->read_bl_len > 512) {
		card->read_bl_len = 512;
	}
	if (card->write_bl_len > 512) {
		card->write_bl_len = 512;
	}

	/* Select the card, and put it into Transfer Mode */
	if (!mmc_host_is_spi(host)) { /* cmd not supported in spi */
		cmd.cmdidx = MMC_CMD_SELECT_CARD;
		cmd.resp_type = MMC_RSP_R1;
		cmd.cmdarg = card->rca << 16;
		err = __mmc_send_cmd(host, &cmd, NULL);
		if (err) {
			return err;
		}
	}

	/*
	 * For SD, its erase group is always one sector
	 */
	card->erase_grp_size = 1;
	card->part_config = MMCPART_NOAVAILABLE;
	if (!IS_SD(card) && (card->version >= MMC_VERSION_4)) {
		/* check  ext_csd version and capacity */
		err = __mmc_send_ext_csd(host, ext_csd);
		if (!err && (ext_csd[EXT_CSD_REV] >= 2)) {
			/*
			 * According to the JEDEC Standard, the value of
			 * ext_csd's capacity is valid if the value is more
			 * than 2GB
			 */
			capacity = ext_csd[EXT_CSD_SEC_CNT] << 0
					| ext_csd[EXT_CSD_SEC_CNT + 1] << 8
					| ext_csd[EXT_CSD_SEC_CNT + 2] << 16
					| (u64)(ext_csd[EXT_CSD_SEC_CNT + 3]) << 24;
			capacity *= 512;
			if ((capacity >> 20) > 2 * 1024) {
				card->capacity = capacity;
			}
		}

		switch (ext_csd[EXT_CSD_REV]) {
		case 1:
			card->version = MMC_VERSION_4_1;
			break;
		case 2:
			card->version = MMC_VERSION_4_2;
			break;
		case 3:
			card->version = MMC_VERSION_4_3;
			break;
		case 5:
			card->version = MMC_VERSION_4_41;
			break;
		case 6:
			card->version = MMC_VERSION_4_5;
			break;
		}

		/*
		 * Check whether GROUP_DEF is set, if yes, read out
		 * group size from ext_csd directly, or calculate
		 * the group size from the csd value.
		 */
		if (ext_csd[EXT_CSD_ERASE_GROUP_DEF]) {
			card->erase_grp_size =
			      ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE] * 512 * 1024;
		} else {
			int erase_gsz, erase_gmul;
			erase_gsz = (card->csd[2] & 0x00007c00) >> 10;
			erase_gmul = (card->csd[2] & 0x000003e0) >> 5;
			card->erase_grp_size = 
					(erase_gsz + 1) * (erase_gmul + 1);
		}

		/* store the partition info of emmc */
		if ((ext_csd[EXT_CSD_PARTITIONING_SUPPORT] & PART_SUPPORT) ||
		    ext_csd[EXT_CSD_BOOT_MULT]) {
			card->part_config = ext_csd[EXT_CSD_PART_CONF];
		}
	}

	if (IS_SD(card)) {
		err = __sd_change_freq(host, card);
	} else {
		err = __mmc_change_freq(host, card);
	}

	if (err) {
		return err;
	}

	/* Restrict card's capabilities by what the host can do */
	card->caps &= host->caps;

	if (IS_SD(card)) {
		if (card->caps & MMC_CAP_MODE_4BIT) {
			cmd.cmdidx = MMC_CMD_APP_CMD;
			cmd.resp_type = MMC_RSP_R1;
			cmd.cmdarg = card->rca << 16;

			err = __mmc_send_cmd(host, &cmd, NULL);
			if (err)
				return err;

			cmd.cmdidx = SD_CMD_APP_SET_BUS_WIDTH;
			cmd.resp_type = MMC_RSP_R1;
			cmd.cmdarg = 2;
			err = __mmc_send_cmd(host, &cmd, NULL);
			if (err) {
				return err;
			}

			__mmc_set_bus_width(host, 4);
		}

		if (card->caps & MMC_CAP_MODE_HS) {
			card->tran_speed = 50000000;
		} else {
			card->tran_speed = 25000000;
		}
	} else {
		int idx;

		/* An array of possible bus widths in order of preference */
		static unsigned ext_csd_bits[] = {
			EXT_CSD_BUS_WIDTH_8,
			EXT_CSD_BUS_WIDTH_4,
			EXT_CSD_BUS_WIDTH_1,
		};

		/* An array to map CSD bus widths to host cap bits */
		static unsigned ext_to_hostcaps[] = {
			[EXT_CSD_BUS_WIDTH_4] = MMC_CAP_MODE_4BIT,
			[EXT_CSD_BUS_WIDTH_8] = MMC_CAP_MODE_8BIT,
		};

		/* An array to map chosen bus width to an integer */
		static unsigned widths[] = {
			8, 4, 1,
		};

		for (idx=0; idx < array_size(ext_csd_bits); idx++) {
			unsigned int extw = ext_csd_bits[idx];

			/*
			 * Check to make sure the controller supports
			 * this bus width, if it's more than 1
			 */
			if (extw != EXT_CSD_BUS_WIDTH_1 &&
			    !(host->caps & ext_to_hostcaps[extw])) {
				continue;
			}

			err = __mmc_switch(host, card, EXT_CSD_CMD_SET_NORMAL,
					EXT_CSD_BUS_WIDTH, extw);
			if (err) {
				continue;
			}

			__mmc_set_bus_width(host, widths[idx]);

			err = __mmc_send_ext_csd(host, test_csd);
			if (!err && ext_csd[EXT_CSD_PARTITIONING_SUPPORT] \
				    == test_csd[EXT_CSD_PARTITIONING_SUPPORT]
				 && ext_csd[EXT_CSD_ERASE_GROUP_DEF] \
				    == test_csd[EXT_CSD_ERASE_GROUP_DEF] \
				 && ext_csd[EXT_CSD_REV] \
				    == test_csd[EXT_CSD_REV]
				 && ext_csd[EXT_CSD_HC_ERASE_GRP_SIZE] \
				    == test_csd[EXT_CSD_HC_ERASE_GRP_SIZE]
				 && memcmp(&ext_csd[EXT_CSD_SEC_CNT], \
					&test_csd[EXT_CSD_SEC_CNT], 4) == 0) {

				card->caps |= ext_to_hostcaps[extw];
				break;
			}
		}

		if (card->caps & MMC_CAP_MODE_HS) {
			if (card->caps & MMC_CAP_MODE_HS_52MHz) {
				card->tran_speed = 52000000;
			} else {
				card->tran_speed = 26000000;
			}
		}
	}

	__mmc_set_clock(host, card->tran_speed);

	return VMM_OK;
}

static int __mmc_detect_card_removed(struct mmc_host *host)
{
	int rc = VMM_OK;

	if (!host) {
		return VMM_EFAIL;
	}

	if (!host->card) {
		rc = VMM_OK;
		goto unplug_done;
	}

	/* FIXME: Need to wait for pending IO on mmc card */

	vmm_blockdev_unregister(host->card->bdev);
	vmm_blockdev_free(host->card->bdev);

	vmm_free(host->card);
	host->card = NULL;

unplug_done:
	return rc;
}

static int mmc_make_request(struct vmm_request_queue *rq, 
			    struct vmm_request *r);
static int mmc_abort_request(struct vmm_request_queue *rq, 
			     struct vmm_request *r);

static int __mmc_detect_card_inserted(struct mmc_host *host)
{
	int rc = VMM_OK;
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

	/* Attempt to detect mmc card */
	if (!__mmc_getcd(host)) {
		rc = VMM_ENOTAVAIL;
		goto detect_freecard_fail;
	}

	/* Do mmc host specific mmc card init */
	rc = __mmc_init_card(host, host->card);
	if (rc) {
		goto detect_freecard_fail;
	}

	/* Set minimum bus_width and minimum clock */
	__mmc_set_bus_width(host, 1);
	__mmc_set_clock(host, 1);

	/* Reset mmc card */
	rc = __mmc_go_idle(host);
	if (rc) {
		goto detect_freecard_fail;
	}

	/* The internal partition reset to user partition(0) at every CMD0 */
	host->card->part_num = 0;

	/* Test for SD version 2 */
	rc = __mmc_send_if_cond(host, host->card);
	if (rc) {
		goto detect_freecard_fail;
	}

	/* Now try to get the SD card's operating condition */
	rc = __sd_send_op_cond(host, host->card);

	/* If the command timed out, we check for an MMC card */
	if (rc == VMM_ETIMEDOUT) {
		rc = __mmc_send_op_cond(host, host->card);
		if (rc) {
			vmm_printf("%s: No response to voltage select!\n", 
				   __func__);
			goto detect_freecard_fail;
		}
	} else if (rc) {
		goto detect_freecard_fail;
	}

	/* Startup mmc card */
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
	vmm_snprintf(bdev->name, sizeof(bdev->name), "mmc%d", host->host_num);
	vmm_snprintf(bdev->desc, sizeof(bdev->desc),
		     "Manufacturer=%06x Serial=%04x%04x "
		     "Product=%c%c%c%c%c%c Rev=%d.%d", 
		     (card->cid[0] >> 24), (card->cid[2] & 0xffff),
		     ((card->cid[3] >> 16) & 0xffff), (card->cid[0] & 0xff),
		     (card->cid[1] >> 24), ((card->cid[1] >> 16) & 0xff),
		     ((card->cid[1] >> 8) & 0xff), (card->cid[1] & 0xff),
		     ((card->cid[2] >> 24) & 0xff), ((card->cid[2] >> 20) & 0xf),
		     ((card->cid[2] >> 16) & 0xf));
	bdev->dev = host->dev;
	bdev->flags = VMM_BLOCKDEV_RW;
	if (card->read_bl_len < card->write_bl_len) {
		bdev->block_size = card->write_bl_len;
	} else {
		bdev->block_size = card->read_bl_len;
	}
	bdev->start_lba = 0;
	bdev->num_blocks = udiv64(card->capacity, bdev->block_size);

	/* Setup request queue for block device instance */
	bdev->rq->make_request = mmc_make_request;
	bdev->rq->abort_request = mmc_abort_request;
	bdev->rq->priv = host;

	rc = vmm_blockdev_register(card->bdev);
	if (rc) {
		goto detect_freebdev_fail;
	}

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

static void __mmc_detect_card_change(struct mmc_host *host)
{
	int timeout = 1000;

	if (!host) {
		return;
	}

	if (host->card) {
		if (__mmc_send_status(host, host->card, timeout)) {
			__mmc_detect_card_removed(host);
		}
	} else {
		if (__mmc_getcd(host)) {
			__mmc_detect_card_inserted(host);
		}
	}
}

static u32 __mmc_write_blocks(struct mmc_host *host, struct mmc_card *card,
			      u64 start, u32 blkcnt, const void *src)
{
	struct mmc_cmd cmd;
	struct mmc_data data;
	int timeout = 1000;

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

	if (__mmc_send_cmd(host, &cmd, &data)) {
		return 0;
	}

	/* SPI multiblock writes terminate using a special
	 * token, not a STOP_TRANSMISSION request.
	 */
	if (!mmc_host_is_spi(host) && blkcnt > 1) {
		cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
		cmd.cmdarg = 0;
		cmd.resp_type = MMC_RSP_R1b;
		if (__mmc_send_cmd(host, &cmd, NULL)) {
			return 0;
		}
	}

	/* Waiting for the ready status */
	if (__mmc_send_status(host, card, timeout)) {
		return 0;
	}

	return blkcnt;
}

static u32 __mmc_bwrite(struct mmc_host *host, struct mmc_card *card,
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

	if (__mmc_send_cmd(host, &cmd, &data)) {
		return 0;
	}

	if (blkcnt > 1) {
		cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
		cmd.cmdarg = 0;
		cmd.resp_type = MMC_RSP_R1b;
		if (__mmc_send_cmd(host, &cmd, NULL)) {
			return 0;
		}
	}

	return blkcnt;
}

static u32 __mmc_bread(struct mmc_host *host, struct mmc_card *card,
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

static int __mmc_blockdev_request(struct mmc_host *host,
				  struct vmm_request_queue *rq, 
				  struct vmm_request *r)
{
	int rc;
	u32 cnt;

	if (!r) {
		return VMM_EFAIL;
	}

	if (!host || !host->card || !rq) {
		vmm_blockdev_fail_request(r);
		return VMM_EFAIL;
	}

	switch (r->type) {
	case VMM_REQUEST_READ:
		cnt = __mmc_bread(host, host->card, r->lba, r->bcnt, r->data);
		if (cnt == r->bcnt) {
			vmm_blockdev_complete_request(r);
			rc = VMM_OK;
		} else {
			vmm_blockdev_fail_request(r);
			rc = VMM_EIO;
		}
		break;
	case VMM_REQUEST_WRITE:
		cnt = __mmc_bwrite(host, host->card, r->lba, r->bcnt, r->data);
		if (cnt == r->bcnt) {
			vmm_blockdev_complete_request(r);
			rc = VMM_OK;
		} else {
			vmm_blockdev_fail_request(r);
			rc = VMM_EIO;
		}
		break;
	default:
		vmm_blockdev_fail_request(r);
		rc = VMM_EFAIL;
		break;
	};

	return rc;
}

static int mmc_host_thread(void *tdata)
{
	u64 tout;
	irq_flags_t flags;
	struct dlist *l;
	struct mmc_host_io *io;
	struct mmc_host *host = tdata;

	while (1) {
		if (host->caps & MMC_CAP_NEEDS_POLL) {
			tout = 1000000000; /* 1 seconds timeout */
			vmm_completion_wait_timeout(&host->io_avail, &tout);
			if (!tout) {
				__mmc_detect_card_change(host);
			}
		} else {
			vmm_completion_wait(&host->io_avail);
		}

		vmm_spin_lock_irqsave(&host->io_list_lock, flags);
		if (list_empty(&host->io_list)) {
			vmm_spin_unlock_irqrestore(&host->io_list_lock, flags);
			continue;
		}
		l = list_pop(&host->io_list);
		vmm_spin_unlock_irqrestore(&host->io_list_lock, flags);

		io = list_entry(l, struct mmc_host_io, head);

		vmm_mutex_lock(&host->lock);

		switch (io->type) {
		case MMC_HOST_IO_DETECT_CARD_CHANGE:
			tout = vmm_timer_timestamp();
			if (tout < io->card_change_tstamp) {
				tout = io->card_change_tstamp - tout;
				tout = udiv64(tout, 1000);
				if (tout) {
					vmm_udelay(tout);
				}
			}
			__mmc_detect_card_change(host);
			break;
		case MMC_HOST_IO_BLOCKDEV_REQUEST:
			__mmc_blockdev_request(host, io->rq, io->r);
			break;
		default:
			break;
		};

		vmm_mutex_unlock(&host->lock);

		vmm_free(io);
	}

	return VMM_OK;
}

static int mmc_make_request(struct vmm_request_queue *rq, 
			    struct vmm_request *r)
{
	irq_flags_t flags;
	struct mmc_host_io *io;
	struct mmc_host *host;

	if (!r || !rq || !rq->priv) {
		return VMM_EFAIL;
	}

	host = rq->priv;

	io = vmm_zalloc(sizeof(struct mmc_host_io));
	if (!io) {
		return VMM_ENOMEM;
	}

	INIT_LIST_HEAD(&io->head);
	io->type = MMC_HOST_IO_BLOCKDEV_REQUEST;
	io->rq = rq;
	io->r = r;

	vmm_spin_lock_irqsave(&host->io_list_lock, flags);
	list_add_tail(&io->head, &host->io_list);
	vmm_spin_unlock_irqrestore(&host->io_list_lock, flags);

	vmm_completion_complete(&host->io_avail);

	return VMM_OK;
}

static int mmc_abort_request(struct vmm_request_queue *rq, 
			     struct vmm_request *r)
{
	bool found;
	irq_flags_t flags;
	struct dlist *l;
	struct mmc_host_io *io;
	struct mmc_host *host;

	if (!r || !rq || !rq->priv) {
		return VMM_EFAIL;
	}

	host = rq->priv;

	vmm_spin_lock_irqsave(&host->io_list_lock, flags);

	found = FALSE;
	list_for_each(l, &host->io_list) {
		io = list_entry(l, struct mmc_host_io, head);
		if (io->r == r && io->rq == rq) {
			found = TRUE;
			break;
		}
	}
	if (found) {
		list_del(&io->head);
		vmm_free(io);
	}

	vmm_spin_unlock_irqrestore(&host->io_list_lock, flags);
	
	return VMM_OK;
}

int mmc_detect_card_change(struct mmc_host *host, unsigned long msecs)
{
	irq_flags_t flags;
	struct mmc_host_io *io;

	if (!host) {
		return VMM_EFAIL;
	}

	io = vmm_zalloc(sizeof(struct mmc_host_io));
	if (!io) {
		return VMM_ENOMEM;
	}

	INIT_LIST_HEAD(&io->head);
	io->type = MMC_HOST_IO_DETECT_CARD_CHANGE;
	io->card_change_tstamp = vmm_timer_timestamp() + 
					((u64)msecs * 1000000ULL);

	vmm_spin_lock_irqsave(&host->io_list_lock, flags);
	list_add_tail(&io->head, &host->io_list);
	vmm_spin_unlock_irqrestore(&host->io_list_lock, flags);

	vmm_completion_complete(&host->io_avail);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(mmc_detect_card_change);

struct mmc_host *mmc_alloc_host(int extra, struct vmm_device *dev)
{
	struct mmc_host *host;

	host = vmm_zalloc(sizeof(struct mmc_host) + extra);
	if (!host) {
		return NULL;
	}

	INIT_LIST_HEAD(&host->link);
	host->dev = dev;

	INIT_LIST_HEAD(&host->io_list);
	INIT_SPIN_LOCK(&host->io_list_lock);

	INIT_COMPLETION(&host->io_avail);
	host->io_thread = NULL;

	INIT_MUTEX(&host->lock);

	return host;
}
VMM_EXPORT_SYMBOL(mmc_alloc_host);

int mmc_add_host(struct mmc_host *host)
{
	char name[32];

	if (!host || host->io_thread) {
		return VMM_EFAIL;
	}

	if (!host->b_max) {
		host->b_max = CONFIG_SYS_MMC_MAX_BLK_COUNT;
	}

	vmm_mutex_lock(&mmc_host_list_mutex);

	INIT_COMPLETION(&host->io_avail);
	vmm_snprintf(name, 32, "mmc%d", mmc_host_count);
	host->io_thread = vmm_threads_create(name, mmc_host_thread, host, 
					     VMM_THREAD_DEF_PRIORITY,
					     VMM_THREAD_DEF_TIME_SLICE);
	if (!host->io_thread) {
		vmm_mutex_unlock(&mmc_host_list_mutex);
		return VMM_EFAIL;
	}

	host->host_num = mmc_host_count;
	mmc_host_count++;
	list_add_tail(&host->link, &mmc_host_list);

	vmm_mutex_unlock(&mmc_host_list_mutex);

	/* Make an attempt to detect mmc card 
	 * Note: If it fails then it means there is not card connected so
	 * we ignore failures.
	 */
	vmm_mutex_lock(&host->lock);
	__mmc_detect_card_inserted(host);
	vmm_mutex_unlock(&host->lock);

	vmm_threads_start(host->io_thread);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(mmc_add_host);

void mmc_remove_host(struct mmc_host *host)
{
	if (!host || !host->io_thread) {
		return;
	}

	vmm_mutex_lock(&host->lock);
	__mmc_detect_card_removed(host);
	vmm_mutex_unlock(&host->lock);

	vmm_mutex_lock(&mmc_host_list_mutex);

	list_del(&host->link);
	mmc_host_count--;

	vmm_threads_stop(host->io_thread);
	vmm_threads_destroy(host->io_thread);
	host->io_thread = NULL;

	vmm_mutex_unlock(&mmc_host_list_mutex);
}
VMM_EXPORT_SYMBOL(mmc_remove_host);

void mmc_free_host(struct mmc_host *host)
{
	if (!host) {
		return;
	}

	vmm_free(host);
}
VMM_EXPORT_SYMBOL(mmc_free_host);

static int __init mmc_core_init(void)
{
	/* Nothing to be done. */
	return VMM_OK;
}

static void __exit mmc_core_exit(void)
{
	/* Nothing to be done. */
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
