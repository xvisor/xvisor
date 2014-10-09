/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @file omap_uart.h
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief Header file for UART serial port driver.
 */

#ifndef __OMAP_UART_H_
#define __OMAP_UART_H_

#include <vmm_types.h>

#define UART_RBR_OFFSET		0 /* In:  Recieve Buffer Register */
#define UART_THR_OFFSET		0 /* Out: Transmitter Holding Register */
#define UART_DLL_OFFSET		0 /* Out: Divisor Latch Low */
#define UART_IER_OFFSET		1 /* I/O: Interrupt Enable Register */
#define UART_DLM_OFFSET		1 /* Out: Divisor Latch High */
#define UART_FCR_OFFSET		2 /* Out: FIFO Control Register */
#define UART_IIR_OFFSET		2 /* I/O: Interrupt Identification Register */
#define UART_LCR_OFFSET		3 /* Out: Line Control Register */
#define UART_MCR_OFFSET		4 /* Out: Modem Control Register */
#define UART_LSR_OFFSET		5 /* In:  Line Status Register */
#define UART_MSR_OFFSET		6 /* In:  Modem Status Register */
#define UART_SCR_OFFSET		7 /* I/O: Scratch Register */

#define UART_LSR_FIFOE		0x80 /* Fifo error */
#define UART_LSR_TEMT		0x40 /* Transmitter empty */
#define UART_LSR_THRE		0x20 /* Transmit-hold-register empty */
#define UART_LSR_BI		0x10 /* Break interrupt indicator */
#define UART_LSR_FE		0x08 /* Frame error indicator */
#define UART_LSR_PE		0x04 /* Parity error indicator */
#define UART_LSR_OE		0x02 /* Overrun error indicator */
#define UART_LSR_DR		0x01 /* Receiver data ready */
#define UART_LSR_BRK_ERROR_BITS	0x1E /* BI, FE, PE, OE bits */

#define UART_IIR_NO_INT		0x01 /* No interrupts pending */
#define UART_IIR_ID		0x06 /* Mask for the interrupt ID */
#define UART_IIR_MSI		0x00 /* Modem status interrupt */
#define UART_IIR_THRI		0x02 /* Transmitter holding register empty */
#define UART_IIR_RDI		0x04 /* Receiver data interrupt */
#define UART_IIR_RLSI		0x06 /* Receiver line status interrupt */
#define UART_IIR_RTO		0x0c /* Receiver timeout interrupt */

#define UART_IER_MSI		0x08 /* Enable Modem status interrupt */
#define UART_IER_RLSI		0x04 /* Enable receiver line status interrupt */
#define UART_IER_THRI		0x02 /* Enable Transmitter holding register int. */
#define UART_IER_RDI		0x01 /* Enable receiver data interrupt */

#define UART_FCR_ENABLE_FIFO	0x01 /* Enable the FIFO */
#define UART_FCR_CLEAR_RCVR	0x02 /* Clear the RCVR FIFO */
#define UART_FCR_CLEAR_XMIT	0x04 /* Clear the XMIT FIFO */
#define UART_FCR_DMA_SELECT	0x08 /* For DMA applications */

/*
 * Note: The FIFO trigger levels are chip specific:
 *	RX:76 = 00  01  10  11	TX:54 = 00  01  10  11
 * ST16C654:	 8  16  56  60		 8  16  32  56	PORT_16654
 */
#define UART_FCR_R_TRIG_00	0x00
#define UART_FCR_R_TRIG_01	0x40
#define UART_FCR_R_TRIG_10	0x80
#define UART_FCR_R_TRIG_11	0xc0
#define UART_FCR_T_TRIG_00	0x00
#define UART_FCR_T_TRIG_01	0x10
#define UART_FCR_T_TRIG_10	0x20
#define UART_FCR_T_TRIG_11	0x30

/*
 * Note: if the word length is 5 bits (UART_LCR_WLEN5), then setting 
 * UART_LCR_STOP will select 1.5 stop bits, not 2 stop bits.
 */
#define UART_LCR_DLAB		0x80 /* Divisor latch access bit */
#define UART_LCR_SBC		0x40 /* Set break control */
#define UART_LCR_SPAR		0x20 /* Stick parity (?) */
#define UART_LCR_EPAR		0x10 /* Even parity select */
#define UART_LCR_PARITY		0x08 /* Parity Enable */
#define UART_LCR_STOP		0x04 /* Stop bits: 0=1 bit, 1=2 bits */
#define UART_LCR_WLEN5		0x00 /* Wordlength: 5 bits */
#define UART_LCR_WLEN6		0x01 /* Wordlength: 6 bits */
#define UART_LCR_WLEN7		0x02 /* Wordlength: 7 bits */
#define UART_LCR_WLEN8		0x03 /* Wordlength: 8 bits */

