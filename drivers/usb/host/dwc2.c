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
 *
 * This source is largely adapted from u-boot sources:
 * <u-boot>/drivers/usb/host/dwc2.c
 *
 * Copyright (C) 2012 Oleksandr Tymoshenko <gonzo@freebsd.org>
 * Copyright (C) 2014 Marek Vasut <marex@denx.de>
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_heap.h>
#include <vmm_cache.h>
#include <vmm_stdio.h>
#include <vmm_delay.h>
#include <vmm_spinlocks.h>
#include <vmm_workqueue.h>
#include <vmm_host_irq.h>
#include <vmm_host_aspace.h>
#include <vmm_mutex.h>
#include <vmm_completion.h>
#include <vmm_threads.h>
#include <vmm_modules.h>
#include <vmm_devdrv.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>

#include <drv/usb/hcd.h>
#include <drv/usb/ch11.h>
#include <drv/usb/roothubdesc.h>

#include "dwc2.h"

#undef DEBUG

#if defined(DEBUG)
#define DPRINTF(msg...)			vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define MODULE_DESC			"Designware USB2.0 HCD Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(USB_CORE_IPRIORITY + 1)
#define	MODULE_INIT			dwc2_driver_init
#define	MODULE_EXIT			dwc2_driver_exit

#define DWC2_STATUS_BUF_SIZE		64
#define DWC2_DATA_BUF_SIZE		(64 * 1024)
#define DWC2_MAX_DEVICE			16
#define DWC2_MAX_ENDPOINT		16

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
 * @dma_burst_size:     Specifies burst len of DMA
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
 * @ic_usb_cap:		True to enable bit26 of GUSBCFG
 *
 * The following parameters may be specified when starting the module. These
 * parameters define how the DWC2 controller should be configured. A
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
	int dma_burst_size;
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
	bool oc_disable;
	int i2c_enable;
	int ulpi_fs_ls;
	int host_support_fs_ls_low_power;
	int host_ls_low_power_phy_clk;
	int ts_dline;
	int reload_ctl;
	int ahbcfg;
	int uframe_sched;
	int ic_usb_cap;
	u32 dma_offset;
};

struct dwc2_hc {
	int index;
	struct dwc2_control *dwc2;
	struct dwc2_hc_regs *regs;
	u8 *status_buffer;

	struct vmm_thread *hc_thread;
};

struct dwc2_control {
	struct usb_hcd *hcd;
	const struct dwc2_core_params *params;
	struct dwc2_core_regs *regs;
	u32 irq;
	u32 rh_devnum;

	u8 in_data_toggle[DWC2_MAX_DEVICE][DWC2_MAX_ENDPOINT];
	u8 out_data_toggle[DWC2_MAX_DEVICE][DWC2_MAX_ENDPOINT];

	u32 hc_count;

	vmm_spinlock_t hc_next_lock;
	u32 hc_next;

	vmm_spinlock_t hc_urb_lock[16];
	struct urb *hc_urb_int[16];
	struct vmm_completion hc_urb_pending[16];
	struct dlist hc_urb_pending_list[16];

	struct dwc2_hc hcs[16];
};

/*
 * DWC2 IP interface
 */

static int wait_for_bit(void *reg, const u32 mask, bool set)
{
	unsigned int timeout = 1000000;
	u32 val;

	while (--timeout) {
		val = vmm_readl(reg);
		if (!set)
			val = ~val;

		if ((val & mask) == mask)
			return 0;

		vmm_udelay(1);
	}

	return VMM_ETIMEDOUT;
}

/*
 * Initializes the FSLSPClkSel field of the HCFG register
 * depending on the PHY type.
 */
static void dwc2_init_fslspclksel(struct dwc2_control *dwc2)
{
	u32 phyclk;

	if (dwc2->params->phy_type == 0) {
		phyclk = DWC2_HCFG_FSLSPCLKSEL_48_MHZ;	/* Full speed PHY */
	} else {
		/* High speed PHY running at full speed or high speed */
		phyclk = DWC2_HCFG_FSLSPCLKSEL_30_60_MHZ;
	}

	if (dwc2->params->ulpi_fs_ls) {
		u32 ghwcfg2 = vmm_readl(&dwc2->regs->ghwcfg2);
		u32 hval = (ghwcfg2 & DWC2_HWCFG2_HS_PHY_TYPE_MASK) >>
				DWC2_HWCFG2_HS_PHY_TYPE_OFFSET;
		u32 fval = (ghwcfg2 & DWC2_HWCFG2_FS_PHY_TYPE_MASK) >>
				DWC2_HWCFG2_FS_PHY_TYPE_OFFSET;
		if (hval == 2 && fval == 1)
			phyclk = DWC2_HCFG_FSLSPCLKSEL_48_MHZ;	/* Full speed PHY */
	}

	vmm_clrsetbits_le32(&dwc2->regs->host_regs.hcfg,
			    DWC2_HCFG_FSLSPCLKSEL_MASK,
			    phyclk << DWC2_HCFG_FSLSPCLKSEL_OFFSET);
}

/*
 * Flush a Tx FIFO.
 */
static void dwc2_flush_tx_fifo(struct dwc2_control *dwc2, const int num)
{
	int ret;

	vmm_writel(DWC2_GRSTCTL_TXFFLSH | (num << DWC2_GRSTCTL_TXFNUM_OFFSET),
		   &dwc2->regs->grstctl);
	ret = wait_for_bit(&dwc2->regs->grstctl, DWC2_GRSTCTL_TXFFLSH, 0);
	if (ret)
		vmm_printf("%s: Timeout!\n", __func__);

	/* Wait for 3 PHY Clocks */
	vmm_usleep(10);
}

/*
 * Flush Rx FIFO.
 */
static void dwc2_flush_rx_fifo(struct dwc2_control *dwc2)
{
	int ret;

	vmm_writel(DWC2_GRSTCTL_RXFFLSH, &dwc2->regs->grstctl);
	ret = wait_for_bit(&dwc2->regs->grstctl, DWC2_GRSTCTL_RXFFLSH, 0);
	if (ret)
		vmm_printf("%s: Timeout!\n", __func__);

	/* Wait for 3 PHY Clocks */
	vmm_usleep(10);
}

/*
 * Do core a soft reset of the core.  Be careful with this because it
 * resets all the internal state machines of the core.
 */
