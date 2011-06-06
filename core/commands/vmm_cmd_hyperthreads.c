/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file vmm_cmd_hyperthreads.c
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Command file for hyperthreads control.
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_version.h>
#include <vmm_mterm.h>
#include <vmm_string.h>

void print_hyperthread_help(void)
{
	vmm_printf("hyperthreads:\n");
	vmm_printf("    - hyperthreads show\n");
	vmm_printf("        Show currently running hyperthreads\n");
}

int cmd_hyperthread_exec(int argc, char **argv)
{
	if (argc <= 1) {
		print_hyperthread_help();
		return -1;
	}

	if (!vmm_strcmp(argv[1], "show")) {
		vmm_hyperthreads_print_all_info();
	} else {
		vmm_printf
		    ("hyperthread %s: Unknown command. Seep help below.\n",
		     argv[1]);
		print_hyperthread_help();
		return -1;
	}

	return VMM_OK;
}

VMM_DECLARE_CMD(hyperthreads, "control commands for hyperthreads",
		cmd_hyperthread_exec, NULL);
