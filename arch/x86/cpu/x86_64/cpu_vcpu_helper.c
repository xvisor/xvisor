/**
 * Copyright (c) 2011 Himanshu Chauhan
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
 * @file cpu_vcpu_helper.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief source of VCPU helper functions
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_manager.h>
#include <vmm_host_aspace.h>
#include <vmm_guest_aspace.h>
#include <cpu_mmu.h>
#include <cpu_features.h>
#include <cpu_vm.h>
#include <arch_cpu.h>
#include <arch_regs.h>
#include <libs/stringlib.h>
#include <arch_guest_helper.h>

void arch_vcpu_emergency_shutdown(struct vcpu_hw_context *context);

/*
 * These are what we need to emulate in CPU:
 *
 * vendor_id: AuthenticAMD
 * cpu family: 6
 * model: 6
 * model name: Xvisor Virtual CPU
 * stepping: 3
 * cpu MHz: 1662.454
 * cache size: 512 KB
 * fpu: yes
 * fpu_exception: yes
 * cpuid level: 4
 * wp: yes
 * flags: fpu  pse  tsc     msr        pae     mce
 *        cx8  apic sep     mtrr       pge     mca
 *        cmov pat  pse36   clflush    mmx     fxsr
 *        sse  sse2 syscall nx         lm      nopl
 *        pni  cx16 popcnt  hypervisor lahf_lm svm
 *        abm  sse4a
 * TLB size: 1024 4K pages
 * clflush size: 64
 * cache_alignment: 64
 * address sizes: 40 bits physical, 48 bits virtual
 */
static void init_cpu_capabilities(enum x86_processor_generation proc_gen, struct vmm_vcpu *vcpu)
{
	u32 funcs;
	struct x86_vcpu_priv *priv = x86_vcpu_priv(vcpu);
	struct cpuid_response *func_response;

	switch(proc_gen) {
	case x86_CPU_AMD_K6:
		for (funcs = CPUID_BASE_VENDORSTRING;
		     funcs < CPUID_BASE_FUNC_LIMIT; funcs++) {
			func_response =
				(struct cpuid_response *)
				&priv->standard_funcs[funcs];

			switch (funcs) {
			case CPUID_BASE_VENDORSTRING:
				func_response->resp_eax = CPUID_BASE_FUNC_LIMIT;
				func_response->resp_ebx = 0x68747541; /* 41757468; htuA*/
				func_response->resp_ecx = 0x444d4163; /* 63414d44; DMAc*/
				func_response->resp_edx = 0x69746e65; /* 656e7469; itne*/
				break;

			case CPUID_BASE_FEATURES:
				func_response->resp_eax =
					((0x0 << CPUID_EXTD_FAMILY_SHIFT)
					 | (0x6 << CPUID_EXTD_MODEL_SHIFT)
					 | (0x6 << CPUID_BASE_FAMILY_SHIFT)
					 | (0x9 << CPUID_BASE_MODEL_SHIFT)
					 | (0x3));
				func_response->resp_ebx =
					((0x0 << 24) /* Local APIC ID */
					 | (0x1 << 16) /* Logical Processor count */
					 | (0x40 << 8)); /* 64 bytes CFFLUSH ? */
				func_response->resp_ecx = 0x0; /* no SSE3, AES etc support */
				func_response->resp_edx =
					(CPUID_FEAT_EDX_CLF
					 | CPUID_FEAT_EDX_FPU);
				break;

			default:
				func_response->resp_eax = 0;
				func_response->resp_ebx = 0;
				func_response->resp_ecx = 0;
				func_response->resp_edx = 0;
				break;
			}
		}

		for (funcs = CPUID_EXTENDED_BASE;
		     funcs < CPUID_EXTENDED_FUNC_LIMIT; funcs++) {
			func_response =
				(struct cpuid_response *)
				&priv->extended_funcs[funcs - CPUID_EXTENDED_BASE];

			switch (funcs) {
			case CPUID_EXTENDED_BASE:
				func_response->resp_eax =
					CPUID_EXTENDED_L2_CACHE_TLB_IDENTIFIER
					- CPUID_EXTENDED_BASE;
				func_response->resp_ebx = 0x73697658; /*sivX*/
				func_response->resp_ecx = 0x7658726f; /*vXor*/
				func_response->resp_edx = 0x726f7369; /*rosi*/
				break;

			case CPUID_EXTENDED_FEATURES: /* replica of base features */
				func_response->resp_eax =
					((0x0 << CPUID_EXTD_FAMILY_SHIFT)
					 | (0x6 << CPUID_EXTD_MODEL_SHIFT)
					 | (0x6 << CPUID_BASE_FAMILY_SHIFT)
					 | (0x9 << CPUID_BASE_MODEL_SHIFT)
					 | (0x3));
				func_response->resp_ebx =
					((0x0 << 24) /* Local APIC ID */
					 | (0x1 << 16) /* Logical Processor count */
					 | (0x40 << 8)); /* 64 bytes CFFLUSH ? */
				func_response->resp_ecx = 0x0; /* no SSE3, AES etc support */
				func_response->resp_edx =
					(CPUID_FEAT_EDX_NX
					 | CPUID_FEAT_EDX_CMOV
					 | CPUID_FEAT_EDX_SEP);
				break;

			case CPUID_EXTENDED_BRANDSTRING:
				memcpy((char *)&func_response->resp_eax,
				       "Xvis", 4);
				memcpy((char *)&func_response->resp_ebx,
				       "or V", 4);
				memcpy((char *)&func_response->resp_ecx,
				       "irtu", 4);
				memcpy((char *)&func_response->resp_edx,
				       "al C", 4);
				break;

			case CPUID_EXTENDED_BRANDSTRINGMORE:
				memcpy((char *)&func_response->resp_eax,
				       "PU v", 4);
				memcpy((char *)&func_response->resp_ebx,
				       "ersi", 4);
				memcpy((char *)&func_response->resp_ecx,
				       "on 0", 4);
				memcpy((char *)&func_response->resp_edx,
				       ".1  ", 4);
				break;

			case CPUID_EXTENDED_BRANDSTRINGEND:
				memset((void *)&func_response->resp_eax, 0, 4);
				memset((void *)&func_response->resp_ebx, 0, 4);
				memset((void *)&func_response->resp_ecx, 0, 4);
				memset((void *)&func_response->resp_edx, 0, 4);
				break;

			case CPUID_EXTENDED_L1_CACHE_TLB_IDENTIFIER:
				cpuid(CPUID_EXTENDED_L1_CACHE_TLB_IDENTIFIER,
				      &func_response->resp_eax,
				      &func_response->resp_ebx,
				      &func_response->resp_ecx,
				      &func_response->resp_edx);
				break;

			case CPUID_EXTENDED_L2_CACHE_TLB_IDENTIFIER:
				cpuid(CPUID_EXTENDED_L2_CACHE_TLB_IDENTIFIER,
				      &func_response->resp_eax,
				      &func_response->resp_ebx,
				      &func_response->resp_ecx,
				      &func_response->resp_edx);
				break;

			default:
				func_response->resp_eax = 0;
				func_response->resp_ebx = 0;
				func_response->resp_ecx = 0;
				func_response->resp_edx = 0;
				break;
			}
		}

		break;

	case x86_CPU_INTEL_PENTIUM:
		VM_LOG(LVL_ERR, "ERROR: VCPU feature init on Intel."
		       "Intel chips not supported yet!\n");
		break;

	case x86_NR_GENERATIONS:
		break;
	}
}

