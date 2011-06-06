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

#include <cpu_regs.h>
#include <cpu_asm_macros.h>

.extern _stack_start
.section .start.text,"ax",@progbits

EXCEPTION_VECTOR(_tlbmiss, 0x00, _handle_tlbmiss)
EXCEPTION_VECTOR(_cache_error, 0x100, _handle_cache_error)
EXCEPTION_VECTOR(_general_exception, 0x180, _handle_general_exception)
/* FIXME: We don't need this when in EIC mode. */
EXCEPTION_VECTOR(_interrupts, 0x200, _handle_interrupt)

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

	/* initialize tlb */
	bal	tlb_init
	move	A0, V0

	la	SP, _stack_start	/* setup the stack (bss segment) */
	la	T0, cpu_init
	j	T0			/* Call the C- code now */
	nop

1:	b 	1b 			/* we should not come here whatsoever */
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

/**
 * tlb_init
 * Initialize the TLB to a power-up state, guaranteeing that all entries
 * are unique and invalid.
 * Arguments:
 * a0 = Maximum TLB index (from MMUSize field of C0_Config1)
 * Returns:
 * No value
 * Restrictions:
 * This routine must be called in unmapped space
 * Algorithm:
 * va = kseg0_base;
 * for (entry = max_TLB_index ; entry >= 0, entry--) {
 *     while (TLB_Probe_Hit(va)) {
 *         va += Page_Size;
 *     }
 *     TLB_Write(entry, va, 0, 0, 0);
 * }
 */
LEAF(tlb_init)
	/* Clear PageMask, EntryLo0 and EntryLo1 so that valid bits are off, PFN values
	 * are zero, and the default page size is used.
	 */
	mtc0 ZERO, CP0_ENTRYLO0
	/* Clear out PFN and valid bits */
	mtc0 ZERO, CP0_ENTRYLO1
	mtc0 ZERO, CP0_PAGEMASK
	/* Clear out mask register */
	/* Start with the base address of kseg0 for the VA part of the TLB */
	li T0, 0x80000000
	/*
	 * Write the VA candidate to EntryHi and probe the TLB to see if if is
	 * already there. If it is, a write to the TLB may cause a machine
	 * check, so just increment the VA candidate by one page and try again.
	 */
10:
	mtc0 T0, CP0_ENTRYHI
	/* Write VA candidate */
	tlbp_write_hazard
	/* Clear EntryHi hazard (ssnop/ehb in R1/2) */
	tlbp
	/* Probe the TLB to check for a match */
	tlbp_read_hazard
	/* Clear Index hazard (ssnop/ehb in R1/2) */
	mfc0 T1, CP0_INDEX
	addiu T0, (1 << S_EntryHiVPN2)
	/* Read back flag to check for match */
	bgez T1, 10b
	nop
	/* Add 1 to VPN index in va */
	/*
	 * A write of the VPN candidate will be unique, so write this entry
	 * into the next index, decrement the index, and continue until the
	 * index goes negative (thereby writing all TLB entries)
	 */
	mtc0 A0, CP0_INDEX
	/* Use this as next TLB index */
	tlbw_write_hazard
	/* Clear Index hazard (ssnop/ehb in R1/2) */
	tlbwi
	/* Write the TLB entry */
	/* Branch if more TLB entries to do */
	addiu A0, A0, -1
	bne A0, ZERO, 10b
	nop
	
	/* Decrement the TLB index */
	/*
	* Clear Index and EntryHi simply to leave the state constant for all
	* returns
	*/
	mtc0 ZERO, CP0_INDEX
	mtc0 ZERO, CP0_ENTRYHI
	jr RA
	/* Return to caller */
	nop
END(tlb_init)

LEAF(_handle_tlbmiss)
	//move K0, SP
	//SAVE_INT_CONTEXT(_int_stack)
	//bal vmm_regs_dump
	//move A0, SP

	b _handle_tlbmiss
	nop
END(_handle_tlbmiss)

.extern generic_int_handler
.extern _int_stack
.extern vmm_regs_dump
LEAF(_handle_interrupt)
	disable_global_interrupts
	SAVE_INT_CONTEXT(_int_stack)
	move A0, SP
	bal generic_int_handler
	nop
	RESTORE_INT_CONTEXT(SP)
	enable_global_interrupts
	eret
END(_handle_interrupt)

LEAF(_handle_cache_error)
	b _handle_cache_error
	nop
END(_handle_cache_error)

LEAF(_handle_general_exception)
	//move K0, SP
	//SAVE_INT_CONTEXT(_int_stack)
	//bal vmm_regs_dump
	//move A0, SP

	b _handle_general_exception
	nop
END(_handle_general_exception)

/**
 * A0 -> Contains virtual address.
 * A1 -> Contains physical address.
 * A2 -> TLB index: If -1 select automatically.
 */
.globl create_tlb_entry
LEAF(create_tlb_entry)
	mtc0 A2, CP0_INDEX /* load the tlb index to be programmed. */
	srl A0, A0, 12 /* get the VPN */
	sll A0, A0, 12
	nop
	mtc0 A0, CP0_ENTRYHI /* load VPN in entry hi */
	addi T0, A1, 0x1000 /* next PFN for entry lo1 in T0 */
	srl A1, A1, 12 /* get the PFN */
	sll A1, A1, 6 /* get the PFN */
	srl T0, T0, 12
	sll T0, T0, 6
	ori A1, A1, 0x7 /* mark the page writable, global and valid */
	mtc0 A1, CP0_ENTRYLO0
	ori T0, T0, 0x7 /* mark the next physical page writable, global and valid */
	nop
	nop
	mtc0 T0, CP0_ENTRYLO1
	nop
	nop
	nop
	tlbwi
	ehb
	j RA
	nop
END(create_tlb_entry)
