/**
 * Copyright (c) 2017 Anup Patel.
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
 * @file cmd_vmsg.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief command for virtual messaging subsystem.
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_version.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <vio/vmm_vmsg.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"Command vmsg"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_vmsg_init
#define	MODULE_EXIT			cmd_vmsg_exit

static void cmd_vmsg_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   vmsg help\n");
	vmm_cprintf(cdev, "   vmsg node_list\n");
	vmm_cprintf(cdev, "   vmsg domain_create <domain_name>\n");
	vmm_cprintf(cdev, "   vmsg domain_destroy <domain_name>\n");
	vmm_cprintf(cdev, "   vmsg domain_list\n");
}

struct cmd_vmsg_list_priv {
	u32 num1;
	u32 num2;
	struct vmm_chardev *cdev;
};

static int cmd_vmsg_node_list_iter(struct vmm_vmsg_node *node, void *data)
{
	struct cmd_vmsg_list_priv *p = data;

	vmm_cprintf(p->cdev, " %-5d %-21s %-21s %-10s 0x%08x\n",
		    p->num1,
		    vmm_vmsg_node_get_name(node),
		    vmm_vmsg_domain_get_name(vmm_vmsg_node_get_domain(node)),
		    vmm_vmsg_node_is_ready(node) ? "READY" : "NOT-READY",
		    vmm_vmsg_node_get_addr(node));
	p->num1++;

	return VMM_OK;
}

static int cmd_vmsg_node_list(struct vmm_chardev *cdev)
{
	struct cmd_vmsg_list_priv p = { .num1 = 0, .num2 = 0, .cdev = cdev };

	vmm_cprintf(cdev, "----------------------------------------"
			  "------------------------------\n");
	vmm_cprintf(cdev, " %-5s %-21s %-21s %-10s %-8s\n",
		    "Num#", "Node", "Domain", "State", "Address");
	vmm_cprintf(cdev, "----------------------------------------"
			  "------------------------------\n");
	vmm_vmsg_node_iterate(NULL, &p, cmd_vmsg_node_list_iter);
	vmm_cprintf(cdev, "----------------------------------------"
			  "------------------------------\n");

	return VMM_OK;
}

static int cmd_vmsg_domain_node_list_iter(struct vmm_vmsg_node *node,
					  void *data)
{
	struct cmd_vmsg_list_priv *p = data;

	if (p->num1 == 0) {
		vmm_cprintf(p->cdev, " %-5d %-21s +--%-41s\n", p->num2,
		vmm_vmsg_domain_get_name(vmm_vmsg_node_get_domain(node)),
		vmm_vmsg_node_get_name(node));
	} else {
		vmm_cprintf(p->cdev, " %-5s %-21s +--%-41s\n",
			    "", "", vmm_vmsg_node_get_name(node));
	}
	p->num1++;

	return VMM_OK;
}

static int cmd_vmsg_domain_list_iter(struct vmm_vmsg_domain *domain,
				     void *data)
{
	struct cmd_vmsg_list_priv *p = data;
	struct cmd_vmsg_list_priv np = { .num1 = 0, .num2 = 0, .cdev = p->cdev };

	np.num2 = p->num1;
	vmm_vmsg_domain_node_iterate(domain, NULL, &np,
				     cmd_vmsg_domain_node_list_iter);
	if (!np.num1) {
		vmm_cprintf(p->cdev, " %-5d %-21s +--%-41s\n",
			    p->num1, vmm_vmsg_domain_get_name(domain), "");
	}

	p->num1++;

	vmm_cprintf(p->cdev, "----------------------------------------"
			     "------------------------------\n");

	return VMM_OK;
}

static int cmd_vmsg_domain_list(struct vmm_chardev *cdev)
{
	struct cmd_vmsg_list_priv p = { .num1 = 0, .num2 = 0, .cdev = cdev };

	vmm_cprintf(cdev, "----------------------------------------"
			  "------------------------------\n");
	vmm_cprintf(cdev, " %-5s %-21s %-41s\n",
			  "Num#", "Domain", "Node List");
	vmm_cprintf(cdev, "----------------------------------------"
			  "------------------------------\n");
	vmm_vmsg_domain_iterate(NULL, &p, cmd_vmsg_domain_list_iter);
	if (p.num1) {
		goto done;
	}
	vmm_cprintf(cdev, "----------------------------------------"
			  "------------------------------\n");

done:
	return VMM_OK;
}

static int cmd_vmsg_domain_create(struct vmm_chardev *cdev, const char *name)
{
	int ret;
	struct vmm_vmsg_domain *domain = vmm_vmsg_domain_find(name);

	if (domain) {
		vmm_cprintf(cdev, "Domain already exist\n");
		return VMM_ENOTAVAIL;
	}

	domain = vmm_vmsg_domain_create(name, NULL);
	if (domain) {
		vmm_cprintf(cdev, "%s: Created\n", name);
		ret = VMM_OK;
	} else {
		vmm_cprintf(cdev, "%s: Failed to create\n", name);
		ret = VMM_EFAIL;
	}

	return ret;
}

static int cmd_vmsg_domain_destroy(struct vmm_chardev *cdev, const char *name)
{
	int ret;
	struct vmm_vmsg_domain *domain = vmm_vmsg_domain_find(name);

	if (!domain) {
		vmm_cprintf(cdev, "Failed to find domain\n");
		return VMM_ENOTAVAIL;
	}

	if ((ret = vmm_vmsg_domain_destroy(domain))) {
		vmm_cprintf(cdev, "%s: Failed to destroy\n", name);
	} else {
		vmm_cprintf(cdev, "%s: Destroyed\n", name);
	}

	return ret;
}

static int cmd_vmsg_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	if (argc <= 1) {
		goto fail;
	}

	if (strcmp(argv[1], "help") == 0) {
		cmd_vmsg_usage(cdev);
		return VMM_OK;
	} else if ((strcmp(argv[1], "node_list") == 0) && (argc == 2)) {
		return cmd_vmsg_node_list(cdev);
	} else if (strcmp(argv[1], "domain_list") == 0 && (argc == 2)) {
		return cmd_vmsg_domain_list(cdev);
	} else if (strcmp(argv[1], "domain_create") == 0 && (argc == 3)) {
		return cmd_vmsg_domain_create(cdev, argv[2]);
	} else if (strcmp(argv[1], "domain_destroy") == 0 && (argc == 3)) {
		return cmd_vmsg_domain_destroy(cdev, argv[2]);
	}

fail:
	cmd_vmsg_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_vmsg = {
	.name = "vmsg",
	.desc = "virtual messaging commands",
	.usage = cmd_vmsg_usage,
	.exec = cmd_vmsg_exec,
};

static int __init cmd_vmsg_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_vmsg);
}

static void __exit cmd_vmsg_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_vmsg);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
