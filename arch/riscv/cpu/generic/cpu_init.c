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
#include <cpu_sbi.h>
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

int riscv_isa_populate_string(unsigned long xlen,
			      const unsigned long *isa_bitmap,
			      char *out, size_t out_sz)
{
	size_t i, pos, index, valid_isa_len;
	const char *valid_isa_order = "IEMAFDQCLBJTPVNSUHKORWXYZG";
	const unsigned long *bmap = (isa_bitmap) ? isa_bitmap : riscv_isa;

	if (!out || (out_sz < 16))
		return VMM_EINVALID;

	if (xlen == 32)
		vmm_snprintf(out, sizeof(out_sz), "rv%d", 32);
	else if (xlen == 64)
		vmm_snprintf(out, sizeof(out_sz), "rv%d", 64);
	else
		return VMM_EINVALID;

	pos = strlen(out);
	valid_isa_len = strlen(valid_isa_order);
	for (i = 0; i < valid_isa_len; i++) {
		index = valid_isa_order[i] - 'A';
		if ((bmap[0] & BIT_MASK(index)) && (pos < (out_sz - 1)))
			out[pos++] = 'a' + index;
	}
	out[pos] = '\0';

	return VMM_OK;
}

int riscv_isa_parse_string(const char *isa,
			   unsigned long *out_xlen,
			   unsigned long *out_bitmap,
			   size_t out_bitmap_sz)
{
	size_t i, isa_len;

	if (!isa || !out_xlen || !out_bitmap ||
	    (out_bitmap_sz < __riscv_xlen))
		return VMM_EINVALID;

	*out_xlen = 0;
	bitmap_zero(out_bitmap, out_bitmap_sz);

	i = 0;
	isa_len = strlen(isa);

	if (isa[i] == 'r' || isa[i] == 'R')
		i++;
	else
		return VMM_EINVALID;

	if (isa[i] == 'v' || isa[i] == 'V')
		i++;
	else
		return VMM_EINVALID;

	if (isa[i] == '3' || isa[i+1] == '2') {
		*out_xlen = 32;
		i += 2;
	} else if (isa[i] == '6' || isa[i+1] == '4') {
		*out_xlen = 64;
		i += 2;
	} else {
		return VMM_EINVALID;
	}

	for (; i < isa_len; ++i) {
		if ('a' <= isa[i] && isa[i] <= 'z')
			__set_bit(isa[i] - 'a', out_bitmap);
		if ('A' <= isa[i] && isa[i] <= 'Z')
			__set_bit(isa[i] - 'A', out_bitmap);
	}

	return VMM_OK;
}

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

unsigned long riscv_xlen = 0;
unsigned long riscv_vmid_bits = 0;
unsigned long riscv_timer_hz = 0;

void arch_cpu_print(struct vmm_chardev *cdev, u32 cpu)
{
	/* FIXME: To be implemented. */
}

void arch_cpu_print_summary(struct vmm_chardev *cdev)
{
	char isa[128];

#ifdef CONFIG_64BIT
	riscv_isa_populate_string(64, NULL, isa, sizeof(isa));
#else
	riscv_isa_populate_string(32, NULL, isa, sizeof(isa));
#endif

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
	DECLARE_BITMAP(this_isa, RISCV_ISA_EXT_MAX);
	struct vmm_devtree_node *dn, *cpus;
	const char *isa, *str;
	unsigned long val, this_xlen;
	int rc = VMM_OK;
	u32 tmp;

	rc = sbi_init();
	if (rc) {
		vmm_printf("%s: SBI init failed (error %d)\n",
			   __func__, rc);
		return rc;
	}

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
		this_xlen = 0;
		bitmap_zero(this_isa, RISCV_ISA_EXT_MAX);

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
		rc = vmm_devtree_read_string(dn, "riscv,isa", &isa);
		if (rc || !isa) {
			vmm_devtree_dref_node(dn);
			rc = VMM_ENOTAVAIL;
			break;
		}

		rc = riscv_isa_parse_string(isa, &this_xlen, this_isa,
					    RISCV_ISA_EXT_MAX);
		if (rc) {
			vmm_devtree_dref_node(dn);
			break;
		}

		if (riscv_xlen) {
			if (riscv_xlen != this_xlen ||
			    riscv_xlen != __riscv_xlen) {
				vmm_devtree_dref_node(dn);
				rc = VMM_EINVALID;
				break;
			}
			bitmap_and(riscv_isa, riscv_isa, this_isa,
				   RISCV_ISA_EXT_MAX);
		} else {
			riscv_xlen = this_xlen;
			bitmap_copy(riscv_isa, this_isa, RISCV_ISA_EXT_MAX);
		}

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
