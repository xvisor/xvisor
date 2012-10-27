/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file cmd_memory.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of memory command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"Command memory"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_memory_init
#define	MODULE_EXIT			cmd_memory_exit

void cmd_memory_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage: ");
	vmm_cprintf(cdev, "   memory help\n");
	vmm_cprintf(cdev, "   memory dump8    <phys_addr> <count>\n");
	vmm_cprintf(cdev, "   memory dump16   <phys_addr> <count>\n");
	vmm_cprintf(cdev, "   memory dump32   <phys_addr> <count>\n");
	vmm_cprintf(cdev, "   memory modify8  <phys_addr> "
						"<val0> <val1> ...\n");
	vmm_cprintf(cdev, "   memory modify16 <phys_addr> "
						"<val0> <val1> ...\n");
	vmm_cprintf(cdev, "   memory modify32 <phys_addr> "
						"<val0> <val1> ...\n");
	vmm_cprintf(cdev, "   memory copy     <phys_addr> <src_phys_addr> "
						"<byte_count>\n");
}

int cmd_memory_dump(struct vmm_chardev *cdev, physical_addr_t addr, 
		    u32 wsz, u32 wcnt)
{
	int rc;
	u32 w;
	bool page_mapped;
	virtual_addr_t page_va, addr_offset;
	physical_addr_t page_pa;
	addr = addr - (addr & (wsz - 1));
	if (sizeof(physical_addr_t) == sizeof(u64)) {
		vmm_cprintf(cdev, "Host physical memory "
				  "0x%016llx - 0x%016llx:",
				  (u64)addr, (u64)(addr + wsz*wcnt));
	} else {
		vmm_cprintf(cdev, "Host physical memory "
				  "0x%08x - 0x%08x:",
				  (u32)addr, (u32)(addr + wsz*wcnt));
	}
	w = 0;
	page_pa = addr - (addr & VMM_PAGE_MASK);
	page_va = vmm_host_iomap(page_pa, VMM_PAGE_SIZE);
	page_mapped = TRUE;
	while (w < wcnt) {
		if (page_pa != (addr - (addr & VMM_PAGE_MASK))) {
			if (page_mapped) {
				rc = vmm_host_iounmap(page_va, VMM_PAGE_SIZE);
				if (rc) {
					vmm_cprintf(cdev, 
					"Error: Failed to unmap memory.\n");
					return rc;
				}
				page_mapped = FALSE;
			}
			page_pa = addr - (addr & VMM_PAGE_MASK);
			page_va = vmm_host_iomap(page_pa, VMM_PAGE_SIZE);
			page_mapped = TRUE;
		}
		if (!(w * wsz & 0x0000000F)) {
			if (sizeof(physical_addr_t) == sizeof(u64)) {
				vmm_cprintf(cdev, "\n%016llx:", addr);
			} else {
				vmm_cprintf(cdev, "\n%08x:", addr);
			}
		}
		addr_offset = (addr & VMM_PAGE_MASK);
		switch (wsz) {
		case 1:
			vmm_cprintf(cdev, " %02x", 
				    *((u8 *)(page_va + addr_offset)));
			break;
		case 2:
			vmm_cprintf(cdev, " %04x", 
				    *((u16 *)(page_va + addr_offset)));
			break;
		case 4:
			vmm_cprintf(cdev, " %08x", 
				    *((u32 *)(page_va + addr_offset)));
			break;
		default:
			break;
		};
		addr += wsz;
		w++;
	}
	vmm_cprintf(cdev, "\n");
	if (page_mapped) {
		rc = vmm_host_iounmap(page_va, VMM_PAGE_SIZE);
		if (rc) {
			vmm_cprintf(cdev, "Error: Failed to unmap memory.\n");
			return rc;
		}
		page_mapped = FALSE;
	}
	return VMM_OK;
}

int cmd_memory_modify(struct vmm_chardev *cdev, physical_addr_t addr, 
		      u32 wsz, int valc, char **valv)
{
	int rc, w = 0;
	bool page_mapped;
	virtual_addr_t page_va, addr_offset;
	physical_addr_t page_pa;
	addr = addr - (addr & (wsz - 1));
	page_pa = addr - (addr & VMM_PAGE_MASK);
	page_va = vmm_host_iomap(page_pa, VMM_PAGE_SIZE);
	page_mapped = TRUE;
	while (w < valc) {
		if (page_pa != (addr - (addr & VMM_PAGE_MASK))) {
			if (page_mapped) {
				rc = vmm_host_iounmap(page_va, VMM_PAGE_SIZE);
				if (rc) {
					vmm_cprintf(cdev, 
					"Error: Failed to unmap memory.\n");
					return rc;
				}
				page_mapped = FALSE;
			}
			page_pa = addr - (addr & VMM_PAGE_MASK);
			page_va = vmm_host_iomap(page_pa, VMM_PAGE_SIZE);
			page_mapped = TRUE;
		}
		addr_offset = (addr & VMM_PAGE_MASK);
		switch (wsz) {
		case 1:
			*((u8 *)(page_va + addr_offset)) = 
						(u8)str2uint(valv[w], 10);
			break;
		case 2:
			*((u16 *)(page_va + addr_offset)) = 
						(u16)str2uint(valv[w], 10);
			break;
		case 4:
			*((u32 *)(page_va + addr_offset)) = 
						(u32)str2uint(valv[w], 10);
			break;
		default:
			break;
		};
		addr += wsz;
		w++;
	}
	if (page_mapped) {
		rc = vmm_host_iounmap(page_va, VMM_PAGE_SIZE);
		if (rc) {
			vmm_cprintf(cdev, "Error: Failed to unmap memory.\n");
			return rc;
		}
		page_mapped = FALSE;
	}
	return VMM_OK;
}

