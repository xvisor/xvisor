/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file plat.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Versatile Express platform configuration
 */
#ifndef __VEXPRESS_PLAT_H__
#define __VEXPRESS_PLAT_H__

/*
 * Memory definitions
 */
#define VEXPRESS_BOOT_ROM_LO          0x30000000	/* DoC Base (64Mb)... */
#define VEXPRESS_BOOT_ROM_HI          0x30000000
#define VEXPRESS_BOOT_ROM_BASE        VEXPRESS_BOOT_ROM_HI	/*  Normal position */
#define VEXPRESS_BOOT_ROM_SIZE        SZ_64M

#define VEXPRESS_SSRAM_BASE	/* VEXPRESS_SSMC_BASE ? */
#define VEXPRESS_SSRAM_SIZE           SZ_2M

/* 
 *  SDRAM
 */
#define VEXPRESS_SDRAM_BASE           0x00000000

/* 
 *  Logic expansion modules
 * 
 */

/* ------------------------------------------------------------------------
 *  RealView Registers
 * ------------------------------------------------------------------------
 * 
 */
#define VEXPRESS_SYS_ID_OFFSET               0x00
#define VEXPRESS_SYS_SW_OFFSET               0x04
#define VEXPRESS_SYS_LED_OFFSET              0x08
#define VEXPRESS_SYS_OSC0_OFFSET             0x0C

#define VEXPRESS_SYS_OSC1_OFFSET             0x10
#define VEXPRESS_SYS_OSC2_OFFSET             0x14
#define VEXPRESS_SYS_OSC3_OFFSET             0x18
#define VEXPRESS_SYS_OSC4_OFFSET             0x1C	/* OSC1 for RealView/AB */

#define VEXPRESS_SYS_LOCK_OFFSET             0x20
#define VEXPRESS_SYS_100HZ_OFFSET            0x24
#define VEXPRESS_SYS_CFGDATA1_OFFSET         0x28
#define VEXPRESS_SYS_CFGDATA2_OFFSET         0x2C
#define VEXPRESS_SYS_FLAGS_OFFSET            0x30
#define VEXPRESS_SYS_FLAGSSET_OFFSET         0x30
#define VEXPRESS_SYS_FLAGSCLR_OFFSET         0x34
#define VEXPRESS_SYS_NVFLAGS_OFFSET          0x38
#define VEXPRESS_SYS_NVFLAGSSET_OFFSET       0x38
#define VEXPRESS_SYS_NVFLAGSCLR_OFFSET       0x3C
#define VEXPRESS_SYS_RESETCTL_OFFSET         0x40
#define VEXPRESS_SYS_PCICTL_OFFSET           0x44
#define VEXPRESS_SYS_MCI_OFFSET              0x48
#define VEXPRESS_SYS_FLASH_OFFSET            0x4C
#define VEXPRESS_SYS_CLCD_OFFSET             0x50
#define VEXPRESS_SYS_CLCDSER_OFFSET          0x54
#define VEXPRESS_SYS_BOOTCS_OFFSET           0x58
#define VEXPRESS_SYS_24MHz_OFFSET            0x5C
#define VEXPRESS_SYS_MISC_OFFSET             0x60
#define VEXPRESS_SYS_IOSEL_OFFSET            0x70
#define VEXPRESS_SYS_PROCID_OFFSET           0x84
#define VEXPRESS_SYS_TEST_OSC0_OFFSET        0xC0
#define VEXPRESS_SYS_TEST_OSC1_OFFSET        0xC4
#define VEXPRESS_SYS_TEST_OSC2_OFFSET        0xC8
#define VEXPRESS_SYS_TEST_OSC3_OFFSET        0xCC
#define VEXPRESS_SYS_TEST_OSC4_OFFSET        0xD0

