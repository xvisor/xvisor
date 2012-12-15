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
 * @file vmm_cmdmgr.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file command manager
 */
#ifndef _VMM_CMDMGR_H__
#define _VMM_CMDMGR_H__

#include <vmm_types.h>
#include <vmm_chardev.h>
#include <libs/list.h>

struct vmm_cmd {
	struct dlist head;
	char name[32];
	char desc[72];
	void (*usage) (struct vmm_chardev *);
	int (*exec) (struct vmm_chardev *, int, char **);
};

/** Register command */
int vmm_cmdmgr_register_cmd(struct vmm_cmd *cmd);

/** Unregister command */
int vmm_cmdmgr_unregister_cmd(struct vmm_cmd *cmd);

/** Find a registered command */
struct vmm_cmd *vmm_cmdmgr_cmd_find(const char *cmd_name);

/** Get a registered command */
struct vmm_cmd *vmm_cmdmgr_cmd(int index);

/** Count of registered commands */
u32 vmm_cmdmgr_cmd_count(void);

/** Execute command */
int vmm_cmdmgr_execute_cmd(struct vmm_chardev *cdev, int argc, char **argv);

/** Execute command string */
int vmm_cmdmgr_execute_cmdstr(struct vmm_chardev *cdev, char *cmds);

/** Initialize command manager */
int vmm_cmdmgr_init(void);

#endif
