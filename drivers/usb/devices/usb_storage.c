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
 * @file usb_storage.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief USB mass storage device driver
 */

#include <vmm_error.h>
#include <vmm_cache.h>
#include <vmm_delay.h>
#include <vmm_heap.h>
#include <vmm_spinlocks.h>
#include <vmm_host_io.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>
#include <libs/stringlib.h>
#include <libs/bitmap.h>
#include <libs/scsi_disk.h>

#include <drv/usb.h>

#define MODULE_DESC			"USB Storage Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(SCSI_DISK_IPRIORITY + \
					 USB_CORE_IPRIORITY + 1)
#define	MODULE_INIT			usb_storage_init
#define	MODULE_EXIT			usb_storage_exit

#define US_MAX_PENDING		128
#define US_BLKS_PER_XFER	16
#define US_MAX_LUNS		4

#define US_MAX_DISKS		32

/* Sub STORAGE Classes */
#define US_SC_RBC		1		/* Typically, flash devices */
#define US_SC_8020		2		/* CD-ROM */
#define US_SC_QIC		3		/* QIC-157 Tapes */
#define US_SC_UFI		4		/* Floppy */
#define US_SC_8070		5		/* Removable media */
#define US_SC_SCSI		6		/* Transparent */
#define US_SC_MIN		US_SC_RBC
#define US_SC_MAX		US_SC_SCSI

/* STORAGE Protocols */
#define US_PR_CB		1		/* Control/Bulk w/o interrupt */
#define US_PR_CBI		0		/* Control/Bulk/Interrupt */
#define US_PR_BULK		0x50		/* bulk only */

/* CBI style */
#define US_CBI_ADSC		0

/* BULK only */
#define US_BBB_RESET		0xff
#define US_BBB_GET_MAX_LUN	0xfe

/* Command Block Wrapper */
struct usb_storage_bbb_cbw {
	u32		dCBWSignature;
#	define CBWSIGNATURE	0x43425355
	u32		dCBWTag;
	u32		dCBWDataTransferLength;
	u8		bCBWFlags;
#	define CBWFLAGS_OUT	0x00
#	define CBWFLAGS_IN	0x80
	u8		bCBWLUN;
	u8		bCDBLength;
#	define CBWCDBLENGTH	16
	u8		CBWCDB[CBWCDBLENGTH];
};
#define UMASS_BBB_CBW_SIZE	31

/* Command Status Wrapper */
struct usb_storage_bbb_csw {
	u32		dCSWSignature;
#	define CSWSIGNATURE	0x53425355
	u32		dCSWTag;
	u32		dCSWDataResidue;
	u8		bCSWStatus;
#	define CSWSTATUS_GOOD	0x0
#	define CSWSTATUS_FAILED 0x1
#	define CSWSTATUS_PHASE	0x2
};
#define UMASS_BBB_CSW_SIZE	13

struct usb_storate_lun {
	int disk_num;
	unsigned char lun;
	struct scsi_disk *disk;
};

struct usb_storage {
	struct usb_device *dev;
	struct usb_interface *intf;
	struct scsi_transport *tr;

	unsigned char ep_in;		/* in endpoint */
	unsigned char ep_out;		/* out ....... */
	unsigned char ep_int;		/* interrupt . */
	unsigned int irqpipe;	 	/* pipe for release_irq */
	unsigned char irqmaxp;		/* max packed for irq Pipe */
	unsigned char irqinterval;	/* Intervall for IRQ Pipe */

	u32 CBWTag;

	u32 luns_count;
	struct usb_storate_lun luns[US_MAX_LUNS];
};

static vmm_spinlock_t us_disk_bmap_lock;
static DECLARE_BITMAP(us_disk_bmap, US_MAX_DISKS);

