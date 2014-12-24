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
 * @file dwc2.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Designware USB2.0 host controller driver.
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_delay.h>
#include <vmm_spinlocks.h>
#include <vmm_workqueue.h>
#include <vmm_host_irq.h>
#include <vmm_host_aspace.h>
#include <vmm_completion.h>
#include <vmm_threads.h>
#include <vmm_modules.h>
#include <vmm_devdrv.h>
#include <drv/usb.h>
#include <drv/usb/ch11.h>
#include <drv/usb/roothubdesc.h>
#include <drv/usb/hcd.h>

#include "dwc2.h"

#define MODULE_DESC			"Designware USB2.0 HCD Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(USB_CORE_IPRIORITY + 1)
#define	MODULE_INIT			dwc2_driver_init
#define	MODULE_EXIT			dwc2_driver_exit

/**
 * Parameters for configuring the dwc2 driver
 *
 * @otg_cap:            Specifies the OTG capabilities.
 *                       0 - HNP and SRP capable
 *                       1 - SRP Only capable
 *                       2 - No HNP/SRP capable (always available)
 *                      Defaults to best available option (0, 1, then 2)
 * @otg_ver:            OTG version supported
 *                       0 - 1.3 (default)
 *                       1 - 2.0
 * @dma_enable:         Specifies whether to use slave or DMA mode for accessing
 *                      the data FIFOs. The driver will automatically detect the
 *                      value for this parameter if none is specified.
 *                       0 - Slave (always available)
 *                       1 - DMA (default, if available)
 * @dma_desc_enable:    When DMA mode is enabled, specifies whether to use
 *                      address DMA mode or descriptor DMA mode for accessing
 *                      the data FIFOs. The driver will automatically detect the
 *                      value for this if none is specified.
 *                       0 - Address DMA
 *                       1 - Descriptor DMA (default, if available)
 * @speed:              Specifies the maximum speed of operation in host and
 *                      device mode. The actual speed depends on the speed of
 *                      the attached device and the value of phy_type.
 *                       0 - High Speed
 *                           (default when phy_type is UTMI+ or ULPI)
 *                       1 - Full Speed
 *                           (default when phy_type is Full Speed)
 * @enable_dynamic_fifo: 0 - Use coreConsultant-specified FIFO size parameters
 *                       1 - Allow dynamic FIFO sizing (default, if available)
 * @en_multiple_tx_fifo: Specifies whether dedicated per-endpoint transmit FIFOs
 *                      are enabled
 * @host_rx_fifo_size:  Number of 4-byte words in the Rx FIFO in host mode when
 *                      dynamic FIFO sizing is enabled
 *                       16 to 32768
 *                      Actual maximum value is autodetected and also
 *                      the default.
 * @host_nperio_tx_fifo_size: Number of 4-byte words in the non-periodic Tx FIFO
 *                      in host mode when dynamic FIFO sizing is enabled
 *                       16 to 32768
 *                      Actual maximum value is autodetected and also
 *                      the default.
 * @host_perio_tx_fifo_size: Number of 4-byte words in the periodic Tx FIFO in
 *                      host mode when dynamic FIFO sizing is enabled
 *                       16 to 32768
 *                      Actual maximum value is autodetected and also
 *                      the default.
 * @max_transfer_size:  The maximum transfer size supported, in bytes
 *                       2047 to 65,535
 *                      Actual maximum value is autodetected and also
 *                      the default.
 * @max_packet_count:   The maximum number of packets in a transfer
 *                       15 to 511
 *                      Actual maximum value is autodetected and also
 *                      the default.
 * @host_channels:      The number of host channel registers to use
 *                       1 to 16
 *                      Actual maximum value is autodetected and also
 *                      the default.
 * @phy_type:           Specifies the type of PHY interface to use. By default,
 *                      the driver will automatically detect the phy_type.
 *                       0 - Full Speed Phy
 *                       1 - UTMI+ Phy
 *                       2 - ULPI Phy
 *                      Defaults to best available option (2, 1, then 0)
 * @phy_utmi_width:     Specifies the UTMI+ Data Width (in bits). This parameter
 *                      is applicable for a phy_type of UTMI+ or ULPI. (For a
 *                      ULPI phy_type, this parameter indicates the data width
 *                      between the MAC and the ULPI Wrapper.) Also, this
 *                      parameter is applicable only if the OTG_HSPHY_WIDTH cC
 *                      parameter was set to "8 and 16 bits", meaning that the
 *                      core has been configured to work at either data path
 *                      width.
 *                       8 or 16 (default 16 if available)
 * @phy_ulpi_ddr:       Specifies whether the ULPI operates at double or single
 *                      data rate. This parameter is only applicable if phy_type
 *                      is ULPI.
 *                       0 - single data rate ULPI interface with 8 bit wide
 *                           data bus (default)
 *                       1 - double data rate ULPI interface with 4 bit wide
 *                           data bus
 * @phy_ulpi_ext_vbus:  For a ULPI phy, specifies whether to use the internal or
 *                      external supply to drive the VBus
 *                       0 - Internal supply (default)
 *                       1 - External supply
 * @i2c_enable:         Specifies whether to use the I2Cinterface for a full
 *                      speed PHY. This parameter is only applicable if phy_type
 *                      is FS.
 *                       0 - No (default)
 *                       1 - Yes
 * @ulpi_fs_ls:         Make ULPI phy operate in FS/LS mode only
 *                       0 - No (default)
 *                       1 - Yes
 * @host_support_fs_ls_low_power: Specifies whether low power mode is supported
 *                      when attached to a Full Speed or Low Speed device in
 *                      host mode.
 *                       0 - Don't support low power mode (default)
 *                       1 - Support low power mode
 * @host_ls_low_power_phy_clk: Specifies the PHY clock rate in low power mode
 *                      when connected to a Low Speed device in host
 *                      mode. This parameter is applicable only if
 *                      host_support_fs_ls_low_power is enabled.
 *                       0 - 48 MHz
 *                           (default when phy_type is UTMI+ or ULPI)
 *                       1 - 6 MHz
 *                           (default when phy_type is Full Speed)
 * @ts_dline:           Enable Term Select Dline pulsing
 *                       0 - No (default)
 *                       1 - Yes
 * @reload_ctl:         Allow dynamic reloading of HFIR register during runtime
 *                       0 - No (default for core < 2.92a)
 *                       1 - Yes (default for core >= 2.92a)
 * @ahbcfg:             This field allows the default value of the GAHBCFG
 *                      register to be overridden
 *                       -1         - GAHBCFG value will be set to 0x06
 *                                    (INCR4, default)
 *                       all others - GAHBCFG value will be overridden with
 *                                    this value
 *                      Not all bits can be controlled like this, the
 *                      bits defined by GAHBCFG_CTRL_MASK are controlled
 *                      by the driver and are ignored in this
 *                      configuration value.
 * @uframe_sched:       True to enable the microframe scheduler
 *
 * The following parameters may be specified when starting the module. These
 * parameters define how the DWC_otg controller should be configured. A
 * value of -1 (or any other out of range value) for any parameter means
 * to read the value from hardware (if possible) or use the builtin
 * default described above.
 */