static void arch_guest_vcpu_trampoline(struct vmm_vcpu *vcpu)
{
	VM_LOG(LVL_DEBUG, "Running VCPU %s\n", vcpu->name);
	cpu_boot_vcpu(x86_vcpu_priv(vcpu)->hw_context);
	VM_LOG(LVL_ERR, "ERROR: Guest VCPU exited from run loop!\n");
	while(1); /* Should never come here! */
}

int arch_vcpu_init(struct vmm_vcpu *vcpu)
{
	int rc, cpuid;
	u64 stack_start;
	const char *attr;
	extern struct cpuinfo_x86 cpu_info;

	if (!vcpu->is_normal) {
		/* For orphan vcpu */
		stack_start = vcpu->stack_va + vcpu->stack_sz - sizeof(u64);
		vcpu->regs.rip = vcpu->start_pc;
		vcpu->regs.rip = vcpu->start_pc;
		vcpu->regs.rsp = stack_start;
		vcpu->regs.cs = VMM_CODE_SEG_SEL;
		vcpu->regs.ss = VMM_DATA_SEG_SEL;
		vcpu->regs.rflags = (X86_EFLAGS_IF | X86_EFLAGS_PF | X86_EFLAGS_CF);
	} else {
		rc = vmm_devtree_read_string(vcpu->node,
				VMM_DEVTREE_COMPATIBLE_ATTR_NAME, &attr);
		if (rc) {
			return rc;
		}

		if (strcmp(attr, "amd-k6") == 0) {
			cpuid = x86_CPU_AMD_K6;
		} else {
			return VMM_EFAIL;
		}

		if (!vcpu->reset_count) {
			vcpu->arch_priv = vmm_zalloc(sizeof(struct x86_vcpu_priv));

			if (!vcpu->arch_priv)
				return VMM_EFAIL;

			INIT_SPIN_LOCK(&x86_vcpu_priv(vcpu)->lock);

			init_cpu_capabilities(cpuid, vcpu);

			x86_vcpu_priv(vcpu)->hw_context = vmm_zalloc(sizeof(struct vcpu_hw_context));
			x86_vcpu_priv(vcpu)->hw_context->assoc_vcpu = vcpu;

			/*
			 * !!KLUDGE!!
			 * The Guest DTS tells the start PC for the guest. The core code
			 * takes this start PC as the PC of the newly formed VCPU. This
			 * address can not be used by the processor to run. The newly
			 * formed VCPU has to run a trampoline code which will run in loop
			 * and switch processor's mode to guest mode and then run the processor
			 * from the address specified in DTS.
			 *
			 * So we save this start PC read from DTS in the VCPU hardware context
			 * and when this VCPU switches to guest mode, it will make processor
			 * run from this address.
			 */
			x86_vcpu_priv(vcpu)->hw_context->guest_start_pc = vcpu->start_pc;
			x86_vcpu_priv(vcpu)->hw_context->vcpu_emergency_shutdown = arch_vcpu_emergency_shutdown;
			cpu_init_vcpu_hw_context(&cpu_info, x86_vcpu_priv(vcpu)->hw_context);

			/*
			 * This vcpu has to run VMM code before and after guest mode
			 * switch. Prepare for the same.
			 */
			stack_start = vcpu->stack_va + vcpu->stack_sz - sizeof(u64);
			vcpu->regs.rip = (u64)arch_guest_vcpu_trampoline;
			vcpu->regs.rsp = stack_start;
			vcpu->regs.cs = VMM_CODE_SEG_SEL;
			vcpu->regs.ss = VMM_DATA_SEG_SEL;
			vcpu->regs.rdi = (u64)vcpu; /* this VCPU as parameter */
			vcpu->regs.rflags = (X86_EFLAGS_IF | X86_EFLAGS_PF | X86_EFLAGS_CF);
		}
	}

	return VMM_OK;
}

