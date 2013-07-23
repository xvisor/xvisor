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
 * @file message.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of usb device message APIs
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_completion.h>
#include <vmm_modules.h>
#include <drv/usb.h>
#include <drv/usb/hcd.h>
#include <drv/usb/hub.h>

#undef DEBUG

#if defined(DEBUG)
#define DPRINTF(msg...)			vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define USB_BUFSIZ			512

static void urb_request_complete(struct urb *u)
{
	struct vmm_completion *uc = u->context;

	vmm_completion_complete(uc);
}

int usb_control_msg(struct usb_device *dev, u32 pipe,
		    u8 request, u8 requesttype, u16 value, u16 index,
		    void *data, u16 size, int timeout)
{
	int rc;
	u64 tout;
	struct urb u;
	struct vmm_completion uc;
	struct usb_devrequest setup_packet;

	/* Initialize setup packet */
	setup_packet.requesttype = requesttype;
	setup_packet.request = request;
	setup_packet.value = vmm_cpu_to_le16(value);
	setup_packet.index = vmm_cpu_to_le16(index);
	setup_packet.length = vmm_cpu_to_le16(size);
	DPRINTF("%s: request: 0x%X, requesttype: 0x%X, " \
		"value 0x%X index 0x%X length 0x%X\n", 
		__func__, request, requesttype, value, index, size);

	/* Initialize URB */
	usb_init_urb(&u);

	/* Initialize URB completion */
	INIT_COMPLETION(&uc);

	/* Fill URB */
	usb_fill_control_urb(&u, dev, pipe, 
			     (unsigned char *)&setup_packet, data, size, 
			     urb_request_complete, &uc);

	/* Submit URB */
	rc = usb_hcd_submit_urb(&u);
	if (rc) {
		return rc;
	}

	/* Wait for completion */
	if (timeout < 1) {
		vmm_completion_wait(&uc);
		rc = VMM_OK;
	} else {
		tout = timeout * 1000000ULL;
		rc = vmm_completion_wait_timeout(&uc, &tout);
	}
	if (rc) {
		return rc;
	}

	/* If URB failed then return status */
	if (u.status < 0) {
		return u.status;
	}

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(usb_control_msg);

int usb_interrupt_msg(struct usb_device *dev, u32 pipe,
		      void *data, int len, int interval)
{
	int rc;
	struct urb u;
	struct vmm_completion uc;

	/* Initialize URB */
	usb_init_urb(&u);

	/* Initialize URB completion */
	INIT_COMPLETION(&uc);

	/* Fill Interrupt URB */
	usb_fill_int_urb(&u, dev, pipe, 
			  data, len, 
			  urb_request_complete, &uc, interval);

	/* Submit URB */
	rc = usb_hcd_submit_urb(&u);
	if (rc) {
		return rc;
	}

	/* Wait for completion */
	vmm_completion_wait(&uc);

	/* If URB failed then return status */
	if (u.status < 0) {
		return u.status;
	}

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(usb_interrupt_msg);

int usb_bulk_msg(struct usb_device *dev, u32 pipe,
		 void *data, int len, int *actual_length, int timeout)
{
	int rc;
	u64 tout;
	struct urb u;
	struct vmm_completion uc;

	/* Initialize URB */
	usb_init_urb(&u);

	/* Initialize URB completion */
	INIT_COMPLETION(&uc);

	/* Fill Bulk URB */
	usb_fill_bulk_urb(&u, dev, pipe, 
			  data, len, 
			  urb_request_complete, &uc);

	/* Submit URB */
	rc = usb_hcd_submit_urb(&u);
	if (rc) {
		return rc;
	}

	/* Wait for completion */
	if (timeout < 1) {
		vmm_completion_wait(&uc);
		rc = VMM_OK;
	} else {
		tout = timeout * 1000000ULL;
		rc = vmm_completion_wait_timeout(&uc, &tout);
	}
	if (rc) {
		return rc;
	}

	/* If URB failed then return status */
	if (u.status < 0) {
		return u.status;
	}

	/* Return actual transfer length */
	if (actual_length) {
		*actual_length = u.actual_length;
	}

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(usb_bulk_msg);

/*
 * returns the max packet size, depending on the pipe direction and
 * the configurations values
 */
int usb_maxpacket(struct usb_device *dev, u32 pipe)
{
	/* direction is out -> use emaxpacket out */
	if ((pipe & USB_DIR_IN) == 0) {
		return dev->epmaxpacketout[((pipe>>15) & 0xf)];
	} else {
		return dev->epmaxpacketin[((pipe>>15) & 0xf)];
	}
}
VMM_EXPORT_SYMBOL(usb_maxpacket);

int usb_get_descriptor(struct usb_device *dev, u8 desctype, 
		       u8 descindex, void *buf, int size)
{
	return usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
			USB_REQ_GET_DESCRIPTOR, USB_DIR_IN,
			(desctype << 8) + descindex, 0,
			buf, size, USB_CNTL_TIMEOUT);
}
VMM_EXPORT_SYMBOL(usb_get_descriptor);

static int usb_get_string(struct usb_device *dev, 
			  unsigned short langid, unsigned char index, 
			  void *buf, int size)
{
	int i, result;

	for (i = 0; i < 3; ++i) {
		/* some devices are flaky */
		result = usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
					USB_REQ_GET_DESCRIPTOR, USB_DIR_IN,
					(USB_DT_STRING << 8) + index, 
					langid, buf, size,
					USB_CNTL_TIMEOUT);
		if (result > 0) {
			break;
		}
	}

	return result;
}

static void usb_try_string_workarounds(unsigned char *buf, int *length)
{
	int newlength, oldlength = *length;

	for (newlength = 2; newlength + 1 < oldlength; newlength += 2) {
		if (!vmm_isprintable(buf[newlength]) || buf[newlength + 1]) {
			break;
		}
	}

	if (newlength > 2) {
		buf[0] = newlength;
		*length = newlength;
	}
}

static int usb_string_sub(struct usb_device *dev, 
			  unsigned int langid, unsigned int index, 
			  unsigned char *buf)
{
	int rc;

	/* Try to read the string descriptor by asking for the maximum
	 * possible number of bytes */
	rc = usb_get_string(dev, langid, index, buf, 255);

	/* If that failed try to read the descriptor length, then
	 * ask for just that many bytes */
	if (rc < 2) {
		rc = usb_get_string(dev, langid, index, buf, 2);
		if (rc == 2) {
			rc = usb_get_string(dev, langid, index, buf, buf[0]);
		}
	}

	if (rc >= 2) {
		if (!buf[0] && !buf[1]) {
			usb_try_string_workarounds(buf, &rc);
		}

		/* There might be extra junk at the end of the descriptor */
		if (buf[0] < rc) {
			rc = buf[0];
		}

		rc = rc - (rc & 1); /* force a multiple of two */
	}

	if (rc < 2) {
		rc = -1;
	}

	return rc;
}

int usb_string(struct usb_device *dev, int index, char *buf, size_t size)
{
	int err;
	unsigned int u, idx;
	unsigned char *tbuf, *mybuf;

	if (size <= 0 || !buf || !index) {
		return VMM_EINVALID;
	}

	mybuf = vmm_malloc(USB_BUFSIZ);
	if (!mybuf) {
		return VMM_ENOMEM;
	}

	buf[0] = 0;
	tbuf = &mybuf[0];

	/* get langid for strings if it's not yet known */
	if (!dev->have_langid) {
		err = usb_string_sub(dev, 0, 0, tbuf);
		if (err < 0) {
			DPRINTF("%s: error getting string descriptor 0 " \
				"(error=%lx)\n", __func__, dev->status);
			goto done;
		} else if (tbuf[0] < 4) {
			DPRINTF("%s: string descriptor 0 too short\n", 
				__func__);
			err = VMM_EINVALID;
			goto done;
		} else {
			dev->have_langid = -1;
			dev->string_langid = tbuf[2] | (tbuf[3] << 8);
				/* always use the first langid listed */
			DPRINTF("%s: USB device number %d default " \
				"language ID 0x%x\n", __func__, 
				dev->devnum, dev->string_langid);
		}
	}

	err = usb_string_sub(dev, dev->string_langid, index, tbuf);
	if (err < 0) {
		goto done;
	}

	size--;		/* leave room for trailing NULL char in output buffer */
	for (idx = 0, u = 2; u < err; u += 2) {
		if (idx >= size) {
			break;
		}
		if (tbuf[u+1]) {		/* high byte */
			buf[idx++] = '?';  /* non-ASCII character */
		} else {
			buf[idx++] = tbuf[u];
		}
	}
	buf[idx] = 0;

	err = idx;

done:
	vmm_free(mybuf);
	return err;
}
VMM_EXPORT_SYMBOL(usb_string);

int usb_set_protocol(struct usb_device *dev, int ifnum, int protocol)
{
	return usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
				USB_REQ_SET_PROTOCOL, 
				USB_TYPE_CLASS | USB_RECIP_INTERFACE,
				protocol, ifnum, 
				NULL, 0, USB_CNTL_TIMEOUT);
}
VMM_EXPORT_SYMBOL(usb_set_protocol);

