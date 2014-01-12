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
 * @file amd_intercept.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief SVM intercept handling/registration code.
 */
#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <vmm_guest_aspace.h>
#include <cpu_vm.h>
#include <cpu_features.h>
#include <cpu_mmu.h>
#include <cpu_pgtbl_helper.h>
#include <vm/amd_intercept.h>
#include <vm/amd_svm.h>
#include <vmm_manager.h>
#include <vmm_main.h>

static char *exception_names[] = {
	"#DivError",	/* 0 */
	"#Debug",	/* 1 */
	"#NMI",		/* 2 */
	"#Breakpoint",	/* 3 */
	"#Overflow",	/* 4 */
	"#OutOfBounds",	/* 5 */
	"#InvOpcode",	/* 6 */
	"#NoDev",	/* 7 */
	"#DoubleFault",	/* 8 */
	"#CoprocOvrrun",/* 9 */
	"#InvalTSS",	/* 10 */
	"#MissingSeg",	/* 11 */
	"#MissingStack",/* 12 */
	"#GPF",		/* 13 */
	"#PGFault",	/* 14 */
	"#CoprocErr",	/* 15 */
	"#AlignCheck",	/* 16 */
	"#MachineCheck",/* 17 */
	"#SIMDErr",	/* 18 */
	"#Unknown19",	/* 19 */
	"#Unknown20",	/* 20 */
	"#Unknown21",	/* 21 */
	"#Unknown22",	/* 22 */
	"#Unknown23",	/* 23 */
	"#Unknown24",	/* 24 */
	"#Unknown25",	/* 25 */
	"#Unknown26",	/* 26 */
	"#Unknown27",	/* 27 */
	"#Unknown28",	/* 28 */
	"#Unknown29",	/* 29 */
	"#Unknown30",	/* 30 */
	"#Unknown31",	/* 31 */
};

extern int realmode_map_memory(struct vcpu_hw_context *context, virtual_addr_t vaddr,
			       physical_addr_t paddr, size_t size);

static inline void dump_guest_exception_insts(struct vmcb *vmcb)
{
	int i;
	u8 *guest_ins_base = (u8 *)((u8 *)(vmcb)) + 0xd0;

	for (i = 0; i < 16; i++) {
		vmm_printf("%x ", guest_ins_base[i]);
		if (i && !(i % 8)) vmm_printf("\n");
	}
}

static inline int guest_in_realmode(struct vcpu_hw_context *context)
{
	return (!(context->vmcb->cr0 & X86_CR0_PE));
}

static u64 guest_read_fault_inst(struct vcpu_hw_context *context)
{
	u64 g_ins;
	physical_addr_t rip_phys = guest_virtual_to_physical(context, context->vmcb->rip);
	if (rip_phys) {
		rip_phys = (context->vmcb->cs.sel << 4) | rip_phys;
		if (vmm_guest_memory_read(context->assoc_vcpu->guest, rip_phys,
					  &g_ins, sizeof(g_ins)) < sizeof(g_ins)) {
			VM_LOG(LVL_ERR, "Failed to read instruction at intercepted "
			       "instruction pointer. (%x)\n", rip_phys);
			return 0;
		}

		return g_ins;
	}

	VM_LOG(LVL_ERR, "Failed to get RIP guest virtual to physical\n");

	return 0;
}

void __handle_vm_npf (struct vcpu_hw_context *context)
{
	VM_LOG(LVL_INFO, "Unhandled Intercept: nested page fault.\n");
	if (context->vcpu_emergency_shutdown)
		context->vcpu_emergency_shutdown(context);

	vmm_hang();
}

void __handle_vm_swint (struct vcpu_hw_context *context)
{
	VM_LOG(LVL_INFO, "Unhandled Intercept: software interrupt.\n");
	if (context->vcpu_emergency_shutdown)
		context->vcpu_emergency_shutdown(context);

	vmm_hang();
}

