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
 * @file cmd_iommu.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief command for IOMMU managment.
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_version.h>
#include <vmm_heap.h>
#include <vmm_iommu.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"Command iommu"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_iommu_init
#define	MODULE_EXIT			cmd_iommu_exit

static void cmd_iommu_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   iommu help\n");
	vmm_cprintf(cdev, "   iommu group_list\n");
	vmm_cprintf(cdev, "   iommu domain_list\n");
	vmm_cprintf(cdev, "   iommu controller_list\n");
	vmm_cprintf(cdev, "   iommu controller_info <controller_name>\n");
	vmm_cprintf(cdev, "   iommu test_iova_to_phys <bus_name> <controller_name> <iova> <phys> <size> <stride>\n");
}

struct cmd_iommu_list_priv {
	u32 prefix_spaces;
	u32 num;
	bool new_line;
	struct vmm_chardev *cdev;
};

static int cmd_iommu_device_list_iter(struct vmm_device *dev,
				      void *data)
{
	u32 s;
	struct cmd_iommu_list_priv *p = data;

	for (s = 0; s < p->prefix_spaces; s++)
		vmm_cprintf(p->cdev, "%c", ' ');

	vmm_cprintf(p->cdev, "%s%s",
		    (p->num == 0) ? ", " : "", dev->name);

	if (p->new_line)
		vmm_cprintf(p->cdev, "%c", '\n');

	p->num++;

	return VMM_OK;
}

static void cmd_iommu_group_list_header(struct vmm_chardev *cdev,
					u32 prefix_spaces)
{
	u32 s;

	for (s = 0; s < prefix_spaces; s++)
		vmm_cprintf(cdev, "%c", ' ');
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------"
			  "----------\n");

	for (s = 0; s < prefix_spaces; s++)
		vmm_cprintf(cdev, "%c", ' ');
	vmm_cprintf(cdev, " %-5s %-20s %-20s %-20s %-20s\n",
		    "Num#", "Group", "Controller", "Domain", "Device");

	for (s = 0; s < prefix_spaces; s++)
		vmm_cprintf(cdev, "%c", ' ');
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------"
			  "----------\n");
}

static int cmd_iommu_group_list_iter(struct vmm_iommu_group *group,
				     void *data)
{
	u32 s;
	struct vmm_iommu_domain *domain;
	struct cmd_iommu_list_priv *p = data;
	struct cmd_iommu_list_priv pnext;

	for (s = 0; s < p->prefix_spaces; s++)
		vmm_cprintf(p->cdev, "%c", ' ');

	vmm_cprintf(p->cdev, " %-5d %-20s", p->num,
		    vmm_iommu_group_name(group));
	domain = vmm_iommu_group_get_domain(group);
	vmm_cprintf(p->cdev, " %-20s %-20s",
		    vmm_iommu_group_controller(group)->name,
		    (domain) ? domain->name : "---");
	if (domain)
		vmm_iommu_domain_dref(domain);
	pnext.prefix_spaces = 0;
	pnext.num = 0;
	pnext.new_line = FALSE;
	pnext.cdev = p->cdev;
	vmm_cprintf(p->cdev, "%c", ' ');
	vmm_iommu_group_for_each_dev(group, &pnext,
				     cmd_iommu_device_list_iter);

	if (p->new_line)
		vmm_cprintf(p->cdev, "%c", '\n');

	p->num++;

	return VMM_OK;
}

static void cmd_iommu_group_list_footer(struct vmm_chardev *cdev,
					u32 prefix_spaces)
{
	u32 s;

	for (s = 0; s < prefix_spaces; s++)
		vmm_cprintf(cdev, "%c", ' ');
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------"
			  "----------\n");
}

static int cmd_iommu_group_list_iter1(struct vmm_iommu_controller *ctrl,
				      void *data)
{
	return vmm_iommu_controller_for_each_group(ctrl, data,
						   cmd_iommu_group_list_iter);
}

static int cmd_iommu_group_list(struct vmm_chardev *cdev)
{
	struct cmd_iommu_list_priv p;

	p.prefix_spaces = 0;
	p.num = 0;
	p.new_line = TRUE;
	p.cdev = cdev;

	cmd_iommu_group_list_header(cdev, 0);
	vmm_iommu_controller_iterate(NULL, &p, cmd_iommu_group_list_iter1);
	cmd_iommu_group_list_footer(cdev, 0);

	return VMM_OK;
}

static void cmd_iommu_domain_list_header(struct vmm_chardev *cdev,
					 u32 prefix_spaces)
{
	u32 s;

	for (s = 0; s < prefix_spaces; s++)
		vmm_cprintf(cdev, "%c", ' ');
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------\n");

	for (s = 0; s < prefix_spaces; s++)
		vmm_cprintf(cdev, "%c", ' ');
	vmm_cprintf(cdev, " %-7s %-20s %-20s\n",
		    "Num#", "Domain", "Controller");

	for (s = 0; s < prefix_spaces; s++)
		vmm_cprintf(cdev, "%c", ' ');
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------\n");
}

