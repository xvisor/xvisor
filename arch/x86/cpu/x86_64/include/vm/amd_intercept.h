/**
 * Copyright (c) 2013 Himanshu Chauhan.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file amd_intercept.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief AMD guest intercept related defines.
 */

#ifndef _H_INTERCEPT_H__
#define _H_INTERCEPT_H__

#include <cpu_vm.h>

extern void handle_vmexit (struct vcpu_hw_context *context);

#define _INTRCPT_READ_CR0_OFFSET	0
#define INTRCPT_READ_CR0		(1 << (_INTRCPT_READ_CR0_OFFSET))
#define INTRCPT_READ_CR1		(1 << (_INTRCPT_READ_CR0_OFFSET + 1))
#define INTRCPT_READ_CR2		(1 << (_INTRCPT_READ_CR0_OFFSET + 2))
#define INTRCPT_READ_CR3		(1 << (_INTRCPT_READ_CR0_OFFSET + 3))
#define INTRCPT_READ_CR4		(1 << (_INTRCPT_READ_CR0_OFFSET + 4))

/* Flags for CRn access interception */
#define _INTRCPT_WRITE_CR0_OFFSET	16
#define INTRCPT_WRITE_CR0		(1 << (_INTRCPT_WRITE_CR0_OFFSET))
#define INTRCPT_WRITE_CR1		(1 << (_INTRCPT_WRITE_CR0_OFFSET + 1))
#define INTRCPT_WRITE_CR2		(1 << (_INTRCPT_WRITE_CR0_OFFSET + 2))
#define INTRCPT_WRITE_CR3		(1 << (_INTRCPT_WRITE_CR0_OFFSET + 3))
#define INTRCPT_WRITE_CR4		(1 << (_INTRCPT_WRITE_CR0_OFFSET + 4))
/*
 * Flags for exception intercept word
 * 1 << vector associated with the exception - VMCB + 8h
 */
#define INTRCPT_EXC_DIV_ERR		(1 << 0)	/* division error */
#define INTRCPT_EXC_DB			(1 << 1)	/* Debug exception */
#define INTRCPT_EXC_NMI			(1 << 2)	/* NMI */
#define INTRCPT_EXC_BP			(1 << 3)	/* Break point */
#define INTRCPT_EXC_OV			(1 << 4)	/* Overflow */
#define INTRCPT_EXC_BOUNDS		(1 << 5)	/* Bounds check */
#define INTRCPT_EXC_INV_OPC		(1 << 6)	/* Invalid opcode */
#define INTRCPT_EXC_NDEV		(1 << 7)	/* No device */
#define INTRCPT_EXC_DFAULT		(1 << 8)	/* Double fault */
#define INTRCPT_EXC_CP_OVRRUN		(1 << 9)	/* Co-processor overrun */
#define INTRCPT_EXC_INV_TSS		(1 << 10)	/* Invalid TSS */
#define INTRCPT_EXC_SEG_NP		(1 << 11)	/* Segment-Not-Present Exception */
#define INTRCPT_EXC_NO_STK_SEG		(1 << 12)	/* Missing stack segment */
#define INTRCPT_EXC_GPF			(1 << 13)	/* General protection */
#define INTRCPT_EXC_PF			(1 << 14)	/* Pagefault exception */

/* Flags for FIRST general intercept word - VMCB + 0Ch */
#define INTRCPT_INTR			(1 << 0)
#define INTRCPT_NMI			(1 << 1)
#define INTRCPT_SMI			(1 << 2)
#define INTRCPT_INIT			(1 << 3)
#define INTRCPT_VINTR			(1 << 4)
#define INTRCPT_CR0_WR			(1 << 5)
#define INTRCPT_IDTR_RD			(1 << 6)
#define INTRCPT_GDTR_RD			(1 << 7)
#define INTRCPT_LDTR_RD			(1 << 8)
#define INTRCPT_TR_RD			(1 << 9)
#define INTRCPT_IDTR_WR			(1 << 10)
#define INTRCPT_GDTR_WR			(1 << 11)
#define INTRCPT_LDTR_WR			(1 << 12)
#define INTRCPT_TR_WR			(1 << 13)
#define INTRCPT_RDTSC			(1 << 14)
#define INTRCPT_RDPMC			(1 << 15)
#define INTRCPT_PUSHF			(1 << 16)
#define INTRCPT_POPF			(1 << 17)
#define INTRCPT_CPUID			(1 << 18)
#define INTRCPT_RSM			(1 << 19)
#define INTRCPT_IRET			(1 << 20)
#define INTRCPT_INTN			(1 << 21)
#define INTRCPT_INVD			(1 << 22)
#define INTRCPT_PAUSE			(1 << 23)
#define INTRCPT_HLT			(1 << 24)
#define INTRCPT_INVLPG			(1 << 25)
#define INTRCPT_INVLPGA			(1 << 26)
#define INTRCPT_IOIO_PROT		(1 << 27)
#define INTRCPT_MSR_PROT		(1 << 28)
#define INTRCPT_TASKSWITCH		(1 << 29)
#define INTRCPT_FERR_FREEZE		(1 << 30)
#define INTRCPT_SHUTDOWN		(1 << 31)

/* Flags for SECOND general intercept word - VMCB + 010h */
#define INTRCPT_VMRUN			(1 << 0)
#define INTRCPT_VMMCALL			(1 << 1)
#define INTRCPT_VMLOAD			(1 << 2)
#define INTRCPT_VMSAVE			(1 << 3)
#define INTRCPT_STDI			(1 << 4)
#define INTRCPT_CLGI			(1 << 5)
#define INTRCPT_SKINIT			(1 << 6)
#define INTRCPT_RDTSCP			(1 << 7)
#define INTRCPT_ICEBP			(1 << 8)
#define INTRCPT_WBINVD			(1 << 9)
#define INTRCPT_MONITOR			(1 << 10)
#define INTRCPT_MWAIT			(1 << 11)
#define INTRCPT_MWAIT_IFARM		(1 << 12)
#define INTRCPT_XSETBV			(1 << 13)

/* values which would create fault when the guest make a syscall */
#define SYSENTER_CS_FAULT		0

#endif