static void dwc2_core_reset(struct dwc2_control *dwc2)
{
	int rc;

	/* Wait for AHB master IDLE state. */
	rc = wait_for_bit(&dwc2->regs->grstctl,
			  DWC2_GRSTCTL_AHBIDLE, 1);
	if (rc == VMM_ETIMEDOUT) {
		vmm_printf("%s: Timeout!\n", __func__);
	}

	/* Core Soft Reset */
	vmm_writel(DWC2_GRSTCTL_CSFTRST, &dwc2->regs->grstctl);
	rc = wait_for_bit(&dwc2->regs->grstctl,
			  DWC2_GRSTCTL_CSFTRST, 0);
	if (rc == VMM_ETIMEDOUT) {
		vmm_printf("%s: Timeout!\n", __func__);
	}

	/*
	 * Wait for core to come out of reset.
	 * NOTE: This long sleep is _very_ important, otherwise the
	 * core will not stay in host mode after a connector ID change!
	 */
	vmm_msleep(100);
}

static u32 dwc2_hc_count(struct dwc2_control *dwc2)
{
	u32 num_channels;

	num_channels = vmm_readl(&dwc2->regs->ghwcfg2);
	num_channels &= DWC2_HWCFG2_NUM_HOST_CHAN_MASK;
	num_channels >>= DWC2_HWCFG2_NUM_HOST_CHAN_OFFSET;
	num_channels += 1;

	return num_channels;
}

/*
 * This function initializes the DWC2 controller registers for
 * host mode.
 *
 * This function flushes the Tx and Rx FIFOs and it flushes any entries in the
 * request queues. Host channels are reset to ensure that they are ready for
 * performing transfers.
 */
static void dwc2_core_host_init(struct dwc2_control *dwc2)
{
	int i, ret, num_channels;
	u32 nptxfifosize = 0, ptxfifosize = 0, hprt0 = 0;

	/* Restart the Phy Clock */
	vmm_writel(0, &dwc2->regs->pcgcctl);

	/* Initialize Host Configuration Register */
	dwc2_init_fslspclksel(dwc2);
	if (dwc2->params->speed == 1) {
		vmm_setbits_le32(&dwc2->regs->host_regs.hcfg,
				 DWC2_HCFG_FSLSSUPP);
	}

	/* Configure data FIFO sizes */
	if (dwc2->params->enable_dynamic_fifo &&
	    (vmm_readl(&dwc2->regs->ghwcfg2) & DWC2_HWCFG2_DYNAMIC_FIFO)) {
		/* Rx FIFO */
		vmm_writel(dwc2->params->host_rx_fifo_size,
			   &dwc2->regs->grxfsiz);

		/* Non-periodic Tx FIFO */
		nptxfifosize |= dwc2->params->host_nperio_tx_fifo_size <<
				DWC2_FIFOSIZE_DEPTH_OFFSET;
		nptxfifosize |= dwc2->params->host_rx_fifo_size <<
				DWC2_FIFOSIZE_STARTADDR_OFFSET;
		vmm_writel(nptxfifosize, &dwc2->regs->gnptxfsiz);

		/* Periodic Tx FIFO */
		ptxfifosize |= dwc2->params->host_perio_tx_fifo_size <<
				DWC2_FIFOSIZE_DEPTH_OFFSET;
		ptxfifosize |= (dwc2->params->host_rx_fifo_size +
				dwc2->params->host_nperio_tx_fifo_size) <<
				DWC2_FIFOSIZE_STARTADDR_OFFSET;
		vmm_writel(ptxfifosize, &dwc2->regs->hptxfsiz);
	}

	/* Clear Host Set HNP Enable in the OTG Control Register */
	vmm_clrbits_le32(&dwc2->regs->gotgctl, DWC2_GOTGCTL_HSTSETHNPEN);

	/* Make sure the FIFOs are flushed. */
	dwc2_flush_tx_fifo(dwc2, 0x10);	/* All Tx FIFOs */
	dwc2_flush_rx_fifo(dwc2);

	/* Flush out any leftover queued requests. */
	num_channels = vmm_readl(&dwc2->regs->ghwcfg2);
	num_channels &= DWC2_HWCFG2_NUM_HOST_CHAN_MASK;
	num_channels >>= DWC2_HWCFG2_NUM_HOST_CHAN_OFFSET;
	num_channels += 1;
	for (i = 0; i < num_channels; i++) {
		vmm_clrsetbits_le32(&dwc2->regs->hc_regs[i].hcchar,
				DWC2_HCCHAR_CHEN | DWC2_HCCHAR_EPDIR,
				DWC2_HCCHAR_CHDIS);
	}

	/* Halt all channels to put them into a known state. */
	for (i = 0; i < num_channels; i++) {
		vmm_clrsetbits_le32(&dwc2->regs->hc_regs[i].hcchar,
				    DWC2_HCCHAR_EPDIR,
				    DWC2_HCCHAR_CHEN | DWC2_HCCHAR_CHDIS);
		ret = wait_for_bit(&dwc2->regs->hc_regs[i].hcchar,
				   DWC2_HCCHAR_CHEN, 0);
		if (ret)
			vmm_printf("%s: Timeout!\n", __func__);
	}

	/* Turn on the vbus power. */
	if (vmm_readl(&dwc2->regs->gintsts) & DWC2_GINTSTS_CURMODE_HOST) {
		hprt0 = vmm_readl(&dwc2->regs->hprt0);
		hprt0 &= ~(DWC2_HPRT0_PRTENA | DWC2_HPRT0_PRTCONNDET);
		hprt0 &= ~(DWC2_HPRT0_PRTENCHNG | DWC2_HPRT0_PRTOVRCURRCHNG);
		if (!(hprt0 & DWC2_HPRT0_PRTPWR)) {
			hprt0 |= DWC2_HPRT0_PRTPWR;
			vmm_writel(hprt0, &dwc2->regs->hprt0);
		}
	}
}

/*
 * This function initializes the DWC2 controller registers and
 * prepares the core for device mode or host mode operation.
 */
