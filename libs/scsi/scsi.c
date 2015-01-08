/**
 * Copyright (C) 2014 Anup Patel.
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
 * @file scsi.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief SCSI generic library
 *
 * This file is partly adapted from u-boot sources:
 * <u-boot>/common/usb_storage.c
 *
 * (C) Copyright 2001
 * Denis Peter, MPL AG Switzerland
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <vmm_error.h>
#include <vmm_cache.h>
#include <vmm_delay.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/scsi.h>

#define MODULE_DESC			"SCSI Generic Library"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(SCSI_IPRIORITY)
#define	MODULE_INIT			NULL
#define	MODULE_EXIT			NULL

#undef _DEBUG
#ifdef _DEBUG
#define DPRINTF(msg...)		vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

/* direction table -- this indicates the direction of the data
 * transfer for each command code -- a 1 indicates input
 */
const unsigned char scsi_direction[256/8] = {
	0x28, 0x81, 0x14, 0x14, 0x20, 0x01, 0x90, 0x77,
	0x0C, 0x20, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
VMM_EXPORT_SYMBOL(scsi_direction);

int scsi_inquiry(struct scsi_request *srb,
		 struct scsi_transport *tr, void *priv)
{
	int retry, rc = VMM_OK;
	unsigned long datalen;

	if (!srb || !srb->data || (srb->datalen < 64) ||
	    !tr || !tr->transport) {
		return VMM_EINVALID;
	}

	datalen = srb->datalen;
	retry = 5;
	do {
		memset(&srb->cmd, 0, sizeof(srb->cmd));
		srb->cmd[0] = SCSI_INQUIRY;
		srb->cmd[1] = srb->lun << 5;
		srb->cmd[4] = 64;
		srb->datalen = 64;
		srb->cmdlen = 12;
		rc = tr->transport(srb, tr, priv);
		DPRINTF("%s: inquiry returns %d\n", __func__, rc);
		if (rc == VMM_OK)
			break;
	} while (--retry);
	srb->datalen = datalen;

	if (!retry) {
		vmm_printf("%s: error in inquiry\n", __func__);
		return VMM_EFAIL;
	}

	return rc;
}
VMM_EXPORT_SYMBOL(scsi_inquiry);

int scsi_request_sense(struct scsi_request *srb,
		       struct scsi_transport *tr, void *priv)
{
	int rc;
	unsigned char *data;
	unsigned long datalen;
	unsigned char __cacheline_aligned sense_buf[sizeof(srb->sense_buf)];

	if (!srb || !tr || !tr->transport) {
		return VMM_EINVALID;
	}

	data = srb->data;
	datalen = srb->datalen;
	memset(&srb->cmd, 0, sizeof(srb->cmd));
	memset(&srb->sense_buf, 0, sizeof(srb->sense_buf));
	srb->cmd[0] = SCSI_REQ_SENSE;
	srb->cmd[1] = srb->lun << 5;
	srb->cmd[4] = sizeof(srb->sense_buf);
	srb->datalen = sizeof(srb->sense_buf);
	srb->data = &sense_buf[0];
	srb->cmdlen = 12;
	rc = tr->transport(srb, tr, priv);
	srb->data = data;
	srb->datalen = datalen;
	memcpy(srb->sense_buf, sense_buf, sizeof(srb->sense_buf));
	DPRINTF("%s: request sense returned %02X %02X %02X\n",
		srb->sense_buf[2], srb->sense_buf[12],
		srb->sense_buf[13]);

	return rc;
}
VMM_EXPORT_SYMBOL(scsi_request_sense);

int scsi_test_unit_ready(struct scsi_request *srb,
			 struct scsi_transport *tr, void *priv)
{
	int rc = VMM_EFAIL, retries = 10;
	unsigned char *data;
	unsigned long datalen;

	if (!srb || !tr || !tr->transport) {
		return VMM_EINVALID;
	}

	data = srb->data;
	datalen = srb->datalen;
	do {
		memset(&srb->cmd, 0, sizeof(srb->cmd));
		srb->cmd[0] = SCSI_TST_U_RDY;
		srb->cmd[1] = srb->lun << 5;
		srb->data = NULL;
		srb->datalen = 0;
		srb->cmdlen = 12;
		rc = tr->transport(srb, tr, priv);
		if (rc == VMM_OK) {
			break;
		}

		rc = scsi_request_sense(srb, tr, priv);
		if (rc != VMM_OK) {
			return rc;
		}

		/*
		 * Check the Key Code Qualifier, if it matches
		 * "Not Ready - medium not present"
		 * (the sense Key equals 0x2 and the ASC is 0x3a)
		 * return immediately as the medium being absent won't change
		 * unless there is a user action.
		 */
		if ((srb->sense_buf[2] == 0x02) &&
		    (srb->sense_buf[12] == 0x3a)) {
			rc = VMM_ENODEV;
			break;
		}

		vmm_mdelay(100);
		rc = VMM_EFAIL;
	} while (retries--);
	srb->data = data;
	srb->datalen = datalen;

	return rc;
}
VMM_EXPORT_SYMBOL(scsi_test_unit_ready);

int scsi_read_capacity(struct scsi_request *srb,
			struct scsi_transport *tr, void *priv)
{
	int rc = VMM_EFAIL, retry = 3;
	unsigned long datalen;

	if (!srb || !srb->data || (srb->datalen < 64) ||
	    !tr || !tr->transport) {
		return VMM_EINVALID;
	}

	datalen = srb->datalen;
	do {
		memset(&srb->cmd, 0, sizeof(srb->cmd));
		srb->cmd[0] = SCSI_RD_CAPAC;
		srb->cmd[1] = srb->lun << 5;
		srb->datalen = 64;
		srb->cmdlen = 12;
		rc = tr->transport(srb, tr, priv);
		if (rc == VMM_OK) {
			break;
		}

		rc = VMM_EFAIL;
	} while (retry--);
	srb->datalen = datalen;

	return rc;
}
VMM_EXPORT_SYMBOL(scsi_read_capacity);

int scsi_read10(struct scsi_request *srb,
		 unsigned long start, unsigned short blocks,
		 struct scsi_transport *tr, void *priv)
{
	if (!srb || !tr || !tr->transport) {
		return VMM_EINVALID;
	}

	memset(&srb->cmd, 0, sizeof(srb->cmd));
	srb->cmd[0] = SCSI_READ10;
	srb->cmd[1] = srb->lun << 5;
	srb->cmd[2] = ((unsigned char)(start >> 24)) & 0xff;
	srb->cmd[3] = ((unsigned char)(start >> 16)) & 0xff;
	srb->cmd[4] = ((unsigned char)(start >> 8)) & 0xff;
	srb->cmd[5] = ((unsigned char)(start)) & 0xff;
	srb->cmd[7] = ((unsigned char)(blocks >> 8)) & 0xff;
	srb->cmd[8] = (unsigned char)blocks & 0xff;
	srb->cmdlen = 12;
	DPRINTF("%s: start %lx blocks %x\n", __func__, start, blocks);

	return tr->transport(srb, tr, priv);
}
VMM_EXPORT_SYMBOL(scsi_read10);

int scsi_write10(struct scsi_request *srb,
		 unsigned long start, unsigned short blocks,
		 struct scsi_transport *tr, void *priv)
{
	if (!srb || !tr || !tr->transport) {
		return VMM_EINVALID;
	}

	memset(&srb->cmd, 0, sizeof(srb->cmd));
	srb->cmd[0] = SCSI_WRITE10;
	srb->cmd[1] = srb->lun << 5;
	srb->cmd[2] = ((unsigned char)(start >> 24)) & 0xff;
	srb->cmd[3] = ((unsigned char)(start >> 16)) & 0xff;
	srb->cmd[4] = ((unsigned char)(start >> 8)) & 0xff;
	srb->cmd[5] = ((unsigned char)(start)) & 0xff;
	srb->cmd[7] = ((unsigned char)(blocks >> 8)) & 0xff;
	srb->cmd[8] = (unsigned char)blocks & 0xff;
	srb->cmdlen = 12;
	DPRINTF("%s: start %lx blocks %x\n", __func__, start, blocks);

	return tr->transport(srb, tr, priv);
}
VMM_EXPORT_SYMBOL(scsi_write10);

int scsi_reset(struct scsi_transport *tr, void *priv)
{
	if (!tr || !tr->reset) {
		return VMM_EINVALID;
	}

	return tr->reset(tr, priv);
}
VMM_EXPORT_SYMBOL(scsi_reset);

int scsi_get_info(struct scsi_info *info, unsigned int lun,
		  struct scsi_transport *tr, void *priv)
{
	int rc;
	u32 *cap;
	unsigned char __cacheline_aligned buf[64];
	struct scsi_request srb;

	if (!info || !tr || !tr->transport) {
		return VMM_EINVALID;
	}

	memset(info, 0, sizeof(*info));

	INIT_SCSI_REQUEST(&srb, lun, &buf, sizeof(buf));
	rc = scsi_inquiry(&srb, tr, priv);
	if (rc) {
		return rc;
	}

	info->lun = lun;
	info->perph_qualifier = (buf[0] & 0xE0) >> 5;
	info->perph_type = buf[0] & 0x1F;
	info->removable = (buf[1] & 0x80) ? TRUE : FALSE;

	memcpy(&info->vendor[0], (const void *)&buf[8], 8);
	memcpy(&info->product[0], (const void *)&buf[16], 16);
	memcpy(&info->revision[0], (const void *)&buf[32], 4);
	info->vendor[8] = 0;
	info->product[16] = 0;
	info->revision[4] = 0;

	INIT_SCSI_REQUEST(&srb, lun, NULL, 0);
	rc = scsi_test_unit_ready(&srb, tr, priv);
	if (rc) {
		return rc;
	}

	INIT_SCSI_REQUEST(&srb, lun, buf, sizeof(buf));
	rc = scsi_read_capacity(&srb, tr, priv);
	if (rc) {
		return rc;
	}
	cap = (u32 *)buf;
	cap[0] = vmm_cpu_to_be32(cap[0]);
	cap[1] = vmm_cpu_to_be32(cap[1]);

	info->capacity = cap[0];
	info->blksz = cap[1];

	info->readonly = TRUE;
	switch (info->perph_type) {
	case 0x00:
	case 0x0C:
	case 0x0E:
	case 0x0F:
		info->readonly = FALSE;
		break;
	default:
		break;
	};

	if (tr->info_fixup) {
		tr->info_fixup(info, tr, priv);
	}

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(scsi_get_info);

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