struct dwc2_core_params {
	/*
	 * Don't add any non-int members here, this will break
	 * dwc2_set_all_params!
	 */
	int otg_cap;
	int otg_ver;
	int dma_enable;
	int dma_desc_enable;
	int speed;
	int enable_dynamic_fifo;
	int en_multiple_tx_fifo;
	int host_rx_fifo_size;
	int host_nperio_tx_fifo_size;
	int host_perio_tx_fifo_size;
	int max_transfer_size;
	int max_packet_count;
	int host_channels;
	int phy_type;
	int phy_utmi_width;
	int phy_ulpi_ddr;
	int phy_ulpi_ext_vbus;
	int i2c_enable;
	int ulpi_fs_ls;
	int host_support_fs_ls_low_power;
	int host_ls_low_power_phy_clk;
	int ts_dline;
	int reload_ctl;
	int ahbcfg;
	int uframe_sched;
};

struct dwc2_control {
	const struct dwc2_core_params *params;
	struct dwc2_core_regs *regs;
	u32 irq;
	u32 rh_devnum;

	vmm_spinlock_t urb_lock;
	struct dlist urb_pending_list;
	struct vmm_completion urb_pending;
	struct vmm_thread *urb_thread;
};

static int dwc2_rh_msg_in_status(struct dwc2_control *dwc2,
				 struct urb *u,
				 struct usb_devrequest *cmd)
{
	int len = 0, rc = VMM_OK;
	u32 hprt0 = 0, port_status = 0, port_change = 0;
	void *buffer = u->transfer_buffer;
	int buffer_len = u->transfer_buffer_length;

