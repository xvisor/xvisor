/*
 * Copyright (C) 2002-2005 Roman Zippel <zippel@linux-m68k.org>
 * Copyright (C) 2002-2005 Sam Ravnborg <sam@ravnborg.org>
 * Released under the terms of the GNU GPL v2.0.
 */

#include <string.h>
#include "openconf.h"

/* file already present in list? If not add it */
struct file *file_lookup(const char *name)
{
	struct file *file;

	for (file = file_list; file; file = file->next) {
		if (!strcmp(name, file->name))
			return file;
	}

	file = malloc(sizeof(*file));
	memset(file, 0, sizeof(*file));
	file->name = strdup(name);
	file->next = file_list;
	file_list = file;
	return file;
}

/* write a dependency file as used by build to track dependencies */
int file_write_dep(const char *name)
{
	char *tname,path[PATH_MAX],tmppath[PATH_MAX];
	struct symbol *sym, *env_sym;
	struct expr *e;
	struct file *file;
	FILE *out;

	tname = getenv(OPENCONF_TMPDIR_ENVNAME);
	if (tname)
		strcpy(tmppath, tname);
	else
		strcpy(tmppath, OPENCONF_TMPDIR_DEFAULT);

	if (!name)
		name = ".openconf.d";
	out = fopen("..config.tmp", "w");
	if (!out)
		return 1;
	fprintf(out, "deps_config := \\\n");
	for (file = file_list; file; file = file->next) {
		if (file->next)
			fprintf(out, "\t%s \\\n", file->name);
		else
			fprintf(out, "\t%s\n", file->name);
	}

	strcpy(path, tmppath);
	strcat(path, "/");
	tname = getenv(OPENCONF_AUTOCONFIG_ENVNAME);
	if (!tname)
		tname = OPENCONF_AUTOCONFIG_DEFAULT;
	strcat(path, tname);
	fprintf(out, "\n%s: \\\n"
		     "\t$(deps_config)\n\n", path);

	expr_list_for_each_sym(sym_env_list, e, sym) {
		struct property *prop;
		const char *value;

		prop = sym_get_env_prop(sym);
		env_sym = prop_get_symbol(prop);
		if (!env_sym)
			continue;
		value = getenv(env_sym->name);
		if (!value)
			value = "";
		fprintf(out, "ifneq \"$(%s)\" \"%s\"\n", env_sym->name, value);
		fprintf(out, "%s: FORCE\n", path);
		fprintf(out, "endif\n");
	}

	fprintf(out, "\n$(deps_config): ;\n");
	fclose(out);
	rename("..config.tmp", name);
	return 0;
}


/* Allocate initial growable sting */
struct gstr str_new(void)
{
	struct gstr gs;
	gs.s = malloc(sizeof(char) * 64);
	gs.len = 64;
	strcpy(gs.s, "\0");
	return gs;
}

/* Allocate and assign growable string */
struct gstr str_assign(const char *s)
{
	struct gstr gs;
	gs.s = strdup(s);
	gs.len = strlen(s) + 1;
	return gs;
}

/* Free storage for growable string */
void str_free(struct gstr *gs)
{
	if (gs->s)
		free(gs->s);
	gs->s = NULL;
	gs->len = 0;
}

/* Append to growable string */
void str_append(struct gstr *gs, const char *s)
{
	size_t l;
	if (s) {
		l = strlen(gs->s) + strlen(s) + 1;
		if (l > gs->len) {
			gs->s   = realloc(gs->s, l);
			gs->len = l;
		}
		strcat(gs->s, s);
	}
}

/* Append printf formatted string to growable string */
void str_printf(struct gstr *gs, const char *fmt, ...)
{
	va_list ap;
	char s[10000]; /* big enough... */
	va_start(ap, fmt);
	vsnprintf(s, sizeof(s), fmt, ap);
	str_append(gs, s);
	va_end(ap);
}

/* Retrieve value of growable string */
const char *str_get(struct gstr *gs)
{
	return gs->s;
}

