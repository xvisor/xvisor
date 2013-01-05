/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file telnetd.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file of managment terminal over telnet
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_version.h>
#include <vmm_devtree.h>
#include <vmm_threads.h>
#include <vmm_chardev.h>
#include <vmm_spinlocks.h>
#include <vmm_completion.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <libs/stringlib.h>
#include <libs/netstack.h>

#define MODULE_DESC			"Telnet Managment Terminal"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			daemon_telnetd_init
#define	MODULE_EXIT			daemon_telnetd_exit

#define TELNETD_TX_QUEUE_SIZE		1024
#define TELNETD_RX_QUEUE_SIZE		1024

static struct telnetd_ctrl {
	u32 port;
	struct netstack_socket *sk;
	struct netstack_socket *active_sk;
	bool disconnected;
	vmm_spinlock_t tx_buf_lock;
	u32 tx_buf_head;
	u32 tx_buf_tail;
	u32 tx_buf_count;
	u8 *tx_buf;
	struct vmm_completion tx_pending;
	u32 rx_buf_pos;
	u32 rx_buf_count;
	u8 *rx_buf;
	struct vmm_chardev cdev;
	struct vmm_thread *rx_thread;
	struct vmm_thread *tx_thread;
#ifdef CONFIG_TELNETD_HISTORY
	struct vmm_history history;
#endif
} tdctrl;

static u32 telnetd_chardev_write(struct vmm_chardev *cdev,
				 u8 *src, u32 len, bool sleep)
{
	int rc;
	u32 tx_count;
	irq_flags_t flags;

	/* If disconnected then just return */
	if (tdctrl.disconnected) {
		return len;
	}

	/* If in we can sleep then directly Tx else use Tx thread */
	if (sleep) {
		rc = netstack_socket_write(tdctrl.active_sk, src, len);
		if (rc) {
			tdctrl.disconnected = TRUE;
		}
		return len;
	}

	/* Lock Tx queue */
	vmm_spin_lock_irqsave(&tdctrl.tx_buf_lock, flags);

	tx_count = 0;
	while ((tx_count < len) &&
	       (tdctrl.tx_buf_count < TELNETD_TX_QUEUE_SIZE)) {
		tdctrl.tx_buf[tdctrl.tx_buf_tail] = src[tx_count];

		tdctrl.tx_buf_tail++;
		if (tdctrl.tx_buf_tail >= TELNETD_TX_QUEUE_SIZE) {
			tdctrl.tx_buf_tail = 0;
		}
		tdctrl.tx_buf_count++;

		tx_count++;
	}

	/* Unlock Tx queue */
	vmm_spin_unlock_irqrestore(&tdctrl.tx_buf_lock, flags);

	/* Signal for Tx Pending */
	vmm_completion_complete(&tdctrl.tx_pending);

	return tx_count;
}

static int telnetd_tx(void *data)
{
	int rc;
	u32 tx_count;
	irq_flags_t flags;
	u8 tx_buf[TELNETD_TX_QUEUE_SIZE];

	while (1) {
		/* Wait for Tx Pending */
		vmm_completion_wait(&tdctrl.tx_pending);

		/* Make sure we have active connection */
		if (!tdctrl.active_sk) {
			continue;
		}

		/* Lock Tx buffer */
		vmm_spin_lock_irqsave(&tdctrl.tx_buf_lock, flags);

		/* Get data from Tx buffer */
		tx_count = 0;
		while (tdctrl.tx_buf_count) {
			tx_buf[tx_count] = tdctrl.tx_buf[tdctrl.tx_buf_head];

			tdctrl.tx_buf_head++;
			if (tdctrl.tx_buf_head >= TELNETD_TX_QUEUE_SIZE) {
				tdctrl.tx_buf_head = 0;
			}
			tdctrl.tx_buf_count--;

			tx_count++;
		}

		/* Unlock Tx buffer */
		vmm_spin_unlock_irqrestore(&tdctrl.tx_buf_lock, flags);

		/* Transmit the pending Tx data */
		if (tx_count) {
			rc = netstack_socket_write(tdctrl.active_sk, 
						   &tx_buf[0], tx_count);
			if (rc) {
				tdctrl.disconnected = TRUE;
			}
		}
	}

	return VMM_OK;
}

