/**
 * Copyright (c) 2013 Himanshu Chauhan.
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
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief header file for boot time or early parameters.
 */
#ifndef _VMM_PARAMS_H__
#define _VMM_PARAMS_H__

struct vmm_setup_param {
        const char *str;
        int (*setup_func)(char *);
        int early;
};

/*
 * Only for really core code.  See vmm_module.h for the normal way.
 * vmm_param "array" too far apart in .init.setup.
 */
#define __setup_param(str, unique_id, fn, early)			\
	static const char __setup_str_##unique_id[] __initconst		\
	__aligned(1) = str;						\
        static struct vmm_setup_param __setup_##unique_id		\
	__used __section(.setup.init)					\
	__attribute__((aligned((sizeof(long)))))			\
	= { __setup_str_##unique_id, fn, early }

#define __setup(str, fn)			\
        __setup_param(str, fn, fn, 0)

/* NOTE: fn is as per module_param, not __setup!  Emits warning if fn
 * returns non-zero. */
#define vmm_early_param(str, fn)		\
        __setup_param(str, fn, fn, 1)

/* Parse boot time or early parameters */
void vmm_parse_early_options(const char *cmdline);

#endif /* _VMM_PARAMS_H__ */
