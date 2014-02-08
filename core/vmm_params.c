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
 * @brief source for boot time or early parameters.
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_compiler.h>
#include <vmm_params.h>
#include <libs/stringlib.h>
#include <arch_cpu_irq.h>

extern const struct vmm_setup_param __setup_start[], __setup_end[];

static char dash2underscore(char c)
{
        if (c == '-')
                return '_';
        return c;
}

static bool parameqn(const char *a, const char *b, size_t n)
{
        size_t i;

        for (i = 0; i < n; i++) {
                if (dash2underscore(a[i]) != dash2underscore(b[i]))
                        return false;
	}
        return true;
}

static bool parameq(const char *a, const char *b)
{
        return parameqn(a, b, strlen(a)+1);
}

/* Check for early params. */
static int __init do_early_param(char *param, char *val, const char *unused)
{
	const struct vmm_setup_param *p;

        for (p = __setup_start; p < __setup_end; p++) {
                if ((p->early && parameq(param, p->str)) ||
			(strcmp(param, "console") == 0 &&
				strcmp(p->str, "earlycon") == 0)
			) {
                        p->setup_func(val);
                }
        }
        /* We accept everything at this stage. */
        return 0;
}

/* You can use " around spaces, but can't escape ". */
/* Hyphens and underscores equivalent in parameter names. */
static char *next_arg(char *args, char **param, char **val)
{
	unsigned int i, equals = 0;
	int in_quote = 0, quoted = 0;
	char *next;

	if (*args == '"') {
		args++;
		in_quote = 1;
		quoted = 1;
	}

	for (i = 0; args[i]; i++) {
		if (isspace(args[i]) && !in_quote)
			break;
		if (equals == 0) {
			if (args[i] == '=')
				equals = i;
		}
		if (args[i] == '"')
			in_quote = !in_quote;
	}

	*param = args;
	if (!equals)
		*val = NULL;
	else {
		args[equals] = '\0';
		*val = args + equals + 1;

		/* Don't include quotes in value. */
		if (**val == '"') {
			(*val)++;
			if (args[i-1] == '"')
				args[i-1] = '\0';
		}
		if (quoted && args[i-1] == '"')
			args[i-1] = '\0';
	}

	if (args[i]) {
		args[i] = '\0';
		next = args + i + 1;
	} else
		next = args + i;

	/* Chew up trailing spaces. */
	return skip_spaces(next);
}

/* Args looks like "foo=bar,bar2 baz=fuz wiz". */
static int parse_args(const char *doing,
		     char *args,
		     unsigned num,
		     s16 min_level,
		     s16 max_level,
		     int (*unknown)(char *param, char *val, const char *doing))
{
	char *param, *val;

	/* Chew leading spaces */
	args = skip_spaces(args);

	while (*args) {
		int ret;

		args = next_arg(args, &param, &val);
		if (unknown) {
			ret = unknown(param, val, doing);

			if (ret)
				return ret;
		}
	}

	/* All parsed OK. */
	return 0;
}

void __init vmm_parse_early_options(const char *cmdline)
{
        parse_args("early options", (char *)cmdline, 0, 0, 0, do_early_param);
}