/*
 * Access to some registers depends on register access / configuration
 * mode.
 */
#define UART_LCR_CONF_MODE_A	UART_LCR_DLAB	/* Configutation mode A */
#define UART_LCR_CONF_MODE_B	0xBF		/* Configutation mode B */

#define UART_MCR_CLKSEL		0x80 /* Divide clock by 4 (TI16C752, EFR[4]=1) */
#define UART_MCR_TCRTLR		0x40 /* Access TCR/TLR (TI16C752, EFR[4]=1) */
#define UART_MCR_XONANY		0x20 /* Enable Xon Any (TI16C752, EFR[4]=1) */
#define UART_MCR_AFE		0x20 /* Enable auto-RTS/CTS (TI16C550C/TI16C750) */
#define UART_MCR_LOOP		0x10 /* Enable loopback test mode */
#define UART_MCR_OUT2		0x08 /* Out2 complement */
#define UART_MCR_OUT1		0x04 /* Out1 complement */
#define UART_MCR_RTS		0x02 /* RTS complement */
#define UART_MCR_DTR		0x01 /* DTR complement */

#define UART_MSR_DCD		0x80 /* Data Carrier Detect */
#define UART_MSR_RI		0x40 /* Ring Indicator */
#define UART_MSR_DSR		0x20 /* Data Set Ready */
#define UART_MSR_CTS		0x10 /* Clear to Send */
#define UART_MSR_DDCD		0x08 /* Delta DCD */
#define UART_MSR_TERI		0x04 /* Trailing edge ring indicator */
#define UART_MSR_DDSR		0x02 /* Delta DSR */
#define UART_MSR_DCTS		0x01 /* Delta CTS */
#define UART_MSR_ANY_DELTA	0x0F /* Any of the delta bits! */

/*
 * LCR=0xBF (or DLAB=1 for 16C660)
 */
#define UART_EFR_OFFSET		2	/* I/O: Extended Features Register */
#define UART_EFR_CTS		0x80 /* CTS flow control */
#define UART_EFR_RTS		0x40 /* RTS flow control */
#define UART_EFR_SCD		0x20 /* Special character detect */
#define UART_EFR_ECB		0x10 /* Enhanced control bit */
/*
 * the low four bits control software flow control
 */

/*
 * LCR=0xBF, TI16C752, ST16650, ST16650A, ST16654
 */
#define UART_XON1_OFFSET	4	/* I/O: Xon character 1 */
#define UART_XON2_OFFSET	5	/* I/O: Xon character 2 */
#define UART_XOFF1_OFFSET	6	/* I/O: Xoff character 1 */
#define UART_XOFF2_OFFSET	7	/* I/O: Xoff character 2 */

/*
 * EFR[4]=1 MCR[6]=1, TI16C752
 */
#define UART_TI752_TCR_OFFSET	6	/* I/O: transmission control register */
#define UART_TI752_TLR_OFFSET	7	/* I/O: trigger level register */

/*
 * LCR=0xBF, XR16C85x
 */
#define UART_TRG_OFFSET	0	/* FCTR bit 7 selects Rx or Tx
				 * In: Fifo count
				 * Out: Fifo custom trigger levels */

/*
 * Extra serial register definitions for the internal UARTs
 * in TI OMAP processors.
 */
#define UART_OMAP_MDR1_OFFSET		0x08	/* Mode definition register */
#define UART_OMAP_MDR2_OFFSET		0x09	/* Mode definition register 2 */
#define UART_OMAP_SCR_OFFSET		0x10	/* Supplementary control register */
#define UART_OMAP_SSR_OFFSET		0x11	/* Supplementary status register */
#define UART_OMAP_EBLR_OFFSET		0x12	/* BOF length register */
#define UART_OMAP_OSC_12M_SEL_OFFSET	0x13	/* OMAP1510 12MHz osc select */
#define UART_OMAP_MVER_OFFSET		0x14	/* Module version register */
#define UART_OMAP_SYSC_OFFSET		0x15	/* System configuration register */
#define UART_OMAP_SYSS_OFFSET		0x16	/* System status register */
#define UART_OMAP_WER_OFFSET		0x17	/* Wake-up enable register */

/*
 * These are the definitions for the MDR1 register
 */
#define UART_OMAP_MDR1_16X_MODE		0x00	/* UART 16x mode */
#define UART_OMAP_MDR1_SIR_MODE		0x01	/* SIR mode */
#define UART_OMAP_MDR1_16X_ABAUD_MODE	0x02	/* UART 16x auto-baud */
#define UART_OMAP_MDR1_13X_MODE		0x03	/* UART 13x mode */
#define UART_OMAP_MDR1_MIR_MODE		0x04	/* MIR mode */
#define UART_OMAP_MDR1_FIR_MODE		0x05	/* FIR mode */
#define UART_OMAP_MDR1_CIR_MODE		0x06	/* CIR mode */
#define UART_OMAP_MDR1_DISABLE		0x07	/* Disable (default state) */

