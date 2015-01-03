/**
 * Copyright (c) 2014 Himanshu Chauhan.
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
 * @file init.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 *
 * All the work under arch/x86/board/common/pci/ is a derived work from Linux's
 * PCI Framework. The following is the commit ID from which it has
 * been derived.
 *
 * commit 97bf6af1f928216fd6c5a66e8a57bfa95a659672
 * Linux 3.19-rc1
 */
#include <linux/pci.h>
#include <linux/init.h>
#include <asm/pci_x86.h>
#include <asm/x86_init.h>

physical_addr_t pci_mem_start = 0xdeadbeeffadedeedull;

/* arch_initcall has too random ordering, so call the initializers
   in the right sequence from here. */
int __init pci_arch_init(void)
{
#ifdef CONFIG_PCI_DIRECT
	int type = 0;

	type = pci_direct_probe();
#endif

	if (!(pci_probe & PCI_PROBE_NOEARLY))
		pci_mmcfg_early_init();

#if 0
	if (x86_init.pci.arch_init && !x86_init.pci.arch_init())
		return 0;
#endif

#ifdef CONFIG_PCI_BIOS
	pci_pcbios_init();
#endif
	/*
	 * don't check for raw_pci_ops here because we want pcbios as last
	 * fallback, yet it's needed to run first to set pcibios_last_bus
	 * in case legacy PCI probing is used. otherwise detecting peer busses
	 * fails.
	 */
#ifdef CONFIG_PCI_DIRECT
	pci_direct_init(type);
#endif
	if (!raw_pci_ops && !raw_pci_ext_ops)
		printk(KERN_ERR
		"PCI: Fatal: No config space access function found\n");

	dmi_check_pciprobe();

	dmi_check_skip_isa_align();

	return 0;
}
