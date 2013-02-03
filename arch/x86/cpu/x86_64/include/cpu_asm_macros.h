/**
 * Copyright (c) 2012 Himanshu Chauhan.
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
 * @file cpu_asm_macros.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief header file for assembly macros.
 */
#ifndef _CPU_ASM_MACROS_H__
#define _CPU_ASM_MACROS_H__

#define SAVE_ALL			\
	pushq %r15;			\
	pushq %r14;			\
	pushq %r13;			\
	pushq %r12;			\
	pushq %r11;			\
	pushq %r10;			\
	pushq %r9;			\
	pushq %r8;			\
	pushq %rbp;			\
	pushq %rsi;			\
	pushq %rdi;			\
	pushq %rdx;			\
	pushq %rcx;			\
	pushq %rbx;			\
	pushq %rax;


#define RESTORE_ALL			\
	popq %rax;			\
	popq %rbx;			\
	popq %rcx;			\
	popq %rdx;			\
	popq %rdi;			\
	popq %rsi;			\
	popq %rbp;			\
	popq %r8;			\
	popq %r9;			\
	popq %r10;			\
	popq %r11;			\
	popq %r12;			\
	popq %r13;			\
	popq %r14;			\
	popq %r15;			\

#define FUNCTION(__symbol)		\
	.globl __symbol;		\
	.type __symbol, @function;	\
__symbol:

#define IRQ_HANDLER(_symbol)		\
	FUNCTION(_symbol)		\
	SAVE_ALL			\

#define END_IRQ_HANDLER			\
	RESTORE_ALL			\
	iretq

#define num_to_string(s)	to_string(s)
#define to_string(s)		#s

#define BUILD_IRQ(__n, __who)			\
	.align 1024;				\
	IRQ_HANDLER(__IRQ_##__n)		\
	movq $__n, %rdi;			\
	movq %rsp, %rsi;			\
	callq __who;				\
	END_IRQ_HANDLER

#endif /* _CPU_ASM_MACROS_H__ */
