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
 * @file vmm_cmd_shutdown.c
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of shutdown command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_main.h>
#include <vmm_mterm.h>

int cmd_shutdown_exec(int argc, char **argv)
{
	/* Reset the hypervisor */
	vmm_shutdown();
	return VMM_OK;
}

VMM_DECLARE_CMD(shutdown, "shutdown hypervisor", cmd_shutdown_exec, NULL);
