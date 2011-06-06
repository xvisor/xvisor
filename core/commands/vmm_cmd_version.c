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
 * @file vmm_cmd_version.c
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of version command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_version.h>
#include <vmm_mterm.h>

int cmd_version_exec(int argc, char **argv)
{
	vmm_printf("%s Version %d.%d (%s %s)\n",
		   VMM_PROJECT_NAME, VMM_PROJECT_VER_MAJOR,
		   VMM_PROJECT_VER_MINOR, __DATE__, __TIME__);
	return VMM_OK;
}

VMM_DECLARE_CMD(version, "show version of hypervisor", cmd_version_exec, NULL);
