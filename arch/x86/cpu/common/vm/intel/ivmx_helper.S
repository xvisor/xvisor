/*
 * entry.S: VMX architecture-specific entry/exit handling.
 * Copyright (c) 2004, Intel Corporation.
 * Copyright (c) 2008, Citrix Systems, Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place - Suite 330, Boston, MA 02111-1307 USA.
 */

#define VMRESUME     .byte 0x0f,0x01,0xc3
#define VMLAUNCH     .byte 0x0f,0x01,0xc2
#define VMREAD(off)  .byte 0x0f,0x78,0x47,((off)-rip)
#define VMWRITE(off) .byte 0x0f,0x79,0x47,((off)-rip)

/* VMCS field encodings */
#define GUEST_RSP    0x681c
#define GUEST_RIP    0x681e
#define GUEST_RFLAGS 0x6820

	.text
	.code64

	.globl vmx_asm_vmexit_handler
vmx_asm_vmexit_handler:

	//call save_guest_states

        popq	%rdi
        popq	%rsi
        popq	%rdx
        popq	%rcx
        popq	%rax
        popq	%r8
        popq	%r9
        popq	%r10
        popq	%r11
        popq	%rbx
        popq	%rbp
        popq	%r12
        popq	%r13
        popq	%r14
        popq	%r15

	sti

	//call vmx_vmexit_handler

.Lvmx_resume:
	//call	restore_guest_states

        VMRESUME
        sti

	/* call vm_resume_fail */
        ud2

	.globl vmx_asm_do_vmentry
vmx_asm_do_vmentry:

        cli

        pushq	%r15
        pushq	%r14
        pushq	%r13
        pushq	%r12
        pushq	%rbp
        pushq	%rbx
        pushq	%r11
        pushq	%r10
        pushq	%r9
        pushq	%r8
        pushq	%rax
        pushq	%rcx
        pushq	%rdx
        pushq	%rsi
        pushq	%rdi

.Lvmx_launch:
	//call	init_guest_states

        VMLAUNCH
        sti

	/* call vm_launch_fail */
        ud2

#if 0
init_guest_states:  /* initialize some guest states when we move into it for the first time */
	movq	$0, first_boot

	movq	$0, %rax
	movq	$0, %rbx
	movq	$0, %rcx
	movq	$0x80, %rdx
	movq	$0, %rsi
	movq	$0, %rdi
	movq	$0, %rbp
	movq	$0, %r8
	movq	$0, %r9
	movq	$0, %r10
	movq	$0, %r11
	movq	$0, %r12
	movq	$0, %r13
	movq	$0, %r14
	movq	$0, %r15

	ret

restore_guest_states:
	movq	g_rax, %rax
	movq	g_rbx, %rbx
	movq	g_rcx, %rcx
	movq	g_rdx, %rdx
	movq	g_rsi, %rsi
	movq	g_rdi, %rdi
	movq	g_rbp, %rbp
	movq	g_r8 , %r8
	movq	g_r9 , %r9
	movq	g_r10, %r10
	movq	g_r11, %r11
	movq	g_r12, %r12
	movq	g_r13, %r13
	movq	g_r14, %r14
	movq	g_r15, %r15

	ret

save_guest_states:
	movq	%rax, g_rax
	movq	%rbx, g_rbx
	movq	%rcx, g_rcx
	movq	%rdx, g_rdx
	movq	%rsi, g_rsi
	movq	%rdi, g_rdi
	movq	%rbp, g_rbp
	movq	%r8 , g_r8
	movq	%r9 , g_r9
	movq	%r10, g_r10
	movq	%r11, g_r11
	movq	%r12, g_r12
	movq	%r13, g_r13
	movq	%r14, g_r14
	movq	%r15, g_r15

	ret
#endif
	/* end of file */
	ud2a
