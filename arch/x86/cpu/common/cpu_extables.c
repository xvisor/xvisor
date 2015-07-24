/**
 * Copyright (c) 2015 Himanshu Chauhan.
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
 * @file cpu_extables.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief VMM exception fixup handler.
 */

#include <cpu_extables.h>

static inline unsigned long
ex_insn_addr(const struct vmm_extable_entry *x)
{
	return (unsigned long)&x->insn + x->insn;
}

static inline unsigned long
ex_fixup_addr(const struct vmm_extable_entry *x)
{
	return (unsigned long)&x->fixup + x->fixup;
}

int fixup_exception(struct arch_regs *regs)
{
	const struct vmm_extable_entry *fixup;
	//unsigned long new_ip;

	fixup = vmm_extable_search(regs->rip);
	if (fixup) {
		//new_ip = ex_fixup_addr(fixup);
		regs->rip = fixup->fixup; //new_ip;

		return 1;
	}

	return 0;
}
