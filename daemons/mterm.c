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
 * @file mterm.c
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief source file of managment terminal
 */

#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_stdio.h>
#include <vmm_version.h>
#include <vmm_devtree.h>
#include <vmm_threads.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>

#define MODULE_VARID			daemon_mterm_module
#define MODULE_NAME			"Managment Terminal"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			daemon_mterm_init
#define	MODULE_EXIT			daemon_mterm_exit

#define MTERM_CMD_STRING_SIZE		256

static struct mterm_ctrl {
	struct vmm_thread *thread;
} mtctrl;

static int mterm_main(void *udata)
{
	size_t cmds_len;
	char cmds[MTERM_CMD_STRING_SIZE];

	/* Print Banner */
	vmm_printf("%s", VMM_BANNER_STRING);

	/* Main loop of VMM */
	while (1) {
		/* Show prompt */
		vmm_printf("XVisor# ");
		vmm_memset(cmds, 0, sizeof(cmds));

		/* Get command string */
		vmm_gets(cmds, MTERM_CMD_STRING_SIZE, '\n');
		cmds_len = vmm_strlen(cmds);
		if (cmds_len > 0) {
			if (cmds[cmds_len - 1] == '\r')
				cmds[cmds_len - 1] = '\0';

			/* Execute command string */
			vmm_cmdmgr_execute_cmdstr(vmm_stdio_device(), cmds);
		}
	}

	return VMM_OK;
}

static int __init daemon_mterm_init(void)
{
	u8 mterm_priority;
	u32 mterm_time_slice;
	struct vmm_devtree_node * node;
	const char * attrval;

	/* Reset the control structure */
	vmm_memset(&mtctrl, 0, sizeof(mtctrl));

	/* Retrive mterm time slice */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_VMMINFO_NODE_NAME);
	if (!node) {
		return VMM_EFAIL;
	}
	attrval = vmm_devtree_attrval(node,
				      "mterm_priority");
	if (attrval) {
		mterm_priority = *((u32 *) attrval);
	} else {
		mterm_priority = VMM_THREAD_DEF_PRIORITY;
	}
	attrval = vmm_devtree_attrval(node,
				      "mterm_time_slice");
	if (attrval) {
		mterm_time_slice = *((u32 *) attrval);
	} else {
		mterm_time_slice = VMM_THREAD_DEF_TIME_SLICE;
	}

	/* Create mterm thread */
	mtctrl.thread = vmm_threads_create("mterm", 
					   &mterm_main, 
					   NULL, 
					   mterm_priority,
					   mterm_time_slice);
	if (!mtctrl.thread) {
		vmm_panic("Creation of system critical thread failed.\n");
	}

	/* Start the mterm thread */
	vmm_threads_start(mtctrl.thread);

	return VMM_OK;
}

static void daemon_mterm_exit(void)
{
	vmm_threads_stop(mtctrl.thread);

	vmm_threads_destroy(mtctrl.thread);
}

VMM_DECLARE_MODULE(MODULE_VARID, 
			MODULE_NAME, 
			MODULE_AUTHOR, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
