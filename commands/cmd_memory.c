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

static u32 crc32_tab[] = {
	0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
	0xe963a535, 0x9e6495a3,	0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
	0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
	0xf3b97148, 0x84be41de,	0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
	0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,	0x14015c4f, 0x63066cd9,
	0xfa0f3d63, 0x8d080df5,	0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
	0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,	0x35b5a8fa, 0x42b2986c,
	0xdbbbc9d6, 0xacbcf940,	0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
	0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
	0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
	0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,	0x76dc4190, 0x01db7106,
	0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
	0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
	0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
	0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
	0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
	0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
	0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
	0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
	0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
	0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
	0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
	0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
	0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
	0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
	0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
	0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
	0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
	0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
	0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
	0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
	0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
	0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
	0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
	0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
	0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
	0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
	0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
	0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
	0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
	0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
	0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
	0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

void cmd_memory_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage: ");
	vmm_cprintf(cdev, "   memory help\n");
	vmm_cprintf(cdev, "   memory dump8    <phys_addr> <count>\n");
	vmm_cprintf(cdev, "   memory dump16   <phys_addr> <count>\n");
	vmm_cprintf(cdev, "   memory dump32   <phys_addr> <count>\n");
	vmm_cprintf(cdev, "   memory crc32    <phys_addr> <count>\n");
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

int cmd_memory_crc32(struct vmm_chardev *cdev, physical_addr_t addr, u32 wcnt)
{
	int rc;
	u32 w, crc = 0;
	bool page_mapped;
	virtual_addr_t page_va, addr_offset;
	physical_addr_t page_pa;

	crc = crc ^ ~0U;

	if (sizeof(physical_addr_t) == sizeof(u64)) {
		vmm_cprintf(cdev, "CRC32 for 0x%016llx - 0x%016llx:\n",
				  (u64)addr, (u64)(addr + wcnt));
	} else {
		vmm_cprintf(cdev, "CRC32 for 0x%08x - 0x%08x:\n",
				  (u32)addr, (u32)(addr + wcnt));
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
		addr_offset = (addr & VMM_PAGE_MASK);
		crc = crc32_tab[(crc ^ *((u8 *)(page_va + addr_offset))) & 0xFF] ^ (crc >> 8);	
		addr += 1;
		w++;
	}
	crc = crc ^ ~0U;
	vmm_cprintf(cdev, "%08x\n", crc);
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
						(u8)strtoul(valv[w], NULL, 0);
			break;
		case 2:
			*((u16 *)(page_va + addr_offset)) = 
						(u16)strtoul(valv[w], NULL, 0);
			break;
		case 4:
			*((u32 *)(page_va + addr_offset)) = 
						(u32)strtoul(valv[w], NULL, 0);
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
	addr = (physical_addr_t)strtoull(argv[2], NULL, 0);
	if (strcmp(argv[1], "dump8") == 0) {
		tmp = strtoull(argv[3], NULL, 0);
		return cmd_memory_dump(cdev, addr, 1, (u32)tmp);
	} else if (strcmp(argv[1], "dump16") == 0) {
		tmp = strtoull(argv[3], NULL, 0);
		return cmd_memory_dump(cdev, addr, 2, (u32)tmp);
	} else if (strcmp(argv[1], "dump32") == 0) {
		tmp = strtoull(argv[3], NULL, 0);
		return cmd_memory_dump(cdev, addr, 4, (u32)tmp);
	} else if (strcmp(argv[1], "crc32") == 0) {
		tmp = strtoull(argv[3], NULL, 0);
		return cmd_memory_crc32(cdev, addr, (u32)tmp);
	} else if (strcmp(argv[1], "modify8") == 0) {
		return cmd_memory_modify(cdev, addr, 1, argc - 3, &argv[3]);
	} else if (strcmp(argv[1], "modify16") == 0) {
		return cmd_memory_modify(cdev, addr, 2, argc - 3, &argv[3]);
	} else if (strcmp(argv[1], "modify32") == 0) {
		return cmd_memory_modify(cdev, addr, 4, argc - 3, &argv[3]);
	} else if (strcmp(argv[1], "copy") == 0 && argc > 4) {
		src_addr = (physical_addr_t)strtoull(argv[3], NULL, 0);
		tmp = strtoul(argv[4], NULL, 0);
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
