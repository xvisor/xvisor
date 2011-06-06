/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file vmm_buddy_alloc.c
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief source file for buddy allocator in VMM
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#define _DEBUG			1

#if _DEBUG
#define VMM_DPRINTK(fmt, ...)	printf(fmt, ##__VA_ARGS__)
#else
#define VMM_DPRINTK(fmt, ...)
#endif

#define MAX_ALLOCS	256

int main (int argc, char *argv[])
{
	int hsize = strtoul(argv[2], NULL, 10);
	void *hstart = malloc(hsize);
	void *allocations[MAX_ALLOCS];
	char *filename;
	char sbuf[256];
	unsigned int msize, idx;
	FILE *fp;
	void *cal;

	memset(&allocations, 0, sizeof(void *) * MAX_ALLOCS);

	if (!hstart) {
		VMM_DPRINTK("Error allocating memory for memory management.\n");
		return -1;
	}

	if (argc < 3) {
		VMM_DPRINTK("Too few arguments\n");
		goto _exit;
	}

	filename = argv[1];
	fp = fopen(filename, "r+");

	if (!fp) {
		VMM_DPRINTK("Can't open file %s\n", filename);
		goto _exit;
	}

	if (!buddy_init(hstart, hsize)) {
		VMM_DPRINTK("Buddy init successful.\n");
	}

	print_current_buddy_state();

	for (idx = 0; idx < MAX_ALLOCS; idx++) {
		if (fgets((char *)&sbuf[0], sizeof(sbuf), fp)) {
			msize = strtoul(sbuf, NULL, 10);
			VMM_DPRINTK("\nNew allocation of size: %dKiB\n", (msize / 1024));
			cal = (void *)buddy_malloc(msize);
			if (!cal) {
				VMM_DPRINTK("Allocation failed for size %dKiB\n", (msize/1024));
				break;
			}
			allocations[idx] = cal;
			print_current_buddy_state();
			VMM_DPRINTK("-------------------------------------------------\n");
		} else {
			VMM_DPRINTK("EOF.\n");
			break;
		}
	}

	VMM_DPRINTK("Starting deallocations\n");
	for (idx = 0; allocations[idx] != NULL; idx++) {
		buddy_free(allocations[idx]);
		print_current_buddy_state();
	}

_exit:
	free(hstart);
	return 0;
}
