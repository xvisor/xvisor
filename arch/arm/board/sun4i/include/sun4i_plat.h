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
 * @file sun4i_plat.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Sun4i SOC platform configuration
 */
#ifndef __SUN4I_PLAT_H__
#define __SUN4I_PLAT_H__

/**
 * Address Map
 *
 */

#define AW_PA_BROM_START                  0xffff0000
#define AW_PA_BROM_END                    0xffff7fff   /* 32KB */

#define AW_PA_SRAM_BASE                   0x00000000
#define AW_PA_SDRAM_START                 0x40000000
#define AW_PA_IO_BASE                     0x01c00000
#define AW_PA_SRAM_IO_BASE                0x01c00000   /* 4KB */
#define AW_PA_DRAM_IO_BASE                0x01c01000
#define AW_PA_DMAC_IO_BASE                0x01c02000
#define AW_PA_NANDFLASHC_IO_BASE          0x01c03000
#define AW_PA_TSI_IO_BASE                 0x01c04000
#define AW_PA_SPI0_IO_BASE                0x01c05000
#define AW_PA_SPI1_IO_BASE                0x01c06000
#define AW_PA_MSCC_IO_BASE                0x01c07000
#define AW_PA_TVD_IO_BASE                 0x01c08000
#define AW_PA_CSI0_IO_BASE                0x01c09000
#define AW_PA_TVE_IO_BASE                 0x01c0a000
#define AW_PA_EMAC_IO_BASE                0x01c0b000
#define AW_PA_TCON0_IO_BASE               0x01c0c000
#define AW_PA_TCON1_IO_BASE               0x01c0d000
#define AW_PA_VE_IO_BASE                  0x01c0e000
#define AW_PA_SDC0_IO_BASE                0x01c0f000
#define AW_PA_SDC1_IO_BASE                0x01c10000
#define AW_PA_SDC2_IO_BASE                0x01c11000
#define AW_PA_SDC3_IO_BASE                0x01c12000
#define AW_PA_USB0_IO_BASE                0x01c13000
#define AW_PA_USB1_IO_BASE                0x01c14000
#define AW_PA_SSE_IO_BASE                 0x01c15000
#define AW_PA_HDMI_IO_BASE                0x01c16000
#define AW_PA_SPI2_IO_BASE                0x01c17000
#define AW_PA_SATA_IO_BASE                0x01c18000
#define AW_PA_PATA_IO_BASE                0x01c19000
#define AW_PA_ACE_IO_BASE                 0x01c1a000
#define AW_PA_TVE1_IO_BASE                0x01c1b000
#define AW_PA_USB2_IO_BASE                0x01c1c000
#define AW_PA_CSI1_IO_BASE                0x01c1d000
#define AW_PA_TZASC_IO_BASE               0x01c1e000
#define AW_PA_SPI3_IO_BASE                0x01c1f000
#define AW_PA_CCM_IO_BASE                 0x01c20000
#define AW_PA_INT_IO_BASE                 0x01c20400
#define AW_PA_PORTC_IO_BASE               0x01c20800
#define AW_PA_TIMERC_IO_BASE              0x01c20c00
#define AW_PA_SPDIF_IO_BASE               0x01c21000
#define AW_PA_AC97_IO_BASE                0x01c21400
#define AW_PA_IR0_IO_BASE                 0x01c21800
#define AW_PA_IR1_IO_BASE                 0x01c21c00
#define AW_PA_IIS_IO_BASE                 0x01c22400
#define AW_PA_LRADC_IO_BASE               0x01c22800
#define AW_PA_ADDA_IO_BASE                0x01c22c00
#define AW_PA_KEYPAD_IO_BASE              0x01c23000
#define AW_PA_TZPC_IO_BASE                0x01c23400
#define AW_PA_SID_IO_BASE                 0x01c23800
#define AW_PA_SJTAG_IO_BASE               0x01c23c00
#define AW_PA_TP_IO_BASE                  0x01c25000
#define AW_PA_PMU_IO_BASE                 0x01c25400
#define AW_PA_UART0_IO_BASE               0x01c28000
#define AW_PA_UART1_IO_BASE               0x01c28400
#define AW_PA_UART2_IO_BASE               0x01c28800
#define AW_PA_UART3_IO_BASE               0x01c28c00
#define AW_PA_UART4_IO_BASE               0x01c29000
#define AW_PA_UART5_IO_BASE               0x01c29400
#define AW_PA_UART6_IO_BASE               0x01c29800
#define AW_PA_UART7_IO_BASE               0x01c29c00
#define AW_PA_PS20_IO_BASE                0x01c2a000
#define AW_PA_PS21_IO_BASE                0x01c2a400
#define AW_PA_TWI0_IO_BASE                0x01c2ac00
#define AW_PA_TWI1_IO_BASE                0x01c2b000
#define AW_PA_TWI2_IO_BASE                0x01c2b400
#define AW_PA_CAN0_IO_BASE                0x01c2bc00
#define AW_PA_CAN1_IO_BASE                0x01c2c000
#define AW_PA_SCR_IO_BASE                 0x01c2c400
#define AW_PA_GPS_IO_BASE                 0x01c30000
#define AW_PA_MALI_IO_BASE                0x01c40000
#define AW_PA_DEFE0_IO_BASE               0x01e00000
#define AW_PA_DEFE1_IO_BASE               0x01e20000
#define AW_PA_DEBE0_IO_BASE               0x01e60000
#define AW_PA_DEBE1_IO_BASE               0x01e40000
#define AW_PA_MP_IO_BASE                  0x01e80000
#define AW_PA_AVG_IO_BASE                 0x01ea0000
#define AW_PA_BROM_BASE                   0xffff0000

#define AW_G2D_MEM_BASE                   0x58000000
#define AW_G2D_MEM_MAX                    0x1000000

#define AW_NR_IRQS			96

#endif
