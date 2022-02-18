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
 * @file cpu_vcpu_sbi.c
 * @author Atish Patra (atish.patra@wdc.com)
 * @brief source of SBI call handling
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_manager.h>
#include <cpu_vcpu_sbi.h>
#include <cpu_vcpu_trap.h>
#include <riscv_sbi.h>

extern const struct cpu_vcpu_sbi_extension vcpu_sbi_time;
extern const struct cpu_vcpu_sbi_extension vcpu_sbi_rfence;
extern const struct cpu_vcpu_sbi_extension vcpu_sbi_ipi;
extern const struct cpu_vcpu_sbi_extension vcpu_sbi_base;
extern const struct cpu_vcpu_sbi_extension vcpu_sbi_hsm;
extern const struct cpu_vcpu_sbi_extension vcpu_sbi_srst;
extern const struct cpu_vcpu_sbi_extension vcpu_sbi_legacy;
extern const struct cpu_vcpu_sbi_extension vcpu_sbi_xvisor;

static const struct cpu_vcpu_sbi_extension *vcpu_sbi[] = {
	&vcpu_sbi_time,
	&vcpu_sbi_rfence,
	&vcpu_sbi_ipi,
	&vcpu_sbi_base,
	&vcpu_sbi_hsm,
	&vcpu_sbi_srst,
	&vcpu_sbi_legacy,
	&vcpu_sbi_xvisor,
};

const struct cpu_vcpu_sbi_extension *cpu_vcpu_sbi_find_extension(
						unsigned long ext_id)
{
	int i;

	for (i = 0; i < array_size(vcpu_sbi); i++) {
		if (ext_id >= vcpu_sbi[i]->extid_start &&
		    ext_id <= vcpu_sbi[i]->extid_end)
			return vcpu_sbi[i];
	}

	return NULL;
}

int cpu_vcpu_sbi_ecall(struct vmm_vcpu *vcpu, ulong cause,
		       arch_regs_t *regs)
{
	int ret = 0;
	const struct cpu_vcpu_sbi_extension *ext;
	unsigned long extension_id = regs->a7;
	unsigned long func_id = regs->a6;
	struct cpu_vcpu_trap trap = { 0 };
	unsigned long out_val = 0;
	bool is_0_1_spec = FALSE;
	unsigned long args[6];

	/* Forward SBI calls from virtual-VS mode to virtual-HS mode */
	if (riscv_nested_virt(vcpu)) {
		trap.sepc = regs->sepc;
		trap.scause = CAUSE_VIRTUAL_SUPERVISOR_ECALL;
		trap.stval = 0;
		trap.htval = 0;
		trap.htinst = 0;
		cpu_vcpu_redirect_trap(vcpu, regs, &trap);
		return VMM_OK;
	}

	args[0] = regs->a0;
	args[1] = regs->a1;
	args[2] = regs->a2;
	args[3] = regs->a3;
	args[4] = regs->a4;
	args[5] = regs->a5;

	ext = cpu_vcpu_sbi_find_extension(extension_id);
	if (ext && ext->handle) {
		ret = ext->handle(vcpu, extension_id, func_id,
				  args, &out_val, &trap);
		if (extension_id >= SBI_EXT_0_1_SET_TIMER &&
		    extension_id <= SBI_EXT_0_1_SHUTDOWN)
			is_0_1_spec = TRUE;
	} else {
		ret = SBI_ERR_NOT_SUPPORTED;
	}

	if (trap.scause) {
		trap.sepc = regs->sepc;
		cpu_vcpu_redirect_trap(vcpu, regs, &trap);
	} else {
		/* This function should return non-zero value only in case of
		 * fatal error. However, there is no good way to distinguish
		 * between a fatal and non-fatal errors yet. That's why we treat
		 * every return value except trap as non-fatal and just return
		 * accordingly for now. Once fatal errors are defined, that
		 * case should be handled differently.
		 */
		regs->sepc += 4;
		regs->a0 = ret;
		if (!is_0_1_spec)
			regs->a1 = out_val;
	}

	return 0;
}