void __handle_vm_exception (struct vcpu_hw_context *context)
{
	switch (context->vmcb->exitcode)
	{
	case VMEXIT_EXCEPTION_PF:
		VM_LOG(LVL_ERR, "Guest fault: 0x%x (rIP: %x)\n",
		       context->vmcb->exitinfo2, context->vmcb->rip);

		u64 fault_gphys = context->vmcb->exitinfo2;
		/* Guest is in real mode so faulting guest virtual is
		 * guest physical address. We just need to add faulting
		 * address as offset to host physical address to get
		 * the destination physical address.
		 */
		struct vmm_region *g_reg = vmm_guest_find_region(context->assoc_vcpu->guest,
								 fault_gphys,
								 VMM_REGION_REAL | VMM_REGION_MEMORY,
								 FALSE);
		if (!g_reg) {
			VM_LOG(LVL_ERR, "ERROR: Can't find the host physical address for guest physical: 0x%lx\n",
			       fault_gphys);
			goto guest_bad_fault;
		}

		if (realmode_map_memory(context, fault_gphys,
					(g_reg->hphys_addr + fault_gphys),
					PAGE_SIZE) != VMM_OK) {
			VM_LOG(LVL_ERR, "ERROR: Failed to create map in guest's shadow page table.\n");
			goto guest_bad_fault;
		}
		context->vmcb->cr2 = context->vmcb->exitinfo2;
		break;

	default:
		VM_LOG(LVL_ERR, "Unhandled guest exception %s (rIP: %x)\n",
		       exception_names[context->vmcb->exitcode - 0x40],
		       context->vmcb->rip);
		goto guest_bad_fault;
		break;
	}

	return;

 guest_bad_fault:
	if (context->vcpu_emergency_shutdown)
		context->vcpu_emergency_shutdown(context);

	vmm_hang();
}

void __handle_vm_wrmsr (struct vcpu_hw_context *context)
{
	VM_LOG(LVL_INFO, "Unhandled Intercept: msr write.\n");
	if (context->vcpu_emergency_shutdown)
		context->vcpu_emergency_shutdown(context);

	vmm_hang();
}

void __handle_popf(struct vcpu_hw_context *context)
{
	VM_LOG(LVL_INFO, "Unhandled Intercept: popf.\n");
	if (context->vcpu_emergency_shutdown)
		context->vcpu_emergency_shutdown(context);

	vmm_hang();
}

void __handle_vm_vmmcall (struct vcpu_hw_context *context)
{
	VM_LOG(LVL_INFO, "Unhandled Intercept: vmmcall.\n");
	if (context->vcpu_emergency_shutdown)
		context->vcpu_emergency_shutdown(context);

	vmm_hang();
}

void __handle_vm_iret(struct vcpu_hw_context *context)
{
	VM_LOG(LVL_INFO, "Unhandled Intercept: iret.\n");
	if (context->vcpu_emergency_shutdown)
		context->vcpu_emergency_shutdown(context);

	vmm_hang();
}

void __handle_crN_read(struct vcpu_hw_context *context)
{
	int crn = context->vmcb->exitcode - VMEXIT_CR0_READ;
	int cr_gpr;

	switch(crn) {
	case 0:
		/* Check if host support instruction decode assistance */
		if (context->cpuinfo->decode_assist) {
			if (context->vmcb->exitinfo1 & VALID_CRN_TRAP) {
				cr_gpr = (context->vmcb->exitinfo1 & 0xf);
				vmm_printf("Guest writing 0x%lx to Cr0 from reg %d.\n",
					   context->g_regs[cr_gpr], cr_gpr);
			}
		} else {
			u8 reg, ext_reg = 0;
			u32 g_ins = (u32)guest_read_fault_inst(context);
			if (((u8 *)&g_ins)[0] == 0x41) {
				reg = ((u8 *)&g_ins)[3] - 0xc0;
				ext_reg = 1;
			} else
				reg = ((u8 *)&g_ins)[2] - 0xc0;

			if (ext_reg) g_ins = ((g_ins << 8) >> 8);
			g_ins &= ~(0xFFFFUL << 16);
			if (g_ins == 0x200f) { /* mov %cr0, register */
				if (!reg)
					context->vmcb->rax = context->vmcb->cr0;

				context->g_regs[reg] = context->vmcb->cr0;
				context->vmcb->rip += MOV_CRn_INST_SZ;
				/* 1 more byte for 64-bit where r8-r15 registers are used. */
				if (ext_reg) context->vmcb->rip += 1;
				VM_LOG(LVL_ERR, "GR: CR0= 0x%lx\n", context->vmcb->cr0);
			} else {
				vmm_printf("unknown fault instruction: %x\n", g_ins);
				goto guest_bad_fault;
			}
		}
		break;
	case 3:
		break;
	default:
		VM_LOG(LVL_ERR, "Unhandled intercept cr%d read\n",
		       crn);
		break;
	}

	return;

 guest_bad_fault:
	if (context->vcpu_emergency_shutdown){
		context->vcpu_emergency_shutdown(context);
	}
	vmm_hang();
}

