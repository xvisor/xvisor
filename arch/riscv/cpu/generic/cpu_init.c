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
#include <vmm_stdio.h>
#include <arch_cpu.h>
#include <libs/bitmap.h>
#include <generic_mmu.h>

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

int riscv_node_to_hartid(struct vmm_devtree_node *node, u32 *hart_id)
{
	int rc;

	if (!node)
		return VMM_EINVALID;
	if (!vmm_devtree_is_compatible(node, "riscv"))
		return VMM_ENODEV;

	if (hart_id) {
		rc = vmm_devtree_read_u32(node, "reg", hart_id);
		if (rc)
			return rc;
	}

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

	memset(out, 0, out_sz);

	if (xlen == 32)
		vmm_snprintf(out, out_sz, "rv%d", 32);
	else if (xlen == 64)
		vmm_snprintf(out, out_sz, "rv%d", 64);
	else
		return VMM_EINVALID;

	pos = strlen(out);
	valid_isa_len = strlen(valid_isa_order);
	for (i = 0; (i < valid_isa_len) && (pos < (out_sz - 1)); i++) {
		index = valid_isa_order[i] - 'A';
		if (test_bit(index, bmap) && (pos < (out_sz - 1)))
			out[pos++] = 'a' + index;
	}

#define SET_ISA_EXT_MAP(name, bit)					\
	do {								\
		if (test_bit(bit, bmap)) {			\
			strncat(&out[pos], "_" name, out_sz - pos - 1);	\
			pos += strlen("_" name);			\
		}							\
	} while (false)							\

	SET_ISA_EXT_MAP("smaia", RISCV_ISA_EXT_SMAIA);
	SET_ISA_EXT_MAP("ssaia", RISCV_ISA_EXT_SSAIA);
	SET_ISA_EXT_MAP("sstc", RISCV_ISA_EXT_SSTC);
#undef SET_ISA_EXT_MAP

	return VMM_OK;
}

int riscv_isa_parse_string(const char *isa,
			   unsigned long *out_xlen,
			   unsigned long *out_bitmap,
			   size_t out_bitmap_sz)
{
	size_t i, j, isa_len;
	char mstr[RISCV_ISA_EXT_NAME_LEN_MAX];

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

	for (; i < isa_len; i++) {
		if (isa[i] == '_')
			break;

		if ('a' <= isa[i] && isa[i] <= 'z')
			__set_bit(isa[i] - 'a', out_bitmap);
		if ('A' <= isa[i] && isa[i] <= 'Z')
			__set_bit(isa[i] - 'A', out_bitmap);
	}

	while (i < isa_len) {
		if (isa[i] != '_') {
			i++;
			continue;
		}

		/* Skip the '_' character */
		i++;

		/* Extract the multi-letter extension name */
		j = 0;
		while ((i < isa_len) && (isa[i] != '_') &&
		       (j < (sizeof(mstr) - 1)))
			mstr[j++] = isa[i++];
		mstr[j] = '\0';

		/* Skip empty multi-letter extension name */
		if (!j)
			continue;

#define SET_ISA_EXT_MAP(name, bit)					\
		do {							\
			if (!strcmp(mstr, name)) {			\
				__set_bit(bit, out_bitmap);		\
				continue;				\
			}						\
		} while (false)						\

		SET_ISA_EXT_MAP("smaia", RISCV_ISA_EXT_SMAIA);
		SET_ISA_EXT_MAP("ssaia", RISCV_ISA_EXT_SSAIA);
		SET_ISA_EXT_MAP("sstc", RISCV_ISA_EXT_SSTC);
#undef SET_ISA_EXT_MAP
	}

	return VMM_OK;
}

const unsigned long *riscv_isa_extension_host(void)
{
	return riscv_isa;
}

bool __riscv_isa_extension_available(const unsigned long *isa_bitmap, int bit)
{
	const unsigned long *bmap = (isa_bitmap) ? isa_bitmap : riscv_isa;

	if (bit >= RISCV_ISA_EXT_MAX)
		return false;

	return test_bit(bit, bmap) ? true : false;
}

unsigned long riscv_xlen = 0;
#ifdef CONFIG_64BIT
unsigned long riscv_stage2_mode = HGATP_MODE_SV39X4;
#else
unsigned long riscv_stage2_mode = HGATP_MODE_SV32X4;
#endif
unsigned long riscv_stage2_vmid_bits = 0;
unsigned long riscv_stage2_vmid_nested = 0;
bool riscv_stage2_use_vmid = false;
unsigned long riscv_timer_hz = 0;

