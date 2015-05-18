/**
 * Copyright (c) 2010-20 Himanshu Chauhan.
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
 * @file cpu_main.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief C code for cpu functions
 */

#include <vmm_stdio.h>
#include <vmm_error.h>
#include <vmm_main.h>
#include <vmm_params.h>
#include <libs/libfdt.h>
#include <libs/stringlib.h>
#include <multiboot.h>
#include <arch_regs.h>
#include <arch_cpu.h>
#include <arch_devtree.h>
#include <acpi.h>
#include <cpu_features.h>
#include <linux/screen_info.h>
#include <cpu_vm.h>

struct multiboot_info boot_info;
u8 boot_cmd_line[MAX_CMD_LINE];

extern u32 dt_blob_start;
volatile int wait_for_gdb = 0;

int arch_devtree_ram_bank_setup(void)
{
	/* For now nothing to do here. */
	return VMM_OK;
}

int arch_devtree_ram_bank_count(u32 *bank_count)
{
	*bank_count = 1;
	return VMM_OK;
}

int arch_devtree_ram_bank_start(u32 bank, physical_addr_t *addr)
{
#if 0
	int rc = VMM_OK;
	struct fdt_fileinfo fdt;
	struct fdt_node_header *fdt_node;
	physical_addr_t data[2];
	
	rc = libfdt_parse_fileinfo((virtual_addr_t)&dt_blob_start, &fdt);
	if (rc) {
		return rc;
	}

	fdt_node = libfdt_find_node(&fdt, 
				    VMM_DEVTREE_PATH_SEPARATOR_STRING
				    VMM_DEVTREE_MEMORY_NODE_NAME);
	if (!fdt_node) {
		return VMM_EFAIL;
	}

	rc = libfdt_get_property(&fdt, fdt_node,
				 VMM_DEVTREE_REG_ATTR_NAME, data);
	if (rc) {
		return rc;
	}

	*addr = data[0];
#else
	if (bank > 0) {
		return VMM_EINVALID;
	}
	*addr = 0x100000UL;
#endif
	return VMM_OK;
}

int arch_devtree_ram_bank_size(u32 bank, physical_size_t *size)
{
	if (bank > 0) {
		return VMM_EINVALID;
	}
	*size = boot_info.mem_upper * 1024;
	return VMM_OK;
}

int arch_devtree_reserve_count(u32 *count)
{
	*count = 0;
	return VMM_OK;
}

int arch_devtree_reserve_addr(u32 index, physical_addr_t *addr)
{
	*addr = 0x0;
	return VMM_OK;
}

int arch_devtree_reserve_size(u32 index, physical_size_t *size)
{
	*size = 0x0;
	return VMM_OK;
}

int arch_devtree_populate(struct vmm_devtree_node **root)
{
	int rc = VMM_OK;
	struct fdt_fileinfo fdt;

	/* Parse skeletal FDT */
	rc = libfdt_parse_fileinfo((virtual_addr_t) & dt_blob_start, &fdt);
	if (rc) {
		return rc;
	}

	/* Populate skeletal FDT */
	rc = libfdt_parse_devtree(&fdt, root, "\0", NULL);
	if (rc) {
		return rc;
	}

	/* FIXME: Populate device tree from ACPI table */	
#if CONFIG_ACPI
	/*
	 * Initialize the ACPI table to help initialize
	 * other devices.
	 */
	acpi_init();
#endif

	return VMM_OK;
}

int __init arch_cpu_early_init(void)
{
	/*
	 * Host virtual memory, device tree, heap is up.
	 * Do necessary early stuff like iomapping devices
	 * memory or boot time memory reservation here.
	 */

	/* Enable and Initialize the VM specific things in CPU */
	return cpu_enable_vm_extensions(&cpu_info);
}

int __init arch_cpu_final_init(void)
{
        return 0;
}

virtual_addr_t arch_code_vaddr_start(void)
{
	return ((virtual_addr_t) (CPU_TEXT_LMA << 20));
}

physical_addr_t arch_code_paddr_start(void)
{
	return ((physical_addr_t) (CPU_TEXT_LMA << 20));
}

extern u8 _code_end;
u8 __x86_vmm_address(virtual_addr_t addr)
{
	if (addr >= (CPU_TEXT_LMA << 20) && addr <= (virtual_addr_t)&_code_end)
		return 1;

	return 0;
}

extern u8 _code_end;
extern u8 _code_start;

virtual_size_t arch_code_size(void)
{
	return (virtual_size_t)(&_code_end - &_code_start);
}

void arch_cpu_print_info(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "%-25s: %s\n", "CPU Name",
		cpu_info.name_string);
	vmm_cprintf(cdev, "%-25s: %s\n", "CPU Model",
		cpu_info.vendor_string);
	vmm_cprintf(cdev, "%-25s: %u\n", "Family", cpu_info.family);
	vmm_cprintf(cdev, "%-25s: %u\n", "Model", cpu_info.model);
	vmm_cprintf(cdev, "%-25s: %u\n", "Stepping",
		cpu_info.stepping);
	vmm_cprintf(cdev, "%-25s: %u KB\n", "L1 I-Cache Size",
		cpu_info.l1_icache_size);
	vmm_cprintf(cdev, "%-25s: %u KB\n", "L1 D-Cache Size",
		cpu_info.l1_dcache_size);
	vmm_cprintf(cdev, "%-25s: %u KB\n", "L2 Cache Size",
		cpu_info.l2_cache_size);
	vmm_cprintf(cdev, "%-25s: %u KB\n", "L3 Cache Size",
		    cpu_info.l3_cache_size);
	vmm_cprintf(cdev, "%-25s: %s\n", "Hardware Virtualization",
		(cpu_info.hw_virt_available ? "Supported" : "Unsupported"));
}

void __init cpu_init(struct multiboot_info *binfo, char *cmdline)
{
	memcpy(&boot_info, binfo, sizeof(struct multiboot_info));
	memcpy(boot_cmd_line, cmdline, sizeof(boot_cmd_line));

	BUG_ON(!(binfo->flags & MULTIBOOT_INFO_MEMORY));

	vmm_parse_early_options((char *)boot_cmd_line);

	indentify_cpu();

	while (wait_for_gdb);

	/* Initialize VMM (APIs only available after this) */
	vmm_init();

	/* We should never come back here. */
	vmm_hang();
}