	switch (cmd->requesttype & ~USB_DIR_IN) {
	case 0:
		*(u16 *)buffer = vmm_cpu_to_le16(1);
		len = 2;
		break;
	case USB_RECIP_INTERFACE:
	case USB_RECIP_ENDPOINT:
		*(u16 *)buffer = vmm_cpu_to_le16(0);
		len = 2;
		break;
	case USB_TYPE_CLASS:
		*(u32 *)buffer = vmm_cpu_to_le32(0);
		len = 4;
		break;
	case USB_RECIP_OTHER | USB_TYPE_CLASS:
		hprt0 = vmm_readl(&dwc2->regs->hprt0);
		if (hprt0 & DWC2_HPRT0_PRTCONNSTS)
			port_status |= USB_PORT_STAT_CONNECTION;
		if (hprt0 & DWC2_HPRT0_PRTENA)
			port_status |= USB_PORT_STAT_ENABLE;
		if (hprt0 & DWC2_HPRT0_PRTSUSP)
			port_status |= USB_PORT_STAT_SUSPEND;
		if (hprt0 & DWC2_HPRT0_PRTOVRCURRACT)
			port_status |= USB_PORT_STAT_OVERCURRENT;
		if (hprt0 & DWC2_HPRT0_PRTRST)
			port_status |= USB_PORT_STAT_RESET;
		if (hprt0 & DWC2_HPRT0_PRTPWR)
			port_status |= USB_PORT_STAT_POWER;

		port_status |= USB_PORT_STAT_HIGH_SPEED;

		if (hprt0 & DWC2_HPRT0_PRTENCHNG)
			port_change |= USB_PORT_STAT_C_ENABLE;
		if (hprt0 & DWC2_HPRT0_PRTCONNDET)
			port_change |= USB_PORT_STAT_C_CONNECTION;
		if (hprt0 & DWC2_HPRT0_PRTOVRCURRCHNG)
			port_change |= USB_PORT_STAT_C_OVERCURRENT;

		*(u32 *)buffer = vmm_cpu_to_le32(port_status |
					(port_change << 16));
		len = 4;
		break;
	default:
		rc = VMM_ENOTAVAIL;
		break;
	};

	if (rc == VMM_ENOTAVAIL) {
		vmm_printf("%s: dev=%s unsupported root hub command\n",
			   __func__, u->dev->dev.name);
	}

	u->actual_length = min(len, buffer_len);

	return rc;
}

static int dwc2_rh_msg_in_descriptor(struct dwc2_control *dwc2,
				     struct urb *u,
				     struct usb_devrequest *cmd)
{
	u32 dsc;
	u8 data[32];
	int len = 0, rc = VMM_OK;
	u16 wValue = vmm_cpu_to_le16(cmd->value);
	u16 wLength = vmm_cpu_to_le16(cmd->length);
	void *buffer = u->transfer_buffer;
	int buffer_len = u->transfer_buffer_length;

