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
 * @file vmm_mterm.c
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
#include <vmm_scheduler.h>
#include <vmm_mterm.h>
#include <vmm_board.h>
#include <vmm_wait.h>

#define VMM_CMD_STRING_SIZE	256
#define VMM_CMD_DELIM_CHAR	';'
#define VMM_CMD_ARG_MAXCOUNT	32
#define VMM_CMD_ARG_DELIM_CHAR	' '
#define VMM_CMD_ARG_DELIM_CHAR1	'\t'

vmm_mterm_ctrl_t mterm_ctrl;

int vmm_mterm_exec_cmd(int argc, char **argv)
{
	int cmd_ret;
	u32 i, found;
	found = 0;
	/* Find & execute the commad */
	for (i = 0; i < mterm_ctrl.cmd_count; i++) {
		/* Match the command name */
		if (vmm_strcmp(mterm_ctrl.table[i].name, argv[0]) == 0) {
			cmd_ret = mterm_ctrl.table[i].exec(argc, argv);
			if (cmd_ret) {
				vmm_printf("Error %d: "
					   "Command Failed\n", cmd_ret);
				return cmd_ret;
			}
			found = 1;
			break;
		}
	}
	/* Print error if command not found */
	if (!found) {
		vmm_printf("Unknown Command - %s\n", argv[0]);
		return VMM_ENOTAVAIL;
	}
	return VMM_OK;
}

int vmm_mterm_proc_cmdstr(char *cmds)
{
	int argc, cmd_ret;
	char *argv[VMM_CMD_ARG_MAXCOUNT];
	char *c = cmds;
	argc = 0;
	while (*c) {
		while (*c == VMM_CMD_ARG_DELIM_CHAR ||
		       *c == VMM_CMD_ARG_DELIM_CHAR1) {
			c++;
		}
		if (*c == '\0') {
			break;
		}
		if (argc < VMM_CMD_ARG_MAXCOUNT && *c != VMM_CMD_DELIM_CHAR) {
			argv[argc] = c;
			argc++;
		}
		while (*c != VMM_CMD_ARG_DELIM_CHAR &&
		       *c != VMM_CMD_ARG_DELIM_CHAR1 &&
		       *c != VMM_CMD_DELIM_CHAR && *c != '\0') {
			c++;
		}
		if ((*c == VMM_CMD_DELIM_CHAR || *c == '\0') && argc > 0) {
			*c = '\0';
			c++;
			cmd_ret = vmm_mterm_exec_cmd(argc, argv);
			if (cmd_ret)
				return cmd_ret;
			argc = 0;
		} else {
			*c = '\0';
			c++;
		}
	}
	if (argc > 0) {
		cmd_ret = vmm_mterm_exec_cmd(argc, argv);
		if (cmd_ret)
			return cmd_ret;
	}
	return VMM_OK;
}

void vmm_mterm_main(void *udata)
{
	size_t cmds_len;
	char cmds[VMM_CMD_STRING_SIZE];

	/* Print Banner */
	vmm_printf("%s", VMM_BANNER_STRING);

	/* Main loop of VMM */
	while (1) {
		/* Show prompt */
		vmm_printf("XVisor# ");
		vmm_memset(cmds, 0, sizeof(cmds));

		/* Get command string */
		vmm_gets(cmds, VMM_CMD_STRING_SIZE, '\n');
		cmds_len = vmm_strlen(cmds);
		if (cmds_len > 0) {
			if (cmds[cmds_len - 1] == '\r')
				cmds[cmds_len - 1] = '\0';
		}

		/* Process command string */
		vmm_mterm_proc_cmdstr(cmds);
	}
}

int vmm_mterm_start(void)
{
	mterm_ctrl.thread =
	    vmm_hyperthread_create("mterm", (void *)&vmm_mterm_main, NULL);

	if (!mterm_ctrl.thread) {
		vmm_panic("Creation of system critical thread failed.\n");
	}

	vmm_hyperthread_run(mterm_ctrl.thread);

	return VMM_OK;
}

int vmm_mterm_init(void)
{
	int ret;
	u32 i;

	/* Reset the control structure */
	vmm_memset(&mterm_ctrl, 0, sizeof(mterm_ctrl));

	/* Initialize the control structure */
	mterm_ctrl.table = (vmm_cmd_t *) vmm_cmdtbl_start();
	mterm_ctrl.table_size = vmm_cmdtbl_size() / sizeof(vmm_cmd_t);
	mterm_ctrl.cmd_count = 0;
	mterm_ctrl.thread = NULL;

	/* Find out available commands and initialize them */
	for (i = 0; i < mterm_ctrl.table_size; i++) {
		/* Check validity of command table entry */
		if (mterm_ctrl.table[i].signature == VMM_CMD_SIGNATURE) {
			/* Initialize command if required */
			if (mterm_ctrl.table[i].init) {
				ret = mterm_ctrl.table[i].init();
				if (ret) {
					vmm_printf("Error %d: "
						   "Initializing Command %s Failed\n",
						   ret,
						   mterm_ctrl.table[i].name);
				}
			}
			/* Increment count in control structure */
			mterm_ctrl.cmd_count++;
		} else {
			break;
		}
	}

	return VMM_OK;
}

int cmd_help_exec(int argc, char **argv)
{
	u32 i;
	for (i = 0; i < mterm_ctrl.cmd_count; i++) {
		if (mterm_ctrl.table[i].name) {
			if (mterm_ctrl.table[i].desc) {
				vmm_printf("%-12s - %s\n",
					   mterm_ctrl.table[i].name,
					   mterm_ctrl.table[i].desc);
			} else {
				vmm_printf("%-12s - \n",
					   mterm_ctrl.table[i].name);
			}
		}
	}
	return 0;
}

VMM_DECLARE_CMD(help, "displays list of all commands", cmd_help_exec, NULL);