int cmd_memory_copy(struct vmm_chardev *cdev, physical_addr_t daddr, 
		    physical_addr_t saddr, u32 bcnt)
{
	int rc;
	u32 b = 0, b2copy;
	bool dpage_mapped, spage_mapped;
	virtual_addr_t dva, dpage_va, sva, spage_va;
	physical_addr_t dpage_pa, spage_pa;
	dpage_pa = daddr - (daddr & VMM_PAGE_MASK);
	dpage_va = vmm_host_iomap(dpage_pa, VMM_PAGE_SIZE);
	dpage_mapped = TRUE;
	spage_pa = saddr - (saddr & VMM_PAGE_MASK);
	spage_va = vmm_host_iomap(spage_pa, VMM_PAGE_SIZE);
	spage_mapped = TRUE;
	while (b < bcnt) {
		if (dpage_pa != (daddr - (daddr & VMM_PAGE_MASK))) {
			if (dpage_mapped) {
				rc = vmm_host_iounmap(dpage_va, VMM_PAGE_SIZE);
				if (rc) {
					vmm_cprintf(cdev, 
					"Error: Failed to unmap memory.\n");
					return rc;
				}
				dpage_mapped = FALSE;
			}
			dpage_pa = daddr - (daddr & VMM_PAGE_MASK);
			dpage_va = vmm_host_iomap(dpage_pa, VMM_PAGE_SIZE);
			dpage_mapped = TRUE;
		}
		dva = dpage_va + (virtual_addr_t)(daddr & VMM_PAGE_MASK);
		if (spage_pa != (saddr - (saddr & VMM_PAGE_MASK))) {
			if (spage_mapped) {
				rc = vmm_host_iounmap(spage_va, VMM_PAGE_SIZE);
				if (rc) {
					vmm_cprintf(cdev, 
					"Error: Failed to unmap memory.\n");
					return rc;
				}
				spage_mapped = FALSE;
			}
			spage_pa = saddr - (saddr & VMM_PAGE_MASK);
			spage_va = vmm_host_iomap(spage_pa, VMM_PAGE_SIZE);
			spage_mapped = TRUE;
		}
		sva = spage_va + (virtual_addr_t)(saddr & VMM_PAGE_MASK);
		if ((daddr & VMM_PAGE_MASK) < (saddr & VMM_PAGE_MASK)) {
			b2copy = VMM_PAGE_SIZE - (u32)(saddr & VMM_PAGE_MASK);
		} else {
			b2copy = VMM_PAGE_SIZE - (u32)(daddr & VMM_PAGE_MASK);
		}
		b2copy = ((bcnt - b) < b2copy) ? (bcnt - b) : b2copy;
		memcpy((void *)dva, (void *)sva, b2copy);
		b += b2copy;
		daddr += b2copy;
		saddr += b2copy;
	}
	vmm_cprintf(cdev, "Copied %d (0x%x) bytes.\n", b, b);
	if (dpage_mapped) {
		rc = vmm_host_iounmap(dpage_va, VMM_PAGE_SIZE);
		if (rc) {
			vmm_cprintf(cdev, "Error: Failed to unmap memory.\n");
			return rc;
		}
		dpage_mapped = FALSE;
	}
	if (spage_mapped) {
		rc = vmm_host_iounmap(spage_va, VMM_PAGE_SIZE);
		if (rc) {
			vmm_cprintf(cdev, "Error: Failed to unmap memory.\n");
			return rc;
		}
		spage_mapped = FALSE;
	}
	return VMM_OK;
}

int cmd_memory_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	u32 tmp;
	physical_addr_t addr, src_addr;
	if (argc < 2) {
		cmd_memory_usage(cdev);
		return VMM_EFAIL;
	} else {
		if (argc == 2) {
			if (strcmp(argv[1], "help") == 0) {
				cmd_memory_usage(cdev);
				return VMM_OK;
			} else {
				cmd_memory_usage(cdev);
				return VMM_EFAIL;
			}
		} else if (argc < 4) {
			cmd_memory_usage(cdev);
			return VMM_EFAIL;
		}
	}
	addr = (physical_addr_t)str2ulonglong(argv[2], 10);
	if (strcmp(argv[1], "dump8") == 0) {
		tmp = str2ulonglong(argv[3], 10);
		return cmd_memory_dump(cdev, addr, 1, (u32)tmp);
	} else if (strcmp(argv[1], "dump16") == 0) {
		tmp = str2ulonglong(argv[3], 10);
		return cmd_memory_dump(cdev, addr, 2, (u32)tmp);
	} else if (strcmp(argv[1], "dump32") == 0) {
		tmp = str2ulonglong(argv[3], 10);
		return cmd_memory_dump(cdev, addr, 4, (u32)tmp);
	} else if (strcmp(argv[1], "modify8") == 0) {
		return cmd_memory_modify(cdev, addr, 1, argc - 3, &argv[3]);
	} else if (strcmp(argv[1], "modify16") == 0) {
		return cmd_memory_modify(cdev, addr, 2, argc - 3, &argv[3]);
	} else if (strcmp(argv[1], "modify32") == 0) {
		return cmd_memory_modify(cdev, addr, 4, argc - 3, &argv[3]);
	} else if (strcmp(argv[1], "copy") == 0 && argc > 4) {
		src_addr = (physical_addr_t)str2ulonglong(argv[3], 10);
		tmp = str2uint(argv[4], 10);
		return cmd_memory_copy(cdev, addr, src_addr, tmp);
	}
	cmd_memory_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_memory = {
	.name = "memory",
	.desc = "memory manipulation commands",
	.usage = cmd_memory_usage,
	.exec = cmd_memory_exec,
};

static int __init cmd_memory_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_memory);
}

static void __exit cmd_memory_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_memory);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
