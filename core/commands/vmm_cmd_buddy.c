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
 * @file vmm_cmd_buddy.c
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Implementation of buddy allocator current usage.
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_version.h>
#include <vmm_mterm.h>
#include <vmm_string.h>
#include <mm/vmm_buddy.h>

void print_buddy_help(void)
{
	vmm_printf("buddy: Show current heap statistics.\n");
	vmm_printf("    - buddy state\n");
	vmm_printf("        Show current allocation statistics.\n");
	vmm_printf("    - buddy hk-state\n");
	vmm_printf("        Show current house keeping nodes state.\n");
}

int cmd_buddy_exec(int argc, char **argv)
{
	if (argc <= 1) {
		print_buddy_help();
		return -1;
	}

	if (!vmm_strcmp(argv[1], "state")) {
		print_current_buddy_state();
	} else if (!vmm_strcmp(argv[1], "hk-state")) {
		print_current_hk_state();
	} else {
		vmm_printf("buddy %s: Unknown command. Seep help below.\n");
		print_buddy_help();
		return -1;
	}

	return VMM_OK;
}

VMM_DECLARE_CMD(buddy, "Show current heap usage.", cmd_buddy_exec, NULL);
