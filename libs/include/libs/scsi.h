/**
 * Copyright (c) 2014 Anup Patel.
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
 * @file scsi.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Interface header for SCSI generic library.
 *
 * This file is partially adapted from u-boot sources:
 * <u-boot>/include/scsi.h
 *
 * (C) Copyright 2001
 * Denis Peter, MPL AG Switzerland
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __SCSI_H__
#define __SCSI_H__

#include <vmm_types.h>
#include <vmm_limits.h>

#define SCSI_IPRIORITY		 (1)

struct scsi_request {
	/* target */
	unsigned char	lun;		/* SCSI Target LUN */
	/* general command */
	unsigned char	cmd[16];	/* Command */
	unsigned char	cmdlen;		/* Command len */
	unsigned char	*data;		/* Pointer to data */
	unsigned long	datalen;	/* Total data length */
	/* request sense */
	unsigned char	sense_buf[18];	/* Sense data */
	/* status */
	unsigned char	status;		/* SCSI Status */
};

#define INIT_SCSI_REQUEST(_srb, _lun, _data, _datalen)	\
	do { \
		(_srb)->lun = (_lun); \
		(_srb)->cmd[0] = 0; \
		(_srb)->cmd[1] = 0; \
		(_srb)->cmd[2] = 0; \
		(_srb)->cmd[3] = 0; \
		(_srb)->cmdlen = 0; \
		(_srb)->data = (unsigned char *)(_data); \
		(_srb)->datalen = (_datalen); \
		(_srb)->sense_buf[0] = 0; \
		(_srb)->sense_buf[1] = 0; \
		(_srb)->sense_buf[2] = 0; \
		(_srb)->sense_buf[3] = 0; \
		(_srb)->status = 0; \
	} while (0)

struct scsi_info {
	unsigned int	lun;
	unsigned char	perph_qualifier;
	unsigned char	perph_type;
	bool		removable;
	char		vendor[9]; /* 8+1 */
	char		product[17]; /* 16+1 */
	char		revision[5]; /* 4+1 */
	unsigned long	capacity;
	unsigned long	blksz;
	bool		readonly;
};

struct scsi_transport {
	char name[VMM_FIELD_NAME_SIZE];
	int (*transport)(struct scsi_request *srb,
			 struct scsi_transport *tr, void *priv);
	int (*reset)(struct scsi_transport *tr, void *priv);
	void (*info_fixup)(struct scsi_info *info,
			   struct scsi_transport *tr, void *priv);
};

extern const unsigned char scsi_direction[256/8];
#define SCSI_CMD_DIRECTION(x) ((scsi_direction[(x)>>3] >> ((x) & 7)) & 1)

/*
 * SCSI  constants.
 */

/*
 * Messages
 */

#define	M_COMPLETE	(0x00)
#define	M_EXTENDED	(0x01)
#define	M_SAVE_DP	(0x02)
#define	M_RESTORE_DP	(0x03)
#define	M_DISCONNECT	(0x04)
#define	M_ID_ERROR	(0x05)
#define	M_ABORT		(0x06)
#define	M_REJECT	(0x07)
#define	M_NOOP		(0x08)
#define	M_PARITY	(0x09)
#define	M_LCOMPLETE	(0x0a)
#define	M_FCOMPLETE	(0x0b)
#define	M_RESET		(0x0c)
#define	M_ABORT_TAG	(0x0d)
#define	M_CLEAR_QUEUE	(0x0e)
#define	M_INIT_REC	(0x0f)
#define	M_REL_REC	(0x10)
#define	M_TERMINATE	(0x11)
#define	M_SIMPLE_TAG	(0x20)
#define	M_HEAD_TAG	(0x21)
#define	M_ORDERED_TAG	(0x22)
#define	M_IGN_RESIDUE	(0x23)
#define	M_IDENTIFY	(0x80)

#define	M_X_MODIFY_DP	(0x00)
#define	M_X_SYNC_REQ	(0x01)
#define	M_X_WIDE_REQ	(0x03)
#define	M_X_PPR_REQ	(0x04)

/*
 * Status
 */

#define	S_GOOD		(0x00)
#define	S_CHECK_COND	(0x02)
#define	S_COND_MET	(0x04)
#define	S_BUSY		(0x08)
#define	S_INT		(0x10)
#define	S_INT_COND_MET	(0x14)
#define	S_CONFLICT	(0x18)
#define	S_TERMINATED	(0x20)
#define	S_QUEUE_FULL	(0x28)
#define	S_ILLEGAL	(0xff)
#define	S_SENSE		(0x80)

/*
 * Sense_keys
 */

#define SENSE_NO_SENSE		0x0
#define SENSE_RECOVERED_ERROR	0x1
#define SENSE_NOT_READY		0x2
#define SENSE_MEDIUM_ERROR	0x3
#define SENSE_HARDWARE_ERROR	0x4
#define SENSE_ILLEGAL_REQUEST	0x5
#define SENSE_UNIT_ATTENTION	0x6
#define SENSE_DATA_PROTECT	0x7
#define SENSE_BLANK_CHECK	0x8
#define SENSE_VENDOR_SPECIFIC	0x9
#define SENSE_COPY_ABORTED	0xA
#define SENSE_ABORTED_COMMAND	0xB
#define SENSE_VOLUME_OVERFLOW	0xD
#define SENSE_MISCOMPARE	0xE


