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
#include <vmm_heap.h>
#include <vmm_workqueue.h>
#include <vmm_host_aspace.h>
#include <vmm_modules.h>
#include <vmm_devdrv.h>
#include <drv/usb.h>
#include <drv/usb/hcd.h>

#define MODULE_DESC			"Designware USB2.0 HCD Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(USB_CORE_IPRIORITY + 1)
#define	MODULE_INIT			dwc2_driver_init
#define	MODULE_EXIT			dwc2_driver_exit

/**
 * struct dwc2_core_params - Parameters for configuring the core
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

static int dwc2_driver_probe(struct vmm_device *dev,
			     const struct vmm_devtree_nodeid *devid)
{
	/* FIXME: */
	return VMM_OK;
}

static int dwc2_driver_remove(struct vmm_device *dev)
{
	/* FIXME: */
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
	{.compatible = "brcm,bcm2835-usb", .data = &params_bcm2835 },
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
