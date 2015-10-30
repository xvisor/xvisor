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
 * @file cmd_host.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of host command
 */

#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_cpumask.h>
#include <vmm_resource.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_host_irq.h>
#include <vmm_host_irqext.h>
#include <vmm_host_ram.h>
#include <vmm_host_vapool.h>
#include <vmm_host_aspace.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <vmm_delay.h>
#include <vmm_scheduler.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <arch_board.h>
#include <arch_cpu.h>

#define MODULE_DESC			"Command host"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_host_init
#define	MODULE_EXIT			cmd_host_exit

static void cmd_host_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   host help\n");
	vmm_cprintf(cdev, "   host info\n");
	vmm_cprintf(cdev, "   host cpu info\n");
	vmm_cprintf(cdev, "   host cpu stats\n");
	vmm_cprintf(cdev, "   host irq stats\n");
	vmm_cprintf(cdev, "   host irq set_affinity <hirq> <hcpu>\n");
	vmm_cprintf(cdev, "   host extirq stats\n");
	vmm_cprintf(cdev, "   host ram info\n");
	vmm_cprintf(cdev, "   host ram bitmap [<column count>]\n");
	vmm_cprintf(cdev, "   host vapool info\n");
	vmm_cprintf(cdev, "   host vapool state\n");
	vmm_cprintf(cdev, "   host vapool bitmap [<column count>]\n");
	vmm_cprintf(cdev, "   host resources\n");
	vmm_cprintf(cdev, "   host bus_list\n");
	vmm_cprintf(cdev, "   host bus_device_list <bus_name>\n");
	vmm_cprintf(cdev, "   host class_list\n");
	vmm_cprintf(cdev, "   host class_device_list <class_name>\n");
}

static void cmd_host_info(struct vmm_chardev *cdev)
{
	const char *attr;
	struct vmm_devtree_node *node;
	u32 total = vmm_host_ram_total_frame_count();

	attr = NULL;
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING);
	if (node) {
		vmm_devtree_read_string(node,
					VMM_DEVTREE_MODEL_ATTR_NAME, &attr);
		vmm_devtree_dref_node(node);
	}
	if (attr) {
		vmm_cprintf(cdev, "%-20s: %s\n", "Host Name", attr);
	} else {
		vmm_cprintf(cdev, "%-20s: %s\n", "Host Name", CONFIG_BOARD);
	}

	vmm_cprintf(cdev, "%-20s: %u\n", "Boot CPU",
		    vmm_smp_bootcpu_id());
	vmm_cprintf(cdev, "%-20s: %u\n", "Total Online CPUs",
		    vmm_num_online_cpus());
	vmm_cprintf(cdev, "%-20s: %u MB\n", "Total VAPOOL",
		    CONFIG_VAPOOL_SIZE_MB);
	vmm_cprintf(cdev, "%-20s: %u MB\n", "Total RAM",
		    ((total *VMM_PAGE_SIZE) >> 20));

	arch_board_print_info(cdev);
}

static void cmd_host_cpu_info(struct vmm_chardev *cdev)
{
	u32 c, khz;
	char name[25];

	vmm_cprintf(cdev, "%-25s: %s\n", "CPU Type", CONFIG_CPU);
	vmm_cprintf(cdev, "%-25s: %d\n",
			  "CPU Present Count", vmm_num_present_cpus());
	vmm_cprintf(cdev, "%-25s: %d\n",
			  "CPU Possible Count", vmm_num_possible_cpus());
	vmm_cprintf(cdev, "%-25s: %u\n",
			  "CPU Online Count", vmm_num_online_cpus());
	for_each_online_cpu(c) {
		vmm_sprintf(name, "CPU%d Estimated Speed", c);
		khz = vmm_delay_estimate_cpu_khz(c);
		vmm_cprintf(cdev, "%-25s: %d.%03d MHz\n",
			    name, udiv32(khz, 1000), umod32(khz, 1000));
	}

	vmm_cprintf(cdev, "\n");

	arch_cpu_print_info(cdev);
}

