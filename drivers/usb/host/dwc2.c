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
 * This source is largely adapted from Raspberry Pi u-boot sources:
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

/* Use only HC channel 0. */
#define DWC2_HC_CHANNEL			0
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
	int i2c_enable;
	int ulpi_fs_ls;
	int host_support_fs_ls_low_power;
	int host_ls_low_power_phy_clk;
	int ts_dline;
	int reload_ctl;
	int ahbcfg;
	int uframe_sched;
	int ic_usb_cap;
};

struct dwc2_control {
	const struct dwc2_core_params *params;
	struct dwc2_core_regs *regs;
	u32 irq;
	u32 rh_devnum;

	int bulk_data_toggle[DWC2_MAX_DEVICE][DWC2_MAX_ENDPOINT];
	int control_data_toggle[DWC2_MAX_DEVICE][DWC2_MAX_ENDPOINT];

	struct vmm_mutex urb_process_mutex;

	vmm_spinlock_t urb_lock;
	struct dlist urb_pending_list;
	struct vmm_completion urb_pending;
	struct vmm_thread *urb_thread;
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
 *
 * @param dwc2 Programming view of DWC2 controller
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
 *
 * @param dwc2 Programming view of DWC2 controller.
 * @param num Tx FIFO to flush.
 */
static void dwc2_flush_tx_fifo(struct dwc2_control *dwc2, const int num)
{
	int ret;

	vmm_writel(DWC2_GRSTCTL_TXFFLSH | (num << DWC2_GRSTCTL_TXFNUM_OFFSET),
		   &dwc2->regs->grstctl);
	ret = wait_for_bit(&dwc2->regs->grstctl, DWC2_GRSTCTL_TXFFLSH, 0);
	if (ret)
		vmm_printf("%s: Timeout!\n", __func__);
}

/*
 * Flush Rx FIFO.
 *
 * @param dwc2 Programming view of DWC2 controller.
 */
static void dwc2_flush_rx_fifo(struct dwc2_control *dwc2)
{
	int ret;

	vmm_writel(DWC2_GRSTCTL_RXFFLSH, &dwc2->regs->grstctl);
	ret = wait_for_bit(&dwc2->regs->grstctl, DWC2_GRSTCTL_RXFFLSH, 0);
	if (ret)
		vmm_printf("%s: Timeout!\n", __func__);
}

/*
 * Do core a soft reset of the core.  Be careful with this because it
 * resets all the internal state machines of the core.
 *
 * @param dwc2 Programming view of DWC2 controller
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

/*
 * This function initializes the DWC2 controller registers for
 * host mode.
 *
 * This function flushes the Tx and Rx FIFOs and it flushes any entries in the
 * request queues. Host channels are reset to ensure that they are ready for
 * performing transfers.
 *
 * @param dwc2 Programming view of DWC2 controller
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
		ptxfifosize |= dwc2->params->host_nperio_tx_fifo_size <<
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
 *
 * @param dwc2 Programming view of the DWC2 controller
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
 *
 * @param dwc2 Programming view of DWC2 controller
 * @param hc Information needed to initialize the host channel
 */
static void dwc2_hc_init(struct dwc2_control *dwc2, u8 hc_num,
			 u8 dev_addr, u8 ep_num, u8 ep_is_in,
			 u8 ep_type, u8 max_packet)
{
	struct dwc2_hc_regs *hc_regs = &dwc2->regs->hc_regs[hc_num];
	const u32 hcchar = (dev_addr << DWC2_HCCHAR_DEVADDR_OFFSET) |
				(ep_num << DWC2_HCCHAR_EPNUM_OFFSET) |
				(ep_is_in << DWC2_HCCHAR_EPDIR_OFFSET) |
				(ep_type << DWC2_HCCHAR_EPTYPE_OFFSET) |
				(max_packet << DWC2_HCCHAR_MPS_OFFSET);

	/* Clear old interrupt conditions for this host channel. */
	vmm_writel(0x3fff, &hc_regs->hcint);

	/*
	 * Program the HCCHARn register with the endpoint characteristics
	 * for the current transfer.
	 */
	vmm_writel(hcchar, &hc_regs->hcchar);

	/* Program the HCSPLIT register for SPLITs */
	vmm_writel(0, &hc_regs->hcsplt);
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

#define DWC2_HCINT_COMP_HLT		(DWC2_HCINT_XFERCOMP | \
					 DWC2_HCINT_CHHLTD)
#define DWC2_HCINT_COMP_HLT_ACK		(DWC2_HCINT_XFERCOMP | \
					 DWC2_HCINT_CHHLTD | \
					 DWC2_HCINT_ACK)

