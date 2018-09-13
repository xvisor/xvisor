/*
 * (C) Copyright 2007
 * Gerald Van Baren, Custom IDEAS, vanbaren@cideas.com
 * Based on code written by:
 *   Pantelis Antoniou <pantelis.antoniou@gmail.com> and
 *   Matthew McClintock <msm@freescale.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <arm_string.h>
#include <arm_stdio.h>
#include <ctype.h>
#include <arm_types.h>
#include <libfdt/fdt.h>
#include <libfdt/libfdt.h>
#include <libfdt/fdt_support.h>

#define MAX_LEVEL	32		/* how deeply nested we will go */
#define SCRATCHPAD	1024		/* bytes of scratchpad memory */

//static int fdt_valid(void);
//static int fdt_parse_prop(char *const*newval, int count, char *data, int *len);
static int fdt_print(const char *pathp, char *prop, int depth);

/*
 * The working_fdt points to our working flattened device tree.
 */
struct fdt_header *working_fdt;

#if 0
static void usage(void)
{
	printf("Usage: fdt addr   <addr> [<length>]        - Set the fdt location to <addr>\n"
		"fdt move   <fdt> <newaddr> <length> - Copy the fdt to <addr> and make it active\n"
		"fdt resize                          - Resize fdt to size + padding to 4k addr\n"
		"fdt print  <path> [<prop>]          - Recursive print starting at <path>\n"
		"fdt list   <path> [<prop>]          - Print one level starting at <path>\n"
		"fdt set    <path> <prop> [<val>]    - Set <property> [to <val>]\n"
		"fdt mknode <path> <node>            - Create a new node after <path>\n"
		"fdt rm     <path> [<prop>]          - Delete the node or <property>\n"
		"fdt header                          - Display header info\n"
		"fdt bootcpu <id>                    - Set boot cpuid\n"
		"fdt memory <addr> <size>            - Add/Update memory node\n"
		"fdt rsvmem print                    - Show current mem reserves\n"
		"fdt rsvmem add <addr> <size>        - Add a mem reserve\n"
		"fdt rsvmem delete <index>           - Delete a mem reserves\n"
		"fdt chosen [<start> <end>]          - Add/update the /chosen branch in the tree\n"
		"                                        <start>/<end> - initrd start/end addr\n"
		"NOTE: Dereference aliases by omiting the leading '/', "
			"e.g. fdt print ethernet0.");
}
#endif

void set_working_fdt_addr(void *addr)
{
	working_fdt = addr;
}

void dump_fdt(unsigned long fdt_addr)
{
	set_working_fdt_addr((void *)fdt_addr);
	fdt_print("/", NULL, 32);
}
/****************************************************************************/

#if 0
static int fdt_valid(void)
{
	int  err;

	if (working_fdt == NULL) {
		printf ("The address of the fdt is invalid (NULL).\n");
		return 0;
	}

	err = fdt_check_header(working_fdt);
	if (err == 0)
		return 1;	/* valid */

	if (err < 0) {
		printf("libfdt fdt_check_header(): %s", fdt_strerror(err));
		/*
		 * Be more informative on bad version.
		 */
		if (err == -FDT_ERR_BADVERSION) {
			if (fdt_version(working_fdt) <
			    FDT_FIRST_SUPPORTED_VERSION) {
				printf (" - too old, fdt %d < %d",
					fdt_version(working_fdt),
					FDT_FIRST_SUPPORTED_VERSION);
				working_fdt = NULL;
			}
			if (fdt_last_comp_version(working_fdt) >
			    FDT_LAST_SUPPORTED_VERSION) {
				printf (" - too new, fdt %d > %d",
					fdt_version(working_fdt),
					FDT_LAST_SUPPORTED_VERSION);
				working_fdt = NULL;
			}
			return 0;
		}
		printf("\n");
		return 0;
	}
	return 1;
}
#endif

/****************************************************************************/

#if 0
/*
 * Parse the user's input, partially heuristic.  Valid formats:
 * <0x00112233 4 05>	- an array of cells.  Numbers follow standard
 *			C conventions.
 * [00 11 22 .. nn] - byte stream
 * "string"	- If the the value doesn't start with "<" or "[", it is
 *			treated as a string.  Note that the quotes are
 *			stripped by the parser before we get the string.
 * newval: An array of strings containing the new property as specified
 *	on the command line
 * count: The number of strings in the array
 * data: A bytestream to be placed in the property
 * len: The length of the resulting bytestream
 */