int usb_set_idle(struct usb_device *dev, int ifnum, 
		 int duration, int report_id)
{
	return usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
				USB_REQ_SET_IDLE, 
				USB_TYPE_CLASS | USB_RECIP_INTERFACE,
				(duration << 8) | report_id, ifnum, 
				NULL, 0, USB_CNTL_TIMEOUT);
}
VMM_EXPORT_SYMBOL(usb_set_idle);

int usb_set_interface(struct usb_device *dev, int ifnum, int alternate)
{
	struct usb_interface *intf = NULL;
	int ret, i;

	for (i = 0; i < dev->config.desc.bNumInterfaces; i++) {
		if (dev->config.intf[i].desc.bInterfaceNumber == ifnum) {
			intf = &dev->config.intf[i];
			break;
		}
	}
	if (!intf) {
		vmm_printf("%s: selecting invalid interface %d", 
			   __func__, ifnum);
		return VMM_EINVALID;
	}

	/* We should return now for devices with only one alternate setting.
	 * According to 9.4.10 of the Universal Serial Bus Specification
	 * Revision 2.0 such devices can return with a STALL. This results in
	 * some USB sticks timeouting during initialization and then being
	 * unusable in U-Boot.
	 */
	if (intf->num_altsetting == 1) {
		return VMM_OK;
	}

	ret = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
				USB_REQ_SET_INTERFACE, 
				USB_RECIP_INTERFACE,
				alternate, ifnum, 
				NULL, 0, USB_CNTL_TIMEOUT * 5);
	if (ret < 0) {
		return ret;
	}

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(usb_set_interface);