int arch_vcpu_deinit(struct vmm_vcpu * vcpu)
{
	return VMM_OK;
}

void arch_vcpu_switch(struct vmm_vcpu *tvcpu, 
		      struct vmm_vcpu *vcpu,
		      arch_regs_t *regs)
{
	if (!tvcpu) {
		/* first time rescheduling */
		memcpy(regs, &vcpu->regs, sizeof(arch_regs_t));
	} else {
		memcpy(&tvcpu->regs, regs, sizeof(arch_regs_t));
		memcpy(regs, &vcpu->regs, sizeof(arch_regs_t));
	}
}

void arch_vcpu_post_switch(struct vmm_vcpu *vcpu,
			   arch_regs_t *regs)
{
	/* Nothing to do here. */
}

void arch_vcpu_preempt_orphan(void)
{
	/* Trigger system call from hypervisor. This will
	 * cause do_generic_int_handler() function to call 
	 * vmm_scheduler_preempt_orphan()
	 */
	asm volatile ("int $0x80\t\n");
}

static void __dump_vcpu_regs(struct vmm_chardev *cdev, arch_regs_t *regs)
{
	vmm_cprintf(cdev, "RAX: 0x%08lx RBX: 0x%08lx RCX: 0x%08lx RDX: 0x%08lx\n", 
		    regs->rax, regs->rbx, regs->rcx, regs->rdx);
	vmm_cprintf(cdev, "RDI: 0x%08lx RSI: 0x%08lx RBP: 0x%08lx R08: 0x%08lx\n",
		    regs->rdi, regs->rsi, regs->rbp, regs->r8);
	vmm_cprintf(cdev, "R09: 0x%08lx R10: 0x%08lx R11: 0x%08lx R12: 0x%08lx\n", 
		    regs->r9, regs->r10, regs->r11, regs->r12);
	vmm_cprintf(cdev, "R13: 0x%08lx R14: 0x%08lx R15: 0x%08lx RIP: 0x%08lx\n", 
		    regs->r13, regs->r14, regs->r15, regs->rip);
	vmm_cprintf(cdev, "RSP: 0x%08lx RFLAGS: 0x%08lx HW-ERR: 0x%08lx\n", 
		    regs->rsp, regs->rflags, regs->hw_err_code);
	vmm_cprintf(cdev, "SS: 0x%08lx CS: 0x%08lx\n", 
		    regs->ss, regs->cs);
}

void dump_vcpu_regs(arch_regs_t *regs)
{
	__dump_vcpu_regs(NULL, regs);
}

void arch_vcpu_stat_dump(struct vmm_chardev *cdev, struct vmm_vcpu *vcpu)
{
	/* For now no arch specific stats */
}

