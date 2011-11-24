/**
 * Copyright (c) Himanshu Chauhan.
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
 * @file cpu_main.c
 * @version 1.0
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief C code for cpu functions
 */

#include <vmm_cpu.h>
#include <vmm_string.h>
#include <vmm_stdio.h>
#include <vmm_error.h>
#include <vmm_main.h>
#include <cpu_asm_macros.h>

void vmm_regs_dump(vmm_user_regs_t *tregs)
{
	vmm_printf("V0: 0x%X\n", tregs->regs[V0_IDX]);
	vmm_printf("V1: 0x%X\n", tregs->regs[V1_IDX]);
	vmm_printf("A0: 0x%X\n", tregs->regs[A0_IDX]);
	vmm_printf("A1: 0x%X\n", tregs->regs[A1_IDX]);
	vmm_printf("A2: 0x%X\n", tregs->regs[A2_IDX]);
	vmm_printf("A3: 0x%X\n", tregs->regs[A3_IDX]);
	vmm_printf("T0: 0x%X\n", tregs->regs[T0_IDX]);
	vmm_printf("T1: 0x%X\n", tregs->regs[T1_IDX]);
	vmm_printf("T2: 0x%X\n", tregs->regs[T2_IDX]);
	vmm_printf("T3: 0x%X\n", tregs->regs[T3_IDX]);
	vmm_printf("T4: 0x%X\n", tregs->regs[T5_IDX]);
	vmm_printf("T6: 0x%X\n", tregs->regs[T6_IDX]);
	vmm_printf("T7: 0x%X\n", tregs->regs[T7_IDX]);
	vmm_printf("S0: 0x%X\n", tregs->regs[S0_IDX]);
	vmm_printf("S1: 0x%X\n", tregs->regs[S1_IDX]);
	vmm_printf("S2: 0x%X\n", tregs->regs[S2_IDX]);
	vmm_printf("S3: 0x%X\n", tregs->regs[S3_IDX]);
	vmm_printf("S4: 0x%X\n", tregs->regs[S4_IDX]);
	vmm_printf("S5: 0x%X\n", tregs->regs[S5_IDX]);
	vmm_printf("S6: 0x%X\n", tregs->regs[S6_IDX]);
	vmm_printf("S7: 0x%X\n", tregs->regs[S7_IDX]);
	vmm_printf("T8: 0x%X\n", tregs->regs[T8_IDX]);
	vmm_printf("T9: 0x%X\n", tregs->regs[T9_IDX]);
	vmm_printf("SP: 0x%X\n", tregs->regs[SP_IDX]);
	vmm_printf("GP: 0x%X\n", tregs->regs[GP_IDX]);
	vmm_printf("S8: 0x%X\n", tregs->regs[S8_IDX]);
	vmm_printf("RA: 0x%X\n", tregs->regs[RA_IDX]);
	vmm_printf("EPC: 0x%X\n", tregs->cp0_epc);

	while(1);
}

int __init vmm_cpu_early_init(void)
{
	/*
	 * Host virtual memory, device tree, heap is up.
	 * Do necessary early stuff like iomapping devices
	 * memory or boot time memory reservation here.
	 */
	return 0;
}

int __init vmm_cpu_final_init(void)
{
        return 0;
}

void __init cpu_init(void)
{
	/* Initialize VMM (APIs only available after this) */
	vmm_init();

	/* We should never come back here. */
	while (1);
}
