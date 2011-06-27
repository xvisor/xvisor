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
 * @file vmm_cmd_vserial.c
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of vserial command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_devtree.h>
#include <vmm_vserial.h>
#include <vmm_mterm.h>

#define VMM_VSERIAL_MAX_LINE_SIZE	256

void cmd_vserial_usage(void)
{
	vmm_printf("Usage:\n");
	vmm_printf("   vserial send <name>\n");
	vmm_printf("   vserial recv <name> [<byte_count>]\n");
	vmm_printf("   vserial help\n");
	vmm_printf("   vserial list\n");
}

int cmd_vserial_send(const char *name)
{
	char line[VMM_VSERIAL_MAX_LINE_SIZE];
	u32 line_sz, line_pos, bytes_send;
	vmm_vserial_t *vser = vmm_vserial_find(name);

	if (!vser) {
		vmm_printf("Failed to find virtual serial port\n");
		return VMM_EFAIL;
	}

	while(1) {
		vmm_gets(line, VMM_VSERIAL_MAX_LINE_SIZE, '\n');
		line_pos = 0;
		line_sz = vmm_strlen(line);
		if (line[line_pos] == '\0') {
			break;
		}
		vmm_strcat(line, "\n");
		line_sz++;
		while (line_sz) {
			bytes_send = vmm_vserial_send(vser, 
						      (u8 *)&line[line_pos], 
						      line_sz);
			line_sz -= bytes_send;
			line_pos += bytes_send;
		}
	}

	return VMM_OK;
}

int cmd_vserial_recv(const char *name, int bcount)
{
	u8 ch;
	vmm_vserial_t *vser = vmm_vserial_find(name);

	if (!vser) {
		vmm_printf("Failed to find virtual serial port\n");
		return VMM_EFAIL;
	}

	if (bcount < 0) {
		while (vmm_vserial_receive(vser, &ch, 1)) {
			vmm_putc(ch);
		}
	} else {
		while (bcount > 0 && vmm_vserial_receive(vser, &ch, 1)) {
			vmm_putc(ch);
			bcount--;
		}
	}

	vmm_printf("\n");

	return VMM_OK;
}

void cmd_vserial_list()
{
	int num, count;
	vmm_vserial_t *vser;
	count = vmm_vserial_count();
	for (num = 0; num < count; num++) {
		vser = vmm_vserial_get(num);
		vmm_printf("%d: %s\n", num, vser->name);
	}
}

int cmd_vserial_exec(int argc, char **argv)
{
	int bcount = -1;
	if (argc == 2) {
		if (vmm_strcmp(argv[1], "help") == 0) {
			cmd_vserial_usage();
			return VMM_OK;
		} else if (vmm_strcmp(argv[1], "list") == 0) {
			cmd_vserial_list();
			return VMM_OK;
		}
	}
	if (argc < 3) {
		cmd_vserial_usage();
		return VMM_EFAIL;
	}
	if (vmm_strcmp(argv[1], "send") == 0) {
		return cmd_vserial_send(argv[2]);
	} else if (vmm_strcmp(argv[1], "recv") == 0) {
		if (4 <= argc) {
			bcount = vmm_str2int(argv[3], 10);
		}
		return cmd_vserial_recv(argv[2], bcount);
	} else {
		cmd_vserial_usage();
		return VMM_EFAIL;
	}
	return VMM_OK;
}

VMM_DECLARE_CMD(vserial, "virtual serial port commands", cmd_vserial_exec, NULL);
