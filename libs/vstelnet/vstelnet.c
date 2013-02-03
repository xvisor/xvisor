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
 * @file vstelnet.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief vserial telnet library implementation
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_heap.h>
#include <vmm_mutex.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/vstelnet.h>

#define MODULE_DESC			"vserial telnet library"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		VSTELNET_IPRIORITY
#define	MODULE_INIT			vstelnet_init
#define	MODULE_EXIT			vstelnet_exit

struct vstelnet_control {
	struct vmm_mutex vst_list_lock;
	struct dlist vst_list;
	struct vmm_notifier_block vser_client;
};

static struct vstelnet_control vstc;

#define VSTELNET_MAX_FLUSH_SIZE		128

static void vstelnet_flush_tx_buffer(struct vstelnet *vst)
{
	int rc;
	u32 tx_count;
	irq_flags_t flags;
	u8 tx_buf[VSTELNET_MAX_FLUSH_SIZE];

	while (1) {
		/* Lock connection state */
		vmm_spin_lock_irqsave(&vst->tx_buf_lock, flags);

		/* Get data from Tx buffer */
		tx_count = 0;
		while (vst->tx_buf_count &&
		       (tx_count < VSTELNET_MAX_FLUSH_SIZE)) {
			tx_buf[tx_count] = vst->tx_buf[vst->tx_buf_head];
			vst->tx_buf_head++;
			if (vst->tx_buf_head >= VSTELNET_TXBUF_SIZE) {
				vst->tx_buf_head = 0;
			}
			vst->tx_buf_count--;
			tx_count++;
		}

		/* Unlock connection state */
		vmm_spin_unlock_irqrestore(&vst->tx_buf_lock, flags);

		/* Transmit the pending Tx data */
		if (tx_count && vst->active_sk) {
			rc = netstack_socket_write(vst->active_sk, 
						   &tx_buf[0], tx_count);
			if (rc) {
				return;
			}
		} else {
			return;
		}
	}
}

static void vstelnet_vserial_recv(struct vmm_vserial *vser, void *priv, u8 ch)
{
	irq_flags_t flags;
	struct vstelnet *vst = priv;

	vmm_spin_lock_irqsave(&vst->tx_buf_lock, flags);

	if (VSTELNET_TXBUF_SIZE == vst->tx_buf_count) {
		vst->tx_buf_head++;
		if (vst->tx_buf_head >= VSTELNET_TXBUF_SIZE) {
			vst->tx_buf_head = 0;
		}
		vst->tx_buf_count--;
	}

	vst->tx_buf[vst->tx_buf_tail] = ch;

	vst->tx_buf_tail++;
	if (vst->tx_buf_tail >= VSTELNET_TXBUF_SIZE) {
		vst->tx_buf_tail = 0;
	}

	vst->tx_buf_count++;
	
	vmm_spin_unlock_irqrestore(&vst->tx_buf_lock, flags);
}

static int vstelnet_main(void *data)
{
	int rc;
	struct vstelnet *vst = data;
	struct netstack_socket_buf buf;

	while (1) {
		rc = netstack_socket_accept(vst->sk, &vst->active_sk);
		if (rc) {
			return rc;
		}

		while (1) {
			vstelnet_flush_tx_buffer(vst);

			rc = netstack_socket_recv(vst->active_sk, &buf, 
						  VSTELNET_RXTIMEOUT_MS);
			if (rc == VMM_ETIMEDOUT) {
				continue;
			} else if (rc) {
				break;
			}

			do {
				vmm_vserial_send(vst->vser, 
						(u8 *)buf.data, buf.len);
			} while (!(rc = netstack_socket_nextbuf(&buf)));

			netstack_socket_freebuf(&buf);
		}

		netstack_socket_close(vst->active_sk);
		netstack_socket_free(vst->active_sk);
		vst->active_sk = NULL;
	}

	return VMM_OK;
}

struct vstelnet *vstelnet_create(u32 port, const char *vser_name)
{
	char name[32];
	bool found;
	struct dlist *l;
	struct vstelnet *vst;
	struct vmm_vserial *vser;

	BUG_ON(!vmm_scheduler_orphan_context());

	if (!vstelnet_valid_port(port)) {
		return NULL;
	}

	vser = vmm_vserial_find(vser_name);
	if (!vser) {
		return NULL;
	}

	vmm_mutex_lock(&vstc.vst_list_lock);

	found = FALSE;
	list_for_each(l, &vstc.vst_list) {
		vst = list_entry(l, struct vstelnet, head);
		if (vst->vser == vser || vst->port == port) {
			found = TRUE;
			break;
		}
	}
	if (found) {
		goto fail;
	}

	vst = vmm_zalloc(sizeof(struct vstelnet));
	if (!vst) {
		goto fail;
	}

	vst->port = port;

	vst->sk = netstack_socket_alloc(NETSTACK_SOCKET_TCP);
	if (!vst->sk) {
		goto fail1;
	}

	if (netstack_socket_bind(vst->sk, NULL, vst->port)) {
		goto fail2;
	}

	if (netstack_socket_listen(vst->sk)) {
		goto fail3;
	}

	vst->active_sk = NULL;

	vst->tx_buf_head = vst->tx_buf_tail = vst->tx_buf_count = 0;
	INIT_SPIN_LOCK(&vst->tx_buf_lock);