static int fdt_parse_prop(char * const *newval, int count, char *data, int *len)
{
	char *cp;		/* temporary char pointer */
	char *newp;		/* temporary newval char pointer */
	unsigned long tmp;	/* holds converted values */
	int stridx = 0;

	*len = 0;
	newp = newval[0];

	/* An array of cells */
	if (*newp == '<') {
		newp++;
		while ((*newp != '>') && (stridx < count)) {
			/*
			 * Keep searching until we find that last ">"
			 * That way users don't have to escape the spaces
			 */
			if (*newp == '\0') {
				newp = newval[++stridx];
				continue;
			}

			cp = newp;
			strtoul(cp, &newp, 0, &tmp);
			*(u32 *)data = cpu_to_be32(tmp);
			data  += 4;
			*len += 4;

			/* If the ptr didn't advance, something went wrong */
			if ((newp - cp) <= 0) {
				printf("Sorry, I could not convert \"%s\"\n",
					cp);
				return 1;
			}

			while (*newp == ' ')
				newp++;
		}

		if (*newp != '>') {
			printf("Unexpected character '%c'\n", *newp);
			return 1;
		}
	} else if (*newp == '[') {
		/*
		 * Byte stream.  Convert the values.
		 */
		newp++;
		while ((stridx < count) && (*newp != ']')) {
			while (*newp == ' ')
				newp++;
			if (*newp == '\0') {
				newp = newval[++stridx];
				continue;
			}
			if (!isxdigit(*newp))
				break;
			strtoul(newp, &newp, 0, &tmp);
			*data++ = tmp & 0xFF;
			*len    = *len + 1;
		}
		if (*newp != ']') {
			printf("Unexpected character '%c'\n", *newp);
			return 1;
		}
	} else {
		/*
		 * Assume it is one or more strings.  Copy it into our
		 * data area for convenience (including the
		 * terminating '\0's).
		 */
		while (stridx < count) {
			size_t length = strlen(newp) + 1;
			strcpy(data, newp);
			data += length;
			*len += length;
			newp = newval[++stridx];
		}
	}
	return 0;
}
#endif

/****************************************************************************/

/*
 * Heuristic to guess if this is a string or concatenated strings.
 */

#define in_range(c, lo, up)  ((u8)c >= lo && (u8)c <= up)
#define arm_isprint(c)           in_range(c, 0x20, 0x7f)

static int is_printable_string(const void *data, int len)
{
	const char *s = data;

	/* zero length is not */
	if (len == 0)
		return 0;

	/* must terminate with zero */
	if (s[len - 1] != '\0')
		return 0;

	/* printable or a null byte (concatenated strings) */
	while (((*s == '\0') || arm_isprint(*s)) && (len > 0)) {
		/*
		 * If we see a null, there are three possibilities:
		 * 1) If len == 1, it is the end of the string, printable
		 * 2) Next character also a null, not printable.
		 * 3) Next character not a null, continue to check.
		 */
		if (s[0] == '\0') {
			if (len == 1)
				return 1;
			if (s[1] == '\0')
				return 0;
		}
		s++;
		len--;
	}

	/* Not the null termination, or not done yet: not printable */
	if (*s != '\0' || (len != 0))
		return 0;

	return 1;
}


/*
 * Print the property in the best format, a heuristic guess.  Print as
 * a string, concatenated strings, a byte, word, double word, or (if all
 * else fails) it is printed as a stream of bytes.
 */
static void print_data(const void *data, int len)
{
	int j;

	/* no data, don't print */
	if (len == 0)
		return;

	/*
	 * It is a string, but it may have multiple strings (embedded '\0's).
	 */
	if (is_printable_string(data, len)) {
		printf("\"");
		j = 0;
		while (j < len) {
			if (j > 0)
				printf("\", \"");
			printf(data);
			j    += strlen(data) + 1;
			data += strlen(data) + 1;
		}
		printf("\"");
		return;
	}

	if ((len %4) == 0) {
		const u32 *p;

		printf("<");
		for (j = 0, p = data; j < len/4; j ++)
			printf("0x%x%s", fdt32_to_cpu(p[j]), j < (len/4 - 1) ? " " : "");
		printf(">");
	} else { /* anything else... hexdump */
		const u8 *s;

		printf("[");
		for (j = 0, s = data; j < len; j++)
			printf("%02x%s", s[j], j < len - 1 ? " " : "");
		printf("]");
	}
}