static void cmd_host_cpu_stats(struct vmm_chardev *cdev)
{
	u32 c, p, khz, util;

	vmm_cprintf(cdev, "----------------------------------------"
			  "-------------------------\n");
	vmm_cprintf(cdev, " %4s %15s %13s %12s %16s\n",
			  "CPU#", "Speed (MHz)", "Util. (%)",
			  "IRQs (%)", "Active VCPUs");
	vmm_cprintf(cdev, "----------------------------------------"
			  "-------------------------\n");

	for_each_online_cpu(c) {
		vmm_cprintf(cdev, " %4d", c);

		khz = vmm_delay_estimate_cpu_khz(c);
		vmm_cprintf(cdev, " %11d.%03d",
			    udiv32(khz, 1000), umod32(khz, 1000));

		util = udiv64(vmm_scheduler_idle_time(c) * 1000,
			      vmm_scheduler_get_sample_period(c));
		util = (util > 1000) ? 1000 : util;
		util = 1000 - util;
		vmm_cprintf(cdev, " %11d.%01d",
			    udiv32(util, 10), umod32(util, 10));

		util = udiv64(vmm_scheduler_irq_time(c) * 1000,
			      vmm_scheduler_get_sample_period(c));
		util = (util > 1000) ? 1000 : util;
		vmm_cprintf(cdev, " %10d.%01d",
			    udiv32(util, 10), umod32(util, 10));

		util = 1;
		for (p = VMM_VCPU_MIN_PRIORITY;
		     p <= VMM_VCPU_MAX_PRIORITY; p++) {
			util += vmm_scheduler_ready_count(c, p);
		}
		vmm_cprintf(cdev, " %15d ", util);

		vmm_cprintf(cdev, "\n");
	}

	vmm_cprintf(cdev, "----------------------------------------"
			  "-------------------------\n");
}

static void cmd_host_irq_stats(struct vmm_chardev *cdev)
{
	const char *irq_name;
	u32 num, cpu, stats, count = vmm_host_irq_count();
	struct vmm_host_irq *irq;
	struct vmm_host_irq_chip *chip;

	vmm_cprintf(cdev, "----------------------------------------");
	for_each_online_cpu(cpu) {
		vmm_cprintf(cdev, "------------");
	}
	vmm_cprintf(cdev, "\n");
	vmm_cprintf(cdev, " %-5s %-20s %-13s",
			  "IRQ#", "Name", "Chip");
	for_each_online_cpu(cpu) {
		vmm_cprintf(cdev, " CPU%-7d", cpu);
	}
	vmm_cprintf(cdev, "\n");
	vmm_cprintf(cdev, "----------------------------------------");
	for_each_online_cpu(cpu) {
		vmm_cprintf(cdev, "------------");
	}
	vmm_cprintf(cdev, "\n");
	for (num = 0; num < count; num++) {
		irq = vmm_host_irq_get(num);
		irq_name = vmm_host_irq_get_name(irq);
		if (vmm_host_irq_is_disabled(irq) || !irq_name) {
			continue;
		}
		chip = vmm_host_irq_get_chip(irq);
		if (!chip || !chip->name) {
			continue;
		}
		vmm_cprintf(cdev, " %-5d %-20s %-13s",
				  num, irq_name, chip->name);
		for_each_online_cpu(cpu) {
			stats = vmm_host_irq_get_count(irq, cpu);
			vmm_cprintf(cdev, " %-10d", stats);
		}
		vmm_cprintf(cdev, "\n");
	}
	vmm_cprintf(cdev, "----------------------------------------");
	for_each_online_cpu(cpu) {
		vmm_cprintf(cdev, "------------");
	}
	vmm_cprintf(cdev, "\n");
}

static int cmd_host_irq_set_affinity(struct vmm_chardev *cdev, u32 hirq, u32 hcpu)
{
	if (CONFIG_CPU_COUNT <= hcpu) {
		vmm_cprintf(cdev, "%s: invalid host CPU%d\n", __func__, hcpu);
		return VMM_EINVALID;
	}

	if (!vmm_cpu_online(hcpu)) {
		vmm_cprintf(cdev, "%s: host CPU%d not online\n", __func__, hcpu);
		return VMM_EINVALID;
	}

	return vmm_host_irq_set_affinity(hirq, vmm_cpumask_of(hcpu), TRUE);
}

static void cmd_host_extirq_stats(struct vmm_chardev *cdev)
{
	vmm_host_irqext_debug_dump(cdev);
}

