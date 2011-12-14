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
 * @file cmd_vserial.c
 * @version 0.01
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of vserial command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_devtree.h>
#include <vmm_vserial.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>

#define MODULE_VARID			cmd_vserial_module
#define MODULE_NAME			"Command vserial"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_vserial_init
#define	MODULE_EXIT			cmd_vserial_exit

void cmd_vserial_usage(vmm_chardev_t *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   vserial bind <name>\n");
	vmm_cprintf(cdev, "   vserial dump <name> [<byte_count>]\n");
	vmm_cprintf(cdev, "   vserial help\n");
	vmm_cprintf(cdev, "   vserial list\n");
}

struct cmd_vserial_recvcntx {
	const char * name;
	u32 chpos;
	vmm_chardev_t *cdev;
};

typedef struct cmd_vserial_recvcntx cmd_vserial_recvcntx_t;

void cmd_vserial_recv(vmm_vserial_t *vser, void * priv, u8 ch)
{
	cmd_vserial_recvcntx_t * recvcntx;
	recvcntx = (cmd_vserial_recvcntx_t *)priv;
	if (!recvcntx) {
		return;
	}
	if (ch == '\n') {
		vmm_cputc(recvcntx->cdev, ch);
		vmm_cprintf(recvcntx->cdev, "[%s] ", recvcntx->name);
		recvcntx->chpos = 0;
	} else if (ch == '\r') {
		while (recvcntx->chpos) {
			vmm_cputc(recvcntx->cdev, '\e');
			vmm_cputc(recvcntx->cdev, '[');
			vmm_cputc(recvcntx->cdev, 'D');
			recvcntx->chpos--;
		}
	} else {
		vmm_cputc(recvcntx->cdev, ch);
		recvcntx->chpos++;
	}
}

int cmd_vserial_bind(vmm_chardev_t *cdev, const char *name)
{
	int rc = VMM_OK;
	u32 ite, epos = 0;
	char ch;
	static const char estr[3] = {'\e', 'x', 'q'}; /* estr is escape string. */
	vmm_vserial_t *vser = vmm_vserial_find(name);
	cmd_vserial_recvcntx_t recvcntx;

	if (!vser) {
		vmm_cprintf(cdev, "Failed to find virtual serial port\n");
		return VMM_EFAIL;
	}

	vmm_cprintf(cdev, "[%s] ", name);

	recvcntx.name = name;
	recvcntx.chpos = 0;
	recvcntx.cdev = cdev;

	rc = vmm_vserial_register_receiver(vser, &cmd_vserial_recv, &recvcntx);
	if (rc) {
		return rc;
	}

	epos = 0;
	while(1) {
		if (!vmm_scanchar(NULL, cdev, &ch, TRUE)) {
			if (epos < sizeof(estr)) {
				if (estr[epos] == ch) {
					epos++;
				} else {
					for (ite = 0; ite < epos; ite++) {
						while (!vmm_vserial_send(vser, (u8 *)&estr[ite], 1)) ;
					}
					epos = 0;
					while (!vmm_vserial_send(vser, (u8 *)&ch, 1)) ;
				}
			} 
			if (epos == sizeof(estr)) {
				epos = 0;
				break;
			}
		}
	}

	vmm_cprintf(cdev, "\n");

	rc = vmm_vserial_unregister_receiver(vser, &cmd_vserial_recv, &recvcntx);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

int cmd_vserial_dump(vmm_chardev_t *cdev, const char *name, int bcount)
{
	u8 ch;
	vmm_vserial_t *vser = vmm_vserial_find(name);

	if (!vser) {
		vmm_cprintf(cdev, "Failed to find virtual serial port\n");
		return VMM_EFAIL;
	}

	if (bcount < 0) {
		while (vmm_vserial_receive(vser, &ch, 1)) {
			vmm_cprintf(cdev, "%c", ch);
		}
	} else {
		while (bcount > 0 && vmm_vserial_receive(vser, &ch, 1)) {
			vmm_cprintf(cdev, "%c", ch);
			bcount--;
		}
	}

	vmm_cprintf(cdev, "\n");

	return VMM_OK;
}

void cmd_vserial_list(vmm_chardev_t *cdev)
{
	int num, count;
	vmm_vserial_t *vser;
	count = vmm_vserial_count();
	for (num = 0; num < count; num++) {
		vser = vmm_vserial_get(num);
		vmm_cprintf(cdev, "%d: %s\n", num, vser->name);
	}
}

int cmd_vserial_exec(vmm_chardev_t *cdev, int argc, char **argv)
{
	int bcount = -1;
	if (argc == 2) {
		if (vmm_strcmp(argv[1], "help") == 0) {
			cmd_vserial_usage(cdev);
			return VMM_OK;
		} else if (vmm_strcmp(argv[1], "list") == 0) {
			cmd_vserial_list(cdev);
			return VMM_OK;
		}
	}
	if (argc < 3) {
		cmd_vserial_usage(cdev);
		return VMM_EFAIL;
	}
	if (vmm_strcmp(argv[1], "bind") == 0) {
		return cmd_vserial_bind(cdev, argv[2]);
	} else if (vmm_strcmp(argv[1], "dump") == 0) {
		if (4 <= argc) {
			bcount = vmm_str2int(argv[3], 10);
		}
		return cmd_vserial_dump(cdev, argv[2], bcount);
	} else {
		cmd_vserial_usage(cdev);
		return VMM_EFAIL;
	}
	return VMM_OK;
}

static vmm_cmd_t cmd_vserial = {
	.name = "vserial",
	.desc = "virtual serial port commands",
	.usage = cmd_vserial_usage,
	.exec = cmd_vserial_exec,
};

static int __init cmd_vserial_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_vserial);
}

static void cmd_vserial_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_vserial);
}

VMM_DECLARE_MODULE(MODULE_VARID, 
			MODULE_NAME, 
			MODULE_AUTHOR, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