static int usb_storage_alloc_disk_num(void)
{
	int i, disk_num = VMM_ENOTAVAIL;
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&us_disk_bmap_lock, flags);
	for (i = 0; i < US_MAX_DISKS; i++) {
		if (bitmap_isset(us_disk_bmap, i)) {
			disk_num = i;
			bitmap_clearbit(us_disk_bmap, i);
			break;
		}
	}
	vmm_spin_unlock_irqrestore(&us_disk_bmap_lock, flags);

	return disk_num;
}

static void usb_storage_free_disk_num(int disk_num)
{
	irq_flags_t flags;

	if ((disk_num < 0) || (US_MAX_DISKS <= disk_num)) {
		return;
	}

	vmm_spin_lock_irqsave(&us_disk_bmap_lock, flags);
	bitmap_setbit(us_disk_bmap, disk_num);
	vmm_spin_unlock_irqrestore(&us_disk_bmap_lock, flags);
}

static int usb_storage_BBB_reset(struct scsi_transport *tr, void *priv)
{
	int rc;
	struct usb_storage *us = priv;

	/*
	 * Reset recovery (5.3.4 in Universal Serial Bus Mass Storage Class)
	 *
	 * For Reset Recovery the host shall issue in the following order:
	 * a) a Bulk-Only Mass Storage Reset
	 * b) a Clear Feature HALT to the Bulk-In endpoint
	 * c) a Clear Feature HALT to the Bulk-Out endpoint
	 *
	 * This is done in 3 steps.
	 *
	 * If the reset doesn't succeed, the device should be port reset.
	 *
	 * This comment stolen from FreeBSD's /sys/dev/usb/umass.c.
	 */
	rc = usb_control_msg(us->dev, usb_sndctrlpipe(us->dev, 0),
			     US_BBB_RESET,
			     USB_TYPE_CLASS | USB_RECIP_INTERFACE, 0,
			     us->intf->desc.bInterfaceNumber,
			     NULL, 0, NULL, USB_CNTL_TIMEOUT * 5);
	if (rc) {
		return rc;
	}

	/* long wait for reset */
	vmm_msleep(150);

	/* clear halt on input end-point */
	rc = usb_clear_halt(us->dev, usb_rcvbulkpipe(us->dev, us->ep_in));
	if (rc) {
		return rc;
	}

	/* long wait for clear halt */
	vmm_msleep(150);

	/* clear halt on output end-point */
	rc = usb_clear_halt(us->dev, usb_sndbulkpipe(us->dev, us->ep_out));
	if (rc) {
		return rc;
	}

	/* long wait for clear halt */
	vmm_msleep(150);

	return VMM_OK;
}

/*
 * Set up the command for a BBB device. Note that the actual SCSI
 * command is copied into cbw.CBWCDB.
 */
static int usb_storage_BBB_comdat(struct scsi_request *srb,
				  struct usb_storage *us)
{
	int actlen;
	unsigned int pipe;
	struct usb_storage_bbb_cbw __cacheline_aligned cbw;

	/* sanity checks */
	if (srb->cmdlen > CBWCDBLENGTH) {
		return VMM_EINVALID;
	}

	/* always OUT to the ep */
	pipe = usb_sndbulkpipe(us->dev, us->ep_out);

	cbw.dCBWSignature = vmm_cpu_to_le32(CBWSIGNATURE);
	cbw.dCBWTag = vmm_cpu_to_le32(us->CBWTag++);
	cbw.dCBWDataTransferLength = vmm_cpu_to_le32(srb->datalen);
	cbw.bCBWFlags = SCSI_CMD_DIRECTION(srb->cmd[0]) ?
					CBWFLAGS_IN : CBWFLAGS_OUT;
	cbw.bCBWLUN = srb->lun;
	cbw.bCDBLength = srb->cmdlen;
	/* copy the command data into the CBW command data buffer */
	/* DST SRC LEN!!! */
	memcpy(cbw.CBWCDB, srb->cmd, srb->cmdlen);

	return usb_bulk_msg(us->dev, pipe, &cbw, UMASS_BBB_CBW_SIZE,
			    &actlen, USB_CNTL_TIMEOUT * 5);
}