static void cmd_host_ram_info(struct vmm_chardev *cdev)
{
	u32 bn, bank_count = vmm_host_ram_bank_count();
	u32 free = vmm_host_ram_total_free_frames();
	u32 count = vmm_host_ram_total_frame_count();
	physical_addr_t start;
	physical_size_t size;

	vmm_cprintf(cdev, "Frame Size        : %d (0x%08x)\n",
					VMM_PAGE_SIZE, VMM_PAGE_SIZE);
	vmm_cprintf(cdev, "Bank Count        : %d (0x%08x)\n",
					bank_count, bank_count);
	vmm_cprintf(cdev, "Total Free Frames : %d (0x%08x)\n",
					free, free);
	vmm_cprintf(cdev, "Total Frame Count : %d (0x%08x)\n",
					count, count);
	for (bn = 0; bn < bank_count; bn++) {
		start = vmm_host_ram_bank_start(bn);
		size = vmm_host_ram_bank_size(bn);
		free = vmm_host_ram_bank_free_frames(bn);
		count = vmm_host_ram_bank_frame_count(bn);
		vmm_cprintf(cdev, "\n");
		if (sizeof(u64) == sizeof(physical_addr_t)) {
			vmm_cprintf(cdev, "Bank%02d Start      : 0x%016llx\n",
					bn, start);
		} else {
			vmm_cprintf(cdev, "Bank%02d Start      : 0x%08x\n",
					bn, start);
		}
		if (sizeof(u64) == sizeof(physical_size_t)) {
			vmm_cprintf(cdev, "Bank%02d Size       : 0x%016llx\n",
					bn, size);
		} else {
			vmm_cprintf(cdev, "Bank%02d Size       : 0x%08x\n",
					bn, size);
		}
		vmm_cprintf(cdev, "Bank%02d Free Frames: %d (0x%08x)\n",
					bn, free, free);
		vmm_cprintf(cdev, "Bank%02d Frame Count: %d (0x%08x)\n",
					bn, count, count);
	}
}

static void cmd_host_ram_bitmap(struct vmm_chardev *cdev, int colcnt)
{
	u32 ite, count, bn, bank_count = vmm_host_ram_bank_count();
	physical_addr_t start;

	for (bn = 0; bn < bank_count; bn++) {
		if (bn) {
			vmm_cprintf(cdev, "\n");
		}
		start = vmm_host_ram_bank_start(bn);
		count = vmm_host_ram_bank_frame_count(bn);
		vmm_cprintf(cdev, "Bank%02d\n", bn);
		vmm_cprintf(cdev, "0 : free\n");
		vmm_cprintf(cdev, "1 : used");
		for (ite = 0; ite < count; ite++) {
			if (umod32(ite, colcnt) == 0) {
				if (sizeof(u64) == sizeof(physical_addr_t)) {
					vmm_cprintf(cdev, "\n0x%016llx: ",
						    start + ite * VMM_PAGE_SIZE);
				} else {
					vmm_cprintf(cdev, "\n0x%08x: ",
						    start + ite * VMM_PAGE_SIZE);
				}
			}
			if (vmm_host_ram_frame_isfree(start + ite * VMM_PAGE_SIZE)) {
				vmm_cprintf(cdev, "0");
			} else {
				vmm_cprintf(cdev, "1");
			}
		}
		vmm_cprintf(cdev, "\n");
	}
}

static void cmd_host_vapool_info(struct vmm_chardev *cdev)
{
	u32 free = vmm_host_vapool_free_page_count();
	u32 total = vmm_host_vapool_total_page_count();
	virtual_addr_t base = vmm_host_vapool_base();

	if (sizeof(u64) == sizeof(virtual_addr_t)) {
		vmm_cprintf(cdev, "Base Address : 0x%016llx\n", base);
	} else {
		vmm_cprintf(cdev, "Base Address : 0x%08x\n", base);
	}
	vmm_cprintf(cdev, "Page Size    : %d (0x%08x)\n",
					VMM_PAGE_SIZE, VMM_PAGE_SIZE);
	vmm_cprintf(cdev, "Free Pages   : %d (0x%08x)\n", free, free);
	vmm_cprintf(cdev, "Total Pages  : %d (0x%08x)\n", total, total);
}

static int cmd_host_vapool_state(struct vmm_chardev *cdev)
{
	return vmm_host_vapool_print_state(cdev);
}

static void cmd_host_vapool_bitmap(struct vmm_chardev *cdev, int colcnt)
{
	u32 ite, total = vmm_host_vapool_total_page_count();
	virtual_addr_t base = vmm_host_vapool_base();

	vmm_cprintf(cdev, "0 : free\n");
	vmm_cprintf(cdev, "1 : used");
	for (ite = 0; ite < total; ite++) {
		if (umod32(ite, colcnt) == 0) {
			if (sizeof(u64) == sizeof(virtual_addr_t)) {
				vmm_cprintf(cdev, "\n0x%016llx: ",
					    base + ite * VMM_PAGE_SIZE);
			} else {
				vmm_cprintf(cdev, "\n0x%08x: ",
					    base + ite * VMM_PAGE_SIZE);
			}
		}
		if (vmm_host_vapool_page_isfree(base + ite * VMM_PAGE_SIZE)) {
			vmm_cprintf(cdev, "0");
		} else {
			vmm_cprintf(cdev, "1");
		}
	}
	vmm_cprintf(cdev, "\n");
}

