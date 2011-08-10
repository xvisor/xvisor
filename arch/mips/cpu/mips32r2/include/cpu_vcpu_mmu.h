/*
 * Copyright (c) 2010-2020 Himanshu Chauhan.
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
 * @author: Himanshu Chauhan <hschauhan@nulltrace.org>
 * @brief: VCPU MMU handling function and structures.
 */

#ifndef __CPU_VCPU_MMU_H_
#define __CPU_VCPU_MMU_H_

#include <vmm_types.h>
#include <vmm_regs.h>

#define TBE_PGMSKD_VPN2(_tlb_entry)			\
	({ unsigned int _res;				\
		_res = (_tlb_entry)->entryhi.		\
			_s_entryhi.vpn2			\
			& _tlb_entry->page_mask;	\
		_res;					\
	})

#define TBE_ASID(_tlb_entry)			\
	({ unsigned int _res;			\
		_res = _tlb_entry->entryhi.	\
			_s_entryhi.asid;	\
			_res;			\
	})

#define TBE_ELO_GLOBAL(_tlb_entry, _ELOT)	\
	({ unsigned int _res;			\
		_res = _tlb_entry->_ELOT.	\
			_s_entrylo.global;	\
		_res;				\
	})

#define TBE_ELO_VALID(_tlb_entry, _ELOT)	\
	({ unsigned int _res;			\
		_res = _tlb_entry->_ELOT.	\
			_s_entrylo.valid;	\
		_res;				\
	})

#define TBE_ELO_INVALIDATE(_tlb_entry, _ELOT)	(_tlb_entry-> \
						 _ELOT._s_entrylo.valid = 0)


int do_vcpu_tlbmiss(vmm_user_regs_t *uregs);
u32 mips_probe_vcpu_tlb(vmm_vcpu_t *vcpu, vmm_user_regs_t *uregs);
u32 mips_read_vcpu_tlb(vmm_vcpu_t *vcpu, vmm_user_regs_t *uregs);
u32 mips_write_vcpu_tlbi(vmm_vcpu_t *vcpu, vmm_user_regs_t *uregs);
u32 mips_write_vcpu_tlbr(vmm_vcpu_t *vcpu, vmm_user_regs_t *uregs);

#endif /* __CPU_VCPU_MMU_H_ */