static int dwc2_control_msg(struct dwc2_control *dwc2,
			    struct urb *u)
{
	void *buffer = u->transfer_buffer;
	int len = u->transfer_buffer_length;
	struct dwc2_hc_regs *hc_regs;
	int done = 0, rc = VMM_OK;
	int devnum = usb_pipedevice(u->pipe);
	int ep = usb_pipeendpoint(u->pipe);
	u32 hctsiz = 0, tmp, hcint;
	unsigned int timeout = 1000000;
	physical_addr_t pa;
	u8 __cacheline_aligned status_buffer[DWC2_STATUS_BUF_SIZE];

	/* Process root hub control messages differently */
	if (u->dev->devnum == dwc2->rh_devnum) {
		return dwc2_control_rh_msg(dwc2, u);
	}

	/* Ensure that transfer buffer is cache aligned */
	if ((unsigned long)buffer & (VMM_CACHE_LINE_SIZE - 1)) {
		vmm_printf("%s: dev=%s transfer buffer not cache aligned\n",
			   __func__, u->dev->dev.name);
		rc = VMM_EIO;
		goto out;
	}

	/* Determine host channel registers */
	hc_regs = &dwc2->regs->hc_regs[DWC2_HC_CHANNEL];

	if (len > DWC2_DATA_BUF_SIZE) {
		vmm_printf("%s: %d is more then available buffer size(%d)\n",
		       __func__, len, DWC2_DATA_BUF_SIZE);
		rc = VMM_EINVALID;
		goto out;
	}

	/* Initialize channel, OUT for setup buffer */
	dwc2_hc_init(dwc2, DWC2_HC_CHANNEL, devnum, ep, 0,
		     DWC2_HCCHAR_EPTYPE_CONTROL,
		     usb_maxpacket(u->dev, u->pipe));

	/* SETUP stage  */
	vmm_writel((8 << DWC2_HCTSIZ_XFERSIZE_OFFSET) |
	       (1 << DWC2_HCTSIZ_PKTCNT_OFFSET) |
	       (DWC2_HC_PID_SETUP << DWC2_HCTSIZ_PID_OFFSET),
	       &hc_regs->hctsiz);

	rc = vmm_host_va2pa((virtual_addr_t)u->setup_packet, &pa);
	if (rc) {
		vmm_printf("%s: VA2PA error!\n", __func__);
		goto out;
	}
	vmm_writel((u32)pa, &hc_regs->hcdma);

	/* Set host channel enable after all other setup is complete. */
	vmm_clrsetbits_le32(&hc_regs->hcchar, DWC2_HCCHAR_MULTICNT_MASK |
			DWC2_HCCHAR_CHEN | DWC2_HCCHAR_CHDIS,
			(1 << DWC2_HCCHAR_MULTICNT_OFFSET) | DWC2_HCCHAR_CHEN);

	rc = wait_for_bit(&hc_regs->hcint, DWC2_HCINT_CHHLTD, 1);
	if (rc) {
		vmm_printf("%s: Timeout!\n", __func__);
		goto out;
	}

	hcint = vmm_readl(&hc_regs->hcint);
	if (!(hcint & DWC2_HCINT_COMP_HLT)) {
		vmm_printf("%s: Error (HCINT=%08x)\n", __func__, hcint);
		rc = VMM_EINVALID;
		goto out;
	}

	/* Clear interrupts */
	vmm_writel(0, &hc_regs->hcintmsk);
	vmm_writel(0xFFFFFFFF, &hc_regs->hcint);