static int cmd_host_resources_print(const char *name,
				    u64 start, u64 end,
				    unsigned long flags,
				    int level, void *arg)
{
	struct vmm_chardev *cdev = arg;

	while (level > 0) {
		vmm_cputs(cdev, "   ");
		level--;
	}

	vmm_cprintf(cdev, "[0x%016llx-0x%016llx] (0x%08lx) %s\n",
		    start, end, (u32)flags, (name) ? name : "Unknown");

	return VMM_OK;
}

static void cmd_host_resources(struct vmm_chardev *cdev)
{
	vmm_walk_tree_res(&vmm_hostio_resource, cdev,
			  cmd_host_resources_print);

	vmm_walk_tree_res(&vmm_hostmem_resource, cdev,
			  cmd_host_resources_print);
}

struct cmd_host_list_iter {
	u32 num;
	struct vmm_chardev *cdev;
};

static int cmd_host_bus_list_iter(struct vmm_bus *b, void *data)
{
	u32 dcount;
	struct cmd_host_list_iter *p = data;

	dcount = vmm_devdrv_bus_device_count(b);
	vmm_cprintf(p->cdev, " %-7d %-15s %-15d\n", p->num++, b->name, dcount);

	return VMM_OK;
}

static void cmd_host_bus_list(struct vmm_chardev *cdev)
{
	struct cmd_host_list_iter p = { .num = 0, .cdev = cdev };

	vmm_cprintf(cdev, "----------------------------------------\n");
	vmm_cprintf(cdev, " %-7s %-15s %-15s\n",
			  "Num#", "Bus Name", "Device Count");
	vmm_cprintf(cdev, "----------------------------------------\n");
	vmm_devdrv_bus_iterate(NULL, &p, cmd_host_bus_list_iter);
	vmm_cprintf(cdev, "----------------------------------------\n");
}

static int cmd_host_bus_device_list_iter(struct vmm_device *d,
					 void *data)
{
	struct cmd_host_list_iter *p = data;

	vmm_cprintf(p->cdev, " %-7d %-25s %-25s\n",
		    p->num++, d->name,
		    (d->parent) ? d->parent->name : "---");

	return VMM_OK;
}

static int cmd_host_bus_device_list(struct vmm_chardev *cdev,
				    const char *bus_name)
{
	struct vmm_bus *b;
	struct cmd_host_list_iter p = { .num = 0, .cdev = cdev };

	b = vmm_devdrv_find_bus(bus_name);
	if (!b) {
		vmm_cprintf(cdev, "Failed to find %s bus\n", bus_name);
		return VMM_ENOTAVAIL;
	}

	vmm_cprintf(cdev, "----------------------------------------");
	vmm_cprintf(cdev, "--------------------\n");
	vmm_cprintf(cdev, " %-7s %-25s %-25s\n",
			  "Num#", "Device Name", "Parent Name");
	vmm_cprintf(cdev, "----------------------------------------");
	vmm_cprintf(cdev, "--------------------\n");
	vmm_devdrv_bus_device_iterate(b, NULL, &p,
				      cmd_host_bus_device_list_iter);
	vmm_cprintf(cdev, "----------------------------------------");
	vmm_cprintf(cdev, "--------------------\n");

	return VMM_OK;
}

static int cmd_host_class_list_iter(struct vmm_class *c, void *data)
{
	u32 dcount;
	struct cmd_host_list_iter *p = data;

	dcount = vmm_devdrv_class_device_count(c);
	vmm_cprintf(p->cdev, " %-7d %-15s %-15d\n", p->num++, c->name, dcount);

	return VMM_OK;
}

static void cmd_host_class_list(struct vmm_chardev *cdev)
{
	struct cmd_host_list_iter p = { .num = 0, .cdev = cdev };

	vmm_cprintf(cdev, "----------------------------------------\n");
	vmm_cprintf(cdev, " %-7s %-15s %-15s\n",
			  "Num#", "Class Name", "Device Count");
	vmm_cprintf(cdev, "----------------------------------------\n");
	vmm_devdrv_class_iterate(NULL, &p, cmd_host_class_list_iter);
	vmm_cprintf(cdev, "----------------------------------------\n");
}

static int cmd_host_class_device_list_iter(struct vmm_device *d,
					   void *data)
{
	struct cmd_host_list_iter *p = data;

	vmm_cprintf(p->cdev, " %-7d %-25s %-25s\n",
		    p->num++, d->name,
		    (d->parent) ? d->parent->name : "---");

	return VMM_OK;
}