#define VEXPRESS_SYS_BASE                    0x10000000
#define VEXPRESS_SYS_ID                      (VEXPRESS_SYS_BASE + VEXPRESS_SYS_ID_OFFSET)
#define VEXPRESS_SYS_SW                      (VEXPRESS_SYS_BASE + VEXPRESS_SYS_SW_OFFSET)
#define VEXPRESS_SYS_LED                     (VEXPRESS_SYS_BASE + VEXPRESS_SYS_LED_OFFSET)
#define VEXPRESS_SYS_OSC0                    (VEXPRESS_SYS_BASE + VEXPRESS_SYS_OSC0_OFFSET)
#define VEXPRESS_SYS_OSC1                    (VEXPRESS_SYS_BASE + VEXPRESS_SYS_OSC1_OFFSET)

#define VEXPRESS_SYS_LOCK                    (VEXPRESS_SYS_BASE + VEXPRESS_SYS_LOCK_OFFSET)
#define VEXPRESS_SYS_100HZ                   (VEXPRESS_SYS_BASE + VEXPRESS_SYS_100HZ_OFFSET)
#define VEXPRESS_SYS_CFGDATA1                (VEXPRESS_SYS_BASE + VEXPRESS_SYS_CFGDATA1_OFFSET)
#define VEXPRESS_SYS_CFGDATA2                (VEXPRESS_SYS_BASE + VEXPRESS_SYS_CFGDATA2_OFFSET)
#define VEXPRESS_SYS_FLAGS                   (VEXPRESS_SYS_BASE + VEXPRESS_SYS_FLAGS_OFFSET)
#define VEXPRESS_SYS_FLAGSSET                (VEXPRESS_SYS_BASE + VEXPRESS_SYS_FLAGSSET_OFFSET)
#define VEXPRESS_SYS_FLAGSCLR                (VEXPRESS_SYS_BASE + VEXPRESS_SYS_FLAGSCLR_OFFSET)
#define VEXPRESS_SYS_NVFLAGS                 (VEXPRESS_SYS_BASE + VEXPRESS_SYS_NVFLAGS_OFFSET)
#define VEXPRESS_SYS_NVFLAGSSET              (VEXPRESS_SYS_BASE + VEXPRESS_SYS_NVFLAGSSET_OFFSET)
#define VEXPRESS_SYS_NVFLAGSCLR              (VEXPRESS_SYS_BASE + VEXPRESS_SYS_NVFLAGSCLR_OFFSET)
#define VEXPRESS_SYS_RESETCTL                (VEXPRESS_SYS_BASE + VEXPRESS_SYS_RESETCTL_OFFSET)
#define VEXPRESS_SYS_PCICTL                  (VEXPRESS_SYS_BASE + VEXPRESS_SYS_PCICTL_OFFSET)
#define VEXPRESS_SYS_MCI                     (VEXPRESS_SYS_BASE + VEXPRESS_SYS_MCI_OFFSET)
#define VEXPRESS_SYS_FLASH                   (VEXPRESS_SYS_BASE + VEXPRESS_SYS_FLASH_OFFSET)
#define VEXPRESS_SYS_CLCD                    (VEXPRESS_SYS_BASE + VEXPRESS_SYS_CLCD_OFFSET)
#define VEXPRESS_SYS_CLCDSER                 (VEXPRESS_SYS_BASE + VEXPRESS_SYS_CLCDSER_OFFSET)
#define VEXPRESS_SYS_BOOTCS                  (VEXPRESS_SYS_BASE + VEXPRESS_SYS_BOOTCS_OFFSET)
#define VEXPRESS_SYS_24MHz                   (VEXPRESS_SYS_BASE + VEXPRESS_SYS_24MHz_OFFSET)
#define VEXPRESS_SYS_MISC                    (VEXPRESS_SYS_BASE + VEXPRESS_SYS_MISC_OFFSET)
#define VEXPRESS_SYS_IOSEL                   (VEXPRESS_SYS_BASE + VEXPRESS_SYS_IOSEL_OFFSET)
#define VEXPRESS_SYS_PROCID                  (VEXPRESS_SYS_BASE + VEXPRESS_SYS_PROCID_OFFSET)
#define VEXPRESS_SYS_TEST_OSC0               (VEXPRESS_SYS_BASE + VEXPRESS_SYS_TEST_OSC0_OFFSET)
#define VEXPRESS_SYS_TEST_OSC1               (VEXPRESS_SYS_BASE + VEXPRESS_SYS_TEST_OSC1_OFFSET)
#define VEXPRESS_SYS_TEST_OSC2               (VEXPRESS_SYS_BASE + VEXPRESS_SYS_TEST_OSC2_OFFSET)
#define VEXPRESS_SYS_TEST_OSC3               (VEXPRESS_SYS_BASE + VEXPRESS_SYS_TEST_OSC3_OFFSET)
#define VEXPRESS_SYS_TEST_OSC4               (VEXPRESS_SYS_BASE + VEXPRESS_SYS_TEST_OSC4_OFFSET)