static void dwc2_core_init(struct dwc2_control *dwc2)
{
	u32 ahbcfg = 0, usbcfg = 0;
	u8 brst_sz = dwc2->params->dma_burst_size;

	/* Common Initialization */
	usbcfg = vmm_readl(&dwc2->regs->gusbcfg);

	/* Program the ULPI External VBUS bit if needed */
	if (dwc2->params->phy_ulpi_ext_vbus) {
		usbcfg |= DWC2_GUSBCFG_ULPI_EXT_VBUS_DRV;

		if (!dwc2->params->oc_disable) {
			usbcfg |= DWC2_GUSBCFG_ULPI_INT_VBUS_INDICATOR |
				  DWC2_GUSBCFG_INDICATOR_PASSTHROUGH;
		}
	} else {
		usbcfg &= ~DWC2_GUSBCFG_ULPI_EXT_VBUS_DRV;
	}

	/* Set external TS Dline pulsing */
	if (dwc2->params->ts_dline) {
		usbcfg |= DWC2_GUSBCFG_TERM_SEL_DL_PULSE;
	} else {
		usbcfg &= ~DWC2_GUSBCFG_TERM_SEL_DL_PULSE;
	}
	vmm_writel(usbcfg, &dwc2->regs->gusbcfg);

	/* Reset the Controller */
	dwc2_core_reset(dwc2);

	/*
	 * This programming sequence needs to happen in FS mode before
	 * any other programming occurs
	 */
	if ((dwc2->params->speed == 1) &&
	    (dwc2->params->phy_type == 0)) {
		/* If FS mode with FS PHY */
		vmm_setbits_le32(&dwc2->regs->gusbcfg, DWC2_GUSBCFG_PHYSEL);

		/* Reset after a PHY select */
		dwc2_core_reset(dwc2);

		/*
		 * Program DCFG.DevSpd or HCFG.FSLSPclkSel to 48Mhz in FS.
		 * Also do this on HNP Dev/Host mode switches (done in
		 * dev_init and host_init).
		 */
		if (vmm_readl(&dwc2->regs->gintsts) &
					DWC2_GINTSTS_CURMODE_HOST)
			dwc2_init_fslspclksel(dwc2);

		if (dwc2->params->i2c_enable) {
			/* Program GUSBCFG.OtgUtmifsSel to I2C */
			vmm_setbits_le32(&dwc2->regs->gusbcfg,
					 DWC2_GUSBCFG_OTGUTMIFSSEL);

			/* Program GI2CCTL.I2CEn */
			vmm_clrsetbits_le32(&dwc2->regs->gi2cctl,
					    DWC2_GI2CCTL_I2CEN |
					    DWC2_GI2CCTL_I2CDEVADDR_MASK,
					    1<<DWC2_GI2CCTL_I2CDEVADDR_OFFSET);
			vmm_setbits_le32(&dwc2->regs->gi2cctl,
					 DWC2_GI2CCTL_I2CEN);
		}
	} else {
		/* High speed PHY. */

		/*
		 * HS PHY parameters. These parameters are preserved during
		 * soft reset so only program the first time. Do a soft reset
		 * immediately after setting phyif.
		 */
		usbcfg &= ~(DWC2_GUSBCFG_ULPI_UTMI_SEL | DWC2_GUSBCFG_PHYIF);
		usbcfg |= dwc2->params->phy_type <<
					DWC2_GUSBCFG_ULPI_UTMI_SEL_OFFSET;

		if (usbcfg & DWC2_GUSBCFG_ULPI_UTMI_SEL) {
			/* ULPI interface */
			if (dwc2->params->phy_ulpi_ddr) {
				usbcfg |= DWC2_GUSBCFG_DDRSEL;
			} else {
				usbcfg &= ~DWC2_GUSBCFG_DDRSEL;
			}
		} else {
			/* UTMI+ interface */
			if (dwc2->params->phy_utmi_width == 16) {
				usbcfg |= DWC2_GUSBCFG_PHYIF;
			}
		}

		vmm_writel(usbcfg, &dwc2->regs->gusbcfg);

		/* Reset after setting the PHY parameters */
		dwc2_core_reset(dwc2);
	}

	usbcfg = vmm_readl(&dwc2->regs->gusbcfg);
	usbcfg &= ~(DWC2_GUSBCFG_ULPI_FSLS | DWC2_GUSBCFG_ULPI_CLK_SUS_M);
	if (dwc2->params->ulpi_fs_ls) {
		u32 ghwcfg2 = vmm_readl(&dwc2->regs->ghwcfg2);
		u32 hval = (ghwcfg2 & DWC2_HWCFG2_HS_PHY_TYPE_MASK) >>
				DWC2_HWCFG2_HS_PHY_TYPE_OFFSET;
		u32 fval = (ghwcfg2 & DWC2_HWCFG2_FS_PHY_TYPE_MASK) >>
				DWC2_HWCFG2_FS_PHY_TYPE_OFFSET;
		if (hval == 2 && fval == 1) {
			usbcfg |= DWC2_GUSBCFG_ULPI_FSLS;
			usbcfg |= DWC2_GUSBCFG_ULPI_CLK_SUS_M;
		}
	}
	vmm_writel(usbcfg, &dwc2->regs->gusbcfg);

	/* Program the GAHBCFG Register. */
	switch (vmm_readl(&dwc2->regs->ghwcfg2) &
				DWC2_HWCFG2_ARCHITECTURE_MASK) {
	case DWC2_HWCFG2_ARCHITECTURE_SLAVE_ONLY:
		break;
	case DWC2_HWCFG2_ARCHITECTURE_EXT_DMA:
		while (brst_sz > 1) {
			ahbcfg |=
				ahbcfg + (1 << DWC2_GAHBCFG_HBURSTLEN_OFFSET);
			ahbcfg &= DWC2_GAHBCFG_HBURSTLEN_MASK;
			brst_sz >>= 1;
		}
		if (dwc2->params->dma_enable) {
			ahbcfg |= DWC2_GAHBCFG_DMAENABLE;
		}
		break;

	case DWC2_HWCFG2_ARCHITECTURE_INT_DMA:
		ahbcfg |= DWC2_GAHBCFG_HBURSTLEN_INCR4;
		if (dwc2->params->dma_enable) {
			ahbcfg |= DWC2_GAHBCFG_DMAENABLE;
		}
		break;
	}

	vmm_writel(ahbcfg, &dwc2->regs->gahbcfg);

	/* Program the GUSBCFG register for HNP/SRP. */
	vmm_setbits_le32(&dwc2->regs->gusbcfg,
			 DWC2_GUSBCFG_HNPCAP | DWC2_GUSBCFG_SRPCAP);

	if (dwc2->params->ic_usb_cap) {
		vmm_setbits_le32(&dwc2->regs->gusbcfg, DWC2_GUSBCFG_IC_USB_CAP);
	}
}

/*
 * Prepares a host channel for transferring packets to/from a specific
 * endpoint. The HCCHARn register is set up with the characteristics specified
 * in _hc. Host channel interrupts that may need to be serviced while this
 * transfer is in progress are enabled.
 */
static void dwc2_hc_init(struct dwc2_hc_regs *hc_regs,
			 u8 dev_addr, u8 ep_num, u8 ep_is_in,
			 u8 ep_type, u16 max_packet)
{
	const u32 hcchar = (dev_addr << DWC2_HCCHAR_DEVADDR_OFFSET) |
				(ep_num << DWC2_HCCHAR_EPNUM_OFFSET) |
				(ep_is_in << DWC2_HCCHAR_EPDIR_OFFSET) |
				(ep_type << DWC2_HCCHAR_EPTYPE_OFFSET) |
				(max_packet << DWC2_HCCHAR_MPS_OFFSET);

	/*
	 * Program the HCCHARn register with the endpoint characteristics
	 * for the current transfer.
	 */
	vmm_writel(hcchar, &hc_regs->hcchar);

	/* Program the HCSPLIT register for SPLITs */
	vmm_writel(0, &hc_regs->hcsplt);
}