	switch (cmd->requesttype & ~USB_DIR_IN) {
	case 0:
		switch (wValue & 0xff00) {
		case 0x0100:	/* device descriptor */
			len = min3(buffer_len,
				   (int)sizeof(root_hub_dev_desc),
				   (int)wLength);
			memcpy(buffer, root_hub_dev_desc, len);
			break;
		case 0x0200:	/* configuration descriptor */
			len = min3(buffer_len,
				   (int)sizeof(root_hub_config_desc),
				   (int)wLength);
			memcpy(buffer, root_hub_config_desc, len);
			break;
		case 0x0300:	/* string descriptors */
			switch (wValue & 0xff) {
			case 0x00:
				len = min3(buffer_len,
					   (int)sizeof(root_hub_str_index0),
					   (int)wLength);
				memcpy(buffer, root_hub_str_index0, len);
				break;
			case 0x01:
				len = min3(buffer_len,
					   (int)sizeof(root_hub_str_index1),
					   (int)wLength);
				memcpy(buffer, root_hub_str_index1, len);
				break;
			case 0x02:
				len = min3(buffer_len,
					   (int)sizeof(root_hub_str_index2),
					   (int)wLength);
				memcpy(buffer, root_hub_str_index2, len);
				break;
			case 0x03:
				len = min3(buffer_len,
					   (int)sizeof(root_hub_str_index3),
					   (int)wLength);
				memcpy(buffer, root_hub_str_index3, len);
				break;
			};
			break;
		default:
			rc = VMM_ENOTAVAIL;
			break;
		};
		break;
	case USB_TYPE_CLASS:
		/* Root port config, set 1 port and nothing else. */
		dsc = 0x00000001;

		data[0] = 9;		/* min length; */
		data[1] = 0x29;
		data[2] = dsc & RH_A_NDP;
		data[3] = 0;
		if (dsc & RH_A_PSM)
			data[3] |= 0x1;
		if (dsc & RH_A_NOCP)
			data[3] |= 0x10;
		else if (dsc & RH_A_OCPM)
			data[3] |= 0x8;

		/* corresponds to data[4-7] */
		data[5] = (dsc & RH_A_POTPGT) >> 24;
		data[7] = dsc & RH_B_DR;
		if (data[2] < 7) {
			data[8] = 0xff;
		} else {
			data[0] += 2;
			data[8] = (dsc & RH_B_DR) >> 8;
			data[9] = 0xff;
			data[10] = data[9];
		}

		len = min3(buffer_len, (int)data[0], (int)wLength);
		memcpy(buffer, data, len);
		break;
	default:
		rc = VMM_ENOTAVAIL;
		break;
	};

	if (rc == VMM_ENOTAVAIL) {
		vmm_printf("%s: dev=%s unsupported root hub command\n",
			   __func__, u->dev->dev.name);
	}

	u->actual_length = min(len, buffer_len);

	return rc;
}

static int dwc2_rh_msg_in_configuration(struct dwc2_control *dwc2,
					struct urb *u,
					struct usb_devrequest *cmd)
{
	int len = 0, rc = VMM_OK;
	void *buffer = u->transfer_buffer;
	int buffer_len = u->transfer_buffer_length;

	switch (cmd->requesttype & ~USB_DIR_IN) {
	case 0:
		*(u8 *)buffer = 0x01;
		len = 1;
		break;
	default:
		rc = VMM_ENOTAVAIL;
		break;
	}

	if (rc == VMM_ENOTAVAIL) {
		vmm_printf("%s: dev=%s unsupported root hub command\n",
			   __func__, u->dev->dev.name);
	}

	u->actual_length = min(len, buffer_len);

	return rc;
}

static int dwc2_rh_msg_in(struct dwc2_control *dwc2,
			   struct urb *u, struct usb_devrequest *cmd)
{
	switch (cmd->request) {
	case USB_REQ_GET_STATUS:
		return dwc2_rh_msg_in_status(dwc2, u, cmd);
	case USB_REQ_GET_DESCRIPTOR:
		return dwc2_rh_msg_in_descriptor(dwc2, u, cmd);
	case USB_REQ_GET_CONFIGURATION:
		return dwc2_rh_msg_in_configuration(dwc2, u, cmd);
	default:
		break;
	}

	vmm_printf("%s: dev=%s unsupported root hub command\n",
		   __func__, u->dev->dev.name);

	return VMM_EINVALID;
}

static int dwc2_rh_msg_out(struct dwc2_control *dwc2,
			   struct urb *u, struct usb_devrequest *cmd)
{
	int rc = VMM_OK;
	u16 bmrtype_breq = cmd->requesttype | (cmd->request << 8);
	u16 wValue = vmm_cpu_to_le16(cmd->value);