int usb_get_configuration_no(struct usb_device *dev, u8 *buffer, int cfgno)
{
	int result;
	unsigned int tmp;
	struct usb_config_descriptor *config;

	config = (struct usb_config_descriptor *)&buffer[0];
	result = usb_get_descriptor(dev, USB_DT_CONFIG, cfgno, buffer, 9);
	if (result < 9) {
		if (result < 0) {
			vmm_printf("%s: unable to get descriptor, error %lX\n",
				   __func__, dev->status);
		} else {
			vmm_printf("%s: config descriptor too short " \
				   "(expected %d, got %d)\n",
				   __func__, 9, result);
		}
		return -1;
	}
	tmp = vmm_le16_to_cpu(config->wTotalLength);

	if (tmp > USB_BUFSIZ) {
		vmm_printf("%s: failed to get descriptor - too long: %d\n", 
			   __func__, tmp);
		return VMM_ENOMEM;
	}

	result = usb_get_descriptor(dev, USB_DT_CONFIG, cfgno, buffer, tmp);
	DPRINTF("%s: cfgno %d, result %d, wLength %d\n", 
		__func__, cfgno, result, tmp);

	return result;
}
VMM_EXPORT_SYMBOL(usb_get_configuration_no);

int usb_get_report(struct usb_device *dev, int ifnum, 
		   u8 type, u8 id, void *buf, u32 size)
{
	return usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
			USB_REQ_GET_REPORT,
			USB_DIR_IN | USB_TYPE_CLASS | USB_RECIP_INTERFACE,
			(type << 8) + id, ifnum, 
			buf, size, USB_CNTL_TIMEOUT);
}
VMM_EXPORT_SYMBOL(usb_get_report);

int usb_get_class_descriptor(struct usb_device *dev, int ifnum,
			     u8 type, u8 id, void *buf, u32 size)
{
	return usb_control_msg(dev, usb_rcvctrlpipe(dev, 0),
				USB_REQ_GET_DESCRIPTOR, 
				USB_RECIP_INTERFACE | USB_DIR_IN,
				(type << 8) + id, ifnum, 
				buf, size, USB_CNTL_TIMEOUT);
}
VMM_EXPORT_SYMBOL(usb_get_class_descriptor);

int usb_clear_halt(struct usb_device *dev, u32 pipe)
{
	int result;
	int endp = usb_pipeendpoint(pipe)|(usb_pipein(pipe)<<7);

	result = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
				 USB_REQ_CLEAR_FEATURE, 
				 USB_RECIP_ENDPOINT, 
				 0, endp, 
				 NULL, 0, USB_CNTL_TIMEOUT * 3);
	/* don't clear if failed */
	if (result < 0) {
		return result;
	}

	/* NOTE: we do not get status and verify reset was successful
	 * as some devices are reported to lock up upon this check..
	 */

	usb_endpoint_running(dev, usb_pipeendpoint(pipe), usb_pipeout(pipe));

	/* toggle is reset on clear */
	usb_settoggle(dev, usb_pipeendpoint(pipe), usb_pipeout(pipe), 0);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(usb_clear_halt);