	vst->vser = vser;
	if (vmm_vserial_register_receiver(vser, &vstelnet_vserial_recv, vst)) {
		goto fail3;
	}

	vmm_snprintf(name, 32, "vstelnet-%d", port);
	vst->thread = vmm_threads_create(name, &vstelnet_main, vst, 
					 VMM_THREAD_DEF_PRIORITY,
					 VMM_THREAD_DEF_TIME_SLICE);
	if (!vst->thread) {
		goto fail4;
	}

	list_add_tail(&vst->head, &vstc.vst_list);

	vmm_mutex_unlock(&vstc.vst_list_lock);

	vmm_threads_start(vst->thread);

	return vst;

fail4:
	vmm_vserial_unregister_receiver(vser, &vstelnet_vserial_recv, vst);
fail3:
	netstack_socket_close(vst->sk);
fail2:
	netstack_socket_free(vst->sk);
fail1:
	vmm_free(vst);
fail:
	vmm_mutex_unlock(&vstc.vst_list_lock);
	return NULL;
}
VMM_EXPORT_SYMBOL(vstelnet_create);

int vstelnet_destroy(struct vstelnet *vst)
{
	if (!vst) {
		return VMM_EINVALID;
	}

	vmm_threads_stop(vst->thread);

	vmm_mutex_lock(&vstc.vst_list_lock);

	list_del(&vst->head);

	vmm_threads_destroy(vst->thread);

	vmm_vserial_unregister_receiver(vst->vser, 
					&vstelnet_vserial_recv, vst);

	if (vst->active_sk) {
		netstack_socket_close(vst->active_sk);
		netstack_socket_free(vst->active_sk);
	}

	netstack_socket_disconnect(vst->sk);
	netstack_socket_close(vst->sk);
	netstack_socket_free(vst->sk);

	vmm_free(vst);

	vmm_mutex_unlock(&vstc.vst_list_lock);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vstelnet_destroy);

struct vstelnet *vstelnet_find(u32 port)
{
	bool found;
	struct dlist *l;
	struct vstelnet *ret;

	BUG_ON(!vmm_scheduler_orphan_context());

	if (!vstelnet_valid_port(port)) {
		return NULL;
	}

	vmm_mutex_lock(&vstc.vst_list_lock);

	ret = NULL;
	found = FALSE;

	list_for_each(l, &vstc.vst_list) {
		ret = list_entry(l, struct vstelnet, head);
		if (ret->port == port) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&vstc.vst_list_lock);

	if (!found) {
		return NULL;
	}

	return ret;
}
VMM_EXPORT_SYMBOL(vstelnet_find);

struct vstelnet *vstelnet_get(int index)
{
	bool found;
	struct dlist *l;
	struct vstelnet *ret;

	BUG_ON(!vmm_scheduler_orphan_context());

	if (index < 0) {
		return NULL;
	}

	vmm_mutex_lock(&vstc.vst_list_lock);

	ret = NULL;
	found = FALSE;

	list_for_each(l, &vstc.vst_list) {
		ret = list_entry(l, struct vstelnet, head);
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	vmm_mutex_unlock(&vstc.vst_list_lock);

	if (!found) {
		return NULL;
	}

	return ret;
}
VMM_EXPORT_SYMBOL(vstelnet_get);

u32 vstelnet_count(void)
{
	u32 retval = 0;
	struct dlist *l;

	BUG_ON(!vmm_scheduler_orphan_context());

	vmm_mutex_lock(&vstc.vst_list_lock);

	list_for_each(l, &vstc.vst_list) {
		retval++;
	}

	vmm_mutex_unlock(&vstc.vst_list_lock);

	return retval;
}
VMM_EXPORT_SYMBOL(vstelnet_count);

static int vstelnet_vserial_notification(struct vmm_notifier_block *nb,
					 unsigned long evt, void *data)
{
	bool found;
	struct dlist *l;
	struct vstelnet *vst;
	struct vmm_vserial_event *e = data;

	if (evt != VMM_VSERIAL_EVENT_DESTROY) {
		/* We are only interested in destroy events so,
		 * don't care about this event.
		 */
		return NOTIFY_DONE;
	}

	vmm_mutex_lock(&vstc.vst_list_lock);

	found = FALSE;
	vst = NULL;
	list_for_each(l, &vstc.vst_list) {
		vst = list_entry(l, struct vstelnet, head);
		if (vst->vser == e->vser) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&vstc.vst_list_lock);

	if (!found) {
		/* Did not find suitable vstelnet so,
		 * don't care about this event.
		 */
		return NOTIFY_DONE;
	}

	/* Auto-destroy vstelnet */
	vstelnet_destroy(vst);

	return NOTIFY_OK;
}

static int __init vstelnet_init(void)
{
	int rc;
	memset(&vstc, 0, sizeof(vstc));

	INIT_MUTEX(&vstc.vst_list_lock);
	INIT_LIST_HEAD(&vstc.vst_list);

	vstc.vser_client.notifier_call = &vstelnet_vserial_notification;
	vstc.vser_client.priority = 0;
	rc = vmm_vserial_register_client(&vstc.vser_client);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

static void __exit vstelnet_exit(void)
{
	vmm_vserial_unregister_client(&vstc.vser_client);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