static void dwc2_hc_init_split(struct dwc2_hc_regs *hc_regs,
			       u8 hub_devnum, u8 hub_port)
{
	u32 hcsplt = 0;

	hcsplt = DWC2_HCSPLT_SPLTENA;
	hcsplt |= hub_devnum << DWC2_HCSPLT_HUBADDR_OFFSET;
	hcsplt |= hub_port << DWC2_HCSPLT_PRTADDR_OFFSET;

	/* Program the HCSPLIT register for SPLITs */
	vmm_writel(hcsplt, &hc_regs->hcsplt);
}

/*
 * DWC2 to USB API interface
 */

/* Direction: In ; Request: Status */
static int dwc2_rh_msg_in_status(struct dwc2_control *dwc2,
				 struct urb *u,
				 struct usb_ctrlrequest *cmd)
{
	int len = 0, rc = VMM_OK;
	u32 hprt0 = 0, port_status = 0, port_change = 0;
	void *buffer = u->transfer_buffer;
	int buffer_len = u->transfer_buffer_length;

	switch (cmd->bRequestType & ~USB_DIR_IN) {
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

/* Direction: In ; Request: Descriptor */
static int dwc2_rh_msg_in_descriptor(struct dwc2_control *dwc2,
				     struct urb *u,
				     struct usb_ctrlrequest *cmd)
{
	u32 dsc;
	u8 data[32];
	int len = 0, rc = VMM_OK;
	u16 wValue = vmm_cpu_to_le16(cmd->wValue);
	u16 wLength = vmm_cpu_to_le16(cmd->wLength);
	void *buffer = u->transfer_buffer;
	int buffer_len = u->transfer_buffer_length;

	switch (cmd->bRequestType & ~USB_DIR_IN) {
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

/* Direction: In ; Request: Configuration */
static int dwc2_rh_msg_in_configuration(struct dwc2_control *dwc2,
					struct urb *u,
					struct usb_ctrlrequest *cmd)
{
	int len = 0, rc = VMM_OK;
	void *buffer = u->transfer_buffer;
	int buffer_len = u->transfer_buffer_length;

	switch (cmd->bRequestType & ~USB_DIR_IN) {
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

/* Direction: In */
static int dwc2_rh_msg_in(struct dwc2_control *dwc2,
			   struct urb *u, struct usb_ctrlrequest *cmd)
{
	switch (cmd->bRequest) {
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

/* Direction: Out */
static int dwc2_rh_msg_out(struct dwc2_control *dwc2,
			   struct urb *u, struct usb_ctrlrequest *cmd)
{
	int rc = VMM_OK;
	u16 bmrtype_breq = cmd->bRequestType | (cmd->bRequest << 8);
	u16 wValue = vmm_cpu_to_le16(cmd->wValue);

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
			vmm_msleep(50);
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
	struct usb_ctrlrequest *cmd =
			(struct usb_ctrlrequest *)u->setup_packet;

	if (cmd->bRequestType & USB_DIR_IN) {
		rc = dwc2_rh_msg_in(dwc2, u, cmd);
	} else {
		rc = dwc2_rh_msg_out(dwc2, u, cmd);
	}

	return rc;
}

static vmm_irq_return_t	dwc2_irq(struct usb_hcd *hcd)
{
	/* For now nothing to do here. */
	return VMM_IRQ_NONE;
}

static int dwc2_eptype[] = {
	DWC2_HCCHAR_EPTYPE_ISOC,
	DWC2_HCCHAR_EPTYPE_INTR,
	DWC2_HCCHAR_EPTYPE_CONTROL,
	DWC2_HCCHAR_EPTYPE_BULK,
};

static int wait_for_chhltd(struct dwc2_hc *hc,
			   u32 *sub, u8 *toggle)
{
	int ret;
	u32 hcint, hctsiz;
	u8 pid = *toggle;

	ret = wait_for_bit(&hc->regs->hcint, DWC2_HCINT_CHHLTD, 1);
	if (ret)
		return ret;

	hcint = vmm_readl(&hc->regs->hcint);
	hctsiz = vmm_readl(&hc->regs->hctsiz);
	*sub = (hctsiz & DWC2_HCTSIZ_XFERSIZE_MASK) >>
			DWC2_HCTSIZ_XFERSIZE_OFFSET;
	*toggle = (hctsiz & DWC2_HCTSIZ_PID_MASK) >> DWC2_HCTSIZ_PID_OFFSET;

	DPRINTF("%s: HCINT=%08x sub=%u toggle=%d\n",
		__func__, hcint, *sub, *toggle);

	if (hcint & DWC2_HCINT_XFERCOMP)
		return VMM_OK;

	/*
	 * The USB function can respond to a Setup packet with ACK or, in
	 * case it's busy, it can ignore the Setup packet. The USB function
	 * usually gets busy if we hammer it with Control EP transfers too
	 * much (ie. sending multiple Get Descriptor requests in a single
	 * microframe tends to trigger it on certain USB sticks). The DWC2
	 * controller will interpret not receiving an ACK after Setup packet
	 * as XACTERR. Check for this condition and if it happens, retry
	 * sending the Setup packet.
	 */

	if (hcint & DWC2_HCINT_XACTERR && (pid == DWC2_HC_PID_SETUP))
		return VMM_EAGAIN;

	if (hcint & (DWC2_HCINT_NAK | DWC2_HCINT_FRMOVRUN))
		return VMM_EAGAIN;

	DPRINTF("%s: Error (HCINT=%08x)\n", __func__, hcint);
	return VMM_EINVALID;
}

static int transfer_chunk(struct dwc2_control *dwc2, struct dwc2_hc *hc,
			  u8 *pid, int in, void *buffer, int num_packets,
			  int xfer_len, int *actual_len, int odd_frame)
{
	int ret = 0;
	u32 sub;
	physical_addr_t pa;

	DPRINTF("%s: chunk: pid %d xfer_len %u pkts %u\n",
		__func__, *pid, xfer_len, num_packets);

	vmm_writel((xfer_len << DWC2_HCTSIZ_XFERSIZE_OFFSET) |
		   (num_packets << DWC2_HCTSIZ_PKTCNT_OFFSET) |
		   (*pid << DWC2_HCTSIZ_PID_OFFSET),
		   &hc->regs->hctsiz);

	pa = vmm_dma_map((virtual_addr_t)buffer, xfer_len,
			 in ? DMA_FROM_DEVICE : DMA_TO_DEVICE);

	vmm_writel((u32)pa + dwc2->params->dma_offset, &hc->regs->hcdma);

	/* Clear old interrupt conditions for this host channel. */
	vmm_writel(0x3fff, &hc->regs->hcint);

	/* Set host channel enable after all other setup is complete. */
	vmm_clrsetbits_le32(&hc->regs->hcchar, DWC2_HCCHAR_MULTICNT_MASK |
					DWC2_HCCHAR_CHEN | DWC2_HCCHAR_CHDIS |
					DWC2_HCCHAR_ODDFRM,
					(1 << DWC2_HCCHAR_MULTICNT_OFFSET) |
					(odd_frame << DWC2_HCCHAR_ODDFRM_OFFSET) |
					DWC2_HCCHAR_CHEN);

	/* Wait for channel to halt */
	ret = wait_for_chhltd(hc, &sub, pid);
	if (ret < 0) {
		vmm_dma_unmap(pa, xfer_len,
			      in ? DMA_FROM_DEVICE : DMA_TO_DEVICE);
		return ret;
	}

	*actual_len = xfer_len;
	vmm_dma_unmap(pa, xfer_len,
		      in ? DMA_FROM_DEVICE : DMA_TO_DEVICE);

	return ret;
}

static int chunk_msg(struct dwc2_control *dwc2, struct dwc2_hc *hc,
		     struct urb *u, u8 *pid, int in, void *buffer, int len)
{
	int ret = 0;
	struct dwc2_host_regs *host_regs = &dwc2->regs->host_regs;
	int devnum = usb_pipedevice(u->pipe);
	int ep = usb_pipeendpoint(u->pipe);
	int max = usb_maxpacket(u->dev, u->pipe);
	int eptype = dwc2_eptype[usb_pipetype(u->pipe)];
	int done = 0;
	int do_split = 0;
	int complete_split = 0;
	u32 xfer_len;
	u32 num_packets;
	int stop_transfer = 0;
	u32 max_xfer_len;
	int ssplit_frame_num = 0;

	DPRINTF("%s: msg: pipe %x pid %d in %d len %d\n",
		__func__, u->pipe, *pid, in, len);

	max_xfer_len = CONFIG_DWC2_MAX_PACKET_COUNT * max;
	if (max_xfer_len > CONFIG_DWC2_MAX_TRANSFER_SIZE)
		max_xfer_len = CONFIG_DWC2_MAX_TRANSFER_SIZE;
	if (max_xfer_len > DWC2_DATA_BUF_SIZE)
		max_xfer_len = DWC2_DATA_BUF_SIZE;

	/* Make sure that max_xfer_len is a multiple of max packet size. */
	num_packets = udiv32(max_xfer_len, max);
	max_xfer_len = num_packets * max;

	/* Initialize channel */
	dwc2_hc_init(hc->regs, devnum, ep, in, eptype, max);

	/* Check if the target is a FS/LS device behind a HS hub */
	if (u->dev->speed != USB_SPEED_HIGH) {
		u8 hub_addr = 0;
		u8 hub_port = 0;
		u32 hprt0 = vmm_readl(&dwc2->regs->hprt0);
		if ((hprt0 & DWC2_HPRT0_PRTSPD_MASK) ==
					DWC2_HPRT0_PRTSPD_HIGH) {
			usb_get_usb2_hub_address_port(u->dev,
						      &hub_addr, &hub_port);
			dwc2_hc_init_split(hc->regs, hub_addr, hub_port);
			do_split = 1;
			num_packets = 1;
			max_xfer_len = max;
		}
	}

	do {
		int actual_len = 0;
		u32 hcint;
		int odd_frame = 0;
		xfer_len = len - done;

		if (xfer_len > max_xfer_len)
			xfer_len = max_xfer_len;
		else if (xfer_len > max)
			num_packets = udiv32((xfer_len + max - 1), max);
		else
			num_packets = 1;

		if (complete_split)
			vmm_setbits_le32(&hc->regs->hcsplt, DWC2_HCSPLT_COMPSPLT);
		else if (do_split)
			vmm_clrbits_le32(&hc->regs->hcsplt, DWC2_HCSPLT_COMPSPLT);

		if (eptype == DWC2_HCCHAR_EPTYPE_INTR) {
			int uframe_num = vmm_readl(&host_regs->hfnum);
			if (!(uframe_num & 0x1))
				odd_frame = 1;
		}

		ret = transfer_chunk(dwc2, hc, pid, in,
				     (char *)buffer + done, num_packets,
				     xfer_len, &actual_len, odd_frame);

		hcint = vmm_readl(&hc->regs->hcint);
		if (complete_split) {
			stop_transfer = 0;
			if (hcint & DWC2_HCINT_NYET) {
				ret = 0;
				int frame_num = DWC2_HFNUM_MAX_FRNUM &
						vmm_readl(&host_regs->hfnum);
				if (((frame_num - ssplit_frame_num) &
						DWC2_HFNUM_MAX_FRNUM) > 4)
					ret = VMM_EAGAIN;
			} else
				complete_split = 0;
		} else if (do_split) {
			if (hcint & DWC2_HCINT_ACK) {
				ssplit_frame_num = DWC2_HFNUM_MAX_FRNUM &
						vmm_readl(&host_regs->hfnum);
				ret = 0;
				complete_split = 1;
			}
		}

		if (ret)
			break;

		if (actual_len < xfer_len)
			stop_transfer = 1;

		done += actual_len;

		/* Transactions are done when when either all data is
		 * transferred or there is a short transfer. In case of
		 * a SPLIT make sure the CSPLIT is executed.
		 */
	} while (((done < len) && !stop_transfer) || complete_split);

	vmm_writel(0, &hc->regs->hcintmsk);
	vmm_writel(0xFFFFFFFF, &hc->regs->hcint);

	u->status = 0;
	u->actual_length = done;

	return ret;
}

static int dwc2_control_msg(struct dwc2_control *dwc2,
			    struct dwc2_hc *hc, struct urb *u)
{
	int ret, act_len;
	int status_direction;
	void *buffer = u->transfer_buffer;
	int len = u->transfer_buffer_length;
	u8 pid;

	/* Process root hub control messages differently */
	if (u->dev->devnum == dwc2->rh_devnum) {
		return dwc2_control_rh_msg(dwc2, u);
	}

	/* SETUP stage */
	pid = DWC2_HC_PID_SETUP;
	do {
		ret = chunk_msg(dwc2, hc, u, &pid, 0, u->setup_packet, 8);
	} while (ret == VMM_EAGAIN);
	if (ret)
		return ret;

	/* DATA stage */
	act_len = 0;
	if (buffer) {
		pid = DWC2_HC_PID_DATA1;
		do {
			ret = chunk_msg(dwc2, hc, u,
					&pid, usb_pipein(u->pipe),
					buffer, len);
			act_len += u->actual_length;
			buffer += u->actual_length;
			len -= u->actual_length;
		} while (ret == VMM_EAGAIN);
		if (ret)
			return ret;
		status_direction = usb_pipeout(u->pipe);
	} else {
		/* No-data CONTROL always ends with an IN transaction */
		status_direction = 1;
	}

	/* STATUS stage */
	pid = DWC2_HC_PID_DATA1;
	do {
		ret = chunk_msg(dwc2, hc, u, &pid, status_direction,
				hc->status_buffer, 0);
	} while (ret == VMM_EAGAIN);
	if (ret)
		return ret;

	u->actual_length = act_len;

	return VMM_OK;
}

static int dwc2_bulk_msg(struct dwc2_control *dwc2,
			 struct dwc2_hc *hc, struct urb *u)
{
	int devnum = u->dev->devnum;
	int ep = usb_pipeendpoint(u->pipe);
	void *buffer = u->transfer_buffer;
	int len = u->transfer_buffer_length;
	u8 *pid;

	if ((devnum >= DWC2_MAX_DEVICE) || (devnum == dwc2->rh_devnum)) {
		u->status = 0;
		return VMM_EINVALID;
	}

	/* Ensure that transfer buffer is cache aligned */
	if ((unsigned long)buffer & (VMM_CACHE_LINE_SIZE - 1)) {
		WARN_ON(1);
		vmm_printf("%s: dev=%s transfer buffer not cache aligned\n",
			   __func__, u->dev->dev.name);
		return VMM_EIO;
	}

	if (usb_pipein(u->pipe))
		pid = &dwc2->in_data_toggle[devnum][ep];
	else
		pid = &dwc2->out_data_toggle[devnum][ep];

	return chunk_msg(dwc2, hc, u, pid, usb_pipein(u->pipe), buffer, len);
}

static int dwc2_int_msg_start(struct dwc2_control *dwc2,
			      struct dwc2_hc *hc, struct urb *u)
{
	u64 timeout;
	int ret;

	timeout = USB_TIMEOUT_MS(u->pipe) * (u64)1000000;
	timeout = timeout + vmm_timer_timestamp();
	for (;;) {
		if (vmm_timer_timestamp() > timeout) {
			vmm_printf("Timeout poll on interrupt endpoint\n");
			return VMM_ETIMEDOUT;
		}
		ret = dwc2_bulk_msg(dwc2, hc, u);
		if (ret != VMM_EAGAIN)
			return ret;
	}

	return VMM_OK;
}

static void dwc2_int_msg_stop(struct dwc2_control *dwc2,
			      struct dwc2_hc *hc,
			      struct urb *u, bool urb_int_active)
{
	if (urb_int_active) {
		/* TODO: Forcefully stop the host channel interrupt message */
	}

	/* Free the URB because we had got URB with incremented ref count */
	usb_free_urb(u);
}

static int dwc2_hc_worker(void *data)
{
	int rc;
	struct urb *u;
	irq_flags_t f;
	struct dwc2_hc *hc = data;
	struct dwc2_control *dwc2 = hc->dwc2;
	struct usb_hcd *hcd = dwc2->hcd;

	while (1) {
		vmm_completion_wait(&dwc2->hc_urb_pending[hc->index]);

		u = NULL;
		vmm_spin_lock_irqsave(&dwc2->hc_urb_lock[hc->index], f);
		if (!list_empty(&dwc2->hc_urb_pending_list[hc->index])) {
			u = list_first_entry(
				&dwc2->hc_urb_pending_list[hc->index],
				struct urb, urb_list);
			list_del(&u->urb_list);
		}
		vmm_spin_unlock_irqrestore(&dwc2->hc_urb_lock[hc->index], f);
		if (!u) {
			continue;
		}

		rc = VMM_OK;
		switch (usb_pipetype(u->pipe)) {
		case USB_PIPE_CONTROL:
			rc = dwc2_control_msg(dwc2, hc, u);
			break;
		case USB_PIPE_BULK:
			rc = dwc2_bulk_msg(dwc2, hc, u);
			break;
		case USB_PIPE_INTERRUPT:
			rc = dwc2_int_msg_start(dwc2, hc, u);
			break;
		default:
			rc = VMM_EINVALID;
			break;
		};

		if (usb_pipetype(u->pipe) != USB_PIPE_INTERRUPT) {
			usb_hcd_giveback_urb(hcd, u, rc);
		}
	}

	return VMM_OK;
}

static void dwc2_flush_work(struct usb_hcd *hcd)
{
	u32 i;
	struct urb *u;
	irq_flags_t f;
	struct dwc2_control *dwc2 =
			(struct dwc2_control *)usb_hcd_priv(hcd);

	for (i = 0; i < dwc2->hc_count; i++) {
		vmm_spin_lock_irqsave(&dwc2->hc_urb_lock[i], f);

		while (!list_empty(&dwc2->hc_urb_pending_list[i])) {
			u = list_first_entry(&dwc2->hc_urb_pending_list[i],
					     struct urb, urb_list);
			list_del(&u->urb_list);
			vmm_spin_unlock_irqrestore(&dwc2->hc_urb_lock[i], f);
			usb_hcd_giveback_urb(hcd, u, VMM_EFAIL);
			vmm_spin_lock_irqsave(&dwc2->hc_urb_lock[i], f);
		}

		vmm_spin_unlock_irqrestore(&dwc2->hc_urb_lock[i], f);
	}
}

static int dwc2_reset(struct usb_hcd *hcd)
{
	struct dwc2_control *dwc2 = usb_hcd_priv(hcd);

	/* Clear root hub device number */
	dwc2->rh_devnum = 0;

	/* Soft-reset controller */
	dwc2_core_reset(dwc2);

	return VMM_OK;
}

static int dwc2_start(struct usb_hcd *hcd)
{
	u32 i, j;
	struct dwc2_control *dwc2 = usb_hcd_priv(hcd);

	/* General init */
	dwc2_core_init(dwc2);

	/* Init host mode */
	dwc2_core_host_init(dwc2);

	/* Reset port0 */
	vmm_clrsetbits_le32(&dwc2->regs->hprt0,
			    DWC2_HPRT0_PRTENA |
			    DWC2_HPRT0_PRTCONNDET |
			    DWC2_HPRT0_PRTENCHNG |
			    DWC2_HPRT0_PRTOVRCURRCHNG,
			    DWC2_HPRT0_PRTRST);
	vmm_msleep(50);
	vmm_clrbits_le32(&dwc2->regs->hprt0,
			 DWC2_HPRT0_PRTENA |
			 DWC2_HPRT0_PRTCONNDET |
			 DWC2_HPRT0_PRTENCHNG |
			 DWC2_HPRT0_PRTOVRCURRCHNG |
			 DWC2_HPRT0_PRTRST);

	/* Control & Bulk endpoint status flags */
	for (i = 0; i < DWC2_MAX_DEVICE; i++) {
		for (j = 0; j < DWC2_MAX_ENDPOINT; j++) {
			dwc2->in_data_toggle[i][j] = DWC2_HC_PID_DATA0;
			dwc2->out_data_toggle[i][j] = DWC2_HC_PID_DATA0;
		}
	}

	/*
	 * Add a 1 second delay here. This gives the host controller
	 * a bit time before the comminucation with the USB devices
	 * is started (the bus is scanned) and fixes the USB detection
	 * problems with some problematic USB keys.
	 */
	if (vmm_readl(&dwc2->regs->gintsts) & DWC2_GINTSTS_CURMODE_HOST)
		vmm_msleep(1000);

	return VMM_OK;
}

static void dwc2_stop(struct usb_hcd *hcd)
{
	struct dwc2_control *dwc2 = usb_hcd_priv(hcd);

	/* Flush the all pending work */
	dwc2_flush_work(hcd);

	/* Put everything in reset. */
	vmm_clrsetbits_le32(&dwc2->regs->hprt0,
			    DWC2_HPRT0_PRTENA |
			    DWC2_HPRT0_PRTCONNDET |
			    DWC2_HPRT0_PRTENCHNG |
			    DWC2_HPRT0_PRTOVRCURRCHNG,
			    DWC2_HPRT0_PRTRST);
}

static int dwc2_urb_enqueue(struct usb_hcd *hcd, struct urb *urb)
{
	u32 count;
	irq_flags_t f;
	struct dwc2_hc *hc;
	struct dwc2_control *dwc2 = usb_hcd_priv(hcd);

	vmm_spin_lock_irqsave(&dwc2->hc_next_lock, f);

	count = 0;
	while (dwc2->hc_urb_int[dwc2->hc_next] &&
	       (count < dwc2->hc_count)) {
		count++;
		dwc2->hc_next++;
		if (dwc2->hc_next == dwc2->hc_count) {
			dwc2->hc_next = 0;
		}
	}
	if (count == dwc2->hc_count) {
		vmm_spin_unlock_irqrestore(&dwc2->hc_next_lock, f);
		return VMM_ENOSPC;
	}

	hc = &dwc2->hcs[dwc2->hc_next];

	dwc2->hc_next++;
	if (dwc2->hc_next == dwc2->hc_count) {
		dwc2->hc_next = 0;
	}

	vmm_spin_unlock_irqrestore(&dwc2->hc_next_lock, f);

	vmm_spin_lock_irqsave(&dwc2->hc_urb_lock[hc->index], f);

	if (usb_pipetype(urb->pipe) == USB_PIPE_INTERRUPT) {
		dwc2->hc_urb_int[hc->index] = urb;
	}

	list_add_tail(&urb->urb_list, &dwc2->hc_urb_pending_list[hc->index]);

	vmm_spin_unlock_irqrestore(&dwc2->hc_urb_lock[hc->index], f);

	vmm_completion_complete(&dwc2->hc_urb_pending[hc->index]);

	return VMM_OK;
}

static int dwc2_urb_dequeue(struct usb_hcd *hcd, struct urb *urb, int status)
{
	u32 i;
	irq_flags_t f;
	struct urb *u;
	bool urb_int_active = FALSE;
	struct dwc2_hc *hc;
	struct dwc2_control *dwc2 = usb_hcd_priv(hcd);

	hc = NULL;
	for (i = 0; i < dwc2->hc_count; i++) {
		vmm_spin_lock_irqsave(&dwc2->hc_urb_lock[i], f);
		list_for_each_entry(u,
			&dwc2->hc_urb_pending_list[i], urb_list) {
			if (u == urb) {
				hc = &dwc2->hcs[i];
				break;
			}
		}
		vmm_spin_unlock_irqrestore(&dwc2->hc_urb_lock[i], f);
		if (hc) {
			break;
		}
	}
	if (!hc) {
		for (i = 0; i < dwc2->hc_count; i++) {
			vmm_spin_lock_irqsave(&dwc2->hc_urb_lock[i], f);
			if (dwc2->hc_urb_int[i] == urb) {
				hc = &dwc2->hcs[i];
				urb_int_active = TRUE;
			}
			vmm_spin_unlock_irqrestore(&dwc2->hc_urb_lock[i], f);
			if (hc) {
				break;
			}
		}
		if (!hc) {
			return VMM_ENOTAVAIL;
		}
	} else {
		vmm_spin_lock_irqsave(&dwc2->hc_urb_lock[hc->index], f);
		list_del(&urb->urb_list);
		vmm_spin_unlock_irqrestore(&dwc2->hc_urb_lock[hc->index], f);
	}

	vmm_spin_lock_irqsave(&dwc2->hc_urb_lock[hc->index], f);

	if (dwc2->hc_urb_int[hc->index] == urb) {
		dwc2_int_msg_stop(dwc2, hc, urb, urb_int_active);
		dwc2->hc_urb_int[hc->index] = NULL;
	}

	vmm_spin_unlock_irqrestore(&dwc2->hc_urb_lock[hc->index], f);

	usb_hcd_giveback_urb(hcd, urb, status);

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
	u32 i, snpsid;
	virtual_addr_t regs;
	struct usb_hcd *hcd;
	struct dwc2_hc *hc;
	struct dwc2_control *dwc2;
	char name[VMM_FIELD_NAME_SIZE];
	const struct dwc2_core_params *params = devid->data;

	hcd = usb_create_hcd(&dwc2_hc, dev, "dwc2");
	if (!hcd) {
		rc = VMM_ENOMEM;
		goto fail;
	}
	dwc2 = (struct dwc2_control *)usb_hcd_priv(hcd);
	dwc2->hcd = hcd;
	dwc2->params = params;

	rc = vmm_devtree_regaddr(dev->of_node, &hcd->rsrc_start, 0);
	if (rc) {
		goto fail_destroy_hcd;
	}

	rc = vmm_devtree_regsize(dev->of_node, &hcd->rsrc_len, 0);
	if (rc) {
		goto fail_destroy_hcd;
	}

	rc = vmm_devtree_request_regmap(dev->of_node, &regs, 0, "DWC2");
	if (rc) {
		goto fail_destroy_hcd;
	}
	dwc2->regs = (struct dwc2_core_regs *)regs;

	dwc2->irq = vmm_devtree_irq_parse_map(dev->of_node, 0);
	if (!dwc2->irq) {
		rc = VMM_ENODEV;
		goto fail_unmap_regs;
	}

	dwc2->rh_devnum = 0;

	snpsid = vmm_readl((void *)&dwc2->regs->gsnpsid);
	if ((snpsid & DWC2_SNPSID_DEVID_MASK) != DWC2_SNPSID_DEVID_VER_2xx) {
		vmm_lerror(dev->name,
			   "SNPSID invalid (not DWC2 OTG device): %08x\n",
			   snpsid);
		rc = VMM_ENODEV;
		goto fail_unmap_regs;
	}

	dwc2->hc_count = dwc2_hc_count(dwc2);

	dwc2->hc_next = 0;
	INIT_SPIN_LOCK(&dwc2->hc_next_lock);

	for (i = 0; i < dwc2->hc_count; i++) {
		INIT_SPIN_LOCK(&dwc2->hc_urb_lock[i]);
		dwc2->hc_urb_int[i] = NULL;
		INIT_COMPLETION(&dwc2->hc_urb_pending[i]);
		INIT_LIST_HEAD(&dwc2->hc_urb_pending_list[i]);
	}

	for (i = 0; i < dwc2->hc_count; i++) {
		hc = &dwc2->hcs[i];
		hc->index = i;
		hc->dwc2 = dwc2;
		hc->regs = &dwc2->regs->hc_regs[i];
		hc->status_buffer = vmm_dma_zalloc(DWC2_STATUS_BUF_SIZE);
		if (!hc->status_buffer) {
			rc = VMM_ENOMEM;
			goto fail_cleanup_hcs;
		}
		vmm_snprintf(name, sizeof(name), "%s/hc%d", dev->name, i);
		hc->hc_thread = vmm_threads_create(name, dwc2_hc_worker, hc,
						   VMM_THREAD_DEF_PRIORITY,
						   VMM_THREAD_DEF_TIME_SLICE);
		if (!hc->hc_thread) {
			rc = VMM_ENOSPC;
			goto fail_cleanup_hcs;
		}
		vmm_threads_start(hc->hc_thread);
	}

	vmm_linfo(dev->name, "Core Release %x.%03x with %d Channels\n",
		  snpsid >> 12 & 0xf, snpsid & 0xfff, dwc2->hc_count);

	rc = usb_add_hcd(hcd, dwc2->irq, 0);
	if (rc) {
		goto fail_cleanup_hcs;
	}

	dev->priv = hcd;

	return VMM_OK;

fail_cleanup_hcs:
	for (i = 0; i < dwc2->hc_count; i++) {
		hc = &dwc2->hcs[i];
		if (hc->hc_thread) {
			vmm_threads_stop(hc->hc_thread);
			vmm_threads_destroy(hc->hc_thread);
			hc->hc_thread = NULL;
		}
		if (hc->status_buffer) {
			vmm_dma_free(hc->status_buffer);
			hc->status_buffer = NULL;
		}
	}
fail_unmap_regs:
	vmm_devtree_regunmap_release(dev->of_node,
				     (virtual_addr_t)dwc2->regs, 0);
fail_destroy_hcd:
	usb_dref_hcd(hcd);
fail:
	return rc;
}

static int dwc2_driver_remove(struct vmm_device *dev)
{
	u32 i;
	struct dwc2_hc *hc;
	struct usb_hcd *hcd = dev->priv;
	struct dwc2_control *dwc2 = usb_hcd_priv(hcd);

	usb_remove_hcd(hcd);

	for (i = 0; i < dwc2->hc_count; i++) {
		hc = &dwc2->hcs[i];
		if (hc->hc_thread) {
			vmm_threads_stop(hc->hc_thread);
			vmm_threads_destroy(hc->hc_thread);
			hc->hc_thread = NULL;
		}
		if (hc->status_buffer) {
			vmm_dma_free(hc->status_buffer);
			hc->status_buffer = NULL;
		}
	}

	vmm_devtree_regunmap_release(dev->of_node,
				     (virtual_addr_t)dwc2->regs, 0);

	usb_dref_hcd(hcd);

	return VMM_OK;
}

static const struct dwc2_core_params params_bcm2835 = {
	.otg_cap			= 0,	/* HNP/SRP capable */
	.otg_ver			= 0,	/* 1.3 */
	.dma_enable			= 1,
	.dma_desc_enable		= 0,
	.dma_burst_size			= 32,
	.speed				= 0,	/* High Speed */
	.enable_dynamic_fifo		= 1,
	.en_multiple_tx_fifo		= 1,
	.host_rx_fifo_size		= 532,	/* 532 DWORDs */
	.host_nperio_tx_fifo_size	= 256,	/* 256 DWORDs */
	.host_perio_tx_fifo_size	= 512,	/* 512 DWORDs */
	.max_transfer_size		= 65535,
	.max_packet_count		= 511,
	.host_channels			= 8,
	.phy_type			= 1,	/* UTMI */
	.phy_utmi_width			= 8,	/* 8 bits */
	.phy_ulpi_ddr			= 0,	/* Single */
	.phy_ulpi_ext_vbus		= 1,
	.oc_disable			= FALSE,
	.i2c_enable			= 0,
	.ulpi_fs_ls			= 0,
	.host_support_fs_ls_low_power	= 0,
	.host_ls_low_power_phy_clk	= 0,	/* 48 MHz */
	.ts_dline			= 0,
	.reload_ctl			= 0,
	.ahbcfg				= 0x10,
	.uframe_sched			= 0,
	.ic_usb_cap			= 0,
	.dma_offset			= 0x40000000,
};

static const struct dwc2_core_params params_bcm2836 = {
	.otg_cap			= 0,	/* HNP/SRP capable */
	.otg_ver			= 0,	/* 1.3 */
	.dma_enable			= 1,
	.dma_desc_enable		= 0,
	.dma_burst_size			= 32,
	.speed				= 0,	/* High Speed */
	.enable_dynamic_fifo		= 1,
	.en_multiple_tx_fifo		= 1,
	.host_rx_fifo_size		= 532,	/* 532 DWORDs */
	.host_nperio_tx_fifo_size	= 256,	/* 256 DWORDs */
	.host_perio_tx_fifo_size	= 512,	/* 512 DWORDs */
	.max_transfer_size		= 65535,
	.max_packet_count		= 511,
	.host_channels			= 8,
	.phy_type			= 1,	/* UTMI */
	.phy_utmi_width			= 8,	/* 8 bits */
	.phy_ulpi_ddr			= 0,	/* Single */
	.phy_ulpi_ext_vbus		= 1,
	.oc_disable			= FALSE,
	.i2c_enable			= 0,
	.ulpi_fs_ls			= 0,
	.host_support_fs_ls_low_power	= 0,
	.host_ls_low_power_phy_clk	= 0,	/* 48 MHz */
	.ts_dline			= 0,
	.reload_ctl			= 0,
	.ahbcfg				= 0x10,
	.uframe_sched			= 0,
	.ic_usb_cap			= 0,
	.dma_offset			= 0xc0000000,
};

static struct vmm_devtree_nodeid dwc2_devid_table[] = {
	{ .compatible = "brcm,bcm2835-usb", .data = &params_bcm2835 },
	{ .compatible = "brcm,bcm2836-usb", .data = &params_bcm2836 },
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