/* 
 * Values for VEXPRESS_SYS_RESET_CTRL
 */
#define VEXPRESS_SYS_CTRL_RESET_CONFIGCLR    0x01
#define VEXPRESS_SYS_CTRL_RESET_CONFIGINIT   0x02
#define VEXPRESS_SYS_CTRL_RESET_DLLRESET     0x03
#define VEXPRESS_SYS_CTRL_RESET_PLLRESET     0x04
#define VEXPRESS_SYS_CTRL_RESET_POR          0x05
#define VEXPRESS_SYS_CTRL_RESET_DoC          0x06

#define VEXPRESS_SYS_CTRL_LED         (1 << 0)

/* ------------------------------------------------------------------------
 *  RealView control registers
 * ------------------------------------------------------------------------
 */

/* 
 * VEXPRESS_IDFIELD
 *
 * 31:24 = manufacturer (0x41 = ARM)
 * 23:16 = architecture (0x08 = AHB system bus, ASB processor bus)
 * 15:12 = FPGA (0x3 = XVC600 or XVC600E)
 * 11:4  = build value
 * 3:0   = revision number (0x1 = rev B (AHB))
 */

/*
 * VEXPRESS_SYS_LOCK
 *     control access to SYS_OSCx, SYS_CFGDATAx, SYS_RESETCTL, 
 *     SYS_CLD, SYS_BOOTCS
 */
#define VEXPRESS_SYS_LOCK_LOCKED    (1 << 16)
#define VEXPRESS_SYS_LOCKVAL		0xA05F
#define VEXPRESS_SYS_LOCKVAL_MASK	0xFFFF	/* write 0xA05F to enable write access */

/*
 * VEXPRESS_SYS_FLASH
 */
#define VEXPRESS_FLASHPROG_FLVPPEN	(1 << 0)	/* Enable writing to flash */

/*
 * VEXPRESS_INTREG
 *     - used to acknowledge and control MMCI and UART interrupts 
 */
#define VEXPRESS_INTREG_WPROT        0x00	/* MMC protection status (no interrupt generated) */
#define VEXPRESS_INTREG_RI0          0x01	/* Ring indicator UART0 is asserted,              */
#define VEXPRESS_INTREG_CARDIN       0x08	/* MMCI card in detect                            */
						/* write 1 to acknowledge and clear               */
#define VEXPRESS_INTREG_RI1          0x02	/* Ring indicator UART1 is asserted,              */
#define VEXPRESS_INTREG_CARDINSERT   0x03	/* Signal insertion of MMC card                   */

/*
 * RealView common peripheral addresses
 */
#define VEXPRESS_SCTL_BASE            0x10001000	/* System controller */
#define VEXPRESS_I2C_BASE             0x10002000	/* I2C control */
#define VEXPRESS_AACI_BASE            0x10004000	/* Audio */
#define VEXPRESS_MMCI0_BASE           0x10005000	/* MMC interface */
#define VEXPRESS_KMI0_BASE            0x10006000	/* KMI interface */
#define VEXPRESS_KMI1_BASE            0x10007000	/* KMI 2nd interface */
#define VEXPRESS_CHAR_LCD_BASE        0x10008000	/* Character LCD */
#define VEXPRESS_SCI_BASE             0x1000E000	/* Smart card controller */
#define VEXPRESS_GPIO1_BASE           0x10014000	/* GPIO port 1 */
#define VEXPRESS_GPIO2_BASE           0x10015000	/* GPIO port 2 */
#define VEXPRESS_DMC_BASE             0x10018000	/* DMC configuration */
#define VEXPRESS_DMAC_BASE            0x10030000	/* DMA controller */