#define REG_UART_RBR(base,align)	((base)+UART_RBR_OFFSET*(align))
#define REG_UART_THR(base,align)	((base)+UART_THR_OFFSET*(align))
#define REG_UART_DLL(base,align)	((base)+UART_DLL_OFFSET*(align))
#define REG_UART_IER(base,align)	((base)+UART_IER_OFFSET*(align))
#define REG_UART_DLM(base,align)	((base)+UART_DLM_OFFSET*(align))
#define REG_UART_IIR(base,align)	((base)+UART_IIR_OFFSET*(align))
#define REG_UART_FCR(base,align)	((base)+UART_FCR_OFFSET*(align))
#define REG_UART_LCR(base,align)	((base)+UART_LCR_OFFSET*(align))
#define REG_UART_MCR(base,align)	((base)+UART_MCR_OFFSET*(align))
#define REG_UART_LSR(base,align)	((base)+UART_LSR_OFFSET*(align))
#define REG_UART_MSR(base,align)	((base)+UART_MSR_OFFSET*(align))
#define REG_UART_SCR(base,align)	((base)+UART_SCR_OFFSET*(align))
#define REG_UART_EFR(base,align)	((base)+UART_EFR_OFFSET*(align))

#define REG_UART_OMAP_MDR1(base,align)	((base)+UART_OMAP_MDR1_OFFSET*(align))
#define REG_UART_OMAP_MDR2(base,align)	((base)+UART_OMAP_MDR2_OFFSET*(align))
#define REG_UART_OMAP_SCR(base,align)	((base)+UART_OMAP_SCR_OFFSET*(align))
#define REG_UART_OMAP_SSR(base,align)	((base)+UART_OMAP_SSR_OFFSET*(align))
#define REG_UART_OMAP_EBLR(base,align)	((base)+UART_OMAP_EBLR_OFFSET*(align))
#define REG_UART_OMAP_OSC_12M_SEL(base,align)	((base)+UART_OMAP_OSC_12M_SEL_OFFSET*(align))
#define REG_UART_OMAP_MVER(base,align)	((base)+UART_OMAP_MVER_OFFSET*(align))
#define REG_UART_OMAP_SYSC(base,align)	((base)+UART_OMAP_SYSC_OFFSET*(align))
#define REG_UART_OMAP_SYSS(base,align)	((base)+UART_OMAP_SYSS_OFFSET*(align))
#define REG_UART_OMAP_WER(base,align)	((base)+UART_OMAP_WER_OFFSET*(align))
#define REG_UART_XON1(base,align)	((base)+UART_XON1_OFFSET*(align))	
#define REG_UART_XON2(base,align)	((base)+UART_XON2_OFFSET*(align))	
#define REG_UART_XOFF1(base,align)	((base)+UART_XOFF1_OFFSET*(align))	
#define REG_UART_XOFF2(base,align)	((base)+UART_XOFF2_OFFSET*(align))	
#define REG_UART_TI752_TCR(base,align)	((base)+UART_TI752_TCR_OFFSET*(align))	
#define REG_UART_TI752_TLR(base,align)	((base)+UART_TI752_TLR_OFFSET*(align))	
#define REG_UART_TRG(base,align)	((base)+UART_TRG_OFFSET*(align))		

#define OMAP_MODE13X_SPEED	230400

/* WER = 0x7F
 * Enable module level wakeup in WER reg
 */
#define OMAP_UART_WER_MOD_WKUP	0X7F

/* Enable XON/XOFF flow control on output */
#define OMAP_UART_SW_TX		0x04

/* Enable XON/XOFF flow control on input */
#define OMAP_UART_SW_RX		0x04

#define OMAP_UART_SYSC_RESET	0X07
#define OMAP_UART_TCR_TRIG	0X0F
#define OMAP_UART_SW_CLR	0XF0
#define OMAP_UART_FIFO_CLR	0X06

bool omap_uart_lowlevel_can_getc(virtual_addr_t base, u32 reg_shift);
u8 omap_uart_lowlevel_getc(virtual_addr_t base, u32 reg_shift);
bool omap_uart_lowlevel_can_putc(virtual_addr_t base, u32 reg_shift);
void omap_uart_lowlevel_putc(virtual_addr_t base, u32 reg_shift, u8 ch);
void omap_uart_lowlevel_init(virtual_addr_t base, u32 reg_shift, 
		u32 baudrate, u32 input_clock);

#endif /* __UART_H_ */
