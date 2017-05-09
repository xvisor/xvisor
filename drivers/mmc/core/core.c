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
 * @file core.c
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

#include "core.h"

#define MODULE_NAME			mmc_core
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

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)			vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

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

/**
 *	mmc_align_data_size - pads a transfer size to a more optimal value
 *	@card: the MMC card associated with the data transfer
 *	@sz: original transfer size
 *
 *	Pads the original data size with a number of extra bytes in
 *	order to avoid controller bugs and/or performance hits
 *	(e.g. some controllers revert to PIO for certain sizes).
 *
 *	Returns the improved size, which might be unmodified.
 *
 *	Note that this function is only relevant when issuing a
 *	single scatter gather entry.
 */
unsigned int mmc_align_data_size(struct mmc_card *card,
				 unsigned int sz)
{
	/*
	 * FIXME: We don't have a system for the controller to tell
	 * the core about its problems yet, so for now we just 32-bit
	 * align the size.
	 */
	sz = ((sz + 3) / 4) * 4;

	return sz;
}

static void __mmc_set_ios(struct mmc_host *host)
{
	if (host->ops.set_ios) {
		host->ops.set_ios(host, &host->ios);
	}
}

void mmc_set_clock(struct mmc_host *host, u32 clock)
{
	if (clock > host->f_max)
		clock = host->f_max;

	if (clock < host->f_min)
		clock = host->f_min;

	host->ios.clock = clock;
	__mmc_set_ios(host);
}

void mmc_set_bus_width(struct mmc_host *host, u32 width)
{
	host->ios.bus_width = width;
	__mmc_set_ios(host);
}

int mmc_init_card(struct mmc_host *host, struct mmc_card *card)
{
	if (host->ops.init_card) {
		return host->ops.init_card(host, card);
	}

	return VMM_OK;
}

int mmc_getcd(struct mmc_host *host)
{
	if (host->ops.get_cd) {
		return host->ops.get_cd(host);
	}

	return VMM_ENOTSUPP;
}

