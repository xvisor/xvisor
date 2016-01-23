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
 * @file vsdaemon.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief vserial daemon library implementation
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_heap.h>
#include <vmm_mutex.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/vsdaemon.h>

#define MODULE_DESC			"vserial telnet library"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		VSDAEMON_IPRIORITY
#define	MODULE_INIT			vsdaemon_init
#define	MODULE_EXIT			vsdaemon_exit

struct vsdaemon_control {
	struct vmm_mutex vsd_list_lock;
	struct dlist vsd_list;
	struct vmm_notifier_block vser_client;
};

static struct vsdaemon_control vsdc;

#define VSDAEMON_MAX_FLUSH_SIZE		128

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

static void vsdaemon_flush_tx_buffer(struct vsdaemon *vsd)
{
	int rc;
	u32 tx_count;
	irq_flags_t flags;
	u8 tx_buf[VSDAEMON_MAX_FLUSH_SIZE];

	while (1) {
		/* Lock connection state */
		vmm_spin_lock_irqsave(&vsd->tx_buf_lock, flags);

		/* Get data from Tx buffer */
		tx_count = 0;
		while (vsd->tx_buf_count &&
		       (tx_count < VSDAEMON_MAX_FLUSH_SIZE)) {
			tx_buf[tx_count] = vsd->tx_buf[vsd->tx_buf_head];
			vsd->tx_buf_head++;
			if (vsd->tx_buf_head >= VSDAEMON_TXBUF_SIZE) {
				vsd->tx_buf_head = 0;
			}
			vsd->tx_buf_count--;
			tx_count++;
		}

		/* Unlock connection state */
		vmm_spin_unlock_irqrestore(&vsd->tx_buf_lock, flags);

		/* Transmit the pending Tx data */
		if (tx_count && vsd->active_sk) {
			rc = netstack_socket_write(vsd->active_sk, 
						   &tx_buf[0], tx_count);
			if (rc) {
				return;
			}
		} else {
			return;
		}
	}
}

static void vsdaemon_vserial_recv(struct vmm_vserial *vser, void *priv, u8 ch)
{
	irq_flags_t flags;
	struct vsdaemon *vsd = priv;

	vmm_spin_lock_irqsave(&vsd->tx_buf_lock, flags);

	if (VSDAEMON_TXBUF_SIZE == vsd->tx_buf_count) {
		vsd->tx_buf_head++;
		if (vsd->tx_buf_head >= VSDAEMON_TXBUF_SIZE) {
			vsd->tx_buf_head = 0;
		}
		vsd->tx_buf_count--;
	}

	vsd->tx_buf[vsd->tx_buf_tail] = ch;

	vsd->tx_buf_tail++;
	if (vsd->tx_buf_tail >= VSDAEMON_TXBUF_SIZE) {
		vsd->tx_buf_tail = 0;
	}

	vsd->tx_buf_count++;

	vmm_spin_unlock_irqrestore(&vsd->tx_buf_lock, flags);
}

