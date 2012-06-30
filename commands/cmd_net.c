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
 * @file cmd_net.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief command for heap status.
 */

#include <net/vmm_netport.h>
#include <net/vmm_netswitch.h>
#include <net/vmm_protocol.h>
#include <net/vmm_netstack.h>
#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_version.h>
#include <vmm_string.h>
#include <vmm_heap.h>
#include <vmm_host_aspace.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>

#define MODULE_VARID			cmd_net_module
#define MODULE_NAME			"Command net"
#define MODULE_AUTHOR			"Sukanto Ghosh"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_net_init
#define	MODULE_EXIT			cmd_net_exit

void cmd_net_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   net help\n");
	vmm_cprintf(cdev, "   net ipconfig [<ipaddr>] [<netmask>]\n");
	vmm_cprintf(cdev, "   net ports\n");
	vmm_cprintf(cdev, "   net switches\n");
}

int cmd_net_port_list(struct vmm_chardev *cdev, int argc, char **argv)
{
	int num, count;
	struct vmm_netport *port;
	char hwaddr[20];
	if(argc != 2) {
		cmd_net_usage(cdev);
		return VMM_EFAIL;
	}
	count = vmm_netport_count();
	vmm_cprintf(cdev, "---------------------------------------------------------------------------\n");
	vmm_cprintf(cdev, " %-3s %-18s %-13s %-6s %-18s %-5s\n", "ID", "Port", "Switch", "Link", "HW-Address", "MTU");
	vmm_cprintf(cdev, "---------------------------------------------------------------------------\n");
	for (num = 0; num < count; num++) {
		port = vmm_netport_get(num);
		vmm_cprintf(cdev, " %-3d %-18s %-13s", num, port->name, port->nsw->name);
		if(port->flags & VMM_NETPORT_LINK_UP) {
			vmm_cprintf(cdev, " %-6s", "UP");
		} else {
			vmm_cprintf(cdev, " %-6s", "DOWN");
		}
		vmm_cprintf(cdev, " %-18s %-5d\n", ethaddr_to_str(hwaddr, port->macaddr), port->mtu);
	}
	vmm_cprintf(cdev, "---------------------------------------------------------------------------\n");
	return VMM_OK;
}

int cmd_net_switch_list(struct vmm_chardev *cdev, int argc, char **argv)
{
	int num, count;
	struct vmm_netswitch *nsw;
	struct dlist *list;
	struct vmm_netport *port;
	if(argc != 2) {
		cmd_net_usage(cdev);
		return VMM_EFAIL;
	}
	count = vmm_netswitch_count();
	vmm_cprintf(cdev, "-----------------------------------------\n");
	vmm_cprintf(cdev, " %-13s     %s\n", "Switch", "Port List");
	for (num = 0; num < count; num++) {
		vmm_cprintf(cdev, "\r-----------------------------------------\n");
		nsw = vmm_netswitch_get(num);
		vmm_cprintf(cdev, " %-13s +", nsw->name);
		list_for_each(list, &nsw->port_list) {
			port = list_port(list);
			vmm_cprintf(cdev, "-- %s\n %-13s +", port->name, "");
		}
	}
	vmm_cprintf(cdev, "\r-----------------------------------------\n");
	return VMM_OK;
}

int cmd_net_ipconfig(struct vmm_chardev *cdev, int argc, char **argv)
{
	u8 buf[4];
	u8 mask[4];
	int rc = VMM_OK;
	char str[30];
	if((argc < 2) || (argc > 4)) {
		cmd_net_usage(cdev);
		return VMM_EFAIL;
	}
	switch(argc) {
	case 2:
		vmm_cprintf(cdev, "Hypervisor Network-Stack Info:\n");
		vmm_netstack_get_ipaddr(buf);
		vmm_cprintf(cdev, "   IP address  : %s\n", ip4addr_to_str(str, buf));
		vmm_netstack_get_ipmask(buf);
		vmm_cprintf(cdev, "   IP netmask  : %s\n", ip4addr_to_str(str, buf));
		vmm_netstack_get_hwaddr(buf);
		vmm_cprintf(cdev, "   HW address  : %s\n", ethaddr_to_str(str, buf));
		vmm_cprintf(cdev, "   TCP/IP-stack: %s\n", vmm_netstack_get_name());
		break;
	case 3:
		vmm_str_to_ipaddr(buf, argv[2]);
		if(ipv4_class_netmask(buf,mask) != -1) {
			vmm_netstack_set_ipaddr(buf);
			vmm_netstack_set_ipmask(mask);
		} else {
			vmm_cprintf(cdev, "ERROR: Invalid IP address\n");
			rc = VMM_EFAIL;
		}
		break;
	case 4:
		vmm_str_to_ipaddr(buf, argv[2]);
		if(ipv4_class_netmask(buf,mask) != -1) {
			vmm_netstack_set_ipaddr(buf);
			vmm_str_to_ipaddr(buf, argv[3]);
			vmm_netstack_set_ipmask(buf);
		} else {
			vmm_cprintf(cdev, "ERROR: Invalid IP address\n");
			rc = VMM_EFAIL;
		}
		break;
	};

	return rc;
}

int cmd_net_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	if (vmm_strcmp(argv[1], "help") == 0) {
		cmd_net_usage(cdev);
		return VMM_OK;
	} else if (vmm_strcmp(argv[1], "ipconfig") == 0) {
		return cmd_net_ipconfig(cdev, argc, argv);
	} else if (vmm_strcmp(argv[1], "ports") == 0) {
		return cmd_net_port_list(cdev, argc, argv);
	} else if (vmm_strcmp(argv[1], "switches") == 0) {
		return cmd_net_switch_list(cdev, argc, argv);
	}
	cmd_net_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_net = {
	.name = "net",
	.desc = "network commands",
	.usage = cmd_net_usage,
	.exec = cmd_net_exec,
};

static int __init cmd_net_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_net);
}

static void cmd_net_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_net);
}

VMM_DECLARE_MODULE(MODULE_VARID, 
			MODULE_NAME, 
			MODULE_AUTHOR, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);


