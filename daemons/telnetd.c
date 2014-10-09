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
#include <vmm_heap.h>
#include <vmm_version.h>
#include <vmm_devtree.h>
#include <vmm_threads.h>
#include <vmm_chardev.h>
#include <vmm_spinlocks.h>
#include <vmm_workqueue.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <libs/stringlib.h>
#include <libs/netstack.h>

#define MODULE_DESC			"Telnet Managment Terminal"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(NETSTACK_IPRIORITY + 1)
#define	MODULE_INIT			daemon_telnetd_init
#define	MODULE_EXIT			daemon_telnetd_exit

#undef TELNETD_DEBUG

#if defined(TELNETD_DEBUG)
#define TELNETD_DPRINTF(msg...)		vmm_printf(msg)
#else
#define TELNETD_DPRINTF(msg...)
#endif

#define TELNETD_TX_BUFFER_SIZE		1024
#define TELNETD_RX_BUFFER_SIZE		(CONFIG_TELNETD_CMD_WIDTH)

static struct telnetd_ctrl {
	u32 port;
	struct netstack_socket *sk;

	struct netstack_socket *active_sk;

	vmm_spinlock_t lock;
	bool disconnected;
	u32 tx_buf_head;
	u32 tx_buf_tail;
	u32 tx_buf_count;
	u8 *tx_buf;
	u32 rx_buf_head;
	u32 rx_buf_tail;
	u32 rx_buf_count;
	u8 *rx_buf;

	bool cdev_ingets;
	bool cdev_incmdexec;
	struct vmm_chardev cdev;
	struct vmm_thread *main_thread;

#ifdef CONFIG_TELNETD_HISTORY
	struct vmm_history history;
#endif
} tdctrl;

static bool telnetd_check_disconnected(void)
{
	bool ret;
	irq_flags_t flags;

	/* Lock connection state */
	vmm_spin_lock_irqsave(&tdctrl.lock, flags);

	ret = tdctrl.disconnected;

	/* Unlock connection state */
	vmm_spin_unlock_irqrestore(&tdctrl.lock, flags);

	return ret;
}

static void telnetd_set_disconnected(void)
{
	irq_flags_t flags;

	/* Lock connection state */
	vmm_spin_lock_irqsave(&tdctrl.lock, flags);

	tdctrl.disconnected = TRUE;

	/* Unlock connection state */
	vmm_spin_unlock_irqrestore(&tdctrl.lock, flags);
}

static void telnetd_clear_disconnected(void)
{
	irq_flags_t flags;

	/* Lock connection state */
	vmm_spin_lock_irqsave(&tdctrl.lock, flags);

	tdctrl.disconnected = FALSE;
	
	/* Clear Tx buffer */
	tdctrl.tx_buf_head = tdctrl.tx_buf_tail = tdctrl.tx_buf_count = 0;

	/* Clear Rx buffer */
	tdctrl.rx_buf_head = tdctrl.rx_buf_tail = tdctrl.rx_buf_count = 0;

	/* Unlock connection state */
	vmm_spin_unlock_irqrestore(&tdctrl.lock, flags);
}

static void telnetd_fill_tx_buffer(u8 *src, u32 len)
{
	u32 tx_count;
	irq_flags_t flags;

	/* Lock connection state */
	vmm_spin_lock_irqsave(&tdctrl.lock, flags);

	/* If disconnected then just return */
	if (tdctrl.disconnected) {
		vmm_spin_unlock_irqrestore(&tdctrl.lock, flags);
		return;
	}

	/* Enqueue to Tx buffer*/
	tx_count = 0;
	while ((tx_count < len) &&
	       (tdctrl.tx_buf_count < TELNETD_TX_BUFFER_SIZE)) {
		tdctrl.tx_buf[tdctrl.tx_buf_tail] = src[tx_count];

		tdctrl.tx_buf_tail++;
		if (tdctrl.tx_buf_tail >= TELNETD_TX_BUFFER_SIZE) {
			tdctrl.tx_buf_tail = 0;
		}
		tdctrl.tx_buf_count++;

		tx_count++;
	}

	/* Unlock connection state */
	vmm_spin_unlock_irqrestore(&tdctrl.lock, flags);
}