int mmc_send_cmd(struct mmc_host *host, 
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

int mmc_send_status(struct mmc_host *host,
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
		err = mmc_send_cmd(host, &cmd, NULL);
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

int mmc_go_idle(struct mmc_host *host)
{
	int err;
	struct mmc_cmd cmd;

	vmm_udelay(1000);

	cmd.cmdidx = MMC_CMD_GO_IDLE_STATE;
	cmd.cmdarg = 0;
	cmd.resp_type = MMC_RSP_NONE;

	err = mmc_send_cmd(host, &cmd, NULL);
	if (err) {
		return err;
	}

	vmm_udelay(2000);

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
	if (host->card->bdev)
		vmm_blockdev_unregister(host->card->bdev);
	if (host->card->bdev->rq)
		vmm_free(host->card->bdev->rq);
	if (host->card->bdev)
		vmm_blockdev_free(host->card->bdev);

	vmm_free(host->card);
	host->card = NULL;

unplug_done:
	return rc;
}

static int __mmc_detect_card_inserted(struct mmc_host *host)
{
	/* SDIO probe followed by SD and MMC probe */
	if (!__sdio_attach(host)) {
		return 0;
	}
	if (!__mmc_sd_attach(host)) {
		return 0;
	}

	return VMM_EIO;
}

static void __mmc_detect_card_change(struct mmc_host *host)
{
	int rc, timeout = 1000;

	if (!host) {
		return;
	}

	rc = mmc_getcd(host);
	if (host->card) {
		if (rc == VMM_ENOTSUPP) {
			if (mmc_send_status(host, host->card, timeout)) {
				__mmc_detect_card_removed(host);
			}
		} else if (rc == 0) {
			__mmc_detect_card_removed(host);
		}
	} else {
		if ((rc == VMM_ENOTSUPP) || (rc > 0)) {
			__mmc_detect_card_inserted(host);
		}
	}
}

static void mmc_host_poll(struct vmm_blockrq *brq, void *priv)
{
	struct mmc_host *host = priv;

	vmm_mutex_lock(&host->lock);
	__mmc_detect_card_change(host);
	vmm_mutex_unlock(&host->lock);

	if (host->caps & MMC_CAP_NEEDS_POLL) {
		/* Poll again after 1 seconds */
		vmm_timer_event_start(&host->poll_ev, 1000000000LL);
	}
}

static void mmc_host_poll_event_handler(struct vmm_timer_event *ev)
{
	struct mmc_host *host = container_of(ev, struct mmc_host, poll_ev);

	vmm_blockrq_queue_work(host->brq, mmc_host_poll, host);
}

static int mmc_blockrq_read(struct vmm_blockrq *brq,
			    struct vmm_request *r, void *priv)
{
	u32 cnt;
	int rc = VMM_OK;
	struct mmc_host *host = priv;

	vmm_mutex_lock(&host->lock);
	cnt = __mmc_sd_bread(host, host->card, r->lba, r->bcnt, r->data);
	if (cnt == r->bcnt) {
		rc = VMM_OK;
	} else {
		rc = VMM_EIO;
	}
	vmm_mutex_unlock(&host->lock);

	return rc;
}

static int mmc_blockrq_write(struct vmm_blockrq *brq,
			     struct vmm_request *r, void *priv)
{
	u32 cnt;
	int rc = VMM_OK;
	struct mmc_host *host = priv;

	vmm_mutex_lock(&host->lock);
	cnt = __mmc_sd_bwrite(host, host->card, r->lba, r->bcnt, r->data);
	if (cnt == r->bcnt) {
		rc = VMM_OK;
	} else {
		rc = VMM_EIO;
	}
	vmm_mutex_unlock(&host->lock);

	return rc;
}

static int mmc_blockrq_abort(struct vmm_blockrq *brq,
			     struct vmm_request *r, void *priv)
{
	/* Nothing to do here. */
	return VMM_OK;
}

static void mmc_blockrq_flush(struct vmm_blockrq *brq, void *priv)
{
	/* Nothing to do here. */
}

int mmc_detect_card_change(struct mmc_host *host, unsigned long msecs)
{
	if (!host) {
		return VMM_EFAIL;
	}

	return vmm_timer_event_start(&host->poll_ev,
				     (u64)msecs * 1000000ULL);
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

	INIT_MUTEX(&host->slot.lock);
	host->slot.cd_irq = VMM_EINVALID;

	host->brq = NULL;
	INIT_TIMER_EVENT(&host->poll_ev, mmc_host_poll_event_handler, host);

	INIT_MUTEX(&host->lock);

	return host;
}
VMM_EXPORT_SYMBOL(mmc_alloc_host);

int mmc_add_host(struct mmc_host *host)
{
	char name[32];

	if (!host || host->brq) {
		return VMM_EFAIL;
	}

	if (!host->b_max) {
		host->b_max = CONFIG_SYS_MMC_MAX_BLK_COUNT;
	}

	vmm_mutex_lock(&mmc_host_list_mutex);

	vmm_snprintf(name, 32, "mmc%d", mmc_host_count);
	host->brq = vmm_blockrq_create(name, 128, FALSE,
				       mmc_blockrq_read,
				       mmc_blockrq_write,
				       mmc_blockrq_abort,
				       mmc_blockrq_flush,
				       host);
	if (!host->brq) {
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

	/* Start polling timer event if required */
	if (host->caps & MMC_CAP_NEEDS_POLL) {
		vmm_timer_event_start(&host->poll_ev, 1000000000LL);
	}

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(mmc_add_host);

void mmc_remove_host(struct mmc_host *host)
{
	if (!host || !host->brq) {
		return;
	}

	vmm_timer_event_stop(&host->poll_ev);

	vmm_mutex_lock(&host->lock);
	__mmc_detect_card_removed(host);
	vmm_mutex_unlock(&host->lock);

	vmm_mutex_lock(&mmc_host_list_mutex);

	list_del(&host->link);
	mmc_host_count--;

	vmm_blockrq_destroy(host->brq);
	host->brq = NULL;

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
	int rc;

	rc = vmm_devdrv_register_bus(&sdio_bus_type);
	if (rc) {
		vmm_printf("sdio bus register failed (%d)\n", rc);
		return rc;
	}

	return VMM_OK;
}

static void __exit mmc_core_exit(void)
{
	int rc;

	rc = vmm_devdrv_unregister_bus(&sdio_bus_type);
	if (rc) {
		vmm_printf("sdio bus un-register failed (%d)\n", rc);
	}

	return;
}

VMM_DECLARE_MODULE2(MODULE_NAME,
			MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
