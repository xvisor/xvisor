/********************************************************************************/
/*                         Xvisor BIOS ROM example                              */
/*       Copyright (c) 2011 Pete Batard (pete@akeo.ie) -  Public Domain         */
/*       Copyright (c) 2018 Himanshu Chauhan (hchauhan@xvisor-x86.org)          */
/*                                                                              */
/*   Thanks Pete for allowing me to use this as a test guest bios for Xvisor    */
/*   This is adapted version of what Pete originally wrote.                     */
/*                                                                              */
/*   This program is free software; you can redistribute it and/or modify       */
/*   it under the terms of the GNU General Public License as published by       */
/*   the Free Software Foundation; either version 2, or (at your option)        */
/*   any later version.                                                         */
/*                                                                              */
/*   This program is distributed in the hope that it will be useful,            */
/*   but WITHOUT ANY WARRANTY; without even the implied warranty of             */
/*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              */
/*   GNU General Public License for more details.                               */
/*                                                                              */
/*   You should have received a copy of the GNU General Public License          */
/*   along with this program; if not, write to the Free Software                */
/*   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.                  */
/********************************************************************************/


/********************************************************************************/
/* GNU Assembler Settings:                                                      */
/********************************************************************************/
.intel_syntax noprefix	/* Use Intel assembler syntax (same as IDA Pro)         */
.code16			/* After reset, the x86 CPU is in real / 16 bit mode    */
/********************************************************************************/


/********************************************************************************/
/* Macros:                                                                      */
/********************************************************************************/
/* This macro allows stackless subroutine calls                                 */
.macro  ROM_CALL addr
	mov  sp, offset 1f	/* Use a local label as we don't know the size  */
	jmp  \addr		/* of the jmp instruction (can be 2 or 3 bytes) */
1:	/* see http://sourceware.org/binutils/docs-2.21/as/Symbol-Names.html    */
.endm


/********************************************************************************/
/* Constants:                                                                   */
/********************************************************************************/
/* 16650 UART setup */
COM_BASE      = 0x3f8	/* Our default COM1 base, after SuperIO init            */
COM_RB        = 0x00	/* Receive Buffer (R)                                   */
COM_TB        = 0x00    /* Transmit Buffer (W)                                  */
COM_BRD_LO    = 0x00	/* Baud Rate Divisor LSB (when bit 7 of LCR is set)     */
COM_BRD_HI    = 0x01	/* Daud Rate Divisor MSB (when bit 7 of LCR is set)     */
COM_IER       = 0x01	/* Interrupt Enable Register                            */
COM_FCR       = 0x02	/* 16650 FIFO Control Register (W)                      */
COM_LCR       = 0x03	/* Line Control Register                                */
COM_MCR       = 0x04	/* Modem Control Registrer                              */
COM_LSR       = 0x05	/* Line Status Register                                 */
/********************************************************************************/


/********************************************************************************/
/* begin : Dummy section marking the very start of the BIOS.                    */
/* This allows the .rom binary to be filled to the right size with objcopy.     */
/********************************************************************************/
.section begin, "a"		/* The 'ALLOC' flag is needed for objcopy       */
	.ascii "X-BIOS v0.01"	/* Dummy ID string                              */
	.align 16
/********************************************************************************/


/********************************************************************************/
/* main:                                                                        */
/* This section will be relocated according to the bios.ld script.              */
/********************************************************************************/
/* 'init' doesn't have to be at the beginning, so you can move it around, as    */
/* long as remains reachable, with a short jump, from the .reset section.       */
.section main, "ax"
.globl init		/* init must be declared global for the linker and must */
init:			/* point to the first instruction of your code section  */
	cli		/* NOTE: This sample BIOS runs with interrupts disabled */
	cld		/* String direction lookup: forward                     */
	mov  ax, cs	/* A real BIOS would keep a copy of ax, dx as well as   */
	mov  ds, ax	/* initialize fs, gs and possibly a GDT for protected   */
	mov  ss, ax	/* mode. We don't do any of this here.                  */

init_serial:		/* Init serial port                                     */
	mov  si, offset serial_conf
	mov  cx, (hello_string - serial_conf)/2
write_serial_conf:
	mov  ax, [si]
	ROM_CALL serial_out
	add  si, 0x02
	loop write_serial_conf

print_hello:		/* Print a string                                       */
	mov  si, offset hello_string
	ROM_CALL print_string

serial_repeater:	/* End the BIOS with a simple serial repeater           */
	ROM_CALL readchar
	ROM_CALL putchar
	jmp serial_repeater

/********************************************************************************/
/* Subroutines:                                                                 */
/********************************************************************************/
serial_out:		/* AL (IN): COM Register index, AH (IN): Data to Write  */
	mov  dx, COM_BASE
	add  dl, al	/* Unless something is wrong, we won't overflow to DH   */
	mov  al, ah
	out  dx, al
	jmp  sp


putchar:		/* AL (IN): character to print                          */
	mov  dx, COM_BASE + COM_LSR
	mov  ah, al
tx_wait:
	in   al, dx
	and  al, 0x20	/* Check that transmit register is empty                */
	jz   tx_wait
	mov  dx, COM_BASE + COM_TB
	mov  al, ah
	out  dx, al
	jmp  sp


readchar:		/* AL (OUT): character read from serial                 */
	mov  dx, COM_BASE + COM_LSR
rx_wait:
	in   al, dx
	and  al, 0x01
	jz   rx_wait
	mov  dx, COM_BASE + COM_RB
	in   al, dx
	jmp  sp


print_string:		/* SI (IN): offset to NUL terminated string             */
	lodsb
	or   al, al
	jnz  write_char
	jmp  sp
write_char:
	shl  esp, 0x10  /* We're calling a sub from a sub => preserve SP        */
	ROM_CALL putchar
	shr  esp, 0x10  /* Restore SP                                           */
	jmp  print_string


/********************************************************************************/
/* Data:                                                                        */
/********************************************************************************/
serial_conf:	/* See http://www.versalogic.com/kb/KB.asp?KBID=1395            */
	.byte COM_MCR,     0x00		/* RTS/DTS off, disable loopback        */
	.byte COM_FCR,     0x07		/* Enable & reset FIFOs. DMA mode 0.    */
	.byte COM_LCR,     0x80		/* Set DLAB (access baudrate registers) */
	.byte COM_BRD_LO,  0x01		/* Baud Rate 115200 = 0x0001            */
	.byte COM_BRD_HI,  0x00
	.byte COM_LCR,     0x03		/* Unset DLAB. Set 8N1 mode             */
hello_string:
	.string "\r\nHello from X-BIOS v0.01!\r\n" /* .string adds a NUL terminator    */
/********************************************************************************/


/********************************************************************************/
/* reset: this section must reside at 0xfffffff0, and be exactly 16 bytes       */
/********************************************************************************/
.section reset, "ax"
	/* Issue a manual jmp to work around a binutils bug.                    */
	/* See coreboot's src/cpu/x86/16bit/reset16.inc                         */
	.byte  0xe9
	.int   init - ( . + 2 )
	.align 16, 0xff	/* fills section to end of ROM (with 0xFF)              */
/********************************************************************************/