static int vsdaemon_main(void *data)
{
	int rc;
	struct vsdaemon *vsd = data;
	struct netstack_socket_buf buf;

	while (1) {
		rc = netstack_socket_accept(vsd->sk, &vsd->active_sk);
		if (rc) {
			return rc;
		}

		while (1) {
			vsdaemon_flush_tx_buffer(vsd);

			rc = netstack_socket_recv(vsd->active_sk, &buf, 
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

		netstack_socket_close(vsd->active_sk);
		netstack_socket_free(vsd->active_sk);
		vsd->active_sk = NULL;
	}

	return VMM_OK;
}

static struct vsdaemon_transport telnet = {
	.name = "telnet",
};

struct vsdaemon_transport *vsdaemon_transport_get(int index)
{
	struct vsdaemon_transport *ret = NULL;

	switch (index) {
	case 0:
		ret = &telnet;
		break;
	default:
		break;
	};

	return ret;
}
VMM_EXPORT_SYMBOL(vsdaemon_transport_get);

u32 vsdaemon_transport_count(void)
{
	return 1;
}
VMM_EXPORT_SYMBOL(vsdaemon_transport_count);

int vsdaemon_create(const char *transport_name,
		    const char *vserial_name,
		    const char *daemon_name,
		    int argc, char **argv)
{
	int rc = VMM_OK;
	bool found;
	struct vsdaemon *vsd;
	struct vsdaemon_transport *trans;
	struct vmm_vserial *vser;
	u32 port;

	if (!transport_name || !vserial_name || !daemon_name) {
		return VMM_EINVALID;
	}

	BUG_ON(!vmm_scheduler_orphan_context());

	if (strncmp(telnet.name, transport_name, sizeof(telnet.name))) {
		return VMM_EINVALID;
	}
	trans = &telnet;

	if (argc < 1) {
		return VMM_EINVALID;
	}
	port = strtoul(argv[0], NULL, 0);
	if (!vsdaemon_valid_port(port)) {
		return VMM_EINVALID;
	}

	vser = vmm_vserial_find(vserial_name);
	if (!vser) {
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&vsdc.vsd_list_lock);

	found = FALSE;
	list_for_each_entry(vsd, &vsdc.vsd_list, head) {
		if (!strncmp(vsd->name, daemon_name, sizeof(vsd->name)) ||
		    vsd->port == port) {
			found = TRUE;
			break;
		}
	}
	if (found) {
		rc = VMM_EEXIST;
		goto fail;
	}

	vsd = vmm_zalloc(sizeof(struct vsdaemon));
	if (!vsd) {
		rc = VMM_ENOMEM;
		goto fail;
	}
	INIT_LIST_HEAD(&vsd->head);
	strlcpy(vsd->name, daemon_name, sizeof(vsd->name) - 1);
	vsd->trans = trans;
	vsd->vser = vser;

	vsd->port = port;

	vsd->sk = netstack_socket_alloc(NETSTACK_SOCKET_TCP);
	if (!vsd->sk) {
		rc = VMM_ENOMEM;
		goto fail1;
	}

	rc = netstack_socket_bind(vsd->sk, NULL, vsd->port); 
	if (rc) {
		goto fail2;
	}

	rc = netstack_socket_listen(vsd->sk);
	if (rc) {
		goto fail3;
	}

	vsd->active_sk = NULL;

	vsd->tx_buf_head = vsd->tx_buf_tail = vsd->tx_buf_count = 0;
	INIT_SPIN_LOCK(&vsd->tx_buf_lock);

	rc = vmm_vserial_register_receiver(vser, &vsdaemon_vserial_recv, vsd);
	if (rc) {
		goto fail3;
	}

	vsd->thread = vmm_threads_create(vsd->name, &vsdaemon_main, vsd, 
					 VMM_THREAD_DEF_PRIORITY,
					 VMM_THREAD_DEF_TIME_SLICE);
	if (!vsd->thread) {
		rc = VMM_EFAIL;
		goto fail4;
	}

	list_add_tail(&vsd->head, &vsdc.vsd_list);

	vmm_threads_start(vsd->thread);

	vmm_mutex_unlock(&vsdc.vsd_list_lock);

	return VMM_OK;

fail4:
	vmm_vserial_unregister_receiver(vser, &vsdaemon_vserial_recv, vsd);
fail3:
	netstack_socket_close(vsd->sk);
fail2:
	netstack_socket_free(vsd->sk);
fail1:
	vmm_free(vsd);
fail:
	vmm_mutex_unlock(&vsdc.vsd_list_lock);
	return rc;
}
VMM_EXPORT_SYMBOL(vsdaemon_create);

/* Note: must be called with vsd_list_lock held */
static int __vsdaemon_destroy(struct vsdaemon *vsd)
{
	vmm_threads_stop(vsd->thread);

	list_del(&vsd->head);

	vmm_vserial_unregister_receiver(vsd->vser, 
					&vsdaemon_vserial_recv, vsd);

	vmm_threads_destroy(vsd->thread);

	/* Telnet disconnect() */
	if (vsd->active_sk) {
		netstack_socket_close(vsd->active_sk);
		netstack_socket_free(vsd->active_sk);
	}
	netstack_socket_disconnect(vsd->sk);

	/* Telent cleanup() */
	netstack_socket_close(vsd->sk);
	netstack_socket_free(vsd->sk);

	vmm_free(vsd);

	return VMM_OK;
}

int vsdaemon_destroy(const char *daemon_name)
{
	int rc = VMM_OK;
	bool found;
	struct vsdaemon *vsd;

	if (!daemon_name) {
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&vsdc.vsd_list_lock);

	vsd = NULL;
	found = FALSE;
	list_for_each_entry(vsd, &vsdc.vsd_list, head) {
		if (!strncmp(vsd->name, daemon_name, sizeof(vsd->name))) {
			found = TRUE;
			break;
		}
	}
	if (!found) {
		rc = VMM_EINVALID;
		goto done;
	}

	rc = __vsdaemon_destroy(vsd);

done:
	vmm_mutex_unlock(&vsdc.vsd_list_lock);

	return rc;
}
VMM_EXPORT_SYMBOL(vsdaemon_destroy);

struct vsdaemon *vsdaemon_get(int index)
{
	bool found;
	struct vsdaemon *vsd;

	BUG_ON(!vmm_scheduler_orphan_context());

	if (index < 0) {
		return NULL;
	}

	vmm_mutex_lock(&vsdc.vsd_list_lock);

	vsd = NULL;
	found = FALSE;
	list_for_each_entry(vsd, &vsdc.vsd_list, head) {
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	vmm_mutex_unlock(&vsdc.vsd_list_lock);

	if (!found) {
		return NULL;
	}

	return vsd;
}
VMM_EXPORT_SYMBOL(vsdaemon_get);

u32 vsdaemon_count(void)
{
	u32 retval = 0;
	struct vsdaemon *vsd;

	BUG_ON(!vmm_scheduler_orphan_context());

	vmm_mutex_lock(&vsdc.vsd_list_lock);

	list_for_each_entry(vsd, &vsdc.vsd_list, head) {
		retval++;
	}

	vmm_mutex_unlock(&vsdc.vsd_list_lock);

	return retval;
}
VMM_EXPORT_SYMBOL(vsdaemon_count);

static int vsdaemon_vserial_notification(struct vmm_notifier_block *nb,
					 unsigned long evt, void *data)
{
	bool found;
	u32 destroy_count = 0;
	struct vsdaemon *vsd;
	struct vmm_vserial_event *e = data;

	if (evt != VMM_VSERIAL_EVENT_DESTROY) {
		/* We are only interested in destroy events so,
		 * don't care about this event.
		 */
		return NOTIFY_DONE;
	}

	vmm_mutex_lock(&vsdc.vsd_list_lock);

again:
	found = FALSE;
	vsd = NULL;
	list_for_each_entry(vsd, &vsdc.vsd_list, head) {
		if (vsd->vser == e->vser) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		__vsdaemon_destroy(vsd);
		destroy_count++;
		goto again;
	}

	vmm_mutex_unlock(&vsdc.vsd_list_lock);

	if (!destroy_count) {
		/* Did not find suitable vsdaemon so,
		 * don't care about this event.
		 */
		return NOTIFY_DONE;
	}

	return NOTIFY_OK;
}

static int __init vsdaemon_init(void)
{
	int rc;

	memset(&vsdc, 0, sizeof(vsdc));

	INIT_MUTEX(&vsdc.vsd_list_lock);
	INIT_LIST_HEAD(&vsdc.vsd_list);

	vsdc.vser_client.notifier_call = &vsdaemon_vserial_notification;
	vsdc.vser_client.priority = 0;
	rc = vmm_vserial_register_client(&vsdc.vser_client);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

static void __exit vsdaemon_exit(void)
{
	vmm_vserial_unregister_client(&vsdc.vser_client);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
