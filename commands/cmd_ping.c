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
 * @file cmd_ping.c
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief Implementation of ping command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_version.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <net/vmm_netstack.h>
#include <net/vmm_protocol.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>

#define MODULE_DESC			"Command ping"
#define MODULE_AUTHOR			"Sukanto Ghosh"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_ping_init
#define	MODULE_EXIT			cmd_ping_exit

void cmd_ping_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage: ");
	vmm_cprintf(cdev, "   ping <ipaddr> [<count>] [<size>]\n");
}

int cmd_ping_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	u16 sent, rcvd, count = 1, size = 56;
	struct vmm_icmp_echo_reply reply;
	char ip_addr_str[30];
	u32 rtt_usecs, rtt_msecs;
	u64 min_rtt = -1, max_rtt = 0, avg_rtt = 0;
	u8 ipaddr[4];
	if((argc < 2) || (argc > 4)) {
		cmd_ping_usage(cdev);
		return VMM_EFAIL;
	}
	if(argc > 2) {
		count = str2int(argv[2], 10);
	}
	if(argc > 3) {
		size = str2int(argv[3], 10);
	}
	str2ipaddr(ipaddr, argv[1]);

	vmm_cprintf(cdev, "PING (%s) %d(%d) bytes of data.\n", 
			  argv[1], size, (size + IP4_HLEN + ICMP_HLEN));

	vmm_netstack_prefetch_arp_mapping(ipaddr);

	for(sent=0, rcvd=0; sent<count; sent++) {
		if(!vmm_netstack_send_icmp_echo(ipaddr, size, sent, &reply)) {
			if(reply.rtt < min_rtt)
				min_rtt = reply.rtt;
			if(reply.rtt > max_rtt)
				max_rtt = reply.rtt;
			avg_rtt += reply.rtt;
			rtt_msecs = udiv64(reply.rtt, 1000);
			rtt_usecs = umod64(reply.rtt, 1000);
			ip4addr_to_str(ip_addr_str, (const u8 *)&reply.ripaddr);
			vmm_cprintf(cdev, "%d bytes from %s: seq=%d "
				    "ttl=%d time=%d.%03dms\n", reply.len,
				    ip_addr_str, reply.seqno, reply.ttl,
				    rtt_msecs, rtt_usecs);
			rcvd++;
		}
	}
	if(rcvd) {
		avg_rtt = udiv64(avg_rtt, rcvd);
	}
	else {
		avg_rtt = 0;
	}
	vmm_cprintf(cdev, "\n----- %s ping statistics -----\n", argv[1]);
	vmm_cprintf(cdev, "%d packets transmitted, %d packets received\n", 
								sent, rcvd);
	vmm_cprintf(cdev, "round-trip min/avg/max = ");
	rtt_msecs = udiv64(min_rtt, 1000);
	rtt_usecs = umod64(min_rtt, 1000);
	vmm_cprintf(cdev, "%d.%03d/", rtt_msecs, rtt_usecs); 
	rtt_msecs = udiv64(avg_rtt, 1000);
	rtt_usecs = umod64(avg_rtt, 1000);
	vmm_cprintf(cdev, "%d.%03d/", rtt_msecs, rtt_usecs); 
	rtt_msecs = udiv64(max_rtt, 1000);
	rtt_usecs = umod64(max_rtt, 1000);
	vmm_cprintf(cdev, "%d.%03d ms\n", rtt_msecs, rtt_usecs); 

	return VMM_OK;
}

static struct vmm_cmd cmd_ping = {
	.name = "ping",
	.desc = "ping target machine on network",
	.usage = cmd_ping_usage,
	.exec = cmd_ping_exec,
};

static int __init cmd_ping_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_ping);
}

static void __exit cmd_ping_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_ping);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
