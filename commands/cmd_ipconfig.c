/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @file cmd_ipconfig.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief command for IP address configuration of network stack.
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_version.h>
#include <vmm_heap.h>
#include <vmm_host_aspace.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <net/vmm_protocol.h>
#include <libs/netstack.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"Command ipconfig"
#define MODULE_AUTHOR			"Sukanto Ghosh"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_ipconfig_init
#define	MODULE_EXIT			cmd_ipconfig_exit

static void cmd_ipconfig_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   ipconfig help\n");
	vmm_cprintf(cdev, "   ipconfig show\n");
	vmm_cprintf(cdev, "   ipconfig update <ipaddr> [<netmask>] [<gateway>]\n");
}

static int cmd_ipconfig_show(struct vmm_chardev *cdev, int argc, char **argv)
{
	u8 buf[4];
	char str[30];
	char *p;

	vmm_cprintf(cdev, "Network stack Configuration:\n");
	p = netstack_get_name();
	vmm_cprintf(cdev, "   TCP/IP stack name  : %s\n", p);
	netstack_get_ipaddr(buf);
	p = ip4addr_to_str(str, buf);
	vmm_cprintf(cdev, "   IP address         : %s\n", p);
	netstack_get_ipmask(buf);
	p = ip4addr_to_str(str, buf);
	vmm_cprintf(cdev, "   IP netmask         : %s\n", p);
	netstack_get_gatewayip(buf);
	p = ip4addr_to_str(str, buf);
	vmm_cprintf(cdev, "   Gateway IP address : %s\n", p);
	netstack_get_hwaddr(buf);
	p = ethaddr_to_str(str, buf);
	vmm_cprintf(cdev, "   HW address         : %s\n", p);

	return VMM_OK;
}

static int cmd_ipconfig_update(struct vmm_chardev *cdev, int argc, char **argv)
{
	u8 buf[4];
	u8 mask[4];
	int rc = VMM_OK;

	switch(argc) {
	case 3:
		str2ipaddr(buf, argv[2]);
		if(ipv4_class_netmask(buf,mask) != -1) {
			netstack_set_ipaddr(buf);
			netstack_set_ipmask(mask);
		} else {
			vmm_cprintf(cdev, "ERROR: Invalid IP address\n");
			rc = VMM_EINVALID;
		}
		break;
	case 4:
		str2ipaddr(buf, argv[2]);
		if(ipv4_class_netmask(buf,mask) != -1) {
			netstack_set_ipaddr(buf);
			str2ipaddr(buf, argv[3]);
			netstack_set_ipmask(buf);
		} else {
			vmm_cprintf(cdev, "ERROR: Invalid IP address\n");
			rc = VMM_EINVALID;
		}
		break;
	case 5:
		str2ipaddr(buf, argv[2]);
		if(ipv4_class_netmask(buf,mask) != -1) {
			netstack_set_ipaddr(buf);
			str2ipaddr(buf, argv[3]);
			netstack_set_ipmask(buf);
			str2ipaddr(buf, argv[4]);
			netstack_set_gatewayip(buf);
		} else {
			vmm_cprintf(cdev, "ERROR: Invalid IP address\n");
			rc = VMM_EINVALID;
		}
		break;
	default:
		rc = VMM_EINVALID;
		break;
	};

	return rc;
}

static int cmd_ipconfig_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	if (argc == 2) {
		if (strcmp(argv[1], "help") == 0) {
			cmd_ipconfig_usage(cdev);
			return VMM_OK;
		} else if (strcmp(argv[1], "show") == 0) {
			return cmd_ipconfig_show(cdev, argc, argv);
		}
	} else if (argc > 2) {
		if (strcmp(argv[1], "update") == 0) {
			return cmd_ipconfig_update(cdev, argc, argv);
		}
	}
	cmd_ipconfig_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_ipconfig = {
	.name = "ipconfig",
	.desc = "IP configuration commands",
	.usage = cmd_ipconfig_usage,
	.exec = cmd_ipconfig_exec,
};

static int __init cmd_ipconfig_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_ipconfig);
}

static void __exit cmd_ipconfig_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_ipconfig);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);