	switch (bmrtype_breq & ~USB_DIR_IN) {
	case (USB_REQ_CLEAR_FEATURE << 8) | USB_RECIP_ENDPOINT:
	case (USB_REQ_CLEAR_FEATURE << 8) | USB_TYPE_CLASS:
		break;
	case (USB_REQ_CLEAR_FEATURE << 8) | USB_RECIP_OTHER | USB_TYPE_CLASS:
		switch (wValue) {
		case USB_PORT_FEAT_C_CONNECTION:
			vmm_setbits_le32(&dwc2->regs->hprt0,
					 DWC2_HPRT0_PRTCONNDET);
			break;
		};
		break;
	case (USB_REQ_SET_FEATURE << 8) | USB_RECIP_OTHER | USB_TYPE_CLASS:
		switch (wValue) {
		case USB_PORT_FEAT_SUSPEND:
			break;
		case USB_PORT_FEAT_RESET:
			vmm_clrsetbits_le32(&dwc2->regs->hprt0,
					DWC2_HPRT0_PRTENA |
					DWC2_HPRT0_PRTCONNDET |
					DWC2_HPRT0_PRTENCHNG |
					DWC2_HPRT0_PRTOVRCURRCHNG,
					DWC2_HPRT0_PRTRST);
			vmm_mdelay(50);
			vmm_clrbits_le32(&dwc2->regs->hprt0,
					DWC2_HPRT0_PRTRST);
			break;
		case USB_PORT_FEAT_POWER:
			vmm_clrsetbits_le32(&dwc2->regs->hprt0,
					DWC2_HPRT0_PRTENA |
					DWC2_HPRT0_PRTCONNDET |
					DWC2_HPRT0_PRTENCHNG |
					DWC2_HPRT0_PRTOVRCURRCHNG,
					DWC2_HPRT0_PRTRST);
			break;
		case USB_PORT_FEAT_ENABLE:
			break;
		};
		break;
	case (USB_REQ_SET_ADDRESS << 8):
		dwc2->rh_devnum = wValue;
		break;
	case (USB_REQ_SET_CONFIGURATION << 8):
		break;
	default:
		rc = VMM_ENOTAVAIL;
		break;
	};

	if (rc == VMM_ENOTAVAIL) {
		vmm_printf("%s: dev=%s unsupported root hub command\n",
			   __func__, u->dev->dev.name);
	}

	u->actual_length = 0;

	return rc;
}

static int dwc2_control_rh_msg(struct dwc2_control *dwc2,
			       struct urb *u)
{
	int rc;
	struct usb_devrequest *cmd =
			(struct usb_devrequest *)u->setup_packet;

	if (cmd->requesttype & USB_DIR_IN) {
		rc = dwc2_rh_msg_in(dwc2, u, cmd);
	} else {
		rc = dwc2_rh_msg_out(dwc2, u, cmd);
	}

	vmm_mdelay(1);

	return rc;
}

static vmm_irq_return_t	dwc2_irq(struct usb_hcd *hcd)
{
	/* For now nothing to do here. */
	return VMM_IRQ_NONE;
}

static int dwc2_control_msg(struct dwc2_control *dwc2,
			    struct urb *u)
{
	if (u->dev->devnum == dwc2->rh_devnum) {
		return dwc2_control_rh_msg(dwc2, u);
	}

	/* FIXME: */
	return VMM_ENOTAVAIL;
}

static int dwc2_bulk_msg(struct dwc2_control *dwc2,
			 struct urb *u)
{
	/* FIXME: */
	return VMM_ENOTAVAIL;
}

static int dwc2_int_msg(struct dwc2_control *dwc2,
			struct urb *u)
{
	vmm_printf("%s: dev=%s pipe=0x%x buf=%p len=%d interval=%d\n",
		   __func__, u->dev->dev.name, u->pipe, u->transfer_buffer,
		   u->transfer_buffer_length, u->interval);
	return VMM_ENOTAVAIL;
}

static int dwc2_worker(void *data)
{
	int rc;
	irq_flags_t flags;
	struct urb *u;
	struct usb_hcd *hcd = data;
	struct dwc2_control *dwc2 = usb_hcd_priv(hcd);

	while (1) {
		vmm_completion_wait(&dwc2->urb_pending);

		u = NULL;
		vmm_spin_lock_irqsave(&dwc2->urb_lock, flags);
		if (!list_empty(&dwc2->urb_pending_list)) {
			u = list_first_entry(&dwc2->urb_pending_list,
					     struct urb, urb_list);
			list_del(&u->urb_list);
		}
		vmm_spin_unlock_irqrestore(&dwc2->urb_lock, flags);
		if (!u) {
			continue;
		}

		switch (usb_pipetype(u->pipe)) {
		case USB_PIPE_CONTROL:
			rc = dwc2_control_msg(dwc2, u);
			break;
		case USB_PIPE_BULK:
			rc = dwc2_bulk_msg(dwc2, u);
			break;
		case USB_PIPE_INTERRUPT:
			rc = dwc2_int_msg(dwc2, u);
			break;
		default:
			rc = VMM_EINVALID;
			break;
		};

		usb_hcd_giveback_urb(hcd, u, rc);
	}

	return VMM_OK;
}