	if (buffer) {
		/* DATA stage */
		dwc2_hc_init(dwc2, DWC2_HC_CHANNEL, devnum, ep,
			     usb_pipein(u->pipe),
			     DWC2_HCCHAR_EPTYPE_CONTROL,
			     usb_maxpacket(u->dev, u->pipe));

		/* TODO: check if len < 64 */
		dwc2->control_data_toggle[devnum][ep] = DWC2_HC_PID_DATA1;
		vmm_writel((len << DWC2_HCTSIZ_XFERSIZE_OFFSET) |
			   (1 << DWC2_HCTSIZ_PKTCNT_OFFSET) |
			   (dwc2->control_data_toggle[devnum][ep] <<
				DWC2_HCTSIZ_PID_OFFSET),
			   &hc_regs->hctsiz);

		rc = vmm_host_va2pa((virtual_addr_t)buffer, &pa);
		if (rc) {
			vmm_printf("%s: VA2PA error!\n", __func__);
			goto out;
		}
		vmm_writel((u32)pa, &hc_regs->hcdma);

		/* Set host channel enable after all other setup is complete */
		vmm_clrsetbits_le32(&hc_regs->hcchar, DWC2_HCCHAR_MULTICNT_MASK |
				DWC2_HCCHAR_CHEN | DWC2_HCCHAR_CHDIS,
				(1 << DWC2_HCCHAR_MULTICNT_OFFSET) |
				DWC2_HCCHAR_CHEN);

		while (1) {
			hcint = vmm_readl(&hc_regs->hcint);
			if (!(hcint & DWC2_HCINT_CHHLTD))
				continue;

			if (hcint & DWC2_HCINT_XFERCOMP) {
				hctsiz = vmm_readl(&hc_regs->hctsiz);
				done = len;

				tmp = hctsiz & DWC2_HCTSIZ_XFERSIZE_MASK;
				tmp >>= DWC2_HCTSIZ_XFERSIZE_OFFSET;

				if (usb_pipein(u->pipe))
					done -= tmp;
			}

			if (hcint & DWC2_HCINT_ACK) {
				tmp = hctsiz & DWC2_HCTSIZ_PID_MASK;
				tmp >>= DWC2_HCTSIZ_PID_OFFSET;
				if (tmp == DWC2_HC_PID_DATA0) {
					dwc2->control_data_toggle[devnum][ep] =
						DWC2_HC_PID_DATA0;
				} else {
					dwc2->control_data_toggle[devnum][ep] =
						DWC2_HC_PID_DATA1;
				}
			}

			if (!(hcint & DWC2_HCINT_COMP_HLT)) {
				vmm_printf("%s: Error (HCINT=%08x)\n",
				       __func__, hcint);
				rc = VMM_EIO;
				goto out;
			}

			if (!--timeout) {
				vmm_printf("%s: Timeout!\n", __func__);
				rc = VMM_ETIMEDOUT;
				goto out;
			}

			break;
		}
	} /* End of DATA stage */

	dwc2_hc_init(dwc2, DWC2_HC_CHANNEL, devnum, ep,
		     ((len == 0) || usb_pipeout(u->pipe)) ? 1 : 0,
		     DWC2_HCCHAR_EPTYPE_CONTROL,
		     usb_maxpacket(u->dev, u->pipe));

	vmm_writel((1 << DWC2_HCTSIZ_PKTCNT_OFFSET) |
	       (DWC2_HC_PID_DATA1 << DWC2_HCTSIZ_PID_OFFSET),
	       &hc_regs->hctsiz);

	rc = vmm_host_va2pa((virtual_addr_t)status_buffer, &pa);
	if (rc) {
		vmm_printf("%s: VA2PA error!\n", __func__);
		goto out;
	}
	vmm_writel((u32)pa, &hc_regs->hcdma);

	/* Set host channel enable after all other setup is complete. */
	vmm_clrsetbits_le32(&hc_regs->hcchar,
			    DWC2_HCCHAR_MULTICNT_MASK |
			    DWC2_HCCHAR_CHEN | DWC2_HCCHAR_CHDIS,
			    (1 << DWC2_HCCHAR_MULTICNT_OFFSET) |
			    DWC2_HCCHAR_CHEN);

