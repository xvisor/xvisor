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

typedef void (*linux_entry_t) (unsigned long zero, unsigned long machine_type, unsigned long kernel_args, unsigned long zero2);

void arch_start_linux_prep(unsigned long kernel_addr,
			   unsigned long fdt_addr,
			   unsigned long initrd_addr,
			   unsigned long initrd_size)
{
	virtual_addr_t nuke_va;

	/* Linux ARM32 kernel expects us to boot from 0x8000
	 * aligned address, perferrably RAM start + 0x8000 address.
	 * The 0x8000 bytes above kernel start address is used by
	 * Linux ARM32 kernel to setup boot page tables.
	 *
	 * It might happen that we are running Basic firmware
	 * after a reboot from Guest Linux in which case both
	 * I-Cache and D-Cache will have stale contents. If we
	 * don't cleanup these stale contents then Linux kernel
	 * will not see correct contents boot page tables after
	 * MMU ON.
	 *
	 * To take care of above described issue, we nuke the
	 * 1MB area containing kernel start and boot page tables.
	 */
	nuke_va = kernel_addr & ~(0x100000 - 1);
	arch_clean_invalidate_dcache_mva_range(nuke_va, nuke_va + 0x100000);
}

void arch_start_linux_jump(unsigned long kernel_addr,
			   unsigned long fdt_addr,
			   unsigned long initrd_addr,
			   unsigned long initrd_size)
{
	/* Jump to Linux Kernel
	 * r2 -> dtb address
	 */
	((linux_entry_t)kernel_addr)(0, -1, fdt_addr, 0);
}