static u32 telnetd_chardev_read(struct vmm_chardev *cdev,
				u8 *dest, u32 len, bool sleep)
{
	int rc;
	u32 rx_count;
	struct netstack_socket_buf buf;

	/* We have bug if read called in non-sleepable context */
	BUG_ON(!sleep);

	/* If disconnected then just return */
	if (tdctrl.disconnected || !tdctrl.active_sk) {
		return len;
	}

	/* If we don't have any data in Rx buffer then read from socket */
	if (!(tdctrl.rx_buf_count - tdctrl.rx_buf_pos)) {
		tdctrl.rx_buf_pos = tdctrl.rx_buf_count = 0;

		rc = netstack_socket_recv(tdctrl.active_sk, &buf);
		if (rc) {
			tdctrl.disconnected = TRUE;
			dest[0] = '\n';
			return len;
		}

		do {
			rx_count = TELNETD_RX_QUEUE_SIZE - tdctrl.rx_buf_count;
			if (buf.len < rx_count) {
				rx_count = buf.len;
			} else {
				rx_count = 0;
			}
			if (rx_count) {
				memcpy(&tdctrl.rx_buf[tdctrl.rx_buf_count], 
					buf.data, rx_count);
				tdctrl.rx_buf_count += rx_count;
			}
		} while (!(rc = netstack_socket_nextbuf(&buf)));

		netstack_socket_freebuf(&buf);
	}

	/* Read from Rx buffer */
	rx_count = len;
	if ((tdctrl.rx_buf_count - tdctrl.rx_buf_pos) < rx_count) {
		rx_count = (tdctrl.rx_buf_count - tdctrl.rx_buf_pos);
	}
	memcpy(dest, &tdctrl.rx_buf[tdctrl.rx_buf_pos], rx_count);
	tdctrl.rx_buf_pos += rx_count;

	return rx_count;
}

static int telnetd_rx(void *data)
{
	int rc;
	size_t cmds_len;
	char cmds[CONFIG_MTERM_CMD_WIDTH];

	/* Create a new socket. */
	tdctrl.sk = netstack_socket_alloc(NETSTACK_SOCKET_TCP);
	if (!tdctrl.sk) {
		return VMM_ENOMEM;
	}

	/* Bind socket to port number */
	rc = netstack_socket_bind(tdctrl.sk, NULL, tdctrl.port);
	if (rc) {
		goto fail;
	}

	/* Tell socket to go into listening mode. */
	rc = netstack_socket_listen(tdctrl.sk);
	if (rc) {
		goto fail1;
	}

	while (1) {
 		/* Grab new connect request. */
		rc = netstack_socket_accept(tdctrl.sk, &tdctrl.active_sk);
		if (rc) {
			goto fail1;
		}

		/* Clear disconnected flag */
		tdctrl.disconnected = FALSE;

		/* Clear Rx buffer */
		tdctrl.rx_buf_pos = tdctrl.rx_buf_count = 0;

		/* Telnetd Banner */
		vmm_cprintf(&tdctrl.cdev, 
				"Connected to Xvisor Telnet daemon\n");

		/* Print Banner */
		vmm_cprintf(&tdctrl.cdev, "%s", VMM_BANNER_STRING);

		while (!tdctrl.disconnected) {
			/* Show prompt */
			vmm_cprintf(&tdctrl.cdev, "XVisor# ");
			memset(cmds, 0, sizeof(cmds));

			/* Check disconnected */
			if (tdctrl.disconnected) {
				break;
			}

			/* Get command string */
#ifdef CONFIG_TELNETD_HISTORY
			vmm_cgets(&tdctrl.cdev, cmds, CONFIG_TELNETD_CMD_WIDTH,
				 '\n', &tdctrl.history);
#else
			vmm_cgets(&tdctrl.cdev, cmds, CONFIG_TELNETD_CMD_WIDTH,
				 '\n', NULL);
#endif

			/* Check disconnected */
			if (tdctrl.disconnected) {
				break;
			}

			/* Process command string */
			cmds_len = strlen(cmds);
			if (cmds_len > 0) {
				if (cmds[cmds_len - 1] == '\r')
					cmds[cmds_len - 1] = '\0';

				/* Execute command string */
				vmm_cmdmgr_execute_cmdstr(&tdctrl.cdev, cmds);
			}

			/* Check disconnected */
			if (tdctrl.disconnected) {
				break;
			}

		}

#if 0
		while (!(rc = netstack_socket_recv(new_sk, &buf))) {
			do {
				if (strncmp((char *)buf.data, "hi", 2) == 0) {
					strcpy(tmp, "telnetd: hello\n");
					rc = netstack_socket_write(new_sk, &tmp, strlen(tmp));
				} else {
					rc = VMM_OK;
				}
				if (rc) {
					break;
				}
			} while (!(rc = netstack_socket_nextbuf(&buf)));

			netstack_socket_freebuf(&buf);

			strcpy(tmp, "you: ");
			rc = netstack_socket_write(new_sk, &tmp, strlen(tmp));
			if (rc) {
				break;
			}
		}
#endif

		netstack_socket_close(tdctrl.active_sk);
		netstack_socket_free(tdctrl.active_sk);
	}

	rc = VMM_OK;

fail1:
	netstack_socket_close(tdctrl.sk);
fail:
	netstack_socket_free(tdctrl.sk);

	return rc;
}