#define TELNETD_MAX_FLUSH_SIZE			128

static void telnetd_flush_tx_buffer(void)
{
	int rc;
	u32 tx_count;
	irq_flags_t flags;
	u8 tx_buf[TELNETD_MAX_FLUSH_SIZE];

	while (1) {
		/* Lock connection state */
		vmm_spin_lock_irqsave(&tdctrl.lock, flags);

		/* If disconnected then return */
		if (tdctrl.disconnected) {
			vmm_spin_unlock_irqrestore(&tdctrl.lock, flags);
			return;
		}

		/* Get data from Tx buffer */
		tx_count = 0;
		while (tdctrl.tx_buf_count &&
		       (tx_count < TELNETD_MAX_FLUSH_SIZE)) {
			tx_buf[tx_count] = tdctrl.tx_buf[tdctrl.tx_buf_head];
			tdctrl.tx_buf_head++;
			if (tdctrl.tx_buf_head >= TELNETD_TX_BUFFER_SIZE) {
				tdctrl.tx_buf_head = 0;
			}
			tdctrl.tx_buf_count--;
			tx_count++;
		}

		/* Unlock connection state */
		vmm_spin_unlock_irqrestore(&tdctrl.lock, flags);

		/* Transmit the pending Tx data */
		if (tx_count) {
			rc = netstack_socket_write(tdctrl.active_sk, 
						   &tx_buf[0], tx_count);
			if (rc) {
				telnetd_set_disconnected();
				TELNETD_DPRINTF("%s: Socket write failed\n", __func__);
				return;
			}
		} else {
			return;
		}
	}
}

static void telnetd_fill_rx_buffer(void)
{
	int rc;
	u32 rx_count;
	irq_flags_t flags;
	struct netstack_socket_buf buf;

	/* Lock connection state */
	vmm_spin_lock_irqsave(&tdctrl.lock, flags);

	/* If disconnected then return */
	if (tdctrl.disconnected) {
		vmm_spin_unlock_irqrestore(&tdctrl.lock, flags);
		return;
	}

	/* If rx buffer not empty then return */
	if (tdctrl.rx_buf_count) {
		vmm_spin_unlock_irqrestore(&tdctrl.lock, flags);
		return;
	}

	/* Unlock connection state */
	vmm_spin_unlock_irqrestore(&tdctrl.lock, flags);

	/* Recieve netstack socket buffer */
	rc = netstack_socket_recv(tdctrl.active_sk, &buf, -1);
	if (rc == VMM_ETIMEDOUT) {
		TELNETD_DPRINTF("%s: Socket read timedout\n", __func__);
		return;
	} else if (rc) {
		telnetd_set_disconnected();
		TELNETD_DPRINTF("%s: Socket read failed (error %d)\n",
				__func__, rc);
		return;
	}

	/* Lock connection state */
	vmm_spin_lock_irqsave(&tdctrl.lock, flags);

	/* Fill Rx buffer */
	do {
		rx_count = 0;
		while ((rx_count < buf.len) &&
		       (tdctrl.rx_buf_count < TELNETD_RX_BUFFER_SIZE)) {
			tdctrl.rx_buf[tdctrl.rx_buf_tail] = ((u8 *)buf.data)[rx_count];

			tdctrl.rx_buf_tail++;
			if (tdctrl.rx_buf_tail >= TELNETD_RX_BUFFER_SIZE) {
				tdctrl.tx_buf_tail = 0;
			}
			tdctrl.rx_buf_count++;

			rx_count++;
		}
	} while (!(rc = netstack_socket_nextbuf(&buf)));

	/* Unlock connection state */
	vmm_spin_unlock_irqrestore(&tdctrl.lock, flags);

	/* Free netstack socket buffer */
	netstack_socket_freebuf(&buf);
}