void __handle_crN_write(struct vcpu_hw_context *context)
{
	int crn = context->vmcb->exitcode - VMEXIT_CR0_WRITE;
	int cr_gpr;

	switch(crn) {
	case 0:
		/* Check if host support instruction decode assistance */
		if (context->cpuinfo->decode_assist) {
			if (context->vmcb->exitinfo1 & VALID_CRN_TRAP) {
				cr_gpr = (context->vmcb->exitinfo1 & 0xf);
				vmm_printf("Guest writing 0x%lx to Cr0 from reg %d.\n",
					   context->g_regs[cr_gpr], cr_gpr);
			}
		} else {
			u8 reg, ext_reg = 0;
			u32 g_ins = (u32)guest_read_fault_inst(context);
			if (((u8 *)&g_ins)[0] == 0x41) {
				reg = ((((u8 *)&g_ins)[3] - 0xc0) + 8);
				ext_reg = 1;
			} else
				reg = ((u8 *)&g_ins)[2] - 0xc0;

			if (ext_reg) g_ins = ((g_ins << 8) >> 8);
			g_ins &= ~(0xFFFFUL << 16);
			if (g_ins == 0x220f) { /* mov register, %cr0 */
				if (!reg)
					context->vmcb->cr0 = context->vmcb->rax;
				else
					context->vmcb->cr0 = context->g_regs[reg];

				context->vmcb->rip += MOV_CRn_INST_SZ;
				/* 1 more byte for 64-bit where r8-r15 registers are used. */
				if (ext_reg) context->vmcb->rip += 1;
				VM_LOG(LVL_ERR, "GW: CR0= 0x%lx\n", context->vmcb->cr0);
			} else {
				vmm_printf("unknown fault instruction: %x\n", g_ins);
				goto guest_bad_fault;
			}
		}
		break;
	case 3:
		break;
	default:
		VM_LOG(LVL_ERR, "Unhandled intercept cr%d write\n",
		       crn);
		break;
	}

	return;

 guest_bad_fault:
	if (context->vcpu_emergency_shutdown){
		context->vcpu_emergency_shutdown(context);
	}
	vmm_hang();
}

void __handle_ioio(struct vcpu_hw_context *context)
{
	u32 io_port = (context->vmcb->exitinfo1 >> 16);
	u8 in_inst = (context->vmcb->exitinfo1 & (0x1 << 0));
	u8 str_op = (context->vmcb->exitinfo1 & (0x1 << 2));
	u8 rep_access = (context->vmcb->exitinfo1 & (0x1 << 3));
	u8 op_size = (context->vmcb->exitinfo1 & (0x1 << 4) ? 8
		      : ((context->vmcb->exitinfo1 & (0x1 << 5)) ? 16
			 : 32));
	u8 seg_num = (context->vmcb->exitinfo1 >> 10) & 0x7;
	u8 guest_rd;
	u8 wval;

	VM_LOG(LVL_VERBOSE, "RIP: %x exitinfo1: %x\n", context->vmcb->rip, context->vmcb->exitinfo1);
	VM_LOG(LVL_VERBOSE, "IOPort: %d is accssed for %sput. Size is %d. Segment: %d String operation? %s Repeated access? %s\n",
		io_port, (in_inst ? "in" : "out"), op_size,seg_num,(str_op ? "yes" : "no"),(rep_access ? "yes" : "no"));

	if (in_inst) {
		if (vmm_devemu_emulate_ioread(context->assoc_vcpu, io_port,
					      &guest_rd, sizeof(guest_rd)) != VMM_OK) {
			vmm_printf("Failed to emulate IO instruction in guest.\n");
			goto _fail;
		}

		context->g_regs[GUEST_REGS_RAX] = guest_rd;
		context->vmcb->rax = guest_rd;
	} else {
		wval = (u8)context->vmcb->rax;
		if (vmm_devemu_emulate_iowrite(context->assoc_vcpu, io_port,
					       &wval, sizeof(wval)) != VMM_OK) {
			vmm_printf("Failed to emulate IO instruction in guest.\n");
			goto _fail;
		}
	}

	context->vmcb->rip += 1;

	return;

 _fail:
	if (context->vcpu_emergency_shutdown){
		context->vcpu_emergency_shutdown(context);
	}
	vmm_hang();
}