static void dwc2_flush_work(struct usb_hcd *hcd)
{
	struct urb *u;
	irq_flags_t flags;
	struct dwc2_control *dwc2 =
			(struct dwc2_control *)usb_hcd_priv(hcd);

	vmm_spin_lock_irqsave(&dwc2->urb_lock, flags);

	while (!list_empty(&dwc2->urb_pending_list)) {
		u = list_first_entry(&dwc2->urb_pending_list,
				     struct urb, urb_list);
		list_del(&u->urb_list);
		usb_hcd_giveback_urb(hcd, u, VMM_EFAIL);
	}

	vmm_spin_unlock_irqrestore(&dwc2->urb_lock, flags);
}

static int dwc2_reset(struct usb_hcd *hcd)
{
	struct dwc2_control *dwc2 = usb_hcd_priv(hcd);

	dwc2->rh_devnum = 0;
	/* FIXME: */

	return VMM_OK;
}

static int dwc2_start(struct usb_hcd *hcd)
{
	/* FIXME: */
	return VMM_OK;
}

static void dwc2_stop(struct usb_hcd *hcd)
{
	/* FIXME: */
}

static int dwc2_urb_enqueue(struct usb_hcd *hcd, struct urb *urb)
{
	irq_flags_t flags;
	struct dwc2_control *dwc2 = usb_hcd_priv(hcd);

	vmm_spin_lock_irqsave(&dwc2->urb_lock, flags);
	list_add_tail(&urb->urb_list, &dwc2->urb_pending_list);
	vmm_spin_unlock_irqrestore(&dwc2->urb_lock, flags);

	vmm_completion_complete(&dwc2->urb_pending);

	return VMM_OK;
}

static int dwc2_urb_dequeue(struct usb_hcd *hcd, struct urb *urb, int status)
{
	irq_flags_t flags;
	struct dwc2_control *dwc2 = usb_hcd_priv(hcd);

	vmm_spin_lock_irqsave(&dwc2->urb_lock, flags);
	list_del(&urb->urb_list);
	usb_hcd_giveback_urb(hcd, urb, status);
	vmm_spin_unlock_irqrestore(&dwc2->urb_lock, flags);

	return VMM_OK;
}

static const struct hc_driver dwc2_hc = {
	.description = "DWC2",
	.product_desc = "Designware USB2.0 OTG Controller",
	.hcd_priv_size = sizeof(struct dwc2_control),
	.flags = (HCD_MEMORY|HCD_USB2),
	.irq = dwc2_irq,
	.reset = dwc2_reset,
	.start = dwc2_start,
	.stop = dwc2_stop,
	.urb_enqueue = dwc2_urb_enqueue,
	.urb_dequeue = dwc2_urb_dequeue,
};

