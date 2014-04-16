/**
 * Copyright (c) 2012 Jean-Christophe Dubois.
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
 * @file brd_main.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @author Anup Patel (anup@brainfault.org)
 * @brief main source file for board specific code
 */

#include <vmm_error.h>
#include <vmm_main.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_host_io.h>
#include <vmm_host_aspace.h>

#include <generic_board.h>

/* ------------------------------------------------------------------------
 *  Versatile Registers
 * ------------------------------------------------------------------------
 * 
 */
#define VERSATILE_SYS_ID_OFFSET               0x00
#define VERSATILE_SYS_SW_OFFSET               0x04
#define VERSATILE_SYS_LED_OFFSET              0x08
#define VERSATILE_SYS_OSC0_OFFSET             0x0C

#if defined(CONFIG_ARCH_VERSATILE_PB)
#define VERSATILE_SYS_OSC1_OFFSET             0x10
#define VERSATILE_SYS_OSC2_OFFSET             0x14
#define VERSATILE_SYS_OSC3_OFFSET             0x18
#define VERSATILE_SYS_OSC4_OFFSET             0x1C
#elif defined(CONFIG_MACH_VERSATILE_AB)
#define VERSATILE_SYS_OSC1_OFFSET             0x1C
#endif

#define VERSATILE_SYS_OSCCLCD_OFFSET          0x1c

#define VERSATILE_SYS_LOCK_OFFSET             0x20
#define VERSATILE_SYS_100HZ_OFFSET            0x24
#define VERSATILE_SYS_CFGDATA1_OFFSET         0x28
#define VERSATILE_SYS_CFGDATA2_OFFSET         0x2C
#define VERSATILE_SYS_FLAGS_OFFSET            0x30
#define VERSATILE_SYS_FLAGSSET_OFFSET         0x30
#define VERSATILE_SYS_FLAGSCLR_OFFSET         0x34
#define VERSATILE_SYS_NVFLAGS_OFFSET          0x38
#define VERSATILE_SYS_NVFLAGSSET_OFFSET       0x38
#define VERSATILE_SYS_NVFLAGSCLR_OFFSET       0x3C
#define VERSATILE_SYS_RESETCTL_OFFSET         0x40
#define VERSATILE_SYS_PCICTL_OFFSET           0x44
#define VERSATILE_SYS_MCI_OFFSET              0x48
#define VERSATILE_SYS_FLASH_OFFSET            0x4C
#define VERSATILE_SYS_CLCD_OFFSET             0x50
#define VERSATILE_SYS_CLCDSER_OFFSET          0x54
#define VERSATILE_SYS_BOOTCS_OFFSET           0x58
#define VERSATILE_SYS_24MHz_OFFSET            0x5C
#define VERSATILE_SYS_MISC_OFFSET             0x60
#define VERSATILE_SYS_TEST_OSC0_OFFSET        0x80
#define VERSATILE_SYS_TEST_OSC1_OFFSET        0x84
#define VERSATILE_SYS_TEST_OSC2_OFFSET        0x88
#define VERSATILE_SYS_TEST_OSC3_OFFSET        0x8C
#define VERSATILE_SYS_TEST_OSC4_OFFSET        0x90

/* 
 * Values for VERSATILE_SYS_RESET_CTRL
 */
#define VERSATILE_SYS_CTRL_RESET_CONFIGCLR    0x01
#define VERSATILE_SYS_CTRL_RESET_CONFIGINIT   0x02
#define VERSATILE_SYS_CTRL_RESET_DLLRESET     0x03
#define VERSATILE_SYS_CTRL_RESET_PLLRESET     0x04
#define VERSATILE_SYS_CTRL_RESET_POR          0x05
#define VERSATILE_SYS_CTRL_RESET_DoC          0x06

#define VERSATILE_SYS_CTRL_LED         (1 << 0)


/* ------------------------------------------------------------------------
 *  Versatile control registers
 * ------------------------------------------------------------------------
 */

/* 
 * VERSATILE_IDFIELD
 *
 * 31:24 = manufacturer (0x41 = ARM)
 * 23:16 = architecture (0x08 = AHB system bus, ASB processor bus)
 * 15:12 = FPGA (0x3 = XVC600 or XVC600E)
 * 11:4  = build value
 * 3:0   = revision number (0x1 = rev B (AHB))
 */

/*
 * VERSATILE_SYS_LOCK
 *     control access to SYS_OSCx, SYS_CFGDATAx, SYS_RESETCTL, 
 *     SYS_CLD, SYS_BOOTCS
 */
#define VERSATILE_SYS_LOCK_LOCKED    (1 << 16)
#define VERSATILE_SYS_LOCKVAL		0xA05F	
#define VERSATILE_SYS_LOCKVAL_MASK	0xFFFF

/*
 * VERSATILE_SYS_FLASH
 */
#define VERSATILE_FLASHPROG_FLVPPEN	(1 << 0)	/* Enable writing to flash */

/*
 * VERSATILE_INTREG
 *     - used to acknowledge and control MMCI and UART interrupts 
 */
#define VERSATILE_INTREG_WPROT        0x00    /* MMC protection status (no interrupt generated) */
#define VERSATILE_INTREG_RI0          0x01    /* Ring indicator UART0 is asserted,              */
#define VERSATILE_INTREG_CARDIN       0x08    /* MMCI card in detect                            */
                                                /* write 1 to acknowledge and clear               */
#define VERSATILE_INTREG_RI1          0x02    /* Ring indicator UART1 is asserted,              */
#define VERSATILE_INTREG_CARDINSERT   0x03    /* Signal insertion of MMC card                   */

/*
 * Global board context
 */

static virtual_addr_t versatile_sys_base;

/*
 * Reset & Shutdown
 */

static int versatile_reset(void)
{
	vmm_writel(0x101, (void *)(versatile_sys_base +
			   VERSATILE_SYS_RESETCTL_OFFSET));

	return VMM_OK;
}

static int versatile_shutdown(void)
{
	/* FIXME: TBD */
	return VMM_EFAIL;
}

/*
 * Initialization functions
 */

static int __init versatile_early_init(struct vmm_devtree_node *node)
{
	int rc;

	/* Host aspace, Heap, Device tree, and Host IRQ available.
	 *
	 * Do necessary early stuff like:
	 * iomapping devices, 
	 * SOC clocking init, 
	 * Setting-up system data in device tree nodes,
	 * ....
	 */

	/* Map sysreg */
	node = vmm_devtree_find_compatible(NULL, NULL, "arm,versatile-sysreg");
	if (!node) {
		return VMM_ENODEV;
	}
	rc = vmm_devtree_regmap(node, &versatile_sys_base, 0);
	if (rc) {
		return rc;
	}

	/* Register reset & shutdown callbacks */
	vmm_register_system_reset(versatile_reset);
	vmm_register_system_shutdown(versatile_shutdown);

	return 0;
}

static int __init versatile_final_init(struct vmm_devtree_node *node)
{
	/* Nothing to do here. */
	return VMM_OK;
}

static struct generic_board versatile_info = {
	.name		= "Versatile",
	.early_init	= versatile_early_init,
	.final_init	= versatile_final_init,
};

GENERIC_BOARD_DECLARE(versatile, "arm,versatile", &versatile_info);
