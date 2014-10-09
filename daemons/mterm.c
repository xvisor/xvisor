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
 * @author Anup Patel (anup@brainfault.org)
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief source file of managment terminal
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_main.h>
#include <vmm_delay.h>
#include <vmm_version.h>
#include <vmm_devtree.h>
#include <vmm_threads.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"Managment Terminal"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			daemon_mterm_init
#define	MODULE_EXIT			daemon_mterm_exit

static struct mterm_ctrl {
	struct vmm_thread *thread;
#ifdef CONFIG_MTERM_HISTORY
	struct vmm_history history;
#endif
} mtctrl;

static int mterm_main(void *udata)
{
	size_t cmds_len;
	char cmds[CONFIG_MTERM_CMD_WIDTH];
	struct vmm_chardev *cdev;

	/* Sleep here if initialization is not complete */
	while (!vmm_init_done()) {
		vmm_msleep(100);
	}

	/* Print Banner */
	vmm_printf("%s", VMM_BANNER_STRING);

	/* Main loop of VMM */
	while (1) {
		/* Show prompt */
		vmm_printf("XVisor# ");
		memset(cmds, 0, sizeof(cmds));

		/* Get command string */
#ifdef CONFIG_MTERM_HISTORY
		vmm_gets(cmds, CONFIG_MTERM_CMD_WIDTH, '\n', &mtctrl.history);
#else
		vmm_gets(cmds, CONFIG_MTERM_CMD_WIDTH, '\n', NULL);
#endif

		/* Process command string */
		cmds_len = strlen(cmds);
		if (cmds_len > 0) {
			if (cmds[cmds_len - 1] == '\r')
				cmds[cmds_len - 1] = '\0';

			/* Execute command string */
			cdev = vmm_stdio_device();
			vmm_cmdmgr_execute_cmdstr(cdev, cmds, NULL);
		}
	}

	return VMM_OK;
}

static int __init daemon_mterm_init(void)
{
	u32 mterm_priority;
	u32 mterm_time_slice;
	struct vmm_devtree_node *node;

	/* Reset the control structure */
	memset(&mtctrl, 0, sizeof(mtctrl));

#ifdef CONFIG_MTERM_HISTORY
	INIT_HISTORY(&mtctrl.history, 
			CONFIG_MTERM_HISTORY_SIZE, CONFIG_MTERM_CMD_WIDTH);
#endif

	/* Retrive mterm time slice */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_VMMINFO_NODE_NAME);
	if (!node) {
		return VMM_EFAIL;
	}
	if (vmm_devtree_read_u32(node,
				 "mterm_priority", &mterm_priority)) {
		mterm_priority = VMM_THREAD_DEF_PRIORITY;
	}
	if (vmm_devtree_read_u32(node,
				 "mterm_time_slice", &mterm_time_slice)) {
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

static void __exit daemon_mterm_exit(void)
{
	vmm_threads_stop(mtctrl.thread);

	vmm_threads_destroy(mtctrl.thread);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