static u32 telnetd_dequeue_rx_buffer(u8 *dest, u32 len)
{
	u32 i, rx_count;
	irq_flags_t flags;

	/* Lock connection state */
	vmm_spin_lock_irqsave(&tdctrl.lock, flags);

	/* If disconnected then return */
	if (tdctrl.disconnected) {
		vmm_spin_unlock_irqrestore(&tdctrl.lock, flags);
		dest[0] = '\n';
		return 1;
	}

	/* Dequeue from Rx buffer */
	rx_count = tdctrl.rx_buf_count;
	if (rx_count) {
		if (len < rx_count) {
			rx_count = len;
		}
		for (i = 0; i < rx_count; i++) {
			dest[i] = tdctrl.rx_buf[tdctrl.rx_buf_head];

			tdctrl.rx_buf_head++;
			if (tdctrl.rx_buf_head >= TELNETD_RX_BUFFER_SIZE) {
				tdctrl.tx_buf_head = 0;
			}
			tdctrl.rx_buf_count--;
		}
	}

	/* Unlock connection state */
	vmm_spin_unlock_irqrestore(&tdctrl.lock, flags);

	return rx_count;
}

static u32 telnetd_chardev_write(struct vmm_chardev *cdev,
			u8 *src, size_t len, off_t __unused *off, bool sleep)
{
	u32 i;
	bool flush_needed = FALSE;

	/* We have bug if write() called in non-sleepable context */
	BUG_ON(!sleep);

	/* Fill the Tx buffer */
	telnetd_fill_tx_buffer(src, len);

	/* Check if we have new-line character in Tx */
	for (i = 0; i < len; i++) {
		if (src[i] == '\n' || src[i] == '\r') {
			flush_needed = TRUE;
		}
	}

	/* If required flush then flush Tx buffer */
	if (tdctrl.cdev_ingets || flush_needed) {
		telnetd_flush_tx_buffer();
	}

	return len;
}

static u32 telnetd_chardev_read(struct vmm_chardev *cdev,
			u8 *dest, size_t len, off_t __unused *off, bool sleep)
{
	/* We have bug if read() called in non-sleepable context */
	BUG_ON(!sleep);

	/* Fill Rx buffer */
	telnetd_fill_rx_buffer();

	/* Dequeue from Rx buffer */
	return telnetd_dequeue_rx_buffer(dest, len);
}

static bool telnetd_cmd_filter(struct vmm_chardev *cdev, int argc, char **argv)
{
	if ((argc > 1) && !strcmp(argv[0], "vserial") &&
	    (!strcmp(argv[1], "bind") || !strcmp(argv[1], "dump"))) {
		/* Filter out "vserial bind" and "vserial dump" commands */
		return TRUE;
	}
	return FALSE;
}

