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
 * @file vsdaemon_telnet.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief vserial daemon telnet transport implementation
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_heap.h>
#include <vmm_spinlocks.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/netstack.h>
#include <libs/vsdaemon.h>

#define MODULE_DESC			"vsdaemon telnet transport"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VSDAEMON_IPRIORITY + \
					 NETSTACK_IPRIORITY + 1)
#define	MODULE_INIT			vsdaemon_telnet_init
#define	MODULE_EXIT			vsdaemon_telnet_exit

#define VSDAEMON_TXBUF_SIZE		4096
#define VSDAEMON_RXTIMEOUT_MS		400

#define VSDAEMON_MAX_FLUSH_SIZE		128

struct vsdaemon_telnet {
	/* tcp port number */
	u32 port;

	/* socket pointers */
	struct netstack_socket *sk;

	/* active connection */
	struct netstack_socket *active_sk;

	/* tx buffer */
	u8 tx_buf[VSDAEMON_TXBUF_SIZE];
	u32 tx_buf_head;
	u32 tx_buf_tail;
	u32 tx_buf_count;
	vmm_spinlock_t tx_buf_lock;
};

static bool vsdaemon_valid_port(u32 port)
{
	if (port < 1024) {
		/* Well-known port number not allowed */
		return FALSE;
	}

	if (65535 < port) {
		/* Maximum port number is 65535 */
		return FALSE;
	}

	return TRUE;
}

static void vsdaemon_flush_tx_buffer(struct vsdaemon_telnet *tnet)
{
	int rc;
	u32 tx_count;
	irq_flags_t flags;
	u8 tx_buf[VSDAEMON_MAX_FLUSH_SIZE];

	while (1) {
		/* Lock connection state */
		vmm_spin_lock_irqsave(&tnet->tx_buf_lock, flags);

		/* Get data from Tx buffer */
		tx_count = 0;
		while (tnet->tx_buf_count &&
		       (tx_count < VSDAEMON_MAX_FLUSH_SIZE)) {
			tx_buf[tx_count] = tnet->tx_buf[tnet->tx_buf_head];
			tnet->tx_buf_head++;
			if (tnet->tx_buf_head >= VSDAEMON_TXBUF_SIZE) {
				tnet->tx_buf_head = 0;
			}
			tnet->tx_buf_count--;
			tx_count++;
		}

		/* Unlock connection state */
		vmm_spin_unlock_irqrestore(&tnet->tx_buf_lock, flags);

		/* Transmit the pending Tx data */
		if (tx_count && tnet->active_sk) {
			rc = netstack_socket_write(tnet->active_sk, 
						   &tx_buf[0], tx_count);
			if (rc) {
				return;
			}
		} else {
			return;
		}
	}
}

static void vsdaemon_telnet_receive_char(struct vsdaemon *vsd, u8 ch)
{
	irq_flags_t flags;
	struct vsdaemon_telnet *tnet = vsdaemon_transport_get_data(vsd);

	vmm_spin_lock_irqsave(&tnet->tx_buf_lock, flags);

	if (VSDAEMON_TXBUF_SIZE == tnet->tx_buf_count) {
		tnet->tx_buf_head++;
		if (tnet->tx_buf_head >= VSDAEMON_TXBUF_SIZE) {
			tnet->tx_buf_head = 0;
		}
		tnet->tx_buf_count--;
	}

	tnet->tx_buf[tnet->tx_buf_tail] = ch;

	tnet->tx_buf_tail++;
	if (tnet->tx_buf_tail >= VSDAEMON_TXBUF_SIZE) {
		tnet->tx_buf_tail = 0;
	}

	tnet->tx_buf_count++;

	vmm_spin_unlock_irqrestore(&tnet->tx_buf_lock, flags);
}

static int vsdaemon_telnet_main_loop(struct vsdaemon *vsd)
{
	int rc;
	struct netstack_socket_buf buf;
	struct vsdaemon_telnet *tnet = vsdaemon_transport_get_data(vsd);

	while (1) {
		rc = netstack_socket_accept(tnet->sk, &tnet->active_sk);
		if (rc) {
			return rc;
		}

		while (1) {
			vsdaemon_flush_tx_buffer(tnet);

			rc = netstack_socket_recv(tnet->active_sk, &buf, 
						  VSDAEMON_RXTIMEOUT_MS);
			if (rc == VMM_ETIMEDOUT) {
				continue;
			} else if (rc) {
				break;
			}

			do {
				vmm_vserial_send(vsd->vser, 
						(u8 *)buf.data, buf.len);
			} while (!(rc = netstack_socket_nextbuf(&buf)));

			netstack_socket_freebuf(&buf);
		}

		netstack_socket_close(tnet->active_sk);
		netstack_socket_free(tnet->active_sk);
		tnet->active_sk = NULL;
	}

	return VMM_OK;
}

static int vsdaemon_telnet_setup(struct vsdaemon *vsd, int argc, char **argv)
{
	int rc = VMM_OK;
	u32 port;
	struct vsdaemon_telnet *tnet;

	port = strtoul(argv[0], NULL, 0);
	if (!vsdaemon_valid_port(port)) {
		return VMM_EINVALID;
	}

	tnet = vmm_zalloc(sizeof(*tnet));
	if (!tnet) {
		return VMM_ENOMEM;
	}

	tnet->port = port;

	tnet->sk = netstack_socket_alloc(NETSTACK_SOCKET_TCP);
	if (!tnet->sk) {
		rc = VMM_ENOMEM;
		goto fail1;
	}

	rc = netstack_socket_bind(tnet->sk, NULL, tnet->port); 
	if (rc) {
		goto fail2;
	}

	rc = netstack_socket_listen(tnet->sk);
	if (rc) {
		goto fail3;
	}

	tnet->active_sk = NULL;

	tnet->tx_buf_head = tnet->tx_buf_tail = tnet->tx_buf_count = 0;
	INIT_SPIN_LOCK(&tnet->tx_buf_lock);

	vsdaemon_transport_set_data(vsd, tnet);

	return VMM_OK;

fail3:
	netstack_socket_close(tnet->sk);
fail2:
	netstack_socket_free(tnet->sk);
fail1:
	vmm_free(tnet);
	return rc;
}

static void vsdaemon_telnet_cleanup(struct vsdaemon *vsd)
{
	struct vsdaemon_telnet *tnet = vsdaemon_transport_get_data(vsd);

	vsdaemon_transport_set_data(vsd, NULL);

	if (tnet->active_sk) {
		netstack_socket_close(tnet->active_sk);
		netstack_socket_free(tnet->active_sk);
	}
	netstack_socket_disconnect(tnet->sk);

	netstack_socket_close(tnet->sk);
	netstack_socket_free(tnet->sk);

	vmm_free(tnet);
}

static struct vsdaemon_transport telnet = {
	.name = "telnet",
	.setup = vsdaemon_telnet_setup,
	.cleanup = vsdaemon_telnet_cleanup,
	.main_loop = vsdaemon_telnet_main_loop,
	.receive_char = vsdaemon_telnet_receive_char,
};

static int __init vsdaemon_telnet_init(void)
{
	return vsdaemon_transport_register(&telnet);
}

static void __exit vsdaemon_telnet_exit(void)
{
	vsdaemon_transport_unregister(&telnet);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