static int cmd_iommu_domain_list_iter(struct vmm_iommu_domain *domain,
				      void *data)
{
	u32 s;
	struct cmd_iommu_list_priv *p = data;

	for (s = 0; s < p->prefix_spaces; s++)
		vmm_cprintf(p->cdev, "%c", ' ');

	vmm_cprintf(p->cdev, " %-7d %-20s %-20s",
		    p->num, domain->name, domain->ctrl->name);

	if (p->new_line)
		vmm_cprintf(p->cdev, "%c", '\n');

	p->num++;

	return VMM_OK;
}

static void cmd_iommu_domain_list_footer(struct vmm_chardev *cdev,
					 u32 prefix_spaces)
{
	u32 s;

	for (s = 0; s < prefix_spaces; s++)
		vmm_cprintf(cdev, "%c", ' ');
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------\n");
}

static int cmd_iommu_domain_list_iter1(struct vmm_iommu_controller *ctrl,
				       void *data)
{
	return vmm_iommu_controller_for_each_domain(ctrl, data,
						cmd_iommu_domain_list_iter);
}

static int cmd_iommu_domain_list(struct vmm_chardev *cdev)
{
	struct cmd_iommu_list_priv p;

	p.prefix_spaces = 0;
	p.num = 0;
	p.new_line = TRUE;
	p.cdev = cdev;

	cmd_iommu_domain_list_header(cdev, 0);
	vmm_iommu_controller_iterate(NULL, &p, cmd_iommu_domain_list_iter1);
	cmd_iommu_domain_list_footer(cdev, 0);

	return VMM_OK;
}

static void cmd_iommu_controller_list_header(struct vmm_chardev *cdev,
					     u32 prefix_spaces)
{
	u32 s;

	for (s = 0; s < prefix_spaces; s++)
		vmm_cprintf(cdev, "%c", ' ');
	vmm_cprintf(cdev, "----------------------------------------"
			  "--------------------\n");

	for (s = 0; s < prefix_spaces; s++)
		vmm_cprintf(cdev, "%c", ' ');
	vmm_cprintf(cdev, " %-4s %-20s %-16s %-16s\n",
		    "Num#", "Controller", "Num Groups", "Num Domains");

	for (s = 0; s < prefix_spaces; s++)
		vmm_cprintf(cdev, "%c", ' ');
	vmm_cprintf(cdev, "----------------------------------------"
			  "--------------------\n");
}

static int cmd_iommu_controller_list_iter(struct vmm_iommu_controller *ctrl,
					  void *data)
{
	u32 s;
	struct cmd_iommu_list_priv *p = data;

	for (s = 0; s < p->prefix_spaces; s++)
		vmm_cprintf(p->cdev, "%c", ' ');

	vmm_cprintf(p->cdev, " %-4d %-20s %-16d %-16d",
		    p->num, ctrl->name,
		    vmm_iommu_controller_group_count(ctrl),
		    vmm_iommu_controller_domain_count(ctrl));

	if (p->new_line)
		vmm_cprintf(p->cdev, "%c", '\n');

	p->num++;

	return VMM_OK;
}

static void cmd_iommu_controller_list_footer(struct vmm_chardev *cdev,
					     u32 prefix_spaces)
{
	u32 s;

	for (s = 0; s < prefix_spaces; s++)
		vmm_cprintf(cdev, "%c", ' ');
	vmm_cprintf(cdev, "----------------------------------------"
			  "--------------------\n");
}

static int cmd_iommu_controller_list(struct vmm_chardev *cdev)
{
	struct cmd_iommu_list_priv p;

	p.prefix_spaces = 0;
	p.num = 0;
	p.new_line = TRUE;
	p.cdev = cdev;

	cmd_iommu_controller_list_header(cdev, 0);
	vmm_iommu_controller_iterate(NULL, &p, cmd_iommu_controller_list_iter);
	cmd_iommu_controller_list_footer(cdev, 0);

	return VMM_OK;
}