void __handle_cpuid(struct vcpu_hw_context *context)
{
	struct x86_vcpu_priv *priv = x86_vcpu_priv(context->assoc_vcpu);
	struct cpuid_response *func;

	switch (context->vmcb->rax) {
	case CPUID_BASE_VENDORSTRING:
		func = &priv->standard_funcs[CPUID_BASE_VENDORSTRING];
		context->vmcb->rax = func->resp_eax;
		context->g_regs[GUEST_REGS_RBX] = func->resp_ebx;
		context->g_regs[GUEST_REGS_RCX] = func->resp_ecx;
		context->g_regs[GUEST_REGS_RDX] = func->resp_edx;
		break;

	case CPUID_BASE_FEATURES:
		func = &priv->standard_funcs[CPUID_BASE_FEATURES];
		context->vmcb->rax = func->resp_eax;
		context->g_regs[GUEST_REGS_RBX] = func->resp_ebx;
		context->g_regs[GUEST_REGS_RCX] = func->resp_ecx;
		context->g_regs[GUEST_REGS_RDX] = func->resp_edx;
		break;

	case CPUID_EXTENDED_BASE:
	case CPUID_EXTENDED_BRANDSTRING:
	case CPUID_EXTENDED_BRANDSTRINGMORE:
		func = &priv->extended_funcs[context->vmcb->rax - CPUID_EXTENDED_BASE];
		context->vmcb->rax = func->resp_eax;
		context->g_regs[GUEST_REGS_RBX] = func->resp_ebx;
		context->g_regs[GUEST_REGS_RCX] = func->resp_ecx;
		context->g_regs[GUEST_REGS_RDX] = func->resp_edx;
		break;

	default:
		VM_LOG(LVL_DEBUG, "GCPUID/R: Func: %x\n", context->vmcb->rax);
		goto _fail;
	}

	context->vmcb->rip += 2;

	return;

 _fail:
	if (context->vcpu_emergency_shutdown){
		context->vcpu_emergency_shutdown(context);
	}
	vmm_hang();
}

/**
 * \brief Handle the shutdown condition in guest.
 *
 * If the guest has seen a shutdown condition (a.k.a. triple fault)
 * give the notification to guest and the guest must be
 * destroyed then. If the guest as multiple vCPUs, all of then
 * should be sent a notification of this.
 *
 * @param context
 * The hardware context of the vcpu of the guest which saw the triple fault.
 */
void __handle_triple_fault(struct vcpu_hw_context *context)
{
	VM_LOG(LVL_ERR, "Triple fault in guest: %s!!\n", context->assoc_vcpu->guest->name);

	if (context->vcpu_emergency_shutdown)
		context->vcpu_emergency_shutdown(context);

	vmm_hang();
}

void handle_vcpuexit(struct vcpu_hw_context *context)
{
	VM_LOG(LVL_DEBUG, "**** #VMEXIT - exit code: %x\n", (u32) context->vmcb->exitcode);

	switch (context->vmcb->exitcode) {
	case VMEXIT_CR0_READ ... VMEXIT_CR15_READ: __handle_crN_read(context); break;
	case VMEXIT_CR0_WRITE ... VMEXIT_CR15_WRITE: __handle_crN_write(context); break;
	case VMEXIT_MSR:
		if (context->vmcb->exitinfo1 == 1) __handle_vm_wrmsr (context);
		break;
	case VMEXIT_EXCEPTION_DE ... VMEXIT_EXCEPTION_XF:
		__handle_vm_exception(context); break;

	case VMEXIT_SWINT: __handle_vm_swint(context); break;
	case VMEXIT_NPF: __handle_vm_npf (context); break;
	case VMEXIT_VMMCALL: __handle_vm_vmmcall(context); break;
	case VMEXIT_IRET: __handle_vm_iret(context); break;
	case VMEXIT_POPF: __handle_popf(context); break;
	case VMEXIT_SHUTDOWN: __handle_triple_fault(context); break;
	case VMEXIT_CPUID: __handle_cpuid(context); break;
	case VMEXIT_IOIO: __handle_ioio(context); break;
	default:
		VM_LOG(LVL_ERR, "#VMEXIT: Unhandled exit code: %x\n",
		       (u32)context->vmcb->exitcode);
		if (context->vcpu_emergency_shutdown)
			context->vcpu_emergency_shutdown(context);
	}
}
