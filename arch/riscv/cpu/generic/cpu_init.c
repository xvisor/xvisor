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
 * @file cpu_init.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief intialization functions for CPU
 */

#include <vmm_error.h>
#include <vmm_main.h>
#include <vmm_params.h>
#include <vmm_devtree.h>
#include <arch_cpu.h>
#include <libs/bitmap.h>

#include <cpu_hwcap.h>
#include <cpu_tlb.h>
#include <riscv_csr.h>
#include <riscv_encoding.h>

extern u8 _code_start;
extern u8 _code_end;
extern physical_addr_t _load_start;
extern physical_addr_t _load_end;

virtual_addr_t arch_code_vaddr_start(void)
{
	return (virtual_addr_t) &_code_start;
}

physical_addr_t arch_code_paddr_start(void)
{
	return (physical_addr_t) _load_start;
}

virtual_size_t arch_code_size(void)
{
	return (virtual_size_t) (&_code_end - &_code_start);
}

int __init arch_cpu_early_init(void)
{
	const char *options;
	struct vmm_devtree_node *node;

	/*
	 * Host virtual memory, device tree, heap is up.
	 * Do necessary early stuff like iomapping devices
	 * memory or boot time memory reservation here.
	 */

	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_CHOSEN_NODE_NAME);
	if (!node) {
		return VMM_ENODEV;
	}

	if (vmm_devtree_read_string(node,
		VMM_DEVTREE_BOOTARGS_ATTR_NAME, &options) == VMM_OK) {
		vmm_parse_early_options(options);
	}

	vmm_devtree_dref_node(node);

	return VMM_OK;
}

/* Host ISA bitmap */
static DECLARE_BITMAP(riscv_isa, RISCV_ISA_EXT_MAX) = { 0 };

unsigned long riscv_isa_extension_base(const unsigned long *isa_bitmap)
{
	if (!isa_bitmap)
		return riscv_isa[0];
	return isa_bitmap[0];
}

bool __riscv_isa_extension_available(const unsigned long *isa_bitmap, int bit)
{
	const unsigned long *bmap = (isa_bitmap) ? isa_bitmap : riscv_isa;

	if (bit >= RISCV_ISA_EXT_MAX)
		return false;

	return test_bit(bit, bmap) ? true : false;
}

unsigned long riscv_vmid_bits = 0;
unsigned long riscv_timer_hz = 0;

void arch_cpu_print(struct vmm_chardev *cdev, u32 cpu)
{
	/* FIXME: To be implemented. */
}

void arch_cpu_print_summary(struct vmm_chardev *cdev)
{
	int i, pos;
	char isa[128];

#ifdef CONFIG_64BIT
	vmm_snprintf(isa, sizeof(isa), "rv%d", 64);
#else
	vmm_snprintf(isa, sizeof(isa), "rv%d", 32);
#endif
	pos = strlen(isa);
	for (i = 0; i < BITS_PER_LONG; i++)
		if (riscv_isa[0] & BIT_MASK(i))
			isa[pos++] = 'a' + i;
	isa[pos] = '\0';

	vmm_cprintf(cdev, "%-25s: %s\n", "CPU ISA String", isa);
	vmm_cprintf(cdev, "%-25s: %ld\n", "CPU VMID Bits", riscv_vmid_bits);
	vmm_cprintf(cdev, "%-25s: %ld Hz\n", "CPU Time Base", riscv_timer_hz);
}

int __init arch_cpu_final_init(void)
{
	/* All VMM API's are available here */
	/* We can register a CPU specific resources here */
	return VMM_OK;
}

int __init cpu_parse_devtree_hwcap(void)
{
	struct vmm_devtree_node *dn, *cpus;
	const char *isa, *str;
	unsigned long val;
	int rc = VMM_OK;
	size_t i, isa_len;
	u32 tmp;

	cpus = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING "cpus");
	if (!cpus) {
		vmm_printf("%s: Failed to find cpus node\n",
			   __func__);
		return VMM_ENOTAVAIL;
	}

	rc = vmm_devtree_read_u32(cpus, "timebase-frequency", &tmp);
	if (rc) {
		vmm_devtree_dref_node(cpus);
		vmm_printf("%s: Failed to read timebase-frequency from "
			   "cpus node\n", __func__);
		return rc;
	}
	riscv_timer_hz = tmp;

	dn = NULL;
	vmm_devtree_for_each_child(dn, cpus) {
		unsigned long this_isa = 0;

		str = NULL;
		rc = vmm_devtree_read_string(dn,
				VMM_DEVTREE_DEVICE_TYPE_ATTR_NAME, &str);
		if (rc || !str) {
			continue;
		}
		if (strcmp(str, VMM_DEVTREE_DEVICE_TYPE_VAL_CPU)) {
			continue;
		}

		isa = NULL;
		rc = vmm_devtree_read_string(dn,
				"riscv,isa", &isa);
		if (rc || !isa) {
			vmm_devtree_dref_node(dn);
			rc = VMM_ENOTAVAIL;
			break;
		}
		i = 0;
		isa_len = strlen(isa);

		if (isa[i] == 'r' || isa[i] == 'R')
			i++;

		if (isa[i] == 'v' || isa[i] == 'V')
			i++;

		if (isa[i] == '3' || isa[i+1] == '2')
			i += 2;

		if (isa[i] == '6' || isa[i+1] == '4')
			i += 2;

		for (; i < isa_len; ++i) {
			if ('a' <= isa[i] && isa[i] <= 'z')
				this_isa |= (1UL << (isa[i] - 'a'));
			if ('A' <= isa[i] && isa[i] <= 'Z')
				this_isa |= (1UL << (isa[i] - 'A'));
		}
		if (riscv_isa[0])
			riscv_isa[0] &= this_isa;
		else
			riscv_isa[0] = this_isa;
		/*
		 * TODO: What should be done if a single hart doesn't have
		 * hyp enabled. Keep a mask and not let guests boot on those.
		 */
	}
	vmm_devtree_dref_node(cpus);

	/* Setup riscv_vmid_bits */
	if (riscv_isa_extension_available(NULL, h)) {
		csr_write(CSR_HGATP, HGATP_VMID_MASK);
		val = csr_read(CSR_HGATP);
		csr_write(CSR_HGATP, 0);
		__hfence_gvma_all();
		riscv_vmid_bits = fls_long(val >> HGATP_VMID_SHIFT);
	}

	return rc;
}

void __init cpu_init(void)
{
	/* Initialize VMM (APIs only available after this) */
	vmm_init();

	/* We will never come back here. */
	vmm_hang();
}