	while (1) {
		hcint = vmm_readl(&hc_regs->hcint);
		if (hcint & DWC2_HCINT_CHHLTD)
			break;
	}

	if (!(hcint & DWC2_HCINT_COMP_HLT)) {
		vmm_printf("%s: Error (HCINT=%08x)\n", __func__, hcint);
		rc = VMM_EIO;
	}

out:
	u->actual_length = done;

	return rc;
}

static int dwc2_bulk_msg(struct dwc2_control *dwc2,
			 struct urb *u)
{
	void *buffer = u->transfer_buffer;
	int len = u->transfer_buffer_length;
	int devnum = usb_pipedevice(u->pipe);
	int ep = usb_pipeendpoint(u->pipe);
	int max = usb_maxpacket(u->dev, u->pipe);
	int done = 0, rc = VMM_OK, stop_transfer = 0;
	u32 hctsiz, hcint, tmp, xfer_len, num_packets;
	struct dwc2_hc_regs *hc_regs;
	physical_addr_t pa;
	unsigned int timeout = 1000000;

	/* Reject root hub bulk messages differently */
	if (u->dev->devnum == dwc2->rh_devnum) {
		return VMM_EINVALID;
	}

	/* Ensure that transfer buffer is cache aligned */
	if ((unsigned long)buffer & (VMM_CACHE_LINE_SIZE - 1)) {
		vmm_printf("%s: dev=%s transfer buffer not cache aligned\n",
			   __func__, u->dev->dev.name);
		rc = VMM_EIO;
		goto out;
	}

	/* Determine host channel registers */
	hc_regs = &dwc2->regs->hc_regs[DWC2_HC_CHANNEL];

	if (len > DWC2_DATA_BUF_SIZE) {
		vmm_printf("%s: %d is more then available buffer size (%d)\n",
		       __func__, len, DWC2_DATA_BUF_SIZE);
		rc = VMM_EINVALID;
		goto out;
	}

	while ((done < len) && !stop_transfer) {
		/* Initialize channel */
		dwc2_hc_init(dwc2, DWC2_HC_CHANNEL, devnum, ep,
			     usb_pipein(u->pipe),
			     DWC2_HCCHAR_EPTYPE_BULK, max);

		xfer_len = len - done;
		/* Make sure that xfer_len is a multiple of max packet size. */
		if (xfer_len > dwc2->params->max_transfer_size)
			xfer_len = dwc2->params->max_transfer_size - max + 1;

		if (xfer_len > 0) {
			num_packets = udiv32((xfer_len + max - 1), max);
			if (num_packets > dwc2->params->max_packet_count) {
				num_packets = dwc2->params->max_packet_count;
				xfer_len = num_packets * max;
			}
		} else {
			num_packets = 1;
		}

		if (usb_pipein(u->pipe))
			xfer_len = num_packets * max;

		vmm_writel((xfer_len << DWC2_HCTSIZ_XFERSIZE_OFFSET) |
			   (num_packets << DWC2_HCTSIZ_PKTCNT_OFFSET) |
			   (dwc2->bulk_data_toggle[devnum][ep] <<
					DWC2_HCTSIZ_PID_OFFSET),
			   &hc_regs->hctsiz);

		rc = vmm_host_va2pa((virtual_addr_t)buffer, &pa);
		if (rc) {
			vmm_printf("%s: VA2PA error!\n", __func__);
			goto out;
		}
		vmm_writel((u32)pa, &hc_regs->hcdma);

		/* Set host channel enable after all other setup is complete. */
		vmm_clrsetbits_le32(&hc_regs->hcchar,
				    DWC2_HCCHAR_MULTICNT_MASK |
				    DWC2_HCCHAR_CHEN | DWC2_HCCHAR_CHDIS,
				    (1 << DWC2_HCCHAR_MULTICNT_OFFSET) |
				    DWC2_HCCHAR_CHEN);

		while (1) {
			hcint = vmm_readl(&hc_regs->hcint);

			if (!(hcint & DWC2_HCINT_CHHLTD))
				continue;

			if (hcint & DWC2_HCINT_XFERCOMP) {
				hctsiz = vmm_readl(&hc_regs->hctsiz);
				done += xfer_len;

				tmp = hctsiz & DWC2_HCTSIZ_XFERSIZE_MASK;
				tmp >>= DWC2_HCTSIZ_XFERSIZE_OFFSET;

				if (usb_pipein(u->pipe)) {
					done -= tmp;
					if (hctsiz & DWC2_HCTSIZ_XFERSIZE_MASK)
						stop_transfer = 1;
				}

				tmp = hctsiz & DWC2_HCTSIZ_PID_MASK;
				tmp >>= DWC2_HCTSIZ_PID_OFFSET;
				if (tmp == DWC2_HC_PID_DATA1) {
					dwc2->bulk_data_toggle[devnum][ep] =
						DWC2_HC_PID_DATA1;
				} else {
					dwc2->bulk_data_toggle[devnum][ep] =
						DWC2_HC_PID_DATA0;
				}
				break;
			}

			if (hcint & DWC2_HCINT_STALL) {
				vmm_printf("%s: Channel halted\n", __func__);
				dwc2->bulk_data_toggle[devnum][ep] =
							DWC2_HC_PID_DATA0;

				stop_transfer = 1;
				break;
			}

			if (!--timeout) {
				vmm_printf("%s: Timeout!\n", __func__);
				break;
			}
		}
	}

