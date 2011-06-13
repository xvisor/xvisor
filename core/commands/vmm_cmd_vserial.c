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
 * @file vmm_cmd_vserial.c
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of vserial command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_devtree.h>
#include <vmm_vserial.h>
#include <vmm_mterm.h>

void cmd_vserial_usage(void)
{
	vmm_printf("Usage:\n");
	vmm_printf("   vserial help\n");
	vmm_printf("   vserial list\n");
}

void cmd_vserial_list()
{
	int num, count;
	vmm_vserial_t *vser;
	count = vmm_vserial_count();
	for (num = 0; num < count; num++) {
		vser = vmm_vserial_get(num);
		vmm_printf("%d: %s\n", num, vser->name);
	}
}

int cmd_vserial_exec(int argc, char **argv)
{
	if (argc == 2) {
		if (vmm_strcmp(argv[1], "help") == 0) {
			cmd_vserial_usage();
			return VMM_OK;
		} else if (vmm_strcmp(argv[1], "list") == 0) {
			cmd_vserial_list();
			return VMM_OK;
		}
	}
	if (argc < 3) {
		cmd_vserial_usage();
		return VMM_EFAIL;
	}
	return VMM_OK;
}

VMM_DECLARE_CMD(vserial, "virtual serial port commands", cmd_vserial_exec, NULL);
