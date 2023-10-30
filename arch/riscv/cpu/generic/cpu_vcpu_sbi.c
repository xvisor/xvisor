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
#include <vmm_heap.h>
#include <vmm_macros.h>
#include <vmm_manager.h>
#include <vmm_stdio.h>
#include <cpu_vcpu_sbi.h>
#include <cpu_vcpu_trap.h>
#include <riscv_sbi.h>

extern const struct cpu_vcpu_sbi_extension vcpu_sbi_time;
extern const struct cpu_vcpu_sbi_extension vcpu_sbi_rfence;
extern const struct cpu_vcpu_sbi_extension vcpu_sbi_ipi;
extern const struct cpu_vcpu_sbi_extension vcpu_sbi_base;
extern const struct cpu_vcpu_sbi_extension vcpu_sbi_hsm;
extern const struct cpu_vcpu_sbi_extension vcpu_sbi_dbcn;
extern const struct cpu_vcpu_sbi_extension vcpu_sbi_srst;
extern const struct cpu_vcpu_sbi_extension vcpu_sbi_legacy;
extern const struct cpu_vcpu_sbi_extension vcpu_sbi_xvisor;

static const struct cpu_vcpu_sbi_extension *sbi_exts[] = {
	&vcpu_sbi_time,
	&vcpu_sbi_rfence,
	&vcpu_sbi_ipi,
	&vcpu_sbi_base,
	&vcpu_sbi_hsm,
	&vcpu_sbi_dbcn,
	&vcpu_sbi_srst,
	&vcpu_sbi_legacy,
	&vcpu_sbi_xvisor,
};

struct cpu_vcpu_sbi {
	const struct cpu_vcpu_sbi_extension **sbi_exts;
};

const struct cpu_vcpu_sbi_extension *cpu_vcpu_sbi_find_extension(
						struct vmm_vcpu *vcpu,
						unsigned long ext_id)
{
	int i;
	struct cpu_vcpu_sbi *s = riscv_sbi_priv(vcpu);

	for (i = 0; i < array_size(sbi_exts); i++) {
		if (!s->sbi_exts[i])
			continue;
		if (ext_id >= s->sbi_exts[i]->extid_start &&
		    ext_id <= s->sbi_exts[i]->extid_end)
			return sbi_exts[i];
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
	struct cpu_vcpu_sbi_return out = { .value = 0, .trap = &trap,
					   .regs_updated = FALSE };
	bool is_0_1_spec = FALSE;
	unsigned long args[6];

	/* Forward SBI calls from virtual-VS mode to virtual-HS mode */
	if (riscv_nested_virt(vcpu)) {
		riscv_stats_priv(vcpu)->nested_sbi++;
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

	ext = cpu_vcpu_sbi_find_extension(vcpu, extension_id);
	if (ext && ext->handle) {
		ret = ext->handle(vcpu, extension_id, func_id, args, &out);
		if (extension_id >= SBI_EXT_0_1_SET_TIMER &&
		    extension_id <= SBI_EXT_0_1_SHUTDOWN)
			is_0_1_spec = TRUE;
	} else {
		ret = SBI_ERR_NOT_SUPPORTED;
	}

	if (trap.scause) {
		trap.sepc = regs->sepc;
		cpu_vcpu_redirect_trap(vcpu, regs, &trap);
	} else if (!out.regs_updated) {
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
			regs->a1 = out.value;
	}

	return 0;
}

int cpu_vcpu_sbi_init(struct vmm_vcpu *vcpu)
{
	const struct cpu_vcpu_sbi_extension *ext;
	struct cpu_vcpu_sbi *s;
	char aname[32];
	int i;

	s = vmm_zalloc(sizeof(*s));
	if (!s)
		return VMM_ENOMEM;

	s->sbi_exts = vmm_calloc(array_size(sbi_exts),
			sizeof(struct cpu_vcpu_sbi_extension *));
	if (!s->sbi_exts) {
		vmm_free(s);
		return VMM_ENOMEM;
	}

	riscv_sbi_priv(vcpu) = s;

	for (i = 0; i < array_size(sbi_exts); i++) {
		ext = sbi_exts[i];

		if (ext->probe && !ext->probe(vcpu))
			continue;

		/* Non-base extensions can be disabled via DT */
		if (SBI_EXT_BASE < ext->extid_start ||
		    ext->extid_end < SBI_EXT_BASE) {
			vmm_snprintf(aname, sizeof(aname),
				     "xvisor,disable-sbi-%s", ext->name);
			if (vmm_devtree_getattr(vcpu->node, aname))
				continue;
		}

		s->sbi_exts[i] = ext;
	}

	return 0;
}

void cpu_vcpu_sbi_deinit(struct vmm_vcpu *vcpu)
{
	struct cpu_vcpu_sbi *s = riscv_sbi_priv(vcpu);

	vmm_free(s->sbi_exts);
	vmm_free(s);
}

int cpu_vcpu_sbi_xlate_error(int xvisor_error)
{
	switch (xvisor_error) {
	case VMM_OK:
		return SBI_SUCCESS;

	case VMM_ENOTAVAIL:
	case VMM_ENOENT:
	case VMM_ENOSYS:
	case VMM_ENODEV:
	case VMM_EOPNOTSUPP:
	case VMM_ENOTSUPP:
		return SBI_ERR_NOT_SUPPORTED;

	case VMM_EINVALID:
		return SBI_ERR_INVALID_PARAM;

	case VMM_EACCESS:
		return SBI_ERR_DENIED;

	case VMM_ERANGE:
		return SBI_ERR_INVALID_ADDRESS;

	case VMM_EALREADY:
	case VMM_EEXIST:
		return SBI_ERR_ALREADY_AVAILABLE;

	default:
		break;
	}

	return SBI_ERR_FAILED;
}