static int __init daemon_telnetd_init(void)
{
	u8 telnetd_priority;
	u32 telnetd_time_slice;
	struct vmm_devtree_node *node;
	const char *attrval;

	/* Reset telnetd control information */
	memset(&tdctrl, 0, sizeof(tdctrl));

#ifdef CONFIG_TELNETD_HISTORY
	INIT_HISTORY(&tdctrl.history, 
			CONFIG_TELNETD_HISTORY_SIZE, CONFIG_TELNETD_CMD_WIDTH);
#endif

	/* Retrive telnetd time slice */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_VMMINFO_NODE_NAME);
	if (!node) {
		return VMM_EFAIL;
	}
	attrval = vmm_devtree_attrval(node,
				      "telnetd_priority");
	if (attrval) {
		telnetd_priority = *((u32 *) attrval);
	} else {
		telnetd_priority = VMM_THREAD_DEF_PRIORITY;
	}
	attrval = vmm_devtree_attrval(node,
				      "telnetd_time_slice");
	if (attrval) {
		telnetd_time_slice = *((u32 *) attrval);
	} else {
		telnetd_time_slice = VMM_THREAD_DEF_TIME_SLICE;
	}
	attrval = vmm_devtree_attrval(node,
				      "telnetd_port");
	if (attrval) {
		tdctrl.port = *((u32 *) attrval);
	} else {
		tdctrl.port = 23;
	}

	/* Sanitize telnetd control information */
	tdctrl.sk = NULL;
	tdctrl.active_sk = NULL;
	tdctrl.disconnected = TRUE;
	INIT_SPIN_LOCK(&tdctrl.tx_buf_lock);
	tdctrl.tx_buf_head = tdctrl.tx_buf_tail = tdctrl.tx_buf_count = 0;
	tdctrl.rx_buf_pos = tdctrl.rx_buf_count = 0;
	tdctrl.tx_buf = tdctrl.rx_buf = NULL;
	INIT_COMPLETION(&tdctrl.tx_pending);

	/* Allocate Tx & Rx buffers */
	tdctrl.tx_buf = vmm_zalloc(TELNETD_TX_QUEUE_SIZE);
	if (!tdctrl.tx_buf) {
		return VMM_ENOMEM;
	}
	tdctrl.rx_buf = vmm_zalloc(TELNETD_RX_QUEUE_SIZE);
	if (!tdctrl.rx_buf) {
		vmm_free(tdctrl.tx_buf);
		return VMM_ENOMEM;
	}

	/* Setup telnetd dummy character device */
	strcpy(tdctrl.cdev.name, "telnetd");
	tdctrl.cdev.dev = NULL;
	tdctrl.cdev.read = telnetd_chardev_read;
	tdctrl.cdev.write = telnetd_chardev_write;
	tdctrl.cdev.ioctl = NULL;

	/* Note: We don't register telnetd dummy character device so that
	 * it is not visible to other part of hypervisor. This way we can
	 * also avoid someone setting telnetd dummy character device as
	 * stdio device.
	 */

	/* Create telnetd Rx (or main) thread */
	tdctrl.rx_thread = vmm_threads_create("telnetd_rx", 
						&telnetd_rx, 
						NULL, 
						telnetd_priority,
						telnetd_time_slice);
	if (!tdctrl.rx_thread) {
		vmm_panic("telnetd: Rx thread creation failed.\n");
	}

	/* Create telnetd Tx thread */
	tdctrl.tx_thread = vmm_threads_create("telnetd_tx", 
						&telnetd_tx, 
						NULL, 
						telnetd_priority,
						telnetd_time_slice);
	if (!tdctrl.tx_thread) {
		vmm_panic("telnetd: Tx thread creation failed.\n");
	}

	/* Start telnetd Rx thread */
	vmm_threads_start(tdctrl.rx_thread);

	/* Start telnetd Tx thread */
	vmm_threads_start(tdctrl.tx_thread);

	return VMM_OK;
}

static void __exit daemon_telnetd_exit(void)
{
	/* Stop and destroy telnetd Tx thread */
	vmm_threads_stop(tdctrl.tx_thread);
	vmm_threads_destroy(tdctrl.tx_thread);

	/* Stop and destroy telnetd Rx thread */
	vmm_threads_stop(tdctrl.rx_thread);
	vmm_threads_destroy(tdctrl.rx_thread);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