static int dwc2_driver_probe(struct vmm_device *dev,
			     const struct vmm_devtree_nodeid *devid)
{
	int rc = VMM_OK;
	u32 snpsid;
	virtual_addr_t regs;
	struct usb_hcd *hcd;
	struct dwc2_control *dwc2;
	const struct dwc2_core_params *params = devid->data;

	hcd = usb_create_hcd(&dwc2_hc, dev, "dwc2");
	if (!hcd) {
		rc = VMM_ENOMEM;
		goto fail;
	}
	dwc2 = (struct dwc2_control *)usb_hcd_priv(hcd);

	dwc2->params = params;

	rc = vmm_devtree_regaddr(dev->node, &hcd->rsrc_start, 0);
	if (rc) {
		goto fail_destroy_hcd;
	}

	rc = vmm_devtree_regsize(dev->node, &hcd->rsrc_len, 0);
	if (rc) {
		goto fail_destroy_hcd;
	}

	rc = vmm_devtree_regmap(dev->node, &regs, 0);
	if (rc) {
		goto fail_destroy_hcd;
	}
	dwc2->regs = (struct dwc2_core_regs *)regs;

	rc = vmm_devtree_irq_get(dev->node, &dwc2->irq, 0);
	if (rc) {
		goto fail_unmap_regs;
	}

	dwc2->rh_devnum = 0;

	snpsid = vmm_readl((void *)&dwc2->regs->gsnpsid);
	vmm_printf("%s: Core Release %x.%03x\n",
		   dev->name, snpsid >> 12 & 0xf, snpsid & 0xfff);
	if ((snpsid & DWC2_SNPSID_DEVID_MASK) != DWC2_SNPSID_DEVID_VER_2xx) {
		vmm_printf("%s: SNPSID invalid (not DWC2 OTG device): %08x\n",
			   dev->name, snpsid);
		rc = VMM_ENODEV;
		goto fail_unmap_regs;
	}

	INIT_SPIN_LOCK(&dwc2->urb_lock);
	INIT_LIST_HEAD(&dwc2->urb_pending_list);
	INIT_COMPLETION(&dwc2->urb_pending);
	dwc2->urb_thread = vmm_threads_create(dev->name, dwc2_worker, hcd,
					      VMM_THREAD_DEF_PRIORITY,
					      VMM_THREAD_DEF_TIME_SLICE);
	if (!dwc2->urb_thread) {
		rc = VMM_ENOSPC;
		goto fail_unmap_regs;
	}

	rc = usb_add_hcd(hcd, dwc2->irq, 0);
	if (rc) {
		goto fail_destroy_thread;
	}

	dev->priv = hcd;

	vmm_threads_start(dwc2->urb_thread);

	return VMM_OK;

fail_destroy_thread:
	vmm_threads_destroy(dwc2->urb_thread);
fail_unmap_regs:
	vmm_devtree_regunmap(dev->node, (virtual_addr_t)dwc2->regs, 0);
fail_destroy_hcd:
	usb_destroy_hcd(hcd);
fail:
	return rc;
}

static int dwc2_driver_remove(struct vmm_device *dev)
{
	struct usb_hcd *hcd = dev->priv;
	struct dwc2_control *dwc2 = usb_hcd_priv(hcd);

	vmm_threads_stop(dwc2->urb_thread);

	usb_remove_hcd(hcd);

	dwc2_flush_work(hcd);

	vmm_threads_destroy(dwc2->urb_thread);

	vmm_devtree_regunmap(dev->node, (virtual_addr_t)dwc2->regs, 0);

	usb_destroy_hcd(hcd);

	return VMM_OK;
}

static const struct dwc2_core_params params_bcm2835 = {
	.otg_cap			= 0,	/* HNP/SRP capable */
	.otg_ver			= 0,	/* 1.3 */
	.dma_enable			= 1,
	.dma_desc_enable		= 0,
	.speed				= 0,	/* High Speed */
	.enable_dynamic_fifo		= 1,
	.en_multiple_tx_fifo		= 1,
	.host_rx_fifo_size		= 774,	/* 774 DWORDs */
	.host_nperio_tx_fifo_size	= 256,	/* 256 DWORDs */
	.host_perio_tx_fifo_size	= 512,	/* 512 DWORDs */
	.max_transfer_size		= 65535,
	.max_packet_count		= 511,
	.host_channels			= 8,
	.phy_type			= 1,	/* UTMI */
	.phy_utmi_width			= 8,	/* 8 bits */
	.phy_ulpi_ddr			= 0,	/* Single */
	.phy_ulpi_ext_vbus		= 0,
	.i2c_enable			= 0,
	.ulpi_fs_ls			= 0,
	.host_support_fs_ls_low_power	= 0,
	.host_ls_low_power_phy_clk	= 0,	/* 48 MHz */
	.ts_dline			= 0,
	.reload_ctl			= 0,
	.ahbcfg				= 0x10,
	.uframe_sched			= 0,
};

static struct vmm_devtree_nodeid dwc2_devid_table[] = {
	{ .compatible = "brcm,bcm2835-usb", .data = &params_bcm2835 },
	{ /* end of list */ },
};

static struct vmm_driver dwc2_driver = {
	.name = "dwc2",
	.match_table = dwc2_devid_table,
	.probe = dwc2_driver_probe,
	.remove = dwc2_driver_remove,
};

static int __init dwc2_driver_init(void)
{
	return vmm_devdrv_register_driver(&dwc2_driver);
}

static void __exit dwc2_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&dwc2_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