/****************************************************************************/

/*
 * Recursively print (a portion of) the working_fdt.  The depth parameter
 * determines how deeply nested the fdt is printed.
 */
static int fdt_print(const char *pathp, char *prop, int depth)
{
	static char tabs[MAX_LEVEL+1] =
		"\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t"
		"\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";
	const void *nodep;	/* property node pointer */
	int  nodeoffset;	/* node offset from libfdt */
	int  nextoffset;	/* next node offset from libfdt */
	u32 tag;		/* tag */
	int  len;		/* length of the property */
	int  level = 0;		/* keep track of nesting level */
	const struct fdt_property *fdt_prop;

	nodeoffset = fdt_path_offset (working_fdt, pathp);
	if (nodeoffset < 0) {
		/*
		 * Not found or something else bad happened.
		 */
		printf ("libfdt fdt_path_offset() returned %s\n",
			fdt_strerror(nodeoffset));
		return 1;
	}
	/*
	 * The user passed in a property as well as node path.
	 * Print only the given property and then return.
	 */
	if (prop) {
		nodep = fdt_getprop (working_fdt, nodeoffset, prop, &len);
		if (len == 0) {
			/* no property value */
			printf("%s %s\n", pathp, prop);
			return 0;
		} else if (len > 0) {
			printf("%s = ", prop);
			print_data (nodep, len);
			printf("\n");
			return 0;
		} else {
			printf ("libfdt fdt_getprop(): %s\n",
				fdt_strerror(len));
			return 1;
		}
	}

	/*
	 * The user passed in a node path and no property,
	 * print the node and all subnodes.
	 */
	while(level >= 0) {
		tag = fdt_next_tag(working_fdt, nodeoffset, &nextoffset);
		switch(tag) {
		case FDT_BEGIN_NODE:
			pathp = fdt_get_name(working_fdt, nodeoffset, NULL);
			if (level <= depth) {
				if (pathp == NULL)
					pathp = "/* NULL pointer error */";
				if (*pathp == '\0')
					pathp = "/";	/* root is nameless */
				printf("%s%s {\n",
					&tabs[MAX_LEVEL - level], pathp);
			}
			level++;
			if (level >= MAX_LEVEL) {
				printf("Nested too deep, aborting.\n");
				return 1;
			}
			break;
		case FDT_END_NODE:
			level--;
			if (level <= depth)
				printf("%s};\n", &tabs[MAX_LEVEL - level]);
			if (level == 0) {
				level = -1;		/* exit the loop */
			}
			break;
		case FDT_PROP:
			fdt_prop = fdt_offset_ptr(working_fdt, nodeoffset,
					sizeof(*fdt_prop));
			pathp    = fdt_string(working_fdt,
					fdt32_to_cpu(fdt_prop->nameoff));
			len      = fdt32_to_cpu(fdt_prop->len);
			nodep    = fdt_prop->data;
			if (len < 0) {
				printf ("libfdt fdt_getprop(): %s\n",
					fdt_strerror(len));
				return 1;
			} else if (len == 0) {
				/* the property has no value */
				if (level <= depth)
					printf("%s%s;\n",
						&tabs[MAX_LEVEL - level],
						pathp);
			} else {
				if (level <= depth) {
					printf("%s%s = ",
						&tabs[MAX_LEVEL - level],
						pathp);
					print_data (nodep, len);
					printf(";\n");
				}
			}
			break;
		case FDT_NOP:
			printf("%s/* NOP */\n", &tabs[MAX_LEVEL - level]);
			break;
		case FDT_END:
			return 1;
		default:
			if (level <= depth)
				printf("Unknown tag 0x%08X\n", tag);
			return 1;
		}
		nodeoffset = nextoffset;
	}
	return 0;
}

