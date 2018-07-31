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
 * @file vmm_net.c
 * @author Sukanto Ghosh <sukantoghosh@gmail.com>
 * @brief Network framework.
 */

#include <vmm_types.h>
#include <vmm_error.h>
#include <vmm_modules.h>
#include <vmm_stdio.h>
#include <net/vmm_net.h>
#include <net/vmm_netswitch.h>

#define MODULE_DESC			"Network Framework"
#define MODULE_AUTHOR			"Sukanto Ghosh"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_NET_CLASS_IPRIORITY)
#define	MODULE_INIT			vmm_net_init
#define	MODULE_EXIT			vmm_net_exit

static int __init vmm_net_init(void)
{
	int rc = VMM_OK;

	rc = vmm_mbufpool_init();
	if (rc) {
		vmm_printf("%s: Failed to init mbuf pool\n", __func__);
		goto mbufpool_init_failed;
	}

	rc = vmm_netswitch_init();
	if (rc) {
		vmm_printf("%s: Failed to init netswitch\n", __func__);
		goto netswitch_init_failed;
	}

	rc = vmm_netport_init();
	if (rc) {
		vmm_printf("%s: Failed to init netport\n", __func__);
		goto netport_init_failed;
	}

	rc = vmm_hub_init();
	if (rc) {
		vmm_printf("%s: Failed to init hub\n", __func__);
		goto hub_init_failed;
	}

	rc = vmm_bridge_init();
	if (rc) {
		vmm_printf("%s: Failed to init bridge\n", __func__);
		goto bridge_init_failed;
	}

#ifdef CONFIG_NET_AUTOCREATE_BRIDGE
	rc = vmm_netswitch_policy_create_switch("bridge",
				CONFIG_NET_AUTOCREATE_BRIDGE_NAME, 0, NULL);
	if (rc) {
		goto net_autocreate_failed;
	}
#endif

	goto net_init_done;

#ifdef CONFIG_NET_AUTOCREATE_BRIDGE
net_autocreate_failed:
	vmm_bridge_exit();
#endif
bridge_init_failed:
	vmm_hub_exit();
hub_init_failed:
	vmm_netport_exit();
netport_init_failed:
	vmm_netswitch_exit();
netswitch_init_failed:
	vmm_mbufpool_exit();
mbufpool_init_failed:

net_init_done:
	return rc;
}

static void __exit vmm_net_exit(void)
{
	vmm_bridge_exit();
	vmm_hub_exit();

	vmm_netport_exit();
	vmm_netswitch_exit();
	vmm_mbufpool_exit();
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