static int telnetd_main(void *data)
{
	int rc;
	size_t cmds_len;
	char cmds[CONFIG_TELNETD_CMD_WIDTH];

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
		TELNETD_DPRINTF("%s: Grab new conn.\n", __func__);

		/* Grab new connect request. */
		rc = netstack_socket_accept(tdctrl.sk, &tdctrl.active_sk);
		if (rc) {
			goto fail1;
		}

		/* Clear disconnected flag */
		telnetd_clear_disconnected();

		/* Telnetd Banner */
		vmm_cprintf(&tdctrl.cdev, 
				"Connected to Xvisor Telnet daemon\n");

		/* Flush Tx buffer */
		telnetd_flush_tx_buffer();

		/* Print Banner */
		vmm_cprintf(&tdctrl.cdev, "%s", VMM_BANNER_STRING);

		/* Flush Tx buffer */
		telnetd_flush_tx_buffer();

		while (!telnetd_check_disconnected()) {
			/* Show prompt */
			vmm_cprintf(&tdctrl.cdev, "XVisor# ");

			/* Flush Tx & Check disconnected */
			telnetd_flush_tx_buffer();
			if (telnetd_check_disconnected()) {
				break;
			}

			/* Get command string */
			memset(cmds, 0, sizeof(cmds));
			tdctrl.cdev_ingets = TRUE;
#ifdef CONFIG_TELNETD_HISTORY
			vmm_cgets(&tdctrl.cdev, cmds, CONFIG_TELNETD_CMD_WIDTH,
				 '\n', &tdctrl.history);
#else
			vmm_cgets(&tdctrl.cdev, cmds, CONFIG_TELNETD_CMD_WIDTH,
				 '\n', NULL);
#endif
			tdctrl.cdev_ingets = FALSE;

			/* Flush Tx & Check disconnected */
			telnetd_flush_tx_buffer();
			if (telnetd_check_disconnected()) {
				break;
			}

			/* Process command string */
			cmds_len = strlen(cmds);
			if (cmds_len > 0) {
				if (cmds[cmds_len - 1] == '\r')
					cmds[cmds_len - 1] = '\0';

				tdctrl.cdev_incmdexec = TRUE;
				vmm_cmdmgr_execute_cmdstr(&tdctrl.cdev, cmds, 
							  telnetd_cmd_filter);
				tdctrl.cdev_incmdexec = FALSE;
			}

			/* Flush Tx & Check disconnected */
			telnetd_flush_tx_buffer();
			if (telnetd_check_disconnected()) {
				break;
			}
		}

		TELNETD_DPRINTF("%s: Close conn.\n", __func__);

		/* Close current connection */
		netstack_socket_close(tdctrl.active_sk);
		netstack_socket_free(tdctrl.active_sk);
		tdctrl.active_sk = NULL;
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
	u32 telnetd_priority;
	u32 telnetd_time_slice;
	struct vmm_devtree_node *node;

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
	if (vmm_devtree_read_u32(node,
				 "telnetd_priority", &telnetd_priority)) {
		telnetd_priority = VMM_THREAD_DEF_PRIORITY;
	}
	if (vmm_devtree_read_u32(node,
				 "telnetd_time_slice", &telnetd_time_slice)) {
		telnetd_time_slice = VMM_THREAD_DEF_TIME_SLICE;
	}
	if (vmm_devtree_read_u32(node,
				 "telnetd_port", &tdctrl.port)) {
		tdctrl.port = 23;
	}

	/* Sanitize telnetd control information */
	tdctrl.sk = NULL;
	tdctrl.active_sk = NULL;
	INIT_SPIN_LOCK(&tdctrl.lock);
	tdctrl.disconnected = TRUE;
	tdctrl.tx_buf_head = tdctrl.tx_buf_tail = tdctrl.tx_buf_count = 0;
	tdctrl.rx_buf_head = tdctrl.rx_buf_tail = tdctrl.rx_buf_count = 0;
	tdctrl.tx_buf = tdctrl.rx_buf = NULL;

	/* Allocate Tx & Rx buffers */
	tdctrl.tx_buf = vmm_zalloc(TELNETD_TX_BUFFER_SIZE);
	if (!tdctrl.tx_buf) {
		return VMM_ENOMEM;
	}
	tdctrl.rx_buf = vmm_zalloc(TELNETD_RX_BUFFER_SIZE);
	if (!tdctrl.rx_buf) {
		vmm_free(tdctrl.tx_buf);
		return VMM_ENOMEM;
	}

	/* Setup telnetd dummy character device */
	tdctrl.cdev_ingets = FALSE;
	tdctrl.cdev_incmdexec = FALSE;
	strcpy(tdctrl.cdev.name, "telnetd");
	tdctrl.cdev.read = telnetd_chardev_read;
	tdctrl.cdev.write = telnetd_chardev_write;

	/* Note: We don't register telnetd dummy character device so that
	 * it is not visible to other part of hypervisor. This way we can
	 * also avoid someone setting telnetd dummy character device as
	 * stdio device.
	 */

	/* Create telnetd main thread */
	tdctrl.main_thread = vmm_threads_create("telnetd", 
						&telnetd_main, 
						NULL, 
						telnetd_priority,
						telnetd_time_slice);
	if (!tdctrl.main_thread) {
		vmm_panic("telnetd: main thread creation failed.\n");
	}

	/* Start telnetd main thread */
	vmm_threads_start(tdctrl.main_thread);

	return VMM_OK;
}

static void __exit daemon_telnetd_exit(void)
{
	/* Stop and destroy telnetd main thread */
	vmm_threads_stop(tdctrl.main_thread);
	vmm_threads_destroy(tdctrl.main_thread);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
