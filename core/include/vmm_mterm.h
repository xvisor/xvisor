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
 * @file vmm_mterm.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file managment terminal
 */
#ifndef _VMM_MTERM_H__
#define _VMM_MTERM_H__

#include <vmm_sections.h>
#include <vmm_types.h>
#include <vmm_threads.h>

#define VMM_CMD_SIGNATURE		0x4D434D44

typedef int (*vmm_cmd_exec_t) (int, char **);
typedef int (*vmm_cmd_init_t) (void);

struct vmm_cmd {
	u32 signature;
	s8 name[16];
	s8 desc[72];
	vmm_cmd_exec_t exec;
	vmm_cmd_init_t init;
};

typedef struct vmm_cmd vmm_cmd_t;

#define VMM_DECLARE_CMD(name,desc,exec,init) __cmdtbl_section \
vmm_cmd_t name = { VMM_CMD_SIGNATURE, #name, desc, exec, init }

struct vmm_mterm_ctrl {
	vmm_cmd_t *table;
	u32 table_size;
	u32 cmd_count;
	vmm_thread_t *thread;
};

typedef struct vmm_mterm_ctrl vmm_mterm_ctrl_t;

/** Execute command */
int vmm_mterm_exec_cmd(int argc, char **argv);

/** Process command string */
int vmm_mterm_proc_cmdstr(char *cmds);

/** Initialize Managment terminal */
int vmm_mterm_init(void);

#endif
