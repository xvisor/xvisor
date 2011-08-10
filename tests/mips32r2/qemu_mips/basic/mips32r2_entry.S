/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file start.S
 * @version 0.1
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief 24Kc startup file.
 */

#include <mips32r2_regs.h>
#include <mips32r2_asm_macros.h>

.extern _stack_start
.section .start.text,"ax",@progbits

LEAF(_start)
	mtc0	ZERO, CP0_CONTEXT
	nop
	nop
	nop

	/* globally disable interrupts until we are prepared. */
	disable_global_interrupts

	/* clear CPU timer counters. We don't want surprises. */
	mtc0	ZERO, CP0_COMPARE
	mtc0	ZERO, CP0_COUNT

	/* Read number of tlb entries from config register */
	bal 	num_tlb_entries
	nop

	/* Map 0x3fc00000 to 0x1fc00000 */
	lui T0, 0x3FC0
	srl T0, T0, 12
	sll T0, T0, 13 /* ((vadddr >> 12) << 13) is VPN2. */
	ori T0, T0, 2 /* Set the asid as 2 */
	mtc0 T0, CP0_ENTRYHI
	lui T0, 0x1FC0
	srl T0, T0, 12
	sll T0, T0, 6 /* ((phys_addr >> 12) << 6) is PFN */
	ori T0, T0, 6
	mtc0 T0, CP0_ENTRYLO0
	nop
	nop
	nop
	mtc0 ZERO, CP0_ENTRYLO1
	nop
	nop
	nop
	ehb
	tlbwi
	nop
	nop
	nop

1:	b 	1b
END(_start)

/*
 * Read config 1 register and return the number
 * of TLB entries in this CPU.
 */
LEAF(num_tlb_entries)
	mfc0	A1, CP0_CONFIG1
	nop
	nop
	nop
	srl	V0, A1, 25
	and	V0, V0, 0x3F
	jr	RA
	nop
END(num_tlb_entries)
