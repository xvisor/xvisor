/**
 * Copyright (c) 2022 Himanshu Chauhan.
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
 * @file x86_debug_log.h
 * @author Himanshu Chauhan (hchauhan@xvisor-x86.org)
 * @brief Header file for addings logs for debugging.
 *
 * This is different then STDIO logging facilities. This debug log
 * facility can be used sub-system wise where each subsystem can
 * have a different log level. This can stop the clutter printed
 * on console when all subsystem use the same log levels.
 *
 * TODO: Make this a wrapper to vmm_stdio logging so that color
 * coding can be used, if required.
 */

#ifndef __X86_DEBUG_LOG_H
#define __X86_DEBUG_LOG_H

enum {
	X86_DEBUG_LOG_LVL_ERR,
	X86_DEBUG_LOG_LVL_INFO,
	X86_DEBUG_LOG_LVL_DEBUG,
	X86_DEBUG_LOG_LVL_VERBOSE
};

#define DECLARE_X86_DEBUG_LOG_SUBSYS_LEVEL(subsys)	\
	int vmm_debug_ ## subsys ## _log_lvl 

#define DEFINE_X86_DEBUG_LOG_SUBSYS_LEVEL(subsys, lvl)		\
	DECLARE_X86_DEBUG_LOG_SUBSYS_LEVEL(subsys) __read_mostly = lvl

#define X86_DEBUG_LOG_SUBSYS_LEVEL(subsys)	\
	vmm_debug_##subsys##_log_lvl

#define X86_DEBUG_LOG(subsys, lvl, fmt, args...)			\
	do {								\
		extern DECLARE_X86_DEBUG_LOG_SUBSYS_LEVEL(subsys);	\
		if (X86_DEBUG_LOG_ ## lvl <= X86_DEBUG_LOG_SUBSYS_LEVEL(subsys)) { \
			vmm_printf(fmt, ##args);			\
		}							\
	} while(0);

#endif
