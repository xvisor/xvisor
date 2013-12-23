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

/* Flags for CRn access interception */
#define _INTRCPT_WRITE_CR0_OFFSET	16
#define INTRCPT_WRITE_CR3		(1 << (_INTRCPT_WRITE_CR0_OFFSET + 3))

/*
 * Flags for exception intercept word
 * 1 << vector associated with the exception - VMCB + 8h
 */
#define INTRCPT_DB			(1 << 1)	/* Debug exception */
#define INTRCPT_TS			(1 << 10)	/* Invalid TSS */
#define INTRCPT_NP			(1 << 11)	/* Segment-Not-Present Exception */
#define INTRCPT_GP			(1 << 13)	/* General protection */
#define INTRCPT_PF			(1 << 14)	/* Pagefault exception */

/* Flags for FIRST general intercept word - VMCB + 0Ch */
#define INTRCPT_INTR			(1 << 0)
#define INTRCPT_READTR			(1 << 9)
#define INTRCPT_IRET			(1 << 20)
#define INTRCPT_POPF			(1 << 17)
#define INTRCPT_INTN			(1 << 21)
#define INTRCPT_IO			(1 << 27)
#define INTRCPT_MSR			(1 << 28)
#define INTRCPT_TASKSWITCH		(1 << 29)

/* Flags for SECOND general intercept word - VMCB + 010h */
#define INTRCPT_VMRUN			(1 << 0)
#define INTRCPT_VMMCALL			(1 << 1)

/* values which would create fault when the guest make a syscall */
#define SYSENTER_CS_FAULT		0

#endif