static void dump_guest_vcpu_state(struct vcpu_hw_context *context)
{
	int i;
	u32 val;

	vmm_printf("\nGUEST %s dump state:\n\n", context->assoc_vcpu->name);

	vmm_printf("RAX: 0x%08x RBX: 0x%08x RCX: 0x%08x RDX: 0x%08x\n",
		   context->vmcb->rax, context->g_regs[GUEST_REGS_RBX],
		   context->g_regs[GUEST_REGS_RCX], context->g_regs[GUEST_REGS_RDX]);
	vmm_printf("R08: 0x%08x R09: 0x%08x R10: 0x%08x R11: 0x%08x\n",
		   context->g_regs[GUEST_REGS_R8], context->g_regs[GUEST_REGS_R9],
		   context->g_regs[GUEST_REGS_R10], context->g_regs[GUEST_REGS_R10]);
	vmm_printf("R12: 0x%08x R13: 0x%08x R14: 0x%08x R15: 0x%08x\n",
		   context->g_regs[GUEST_REGS_R12], context->g_regs[GUEST_REGS_R13],
		   context->g_regs[GUEST_REGS_R14], context->g_regs[GUEST_REGS_R15]);
	vmm_printf("RSP: 0x%08x RBP: 0x%08x RDI: 0x%08x RSI: 0x%08x\n",
		   context->vmcb->rsp, context->g_regs[GUEST_REGS_RBP],
		   context->g_regs[GUEST_REGS_RDI], context->g_regs[GUEST_REGS_RSI]);
	vmm_printf("RIP: 0x%08x\n\n", context->vmcb->rip);
	vmm_printf("CR0: 0x%08x CR2: 0x%08x CR3: 0x%08x CR4: 0x%08x\n",
		   context->vmcb->cr0, context->vmcb->cr2,
		   context->vmcb->cr3, context->vmcb->cr4);

	dump_seg_selector("CS ", &context->vmcb->cs);
	dump_seg_selector("DS ", &context->vmcb->ds);
	dump_seg_selector("ES ", &context->vmcb->es);
	dump_seg_selector("SS ", &context->vmcb->ss);
	dump_seg_selector("FS ", &context->vmcb->fs);
	dump_seg_selector("GS ", &context->vmcb->gs);
	dump_seg_selector("GDT", &context->vmcb->gdtr);
	dump_seg_selector("LDT", &context->vmcb->ldtr);
	dump_seg_selector("IDT", &context->vmcb->idtr);
	dump_seg_selector("TR ", &context->vmcb->tr);


	vmm_printf("RFLAGS: 0x%08x    [ ", context->vmcb->rflags);
	for (i = 0; i < 32; i++) {
		val = context->vmcb->rflags & (0x1UL << i);
		switch(val) {
		case X86_EFLAGS_CF:
			vmm_printf("CF ");
			break;
		case X86_EFLAGS_PF:
			vmm_printf("PF ");
			break;
		case X86_EFLAGS_AF:
			vmm_printf("AF ");
			break;
		case X86_EFLAGS_ZF:
			vmm_printf("ZF ");
			break;
		case X86_EFLAGS_SF:
			vmm_printf("SF ");
			break;
		case X86_EFLAGS_TF:
			vmm_printf("TF ");
			break;
		case X86_EFLAGS_IF:
			vmm_printf("IF ");
			break;
		case X86_EFLAGS_DF:
			vmm_printf("DF ");
			break;
		case X86_EFLAGS_OF:
			vmm_printf("OF ");
			break;
		case X86_EFLAGS_NT:
			vmm_printf("NT ");
			break;
		case X86_EFLAGS_RF:
			vmm_printf("RF ");
			break;
		case X86_EFLAGS_VM:
			vmm_printf("VM ");
			break;
		case X86_EFLAGS_AC:
			vmm_printf("AC ");
			break;
		case X86_EFLAGS_VIF:
			vmm_printf("VIF ");
			break;
		case X86_EFLAGS_VIP:
			vmm_printf("VIP ");
			break;
		case X86_EFLAGS_ID:
			vmm_printf("ID ");
			break;
		}
	}
	vmm_printf("]\n");
}

void arch_vcpu_regs_dump(struct vmm_chardev *cdev, struct vmm_vcpu *vcpu)
{
	struct vcpu_hw_context *context = x86_vcpu_hw_context(vcpu);

	if (context) {
		dump_guest_vcpu_state(context);
	}
}

void arch_vcpu_emergency_shutdown(struct vcpu_hw_context *context)
{
	dump_guest_vcpu_state(context);
	vmm_manager_guest_halt(context->assoc_vcpu->guest);
}