/* PCI space */
#define VEXPRESS_PCI_BASE             0x41000000	/* PCI Interface */
#define VEXPRESS_PCI_CFG_BASE	      0x42000000
#define VEXPRESS_PCI_MEM_BASE0        0x44000000
#define VEXPRESS_PCI_MEM_BASE1        0x50000000
#define VEXPRESS_PCI_MEM_BASE2        0x60000000
/* Sizes of above maps */
#define VEXPRESS_PCI_BASE_SIZE	       0x01000000
#define VEXPRESS_PCI_CFG_BASE_SIZE    0x02000000
#define VEXPRESS_PCI_MEM_BASE0_SIZE   0x0c000000	/* 32Mb */
#define VEXPRESS_PCI_MEM_BASE1_SIZE   0x10000000	/* 256Mb */
#define VEXPRESS_PCI_MEM_BASE2_SIZE   0x10000000	/* 256Mb */

#define VEXPRESS_SDRAM67_BASE         0x70000000	/* SDRAM banks 6 and 7 */
#define VEXPRESS_LT_BASE              0x80000000	/* Logic Tile expansion */

/*
 * CompactFlash
 */
#define VEXPRESS_CF_BASE		0x18000000	/* CompactFlash */
#define VEXPRESS_CF_MEM_BASE		0x18003000	/* SMC for CompactFlash */

/*
 * Disk on Chip
 */
#define VEXPRESS_DOC_BASE             0x2C000000
#define VEXPRESS_DOC_SIZE             (16 << 20)
#define VEXPRESS_DOC_PAGE_SIZE        512
#define VEXPRESS_DOC_TOTAL_PAGES     (DOC_SIZE / PAGE_SIZE)

#define ERASE_UNIT_PAGES    32
#define START_PAGE          0x80

/* 
 *  LED settings, bits [7:0]
 */
#define VEXPRESS_SYS_LED0             (1 << 0)
#define VEXPRESS_SYS_LED1             (1 << 1)
#define VEXPRESS_SYS_LED2             (1 << 2)
#define VEXPRESS_SYS_LED3             (1 << 3)
#define VEXPRESS_SYS_LED4             (1 << 4)
#define VEXPRESS_SYS_LED5             (1 << 5)
#define VEXPRESS_SYS_LED6             (1 << 6)
#define VEXPRESS_SYS_LED7             (1 << 7)

#define ALL_LEDS                  0xFF

#define LED_BANK                  VEXPRESS_SYS_LED

/* 
 * Control registers
 */
#define VEXPRESS_IDFIELD_OFFSET	0x0	/* RealView build information */
#define VEXPRESS_FLASHPROG_OFFSET	0x4	/* Flash devices */
#define VEXPRESS_INTREG_OFFSET		0x8	/* Interrupt control */
#define VEXPRESS_DECODE_OFFSET		0xC	/* Fitted logic modules */

/* 
 *  Clean base - dummy
 * 
 */
#define CLEAN_BASE                      VEXPRESS_BOOT_ROM_HI

/*
 * System controller bit assignment
 */
#define VEXPRESS_REFCLK	0
#define VEXPRESS_TIMCLK	1

#define VEXPRESS_TIMER1_EnSel	15
#define VEXPRESS_TIMER2_EnSel	17
#define VEXPRESS_TIMER3_EnSel	19
#define VEXPRESS_TIMER4_EnSel	21

#define MAX_TIMER                       2
#define MAX_PERIOD                      699050
#define TICKS_PER_uSEC                  1

/* 
 *  These are useconds NOT ticks.  
 * 
 */
#define mSEC_1                          1000
#define mSEC_5                          (mSEC_1 * 5)
#define mSEC_10                         (mSEC_1 * 10)
#define mSEC_25                         (mSEC_1 * 25)
#define SEC_1                           (mSEC_1 * 1000)

#define VEXPRESS_CSR_BASE             0x10000000
#define VEXPRESS_CSR_SIZE             0x10000000

#endif
