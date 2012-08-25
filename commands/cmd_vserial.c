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
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of vserial command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_vserial.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <stringlib.h>

#define MODULE_DESC			"Command vserial"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_vserial_init
#define	MODULE_EXIT			cmd_vserial_exit

void cmd_vserial_usage(struct vmm_chardev *cdev)
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
	int chcount;
	int ecount;
	struct vmm_chardev *cdev;
};

void cmd_vserial_recv(struct vmm_vserial *vser, void * priv, u8 ch)
{
	struct cmd_vserial_recvcntx * recvcntx;
	recvcntx = (struct cmd_vserial_recvcntx *)priv;
	if (!recvcntx) {
		return;
	}
	if (!recvcntx->chcount) {
		return;
	}
	if (ch == '\n') {
		/* Emulate the effect of '\n' character */
		vmm_cputc(recvcntx->cdev, ch);
		vmm_cprintf(recvcntx->cdev, "[%s] ", recvcntx->name);
		recvcntx->chpos = 0;
	} else if (ch == '\r') {
		/* Emulate the effect of '\r' character */
		vmm_cprintf(recvcntx->cdev, "\e[%dD", recvcntx->chpos);
		recvcntx->chpos = 0;
	} else if (ch == '\e' || recvcntx->ecount > 0) {
		/* Increment ecount till entire ANSI/VT100
		 * command is detected. Upon detecting end 
		 * of ANSI/VT100 command set ecount to 0
		 */
		vmm_cputc(recvcntx->cdev, ch);
		if (('a' <= ch && ch <= 'z') ||
		    ('A' <= ch && ch <= 'Z')) {
			/* ANSI/VT100 command usually ends 
			 * with an alphbet lower or upper case
			 */
			recvcntx->ecount = 0;
		} else if ((recvcntx->ecount == 1) &&
			   (ch == '7' || ch == '8')) {
			/* ANSI/VT100 <ESC>7 or <ESC>8 */
			recvcntx->ecount = 0;
		} else if ((recvcntx->ecount == 1) &&
			   (ch == '(' || ch == ')')) {
			/* ANSI/VT100 <ESC>( or <ESC>) */
			recvcntx->ecount = 0;
		} else {
			recvcntx->ecount++;
		}
	} else {
		/* By default keep printing incoming character
		 * but increment chpos for only printable characters.
		 */
		vmm_cputc(recvcntx->cdev, ch);
		if (vmm_isprintable(ch)) {
			recvcntx->chpos++;
		}
	}
	if (-1 < recvcntx->chcount) {
		if (recvcntx->chcount) {
			recvcntx->chcount--;
		}
	}
}

int cmd_vserial_bind(struct vmm_chardev *cdev, const char *name)
{
	int rc = VMM_OK;
	u32 ite, epos = 0;
	char ch;
	static const char estr[3] = {'\e', 'x', 'q'}; /* estr is escape string. */
	struct vmm_vserial *vser = vmm_vserial_find(name);
	struct cmd_vserial_recvcntx recvcntx;

	if (!vser) {
		vmm_cprintf(cdev, "Failed to find virtual serial port\n");
		return VMM_EFAIL;
	}

	vmm_cprintf(cdev, "[%s] ", name);

	recvcntx.name = name;
	recvcntx.chpos = 0;
	recvcntx.chcount = -1;
	recvcntx.ecount = 0;
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

int cmd_vserial_dump(struct vmm_chardev *cdev, const char *name, int bcount)
{
	int rc = VMM_OK;
	struct vmm_vserial *vser = vmm_vserial_find(name);
	struct cmd_vserial_recvcntx recvcntx;

	if (!vser) {
		vmm_cprintf(cdev, "Failed to find virtual serial port\n");
		return VMM_EFAIL;
	}

	vmm_cprintf(cdev, "[%s] ", name);

	recvcntx.name = name;
	recvcntx.chpos = 0;
	recvcntx.chcount = (0 < bcount) ? bcount : -1;
	recvcntx.ecount = 0;
	recvcntx.cdev = cdev;

	rc = vmm_vserial_register_receiver(vser, &cmd_vserial_recv, &recvcntx);
	if (rc) {
		return rc;
	}

	vmm_cprintf(cdev, "\n");

	rc = vmm_vserial_unregister_receiver(vser, &cmd_vserial_recv, &recvcntx);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

void cmd_vserial_list(struct vmm_chardev *cdev)
{
	int num, count;
	struct vmm_vserial *vser;
	count = vmm_vserial_count();
	for (num = 0; num < count; num++) {
		vser = vmm_vserial_get(num);
		vmm_cprintf(cdev, "%d: %s\n", num, vser->name);
	}
}

int cmd_vserial_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	int bcount = -1;
	if (argc == 2) {
		if (strcmp(argv[1], "help") == 0) {
			cmd_vserial_usage(cdev);
			return VMM_OK;
		} else if (strcmp(argv[1], "list") == 0) {
			cmd_vserial_list(cdev);
			return VMM_OK;
		}
	}
	if (argc < 3) {
		cmd_vserial_usage(cdev);
		return VMM_EFAIL;
	}
	if (strcmp(argv[1], "bind") == 0) {
		return cmd_vserial_bind(cdev, argv[2]);
	} else if (strcmp(argv[1], "dump") == 0) {
		if (4 <= argc) {
			bcount = str2int(argv[3], 10);
		}
		return cmd_vserial_dump(cdev, argv[2], bcount);
	} else {
		cmd_vserial_usage(cdev);
		return VMM_EFAIL;
	}
	return VMM_OK;
}

static struct vmm_cmd cmd_vserial = {
	.name = "vserial",
	.desc = "virtual serial port commands",
	.usage = cmd_vserial_usage,
	.exec = cmd_vserial_exec,
};

static int __init cmd_vserial_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_vserial);
}

static void __exit cmd_vserial_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_vserial);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