	vmm_writel(0, &hc_regs->hcintmsk);
	vmm_writel(0xFFFFFFFF, &hc_regs->hcint);

out:
	u->actual_length = done;

	return rc;
}

static int dwc2_int_msg(struct dwc2_control *dwc2,
			struct urb *u)
{
	vmm_printf("%s: dev=%s pipe=0x%x buf=%p len=%d interval=%d\n",
		   __func__, u->dev->dev.name, u->pipe, u->transfer_buffer,
		   u->transfer_buffer_length, u->interval);
	return VMM_ENOTAVAIL;
}

static void dwc2_urb_process(struct usb_hcd *hcd,
			     struct dwc2_control *dwc2,
			     struct urb *u)
{
	int rc;

	vmm_mutex_lock(&dwc2->urb_process_mutex);

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

	vmm_mutex_unlock(&dwc2->urb_process_mutex);

	usb_hcd_giveback_urb(hcd, u, rc);
}

static int dwc2_worker(void *data)
{
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

		dwc2_urb_process(hcd, dwc2, u);
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
			dwc2->control_data_toggle[i][j] = DWC2_HC_PID_DATA1;
			dwc2->bulk_data_toggle[i][j] = DWC2_HC_PID_DATA0;
		}
	}

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
	irq_flags_t flags;
	struct dwc2_control *dwc2 = usb_hcd_priv(hcd);

	if (vmm_scheduler_orphan_context()) {
		dwc2_urb_process(hcd, dwc2, urb);
	} else {
		vmm_spin_lock_irqsave(&dwc2->urb_lock, flags);
		list_add_tail(&urb->urb_list, &dwc2->urb_pending_list);
		vmm_spin_unlock_irqrestore(&dwc2->urb_lock, flags);

		vmm_completion_complete(&dwc2->urb_pending);
	}

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

	INIT_MUTEX(&dwc2->urb_process_mutex);
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
	usb_dref_hcd(hcd);
fail:
	return rc;
}

static int dwc2_driver_remove(struct vmm_device *dev)
{
	struct usb_hcd *hcd = dev->priv;
	struct dwc2_control *dwc2 = usb_hcd_priv(hcd);

	vmm_threads_stop(dwc2->urb_thread);

	usb_remove_hcd(hcd);

	vmm_threads_destroy(dwc2->urb_thread);

	vmm_devtree_regunmap(dev->node, (virtual_addr_t)dwc2->regs, 0);

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
	.ic_usb_cap			= 0,
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