static int cmd_iommu_controller_info(struct vmm_chardev *cdev, char *name)
{
	struct vmm_iommu_controller *ctrl;
	struct cmd_iommu_list_priv p;

	ctrl = vmm_iommu_controller_find(name);
	if (!ctrl) {
		vmm_cprintf(cdev, "Failed to find IOMMU controller %s\n",
			    name);
		return VMM_EINVALID;
	}

	vmm_cprintf(cdev,
		    "Controller : %-20s\n"
		    "Num Groups : %-16d\n"
		    "Num Domains: %-16d\n",
		    ctrl->name,
		    vmm_iommu_controller_group_count(ctrl),
		    vmm_iommu_controller_domain_count(ctrl));

	p.prefix_spaces = 0;
	p.num = 0;
	p.new_line = TRUE;
	p.cdev = cdev;

	cmd_iommu_domain_list_header(cdev, 0);
	vmm_iommu_controller_for_each_domain(ctrl, &p,
					cmd_iommu_domain_list_iter);
	cmd_iommu_domain_list_footer(cdev, 0);

	p.prefix_spaces = 0;
	p.num = 0;
	p.new_line = TRUE;
	p.cdev = cdev;

	cmd_iommu_group_list_header(cdev, 0);
	vmm_iommu_controller_for_each_group(ctrl, &p,
					cmd_iommu_group_list_iter);
	cmd_iommu_group_list_footer(cdev, 0);

	return VMM_OK;
}

static int cmd_iommu_test_iova_to_phys(struct vmm_chardev *cdev,
				       char *bus_name,
				       char *ctrl_name,
				       physical_addr_t iova,
				       physical_addr_t phys,
				       size_t size, size_t stride)
{
	size_t pass, fail;
	int ret = VMM_OK;
	struct vmm_bus *bus;
	struct vmm_iommu_domain *domain;
	struct vmm_iommu_controller *ctrl;
	physical_addr_t tiova, tphys;

	if (!size) {
		vmm_cprintf(cdev, "Invalid size 0x%zx\n", size);
		return VMM_EINVALID;
	}

	if (!stride) {
		vmm_cprintf(cdev, "Invalid stride 0x%zx\n", stride);
		return VMM_EINVALID;
	}

	bus = vmm_devdrv_find_bus(bus_name);
	if (!bus) {
		vmm_cprintf(cdev, "Failed to find bus %s\n", bus_name);
		return VMM_EINVALID;
	}

	ctrl = vmm_iommu_controller_find(ctrl_name);
	if (!ctrl) {
		vmm_cprintf(cdev, "Failed to find IOMMU controller %s\n",
			    ctrl_name);
		return VMM_EINVALID;
	}

	domain = vmm_iommu_domain_alloc("test_iova_to_phys", bus, ctrl,
					VMM_IOMMU_DOMAIN_UNMANAGED);
	if (!domain) {
		vmm_cprintf(cdev, "Failed to alloc IOMMU domain\n");
		return VMM_EFAIL;
	}

	ret = vmm_iommu_map(domain, iova, phys, size,
			    VMM_IOMMU_READ | VMM_IOMMU_WRITE);
	if (ret) {
		goto done_dref_domain;
	}

	pass = fail = 0;
	for (tiova = iova; tiova < (iova + size); tiova += stride) {
		tphys = vmm_iommu_iova_to_phys(domain, tiova);
		vmm_cprintf(cdev, "0x%"PRIPADDR" => 0x%"PRIPADDR,
			    tiova, tphys);
		if (tphys == (phys + (tiova - iova))) {
			vmm_cprintf(cdev, " (pass)\n");
			pass++;
		} else {
			vmm_cprintf(cdev, " (fail)\n");
			fail++;
		}
	}

	vmm_cprintf(cdev, "Summary total=%zd pass=%zd fail=%zd\n",
		    (pass + fail), pass, fail);

	vmm_iommu_unmap(domain, iova, size);

done_dref_domain:
	vmm_iommu_domain_dref(domain);

	return ret;
}

static int cmd_iommu_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	physical_addr_t iova, phys;
	size_t size, stride;

	if (argc <= 1) {
		goto fail;
	}

	if (strcmp(argv[1], "help") == 0) {
		cmd_iommu_usage(cdev);
		return VMM_OK;
	} else if (strcmp(argv[1], "group_list") == 0) {
		return cmd_iommu_group_list(cdev);
	} else if (strcmp(argv[1], "domain_list") == 0) {
		return cmd_iommu_domain_list(cdev);
	} else if (strcmp(argv[1], "controller_list") == 0) {
		return cmd_iommu_controller_list(cdev);
	} else if (strcmp(argv[1], "controller_info") == 0 && (argc > 2)) {
		return cmd_iommu_controller_info(cdev, argv[2]);
	} else if (strcmp(argv[1], "test_iova_to_phys") == 0 && (argc > 7)) {
		iova = strtoull(argv[4], NULL, 0);
		phys = strtoull(argv[5], NULL, 0);
		size = strtoul(argv[6], NULL, 0);
		stride = strtoul(argv[7], NULL, 0);
		return cmd_iommu_test_iova_to_phys(cdev, argv[2], argv[3],
						   iova, phys, size, stride);
	}

fail:
	cmd_iommu_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_iommu = {
	.name = "iommu",
	.desc = "iommu commands",
	.usage = cmd_iommu_usage,
	.exec = cmd_iommu_exec,
};

static int __init cmd_iommu_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_iommu);
}

static void __exit cmd_iommu_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_iommu);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
