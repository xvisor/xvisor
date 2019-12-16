/**
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
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
 * @file cpu_sbi.c
 * @author Anup Patel (anup.patel@wdc.com)
 * @brief Supervisor binary interface (SBI) helper functions source
 */

#include <vmm_types.h>
#include <cpu_sbi.h>
#include <riscv_sbi.h>

#define SBI_CALL(which, arg0, arg1, arg2, arg3) ({		\
	register ulong a0 asm ("a0") = (ulong)(arg0);		\
	register ulong a1 asm ("a1") = (ulong)(arg1);		\
	register ulong a2 asm ("a2") = (ulong)(arg2);		\
	register ulong a3 asm ("a3") = (ulong)(arg3);		\
	register ulong a7 asm ("a7") = (ulong)(which);		\
	asm volatile ("ecall"					\
		      : "+r" (a0)				\
		      : "r" (a1), "r" (a2), "r" (a3), "r" (a7)	\
		      : "memory");				\
	a0;							\
})

/* Lazy implementations until SBI is finalized */
#define SBI_CALL_0(which) SBI_CALL(which, 0, 0, 0, 0)
#define SBI_CALL_1(which, arg0) SBI_CALL(which, arg0, 0, 0, 0)
#define SBI_CALL_2(which, arg0, arg1) SBI_CALL(which, arg0, arg1, 0, 0)
#define SBI_CALL_3(which, arg0, arg1, arg2) \
		SBI_CALL(which, arg0, arg1, arg2, 0)
#define SBI_CALL_4(which, arg0, arg1, arg2, arg3) \
		SBI_CALL(which, arg0, arg1, arg2, arg3)

void sbi_console_putchar(int ch)
{
	SBI_CALL_1(SBI_EXT_0_1_CONSOLE_PUTCHAR, ch);
}

int sbi_console_getchar(void)
{
	return SBI_CALL_0(SBI_EXT_0_1_CONSOLE_GETCHAR);
}

void sbi_set_timer(u64 stime_value)
{
#ifdef CONFIG_64BIT
	SBI_CALL_1(SBI_EXT_0_1_SET_TIMER, stime_value);
#else
	SBI_CALL_2(SBI_EXT_0_1_SET_TIMER, stime_value, stime_value >> 32);
#endif
}

void sbi_shutdown(void)
{
	SBI_CALL_0(SBI_EXT_0_1_SHUTDOWN);
}

void sbi_clear_ipi(void)
{
	SBI_CALL_0(SBI_EXT_0_1_CLEAR_IPI);
}

void sbi_send_ipi(const unsigned long *hart_mask)
{
	SBI_CALL_1(SBI_EXT_0_1_SEND_IPI, hart_mask);
}

void sbi_remote_fence_i(const unsigned long *hart_mask)
{
	SBI_CALL_1(SBI_EXT_0_1_REMOTE_FENCE_I, hart_mask);
}

void sbi_remote_sfence_vma(const unsigned long *hart_mask,
			   unsigned long start,
			   unsigned long size)
{
	SBI_CALL_3(SBI_EXT_0_1_REMOTE_SFENCE_VMA, hart_mask, start, size);
}

void sbi_remote_sfence_vma_asid(const unsigned long *hart_mask,
				unsigned long start,
				unsigned long size,
				unsigned long asid)
{
	SBI_CALL_4(SBI_EXT_0_1_REMOTE_SFENCE_VMA_ASID, hart_mask,
		   start, size, asid);
}