int __init arch_cpu_nascent_init(void)
{
	DECLARE_BITMAP(this_isa, RISCV_ISA_EXT_MAX);
	struct vmm_devtree_node *dn, *cpus;
	const char *isa, *str;
	unsigned long val, this_xlen;
	int rc = VMM_OK;
	u32 tmp;

	/* Host aspace, Heap, and Device tree available. */

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
			rc = 0;
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

	/* Setup Stage2 mode and Stage2 VMID bits */
	if (riscv_isa_extension_available(NULL, h)) {
		csr_write(CSR_HGATP, HGATP_VMID);
		val = csr_read(CSR_HGATP) & HGATP_VMID;
		riscv_stage2_vmid_bits = fls_long(val >> HGATP_VMID_SHIFT);
		riscv_stage2_vmid_nested = (1UL << riscv_stage2_vmid_bits) / 2;

#ifdef CONFIG_64BIT
		/* Try Sv57x4 MMU mode */
		csr_write(CSR_HGATP, HGATP_VMID |
				     (HGATP_MODE_SV57X4 << HGATP_MODE_SHIFT));
		val = csr_read(CSR_HGATP) >> HGATP_MODE_SHIFT;
		if (val == HGATP_MODE_SV57X4) {
			riscv_stage2_mode = HGATP_MODE_SV57X4;
			goto skip_hgatp_sv48x4_test;
		}

		/* Try Sv48x4 MMU mode */
		csr_write(CSR_HGATP, HGATP_VMID |
				     (HGATP_MODE_SV48X4 << HGATP_MODE_SHIFT));
		val = csr_read(CSR_HGATP) >> HGATP_MODE_SHIFT;
		if (val == HGATP_MODE_SV48X4) {
			riscv_stage2_mode = HGATP_MODE_SV48X4;
		}
skip_hgatp_sv48x4_test:
#endif

		csr_write(CSR_HGATP, 0);
		__hfence_gvma_all();
	}

	return rc;
}

int __init arch_cpu_early_init(void)
{
	int rc;
	const char *options;
	struct vmm_devtree_node *node;

	/*
	 * Host virtual memory, device tree, heap, and host irq available.
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

	rc = sbi_ipi_init();
	if (rc) {
		vmm_printf("%s: SBI IPI init failed (error %d)\n",
			   __func__, rc);
		return rc;
	}

	return VMM_OK;
}


void arch_cpu_print(struct vmm_chardev *cdev, u32 cpu)
{
	/* FIXME: To be implemented. */
}

void arch_cpu_print_summary(struct vmm_chardev *cdev)
{
	char isa[256];
#ifdef CONFIG_64BIT
	riscv_isa_populate_string(64, NULL, isa, sizeof(isa));
#else
	riscv_isa_populate_string(32, NULL, isa, sizeof(isa));
#endif

	vmm_cprintf(cdev, "%-25s: %s\n", "CPU ISA String", isa);
	switch (riscv_stage1_mode) {
	case SATP_MODE_SV32:
		strcpy(isa, "Sv32");
		break;
	case SATP_MODE_SV39:
		strcpy(isa, "Sv39");
		break;
	case SATP_MODE_SV48:
		strcpy(isa, "Sv48");
		break;
	case SATP_MODE_SV57:
		strcpy(isa, "Sv57");
		break;
	default:
		strcpy(isa, "Unknown");
		break;
	};
	vmm_cprintf(cdev, "%-25s: %s\n", "CPU Hypervisor MMU Mode", isa);
	switch (riscv_stage2_mode) {
	case HGATP_MODE_SV32X4:
		strcpy(isa, "Sv32x4");
		break;
	case HGATP_MODE_SV39X4:
		strcpy(isa, "Sv39x4");
		break;
	case HGATP_MODE_SV48X4:
		strcpy(isa, "Sv48x4");
		break;
	case HGATP_MODE_SV57X4:
		strcpy(isa, "Sv57x4");
		break;
	default:
		strcpy(isa, "Unknown");
		break;
	};
	vmm_cprintf(cdev, "%-25s: %s\n", "CPU Stage2 MMU Mode", isa);
	vmm_cprintf(cdev, "%-25s: %ld\n",
		    "CPU Stage2 VMID Bits", riscv_stage2_vmid_bits);
	vmm_cprintf(cdev, "%-25s: %ld Hz\n", "CPU Time Base", riscv_timer_hz);
}

int __init arch_cpu_final_init(void)
{
	/* All VMM API's are available here */
	/* We can register a CPU specific resources here */
	return VMM_OK;
}

void __init cpu_init(void)
{
	/* Initialize VMM (APIs only available after this) */
	vmm_init();

	/* We will never come back here. */
	vmm_hang();
}