static int cmd_host_class_device_list(struct vmm_chardev *cdev,
				      const char *class_name)
{
	struct vmm_class *c;
	struct cmd_host_list_iter p = { .num = 0, .cdev = cdev };

	c = vmm_devdrv_find_class(class_name);
	if (!c) {
		vmm_cprintf(cdev, "Failed to find %s class\n", class_name);
		return VMM_ENOTAVAIL;
	}

	vmm_cprintf(cdev, "----------------------------------------");
	vmm_cprintf(cdev, "--------------------\n");
	vmm_cprintf(cdev, " %-7s %-25s %-25s\n",
			  "Num#", "Device Name", "Parent Name");
	vmm_cprintf(cdev, "----------------------------------------");
	vmm_cprintf(cdev, "--------------------\n");
	vmm_devdrv_class_device_iterate(c, NULL, &p,
					cmd_host_class_device_list_iter);
	vmm_cprintf(cdev, "----------------------------------------");
	vmm_cprintf(cdev, "--------------------\n");

	return VMM_OK;
}

static int cmd_host_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	int hirq, hcpu, colcnt;

	if (argc <= 1) {
		goto fail;
	}

	if (strcmp(argv[1], "help") == 0) {
		cmd_host_usage(cdev);
		return VMM_OK;
	} else if (strcmp(argv[1], "info") == 0) {
		cmd_host_info(cdev);
		return VMM_OK;
	} else if ((strcmp(argv[1], "cpu") == 0) && (2 < argc)) {
		if (strcmp(argv[2], "info") == 0) {
			cmd_host_cpu_info(cdev);
			return VMM_OK;
		} else if (strcmp(argv[2], "stats") == 0) {
			cmd_host_cpu_stats(cdev);
			return VMM_OK;
		}
	} else if ((strcmp(argv[1], "irq") == 0) && (2 < argc)) {
		if (strcmp(argv[2], "stats") == 0) {
			cmd_host_irq_stats(cdev);
			return VMM_OK;
		} else if ((strcmp(argv[2], "set_affinity") == 0) && (4 < argc)) {
			hirq = atoi(argv[3]);
			hcpu = atoi(argv[4]);
			return cmd_host_irq_set_affinity(cdev, hirq, hcpu);
		}
	} else if ((strcmp(argv[1], "extirq") == 0) && (2 < argc)) {
		if (strcmp(argv[2], "stats") == 0) {
			cmd_host_extirq_stats(cdev);
			return VMM_OK;
		}
	} else if ((strcmp(argv[1], "ram") == 0) && (2 < argc)) {
		if (strcmp(argv[2], "info") == 0) {
			cmd_host_ram_info(cdev);
			return VMM_OK;
		} else if (strcmp(argv[2], "bitmap") == 0) {
			if (3 < argc) {
				colcnt = atoi(argv[3]);
			} else {
				colcnt = 64;
			}
			cmd_host_ram_bitmap(cdev, colcnt);
			return VMM_OK;
		}
	} else if ((strcmp(argv[1], "vapool") == 0) && (2 < argc)) {
		if (strcmp(argv[2], "info") == 0) {
			cmd_host_vapool_info(cdev);
			return VMM_OK;
		} else if (strcmp(argv[2], "state") == 0) {
			return cmd_host_vapool_state(cdev);
		} else if (strcmp(argv[2], "bitmap") == 0) {
			if (3 < argc) {
				colcnt = atoi(argv[3]);
			} else {
				colcnt = 64;
			}
			cmd_host_vapool_bitmap(cdev, colcnt);
			return VMM_OK;
		}
	} else if ((strcmp(argv[1], "resources") == 0) && (2 == argc)) {
		cmd_host_resources(cdev);
		return VMM_OK;
	} else if ((strcmp(argv[1], "bus_list") == 0) && (2 == argc)) {
		cmd_host_bus_list(cdev);
		return VMM_OK;
	} else if ((strcmp(argv[1], "bus_device_list") == 0) && (3 == argc)) {
		cmd_host_bus_device_list(cdev, argv[2]);
		return VMM_OK;
	} else if ((strcmp(argv[1], "class_list") == 0) && (2 == argc)) {
		cmd_host_class_list(cdev);
		return VMM_OK;
	} else if ((strcmp(argv[1], "class_device_list") == 0) && (3 == argc)) {
		cmd_host_class_device_list(cdev, argv[2]);
		return VMM_OK;
	}

fail:
	cmd_host_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_host = {
	.name = "host",
	.desc = "host information",
	.usage = cmd_host_usage,
	.exec = cmd_host_exec,
};

static int __init cmd_host_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_host);
}

static void __exit cmd_host_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_host);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
