/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file vsdaemon_mterm.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief vserial daemon mterm transport implementation
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_heap.h>
#include <vmm_chardev.h>
#include <vmm_stdio.h>
#include <vmm_spinlocks.h>
#include <vmm_completion.h>
#include <vmm_cmdmgr.h>
#include <vmm_modules.h>
#include <libs/fifo.h>
#include <libs/vsdaemon.h>

#define MODULE_DESC			"vsdaemon mterm transport"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VSDAEMON_IPRIORITY + 1)
#define	MODULE_INIT			vsdaemon_mterm_init
#define	MODULE_EXIT			vsdaemon_mterm_exit

struct vsdaemon_mterm {
	/* pointer to original vsdaemon */
	struct vsdaemon *vsd;

	/* command buffer */
	char cmds[CONFIG_VSDAEMON_MTERM_CMD_WIDTH];

	/* dummy character device */
	struct vmm_chardev cdev;

	/* rx fifo and completion */
	struct fifo *rx_fifo;
	struct vmm_completion rx_avail;

#ifdef CONFIG_VSDAEMON_MTERM_HISTORY
	/* stdio history */
	struct vmm_history history;
#endif
};

static u32 vsdaemon_mterm_chardev_write(struct vmm_chardev *cdev,
			u8 *src, size_t len, off_t __unused *off, bool sleep)
{
	struct vsdaemon_mterm *vmterm;

	if (!(cdev && src && cdev->priv)) {
		return 0;
	}
	vmterm = cdev->priv;

	return vmm_vserial_send(vmterm->vsd->vser, src, len);
}

static u32 vsdaemon_mterm_chardev_read(struct vmm_chardev *cdev,
			u8 *dest, size_t len, off_t __unused *off, bool sleep)
{
	u32 i;
	struct vsdaemon_mterm *vmterm;

	if (!(cdev && dest && cdev->priv)) {
		return 0;
	}
	vmterm = cdev->priv;

	if (sleep) {
		for (i = 0; i < len; i++) {
			while (!fifo_dequeue(vmterm->rx_fifo, &dest[i])) {
				vmm_completion_wait(&vmterm->rx_avail);
			}
		}
	} else {
		for (i = 0; i < len; i++) {
			if (!fifo_dequeue(vmterm->rx_fifo, &dest[i])) {
				break;
			}
		}
	}

	return i;
}

static bool vsdaemon_mterm_cmd_filter(struct vmm_chardev *cdev,
				      int argc, char **argv)
{
	if ((argc > 1) && !strcmp(argv[0], "vserial") &&
	    (!strcmp(argv[1], "bind") || !strcmp(argv[1], "dump"))) {
		/* Filter out "vserial bind" and "vserial dump" commands */
		return TRUE;
	}
	return FALSE;
}

static void vsdaemon_mterm_receive_char(struct vsdaemon *vsd, u8 ch)
{
	struct vsdaemon_mterm *vmterm = vsdaemon_transport_get_data(vsd);

	fifo_enqueue(vmterm->rx_fifo, &ch, FALSE);

	vmm_completion_complete(&vmterm->rx_avail);
}

static int vsdaemon_mterm_main_loop(struct vsdaemon *vsd)
{
	size_t cmds_len;
	struct vsdaemon_mterm *vmterm = vsdaemon_transport_get_data(vsd);

	while (1) {
		vmm_cprintf(&vmterm->cdev, "XVisor# ");

		memset(vmterm->cmds, 0, sizeof(vmterm->cmds));

		/* Get command string */
#ifdef CONFIG_VSDAEMON_MTERM_HISTORY
		vmm_cgets(&vmterm->cdev, vmterm->cmds,
			  CONFIG_VSDAEMON_MTERM_CMD_WIDTH,
			 '\n', &vmterm->history, TRUE);
#else
		vmm_cgets(&vmterm->cdev, vmterm->cmds,
			  CONFIG_VSDAEMON_MTERM_CMD_WIDTH,
			 '\n', NULL, TRUE);
#endif

		/* Process command string */
		cmds_len = strlen(vmterm->cmds);
		if (cmds_len > 0) {
			if (vmterm->cmds[cmds_len - 1] == '\r')
				vmterm->cmds[cmds_len - 1] = '\0';

			vmm_cmdmgr_execute_cmdstr(&vmterm->cdev, vmterm->cmds,
						  vsdaemon_mterm_cmd_filter);
		}
	}

	return VMM_OK;
}

static int vsdaemon_mterm_setup(struct vsdaemon *vsd, int argc, char **argv)
{
	struct vsdaemon_mterm *vmterm;

	vmterm = vmm_zalloc(sizeof(*vmterm));
	if (!vmterm) {
		return VMM_ENOMEM;
	}
	vmterm->vsd = vsd;

	strncpy(vmterm->cdev.name, vsd->name, sizeof(vmterm->cdev.name));
	vmterm->cdev.read = vsdaemon_mterm_chardev_read;
	vmterm->cdev.write = vsdaemon_mterm_chardev_write;
	vmterm->cdev.priv = vmterm;

	vmterm->rx_fifo = fifo_alloc(1, CONFIG_VSDAEMON_MTERM_CMD_WIDTH);
	if (!vmterm->rx_fifo) {
		vmm_free(vmterm);
		return VMM_ENOMEM;
	}
	INIT_COMPLETION(&vmterm->rx_avail);

#ifdef CONFIG_VSDAEMON_MTERM_HISTORY
	INIT_HISTORY(&vmterm->history, 
		     CONFIG_VSDAEMON_MTERM_HISTORY_SIZE,
		     CONFIG_VSDAEMON_MTERM_CMD_WIDTH);
#endif

	vsdaemon_transport_set_data(vsd, vmterm);

	return VMM_OK;
}

static void vsdaemon_mterm_cleanup(struct vsdaemon *vsd)
{
	struct vsdaemon_mterm *vmterm = vsdaemon_transport_get_data(vsd);

	vsdaemon_transport_set_data(vsd, NULL);

#ifdef CONFIG_VSDAEMON_MTERM_HISTORY
	CLEANUP_HISTORY(&vmterm->history);
#endif

	fifo_free(vmterm->rx_fifo);

	vmm_free(vmterm);
}

static struct vsdaemon_transport mterm = {
	.name = "mterm",
	.setup = vsdaemon_mterm_setup,
	.cleanup = vsdaemon_mterm_cleanup,
	.main_loop = vsdaemon_mterm_main_loop,
	.receive_char = vsdaemon_mterm_receive_char,
};

static int __init vsdaemon_mterm_init(void)
{
	return vsdaemon_transport_register(&mterm);
}

static void __exit vsdaemon_mterm_exit(void)
{
	vsdaemon_transport_unregister(&mterm);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
