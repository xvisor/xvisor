/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file cpu_vcpu_mem.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief This source file is for VCPU memory read/write emulation
 */

#include <vmm_error.h>
#include <vmm_devemu.h>
#include <cpu_inline_asm.h>
#include <cpu_vcpu_helper.h>
#include <cpu_vcpu_mem.h>

int cpu_vcpu_mem_read(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs,
			virtual_addr_t addr, 
			void *dst, u32 dst_len, 
			bool force_unpriv)
{
	int rc;
	u8 data8;
	u16 data16;
	u32 data32;
	physical_addr_t guest_pa;
	enum vmm_devemu_endianness data_endian;

	/* Determine data endianness */
	if (regs->cpsr & CPSR_BE_ENABLED) {
		data_endian = VMM_DEVEMU_BIG_ENDIAN;
	} else {
		data_endian = VMM_DEVEMU_LITTLE_ENDIAN;
	}

	/* Determine guest physical address */
	va2pa_c_pr(addr);
	guest_pa = read_par64();
	guest_pa &= PAR64_PA_MASK;
	guest_pa |= (addr & 0x00000FFF);

	/* Do guest memory read */
	switch (dst_len) {
	case 1:
		rc = vmm_devemu_emulate_read(vcpu, guest_pa,
					     &data8, sizeof(data8),
					     data_endian);
		*((u8 *)dst) = (!rc) ? data8 : 0;
		break;
	case 2:
		rc = vmm_devemu_emulate_read(vcpu, guest_pa,
					     &data16, sizeof(data16),
					     data_endian);
		*((u16 *)dst) = (!rc) ? data16 : 0;
		break;
	case 4:
		rc = vmm_devemu_emulate_read(vcpu, guest_pa,
					     &data32, sizeof(data32),
					     data_endian);
		*((u32 *)dst) = (!rc) ? data32 : 0;
		break;
	default:
		rc = VMM_EFAIL;
		break;
	};

	return rc;
}

int cpu_vcpu_mem_write(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs,
			virtual_addr_t addr, 
			void *src, u32 src_len,
			bool force_unpriv)
{
	int rc;
	u8 data8;
	u16 data16;
	u32 data32;
	physical_addr_t guest_pa;
	enum vmm_devemu_endianness data_endian;

	/* Determine data endianness */
	if (regs->cpsr & CPSR_BE_ENABLED) {
		data_endian = VMM_DEVEMU_BIG_ENDIAN;
	} else {
		data_endian = VMM_DEVEMU_LITTLE_ENDIAN;
	}

	/* Determine guest physical address */
	va2pa_c_pr(addr);
	guest_pa = read_par64();
	guest_pa &= PAR64_PA_MASK;
	guest_pa |= (addr & 0x00000FFF);

	/* Do guest memory read */
	switch (src_len) {
	case 1:
		data8 = *((u8 *)src);
		rc = vmm_devemu_emulate_write(vcpu, guest_pa,
					      &data8, sizeof(data8),
					      data_endian);
		break;
	case 2:
		data16 = *((u16 *)src);
		rc = vmm_devemu_emulate_write(vcpu, guest_pa,
					      &data16, sizeof(data16),
					      data_endian);
		break;
	case 4:
		data32 = *((u32 *)src);
		rc = vmm_devemu_emulate_write(vcpu, guest_pa,
					      &data32, sizeof(data32),
					      data_endian);
		break;
	default:
		rc = VMM_EFAIL;
		break;
	};

	return rc;
}

int cpu_vcpu_mem_readex(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs,
			virtual_addr_t addr, 
			void *dst, u32 dst_len, 
			bool force_unpriv)
{
	/* Not supported */
	return VMM_EFAIL;
}

int cpu_vcpu_mem_writeex(struct vmm_vcpu *vcpu, 
			arch_regs_t *regs,
			virtual_addr_t addr, 
			void *src, u32 src_len,
			bool force_unpriv)
{
	/* Not supported */
	return VMM_EFAIL;
}

