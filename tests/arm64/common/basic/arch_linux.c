/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file arch_linux.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source for arch specific linux booting
 */

#include <arch_cache.h>
#include <arch_linux.h>

typedef void (*linux_entry_t) (unsigned long fdt_addr, unsigned long arg0, unsigned long arg1, unsigned long arg2);

void arch_start_linux_prep(unsigned long kernel_addr,
			   unsigned long fdt_addr,
			   unsigned long initrd_addr,
			   unsigned long initrd_size)
{
	virtual_addr_t nuke_va;

	/* Linux ARM64 kernel expects us to boot from 0x80000
	 * aligned address, perferrably RAM start + 0x80000 address.
	 * The 0x80000 bytes above kernel start address is used by
	 * Linux ARM64 kernel to setup boot page tables.
	 *
	 * It might happen that we are running Basic firmware
	 * after a reboot from Guest Linux in which case both
	 * I-Cache and D-Cache will have stale contents. If we
	 * don't cleanup these stale contents then Linux kernel
	 * will not see correct contents boot page tables after
	 * MMU ON.
	 *
	 * To take care of above described issue, we nuke the
	 * 2MB area containing kernel start and boot page tables.
	 */
	nuke_va = kernel_addr & ~(0x200000 - 1);
	arch_clean_invalidate_dcache_mva_range(nuke_va, nuke_va + 0x200000);
}

void arch_start_linux_jump(unsigned long kernel_addr,
			   unsigned long fdt_addr,
			   unsigned long initrd_addr,
			   unsigned long initrd_size)
{
	/* Jump to Linux Kernel
	 * x0 -> dtb address
	 */
	((linux_entry_t)kernel_addr)(fdt_addr, 0x0, 0x0, 0x0);
}