/* clear a stall on an endpoint - special for BBB devices */
static int usb_storage_BBB_clear_endpt_stall(struct usb_storage *us, u8 endpt)
{
	/* ENDPOINT_HALT = 0, so set value to 0 */
	return usb_control_msg(us->dev, usb_sndctrlpipe(us->dev, 0),
			       USB_REQ_CLEAR_FEATURE, USB_RECIP_ENDPOINT,
			       0, endpt, NULL, 0, NULL,
			       USB_CNTL_TIMEOUT * 5);
}

static int usb_storage_BBB_transport(struct scsi_request *srb,
				     struct scsi_transport *tr, void *priv)
{
	int rc, retry;
	int actlen, data_actlen;
	unsigned int ep, pipe;
	struct usb_storage *us = priv;
	struct usb_storage_bbb_csw __cacheline_aligned csw;

	/* COMMAND phase */
	rc = usb_storage_BBB_comdat(srb, us);
	if (rc < 0) {
		usb_storage_BBB_reset(tr, us);
		return rc;
	}

	/* Wait here for data to be ready */
	vmm_msleep(10);

	/* DATA phase + error handling */
	data_actlen = 0;
	/* no data, go immediately to the STATUS phase */
	if (srb->datalen == 0)
		goto st;
	pipe = SCSI_CMD_DIRECTION(srb->cmd[0]) ?
				usb_rcvbulkpipe(us->dev, us->ep_in) :
				usb_sndbulkpipe(us->dev, us->ep_out);
	ep = SCSI_CMD_DIRECTION(srb->cmd[0]) ? us->ep_in : us->ep_out;
	rc = usb_bulk_msg(us->dev, pipe, srb->data, srb->datalen,
			  &data_actlen, USB_CNTL_TIMEOUT * 5);
	/* special handling of STALL in DATA phase */
	if (rc < 0) {
		/* clear the STALL on the endpoint */
		rc = usb_storage_BBB_clear_endpt_stall(us, ep);
		if (rc >= 0)
			/* continue on to STATUS phase */
			goto st;
	}
	if (rc < 0) {
		usb_storage_BBB_reset(tr, us);
		return rc;
	}

	/* STATUS phase + error handling */
st:
	retry = 0;
again:
	rc = usb_bulk_msg(us->dev, usb_rcvbulkpipe(us->dev, us->ep_in),
			  &csw, UMASS_BBB_CSW_SIZE,
			  &actlen, USB_CNTL_TIMEOUT*5);
	/* special handling of STALL in STATUS phase */
	if ((rc < 0) && (retry < 1)) {
		/* clear the STALL on the endpoint */
		rc = usb_storage_BBB_clear_endpt_stall(us, us->ep_in);
		if (rc >= 0 && (retry++ < 1))
			/* do a retry */
			goto again;
	}
	if (rc < 0) {
		usb_storage_BBB_reset(tr, us);
		return rc;
	}

	/* misuse pipe to get the residue */
	pipe = vmm_le32_to_cpu(csw.dCSWDataResidue);
	if ((pipe == 0) &&
	    (srb->datalen != 0) &&
	    (srb->datalen - data_actlen != 0))
		pipe = srb->datalen - data_actlen;
	if (CSWSIGNATURE != vmm_le32_to_cpu(csw.dCSWSignature)) {
		usb_storage_BBB_reset(tr, us);
		return VMM_EIO;
	} else if ((us->CBWTag - 1) != vmm_le32_to_cpu(csw.dCSWTag)) {
		usb_storage_BBB_reset(tr, us);
		return VMM_EIO;
	} else if (csw.bCSWStatus > CSWSTATUS_PHASE) {
		usb_storage_BBB_reset(tr, us);
		return VMM_EIO;
	} else if (csw.bCSWStatus == CSWSTATUS_PHASE) {
		usb_storage_BBB_reset(tr, us);
		return VMM_EIO;
	} else if (data_actlen > srb->datalen) {
		return VMM_EIO;
	} else if (csw.bCSWStatus == CSWSTATUS_FAILED) {
		return VMM_EIO;
	}