#define SCSI_CHANGE_DEF		0x40	/* Change Definition (Optional) */
#define SCSI_COMPARE		0x39	/* Compare (O) */
#define SCSI_COPY		0x18	/* Copy (O) */
#define SCSI_COP_VERIFY		0x3A	/* Copy and Verify (O) */
#define SCSI_INQUIRY		0x12	/* Inquiry (MANDATORY) */
#define SCSI_LOG_SELECT		0x4C	/* Log Select (O) */
#define SCSI_LOG_SENSE		0x4D	/* Log Sense (O) */
#define SCSI_MODE_SEL6		0x15	/* Mode Select 6-byte (Device Specific) */
#define SCSI_MODE_SEL10		0x55	/* Mode Select 10-byte (Device Specific) */
#define SCSI_MODE_SEN6		0x1A	/* Mode Sense 6-byte (Device Specific) */
#define SCSI_MODE_SEN10		0x5A	/* Mode Sense 10-byte (Device Specific) */
#define SCSI_READ_BUFF		0x3C	/* Read Buffer (O) */
#define SCSI_REQ_SENSE		0x03	/* Request Sense (MANDATORY) */
#define SCSI_SEND_DIAG		0x1D	/* Send Diagnostic (O) */
#define SCSI_TST_U_RDY		0x00	/* Test Unit Ready (MANDATORY) */
#define SCSI_WRITE_BUFF		0x3B	/* Write Buffer (O) */

/*
 *  %%% Commands Unique to Direct Access Devices %%%
 */
#define SCSI_COMPARE	0x39		/* Compare (O) */
#define SCSI_FORMAT	0x04		/* Format Unit (MANDATORY) */
#define SCSI_LCK_UN_CAC	0x36		/* Lock Unlock Cache (O) */
#define SCSI_PREFETCH	0x34		/* Prefetch (O) */
#define SCSI_MED_REMOVL	0x1E		/* Prevent/Allow medium Removal (O) */
#define SCSI_READ6	0x08		/* Read 6-byte (MANDATORY) */
#define SCSI_READ10	0x28		/* Read 10-byte (MANDATORY) */
#define SCSI_RD_CAPAC	0x25		/* Read Capacity (MANDATORY) */
#define SCSI_RD_CAPAC10	SCSI_RD_CAPAC	/* Read Capacity (10) */
#define SCSI_RD_CAPAC16	0x9e		/* Read Capacity (16) */
#define SCSI_RD_DEFECT	0x37		/* Read Defect Data (O) */
#define SCSI_READ_LONG	0x3E		/* Read Long (O) */
#define SCSI_REASS_BLK	0x07		/* Reassign Blocks (O) */
#define SCSI_RCV_DIAG	0x1C		/* Receive Diagnostic Results (O) */
#define SCSI_RELEASE	0x17		/* Release Unit (MANDATORY) */
#define SCSI_REZERO	0x01		/* Rezero Unit (O) */
#define SCSI_SRCH_DAT_E	0x31		/* Search Data Equal (O) */
#define SCSI_SRCH_DAT_H	0x30		/* Search Data High (O) */
#define SCSI_SRCH_DAT_L	0x32		/* Search Data Low (O) */
#define SCSI_SEEK6	0x0B		/* Seek 6-Byte (O) */
#define SCSI_SEEK10	0x2B		/* Seek 10-Byte (O) */
#define SCSI_SEND_DIAG	0x1D		/* Send Diagnostics (MANDATORY) */
#define SCSI_SET_LIMIT	0x33		/* Set Limits (O) */
#define SCSI_START_STP	0x1B		/* Start/Stop Unit (O) */
#define SCSI_SYNC_CACHE	0x35		/* Synchronize Cache (O) */
#define SCSI_VERIFY	0x2F		/* Verify (O) */
#define SCSI_WRITE6	0x0A		/* Write 6-Byte (MANDATORY) */
#define SCSI_WRITE10	0x2A		/* Write 10-Byte (MANDATORY) */
#define SCSI_WRT_VERIFY	0x2E		/* Write and Verify (O) */
#define SCSI_WRITE_LONG	0x3F		/* Write Long (O) */
#define SCSI_WRITE_SAME	0x41		/* Write Same (O) */

/*
 * functions exported by SCSI library
 */

int scsi_inquiry(struct scsi_request *srb,
		 struct scsi_transport *tr, void *priv);

int scsi_request_sense(struct scsi_request *srb,
		       struct scsi_transport *tr, void *priv);

int scsi_test_unit_ready(struct scsi_request *srb,
			 struct scsi_transport *tr, void *priv);

int scsi_read_capacity(struct scsi_request *srb,
			struct scsi_transport *tr, void *priv);

int scsi_read10(struct scsi_request *srb,
		 unsigned long start, unsigned short blocks,
		 struct scsi_transport *tr, void *priv);

int scsi_write10(struct scsi_request *srb,
		 unsigned long start, unsigned short blocks,
		 struct scsi_transport *tr, void *priv);

int scsi_reset(struct scsi_transport *tr, void *priv);

int scsi_get_info(struct scsi_info *info, unsigned int lun,
		  struct scsi_transport *tr, void *priv);

#endif /* __SCSI_H__ */