	return VMM_OK;
}

static void usb_storage_info_fixup(struct scsi_info *info,
				   struct scsi_transport *tr, void *priv)
{
	struct usb_storage *us = priv;
	if ((us->dev->descriptor.idVendor == 0x0424) &&
	    (us->dev->descriptor.idProduct == 0x223a)) {
		strncpy(info->vendor, "SMSC", sizeof(info->vendor));
		strncpy(info->product, "Flash Controller",
			sizeof(info->product));
	}
}

static int usb_storage_max_luns(struct usb_device *dev,
				int ifnum, unsigned char *max_luns)
{
	int rc, len;
	unsigned char __cacheline_aligned result;

	rc = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
			     US_BBB_GET_MAX_LUN,
			     USB_TYPE_CLASS | USB_RECIP_INTERFACE | USB_DIR_IN,
			     0, ifnum, &result, sizeof(result),
			     &len, USB_CNTL_TIMEOUT * 5);
	if (rc) {
		return rc;
	}

	if (max_luns && (len > 0)) {
		*max_luns = result;
	}

	return VMM_OK;
}

static int usb_storage_probe(struct usb_interface *intf,
			     const struct usb_device_id *id)
{
	int rc = VMM_OK;;
	u32 i, lun;
	char name[VMM_FIELD_NAME_SIZE];
	unsigned char max_luns = 0;
	struct usb_storage *us;
	struct usb_endpoint_descriptor *ep_desc;
	struct usb_device *dev = interface_to_usbdev(intf);
	struct scsi_disk *disk;
	struct scsi_transport *tr = (struct scsi_transport *)id->driver_info;

	/* Get number of LUNs */
	rc = usb_storage_max_luns(dev, intf->desc.bInterfaceNumber, &max_luns);
	if (rc) {
		goto fail;
	}

	/* Update curent settings of usb interface */
	rc = usb_set_interface(dev, intf->desc.bInterfaceNumber, 0);
	if (rc) {
		goto fail;
	}

	/* Alloc usb storage instance */
	us = vmm_zalloc(sizeof(*us));
	if (!us) {
		rc = VMM_ENOMEM;
		goto fail;
	}
	usb_ref_device(dev);
	us->dev = dev;
	us->intf = intf;
	us->tr = tr;

	/*
	 * We are expecting a minimum of 2 endpoints - in and out (bulk).
	 * An optional interrupt is OK (necessary for CBI protocol).
	 * We will ignore any others.
	 */
	for (i = 0; i < intf->desc.bNumEndpoints; i++) {
		ep_desc = &intf->ep_desc[i];

		/* is it an BULK endpoint? */
		if ((ep_desc->bmAttributes &
		     USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK) {
			if (ep_desc->bEndpointAddress & USB_DIR_IN)
				us->ep_in = ep_desc->bEndpointAddress &
						USB_ENDPOINT_NUMBER_MASK;
			else
				us->ep_out =
					ep_desc->bEndpointAddress &
					USB_ENDPOINT_NUMBER_MASK;
		}

		/* is it an interrupt endpoint? */
		if ((ep_desc->bmAttributes &
		     USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_INT) {
			us->ep_int = ep_desc->bEndpointAddress &
						USB_ENDPOINT_NUMBER_MASK;
			us->irqinterval = ep_desc->bInterval;
		}
	}

	/* Do some basic sanity checks, and bail if we find a problem */
	if (!us->ep_in || !us->ep_out ||
	    (intf->desc.bInterfaceProtocol == US_PR_CBI && !us->ep_int)) {
		rc = VMM_ENODEV;
		goto fail_free_device;
	}

	/* we had found an interrupt endpoint, prepare irq pipe
	 * set up the IRQ pipe and handler
	 */
	if (us->ep_int) {
		us->irqinterval = (us->irqinterval > 0) ?
						us->irqinterval : 255;
		us->irqpipe = usb_rcvintpipe(us->dev, us->ep_int);
		us->irqmaxp = usb_maxpacket(us->dev, us->irqpipe);
	}

	/* Save number of LUNs */
	us->luns_count = max_luns + 1;
	if (us->luns_count > US_MAX_LUNS) {
		us->luns_count = US_MAX_LUNS;
	}

	/* Create SCSI disk for each LUN */
	vmm_printf("%s: USB Mass Storage Device\n", intf->dev.name);
	for (lun = 0; lun < us->luns_count; lun++) {
		rc = usb_storage_alloc_disk_num();
		if (rc < 0) {
			goto fail_free_disks;
		}
		us->luns[lun].disk_num = rc;
		us->luns[lun].lun = lun;
		vmm_snprintf(name, sizeof(name),
			     "usbdisk%d", us->luns[lun].disk_num);
		disk = scsi_create_disk(name, lun,
				US_MAX_PENDING, US_BLKS_PER_XFER,
				&us->intf->dev, us->tr, us);
		if (VMM_IS_ERR_OR_NULL(disk)) {
			rc = VMM_PTR_ERR(disk);
			usb_storage_free_disk_num(us->luns[lun].disk_num);
			goto fail_free_disks;
		}
		us->luns[lun].disk = disk;
		vmm_printf("%s: Created SCSI Disk %s\n",
			   intf->dev.name, name);
	}

	/* Set usb interface data */
	interface_set_data(intf, us);

	return VMM_OK;

fail_free_disks:
	for (lun = 0; lun < us->luns_count; lun++) {
		if (us->luns[lun].disk) {
			usb_storage_free_disk_num(us->luns[lun].disk_num);
			scsi_destroy_disk(us->luns[lun].disk);
			us->luns[lun].disk = NULL;
		}
	}
fail_free_device:
	usb_dref_device(us->dev);
	vmm_free(us);
fail:
	return rc;
}

static void usb_storage_disconnect(struct usb_interface *intf)
{
	u32 lun;
	struct usb_storage *us = interface_get_data(intf);

	/* Clear usb interface data */
	interface_set_data(intf, NULL);

	/* Destroy SCSI disk of each LUN */
	for (lun = 0; lun < us->luns_count; lun++) {
		if (us->luns[lun].disk) {
			usb_storage_free_disk_num(us->luns[lun].disk_num);
			scsi_destroy_disk(us->luns[lun].disk);
			us->luns[lun].disk = NULL;
		}
	}

	/* Free the USB device */
	usb_dref_device(us->dev);

	/* Free usb storage instance */
	vmm_free(us);
}

static struct scsi_transport bulk = {
	.name = "Bulk/Bulk/Bulk",
	.transport = usb_storage_BBB_transport,
	.reset = usb_storage_BBB_reset,
	.info_fixup = usb_storage_info_fixup,
};

static const struct usb_device_id usb_storage_products[] = {
{
	USB_INTERFACE_INFO(USB_CLASS_MASS_STORAGE, US_SC_SCSI, US_PR_BULK),
	.driver_info = (unsigned long)&bulk,
},
{
	USB_INTERFACE_INFO(USB_CLASS_MASS_STORAGE, US_SC_UFI, US_PR_BULK),
	.driver_info = (unsigned long)&bulk,
},
{
	USB_INTERFACE_INFO(USB_CLASS_MASS_STORAGE, US_SC_8070, US_PR_BULK),
	.driver_info = (unsigned long)&bulk,
},
{ },	/* END */
};

static struct usb_driver usb_storage_driver = {
	.name		= "usb_storage",
	.id_table	= usb_storage_products,
	.probe		= usb_storage_probe,
	.disconnect	= usb_storage_disconnect,
};

static int __init usb_storage_init(void)
{
	INIT_SPIN_LOCK(&us_disk_bmap_lock);
	bitmap_fill(us_disk_bmap, US_MAX_DISKS);
	return usb_register(&usb_storage_driver);
}

static void __exit usb_storage_exit(void)
{
	usb_deregister(&usb_storage_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
