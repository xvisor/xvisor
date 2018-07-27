/**
 * Copyright (c) 2017 Anup Patel.
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
 * @file vmm_vmsg.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief implementation of virtual messaging subsystem
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_host_aspace.h>
#include <vmm_modules.h>
#include <vmm_workqueue.h>
#include <vio/vmm_vmsg.h>
#include <libs/idr.h>
#include <libs/stringlib.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)			vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define MODULE_DESC			"Virtual Messaging Framework"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_VMSG_IPRIORITY)
#define	MODULE_INIT			vmm_vmsg_init
#define	MODULE_EXIT			vmm_vmsg_exit

struct vmm_vmsg_control {
	struct vmm_mutex lock;
	struct dlist domain_list;
	struct dlist node_list;
	DECLARE_IDA(node_ida);
	struct vmm_blocking_notifier_chain notifier_chain;
	struct vmm_vmsg_domain *default_domain;
};

static struct vmm_vmsg_control vmctrl;

int vmm_vmsg_register_client(struct vmm_notifier_block *nb)
{
	return vmm_blocking_notifier_register(&vmctrl.notifier_chain, nb);
}
VMM_EXPORT_SYMBOL(vmm_vmsg_register_client);

int vmm_vmsg_unregister_client(struct vmm_notifier_block *nb)
{
	return vmm_blocking_notifier_unregister(&vmctrl.notifier_chain, nb);
}
VMM_EXPORT_SYMBOL(vmm_vmsg_unregister_client);

void vmm_vmsg_ref(struct vmm_vmsg *msg)
{
	if (msg) {
		xref_get(&msg->ref_count);
	}
}
VMM_EXPORT_SYMBOL(vmm_vmsg_unregister_client);

static void __vmsg_free(struct xref *ref)
{
	struct vmm_vmsg *msg = container_of(ref, struct vmm_vmsg, ref_count);

	if (msg->free_data) {
		msg->free_data(msg);
	}

	if (msg->free_hdr) {
		msg->free_hdr(msg);
	}
}

void vmm_vmsg_dref(struct vmm_vmsg *msg)
{
	if (msg) {
		xref_put(&msg->ref_count, __vmsg_free);
	}
}
VMM_EXPORT_SYMBOL(vmm_vmsg_dref);

static void vmsg_free_data(struct vmm_vmsg *msg)
{
	vmm_free(msg->data);
}

static void vmsg_free_hdr(struct vmm_vmsg *msg)
{
	vmm_free(msg);
}

struct vmm_vmsg *vmm_vmsg_alloc_ext(u32 dst, u32 src, u32 local,
				    void *data, size_t len, void *priv,
				    void (*free_data)(struct vmm_vmsg *))
{
	struct vmm_vmsg *msg;

	msg = vmm_malloc(sizeof(*msg));
	if (!msg) {
		return NULL;
	}

	INIT_VMSG(msg, dst, src, local, data, len, priv,
		  free_data, vmsg_free_hdr);

	return msg;
}
VMM_EXPORT_SYMBOL(vmm_vmsg_alloc_ext);

struct vmm_vmsg *vmm_vmsg_alloc(u32 dst, u32 src, u32 local,
				size_t len, void *priv)
{
	void *data;
	struct vmm_vmsg *msg;

	if (!len) {
		return NULL;
	}

	data = vmm_malloc(len);
	if (!data) {
		return NULL;
	}

	msg = vmm_vmsg_alloc_ext(dst, src, local, data, len,
				 priv, vmsg_free_data);
	if (!msg) {
		vmm_free(data);
		return NULL;
	}

	return msg;
}
VMM_EXPORT_SYMBOL(vmm_vmsg_alloc);

struct vmsg_work {
	struct dlist head;
	struct vmm_vmsg_domain *domain;
	struct vmm_vmsg *msg;
	char name[VMM_FIELD_NAME_SIZE];
	u32 addr;
	void *data;
	void *data1;
	int (*func) (struct vmsg_work *work);
	void (*free) (struct vmsg_work *work);
};

static void vmsg_free_pool_work(struct vmsg_work *work)
{
	mempool_free(work->domain->work_pool, work);
}

static void vmsg_free_heap_work(struct vmsg_work *work)
{
	vmm_free(work);
}

static int vmsg_domain_enqueue_work(struct vmm_vmsg_domain *domain,
				    struct vmm_vmsg *msg,
				    const char *name, u32 addr,
				    void *data, void *data1,
				    int (*func) (struct vmsg_work *))
{
	irq_flags_t flags;
	struct vmsg_work *work;

	if (!domain) {
		return VMM_EINVALID;
	}

	work = mempool_malloc(domain->work_pool);
	if (!work) {
		work = vmm_malloc(sizeof(*work));
		if (!work) {
			return VMM_ENOMEM;
		}
		work->free = vmsg_free_heap_work;
	} else {
		work->free = vmsg_free_pool_work;
	}

	INIT_LIST_HEAD(&work->head);
	work->domain = domain;
	work->msg = msg;
	strncpy(work->name, name, sizeof(work->name));
	work->addr = addr;
	work->data = data;
	work->data1 = data1;
	work->func = func;

	if (work->msg) {
		vmm_vmsg_ref(work->msg);
	}

	vmm_spin_lock_irqsave(&domain->work_lock, flags);
	list_add_tail(&work->head, &domain->work_list);
	vmm_spin_unlock_irqrestore(&domain->work_lock, flags);

	vmm_completion_complete(&domain->work_avail);

	return VMM_OK;
}

static int vmsg_domain_worker_main(void *data)
{
	int rc;
	irq_flags_t f;
	struct vmm_vmsg_domain *vmd = data;
	struct vmsg_work *work;

	while (1) {
		vmm_completion_wait(&vmd->work_avail);

		work = NULL;
		vmm_spin_lock_irqsave(&vmd->work_lock, f);
		if (!list_empty(&vmd->work_list)) {
			work = list_first_entry(&vmd->work_list,
						struct vmsg_work, head);
			list_del(&work->head);
		}
		vmm_spin_unlock_irqrestore(&vmd->work_lock, f);
		if (!work) {
			continue;
		}

		if (work->func) {
			rc = work->func(work);
			if (rc == VMM_EAGAIN) {
				vmm_spin_lock_irqsave(&vmd->work_lock, f);
				list_add_tail(&work->head, &vmd->work_list);
				vmm_spin_unlock_irqrestore(&vmd->work_lock, f);

				vmm_completion_complete(&vmd->work_avail);

				continue;
			}
		}

		if (work->msg) {
			vmm_vmsg_dref(work->msg);
			work->msg = NULL;
		}

		work->free(work);
	}

	return VMM_OK;
}

static int vmsg_node_peer_down_func(struct vmsg_work *work)
{
	struct vmm_vmsg_node *node;
	struct vmm_vmsg_domain *domain = work->domain;
	const char *peer_name = work->name;
	u32 peer_addr = work->addr;

	vmm_mutex_lock(&domain->node_lock);

	list_for_each_entry(node, &domain->node_list, domain_head) {
		if ((node->addr == peer_addr) ||
		    !arch_atomic_read(&node->is_ready))
			continue;

		if (node->ops->peer_down)
			node->ops->peer_down(node, peer_name, peer_addr);
	}

	vmm_mutex_unlock(&domain->node_lock);

	return VMM_OK;
}

static int vmsg_node_peer_down(struct vmm_vmsg_node *node)
{
	int err;
	struct vmm_vmsg_domain *domain = node->domain;

	DPRINTF("%s: node=%s\n", __func__, node->name);

	if (arch_atomic_cmpxchg(&node->is_ready, 1, 0)) {
		/* TODO: purge all work with work->addr == node->addr */

		err = vmsg_domain_enqueue_work(domain, NULL,
					       node->name, node->addr,
					       NULL, NULL,
					       vmsg_node_peer_down_func);
		if (err) {
			vmm_printf("%s: node=%s error=%d\n",
				   __func__, node->name, err);
			return err;
		}
	}

	return VMM_OK;
}

static int vmsg_node_peer_up_func(struct vmsg_work *work)
{
	struct vmm_vmsg_node *node, *peer_node;
	struct vmm_vmsg_domain *domain = work->domain;
	const char *peer_name = work->name;
	u32 peer_addr = work->addr;

	vmm_mutex_lock(&domain->node_lock);

	peer_node = NULL;
	list_for_each_entry(node, &domain->node_list, domain_head) {
		if (node->addr == peer_addr) {
			peer_node = node;
			break;
		}
	}

	list_for_each_entry(node, &domain->node_list, domain_head) {
		if ((node->addr == peer_addr) ||
		    !arch_atomic_read(&node->is_ready))
			continue;

		if (node->ops->peer_up)
			node->ops->peer_up(node, peer_name, peer_addr);
		if (peer_node && peer_node->ops->peer_up)
			peer_node->ops->peer_up(peer_node,
						node->name, node->addr);
	}

	vmm_mutex_unlock(&domain->node_lock);

	return VMM_OK;
}

static int vmsg_node_peer_up(struct vmm_vmsg_node *node)
{
	int err;
	struct vmm_vmsg_domain *domain = node->domain;

	DPRINTF("%s: node=%s\n", __func__, node->name);

	if (!arch_atomic_cmpxchg(&node->is_ready, 0, 1)) {
		err = vmsg_domain_enqueue_work(domain, NULL,
					       node->name, node->addr,
					       NULL, NULL,
					       vmsg_node_peer_up_func);
		if (err) {
			vmm_printf("%s: node=%s error=%d\n",
				   __func__, node->name, err);
			return err;
		}
	}

	return VMM_OK;
}

static int vmsg_node_send_fast_func(struct vmm_vmsg *msg,
				    struct vmm_vmsg_domain *domain)
{
	int err;
	struct vmm_vmsg_node *node;

	vmm_mutex_lock(&domain->node_lock);

	list_for_each_entry(node, &domain->node_list, domain_head) {
		if ((node->addr == msg->src) ||
		    !arch_atomic_read(&node->is_ready))
			continue;

		if (((node->addr == msg->dst) ||
		    (msg->dst == VMM_VMSG_NODE_ADDR_ANY)) &&
		    (msg->len <= node->max_data_len)) {
			if (node->ops->can_recv_msg && node->ops->recv_msg) {
				if (!node->ops->can_recv_msg(node) &&
				    (msg->dst != VMM_VMSG_NODE_ADDR_ANY)) {
					vmm_mutex_unlock(&domain->node_lock);
					return VMM_EAGAIN;
				}

				err = node->ops->recv_msg(node, msg);
				if (err) {
					vmm_printf("%s: node=%s error=%d\n",
						   __func__, node->name, err);
				}
			}
		}
	}

	vmm_mutex_unlock(&domain->node_lock);

	return VMM_OK;
}

static int vmsg_node_send_func(struct vmsg_work *work)
{
	return vmsg_node_send_fast_func(work->msg, work->domain);
}

static int vmsg_node_send(struct vmm_vmsg_node *node,
			  struct vmm_vmsg *msg, bool fast)
{
	if (!node || !node->domain ||
	    !msg || !msg->data || !msg->len ||
	    (msg->dst == node->addr) ||
	    (msg->dst < VMM_VMSG_NODE_ADDR_MIN)) {
		return VMM_EINVALID;
	}

	msg->src = node->addr;

	DPRINTF("%s: node=%s src=0x%x dst=0x%x len=0x%zx\n",
		__func__, node->name, msg->src, msg->dst, msg->len);

	if (fast) {
		return vmsg_node_send_fast_func(msg, node->domain);
	}

	return vmsg_domain_enqueue_work(node->domain, msg,
					node->name, node->addr,
					NULL, NULL,
					vmsg_node_send_func);
}

static int vmsg_node_start_work_func(struct vmsg_work *work)
{
	int (*fn) (void *) = work->data1;

	return fn(work->data);
}

static int vmsg_node_start_work(struct vmm_vmsg_node *node,
				void *data, int (*fn) (void *))
{
	if (!node || !fn) {
		return VMM_EINVALID;
	}

	DPRINTF("%s: node=%s data=0x%p fn=0x%p\n",
		__func__, node->name, data, fn);

	return vmsg_domain_enqueue_work(node->domain, NULL,
					node->name, node->addr,
					data, fn,
					vmsg_node_start_work_func);
}

static int vmsg_node_stop_work(struct vmm_vmsg_node *node,
			       void *data, int (*fn) (void *))
{
	irq_flags_t flags;
	struct vmsg_work *work, *work1;
	struct vmm_vmsg_domain *domain;

	if (!node || !fn) {
		return VMM_EINVALID;
	}
	domain = node->domain;

	vmm_spin_lock_irqsave(&domain->work_lock, flags);

	list_for_each_entry_safe(work, work1, &domain->work_list, head) {
		if ((work->addr == node->addr) &&
		    (work->data == data) &&
		    (work->data1 == fn) &&
		    (work->msg == NULL)) {
			list_del(&work->head);
			work->free(work);
		}
	}

	vmm_spin_unlock_irqrestore(&domain->work_lock, flags);

	return VMM_OK;
}

struct vmm_vmsg_domain *vmm_vmsg_domain_create(const char *name,
				u32 work_pool_pages, void *priv)
{
	int ret;
	bool found;
	struct vmm_vmsg_event event;
	struct vmm_vmsg_domain *vmd, *new_vmd;

	if (!name || !work_pool_pages) {
		return NULL;
	}

	vmm_mutex_lock(&vmctrl.lock);

	found = FALSE;
	list_for_each_entry(vmd, &vmctrl.domain_list, head) {
		if (strcmp(vmd->name, name) == 0) {
			found = TRUE;
			break;
		}
	}
	if (found) {
		vmm_mutex_unlock(&vmctrl.lock);
		return NULL;
	}

	new_vmd = vmm_zalloc(sizeof(*new_vmd));
	if (!new_vmd) {
		vmm_mutex_unlock(&vmctrl.lock);
		return NULL;
	}

	INIT_LIST_HEAD(&new_vmd->head);
	strncpy(new_vmd->name, name, sizeof(new_vmd->name));
	new_vmd->priv = priv;
	new_vmd->worker = NULL;
	new_vmd->work_pool = NULL;
	INIT_COMPLETION(&new_vmd->work_avail);
	INIT_SPIN_LOCK(&new_vmd->work_lock);
	INIT_LIST_HEAD(&new_vmd->work_list);
	INIT_MUTEX(&new_vmd->node_lock);
	INIT_LIST_HEAD(&new_vmd->node_list);

	new_vmd->work_pool = mempool_ram_create(sizeof(struct vmsg_work),
						work_pool_pages,
						VMM_PAGEPOOL_NORMAL);
	if (!new_vmd->work_pool) {
		vmm_free(new_vmd);
		vmm_mutex_unlock(&vmctrl.lock);
		return NULL;
	}

	new_vmd->worker = vmm_threads_create(name,
					     vmsg_domain_worker_main,
					     new_vmd,
					     VMM_THREAD_DEF_PRIORITY,
					     VMM_THREAD_DEF_TIME_SLICE);
	if (!new_vmd->worker) {
		mempool_destroy(new_vmd->work_pool);
		vmm_free(new_vmd);
		vmm_mutex_unlock(&vmctrl.lock);
		return NULL;
	}

	ret = vmm_threads_start(new_vmd->worker);
	if (ret) {
		vmm_threads_destroy(new_vmd->worker);
		mempool_destroy(new_vmd->work_pool);
		vmm_free(new_vmd);
		vmm_mutex_unlock(&vmctrl.lock);
		return NULL;
	}

	list_add_tail(&new_vmd->head, &vmctrl.domain_list);

	vmm_mutex_unlock(&vmctrl.lock);

	event.data = new_vmd;
	vmm_blocking_notifier_call(&vmctrl.notifier_chain,
				   VMM_VMSG_EVENT_CREATE_DOMAIN,
				   &event);

	return new_vmd;
}
VMM_EXPORT_SYMBOL(vmm_vmsg_domain_create);

int vmm_vmsg_domain_destroy(struct vmm_vmsg_domain *domain)
{
	bool found;
	struct vmm_vmsg_event event;
	struct vmm_vmsg_domain *vmd;

	if (!domain) {
		return VMM_EINVALID;
	}

	event.data = domain;
	vmm_blocking_notifier_call(&vmctrl.notifier_chain,
				   VMM_VMSG_EVENT_DESTROY_DOMAIN,
				   &event);

	vmm_mutex_lock(&vmctrl.lock);

	vmm_mutex_lock(&domain->node_lock);
	if (!list_empty(&domain->node_list)) {
		vmm_mutex_unlock(&domain->node_lock);
		vmm_mutex_unlock(&vmctrl.lock);
		return VMM_EBUSY;
	}
	vmm_mutex_unlock(&domain->node_lock);

	found = FALSE;
	list_for_each_entry(vmd, &vmctrl.domain_list, head) {
		if (strcmp(vmd->name, domain->name) == 0) {
			found = TRUE;
			break;
		}
	}
	if (!found) {
		vmm_mutex_unlock(&vmctrl.lock);
		return VMM_ENOTAVAIL;
	}

	list_del(&domain->head);
	vmm_threads_destroy(domain->worker);
	mempool_destroy(domain->work_pool);
	vmm_free(domain);

	vmm_mutex_unlock(&vmctrl.lock);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_vmsg_domain_destroy);

int vmm_vmsg_domain_iterate(struct vmm_vmsg_domain *start, void *data,
			    int (*fn)(struct vmm_vmsg_domain *, void *))
{
	int rc = VMM_OK;
	bool start_found = (start) ? FALSE : TRUE;
	struct vmm_vmsg_domain *vmd = NULL;

	if (!fn) {
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&vmctrl.lock);

	list_for_each_entry(vmd, &vmctrl.domain_list, head) {
		if (!start_found) {
			if (start && start == vmd) {
				start_found = TRUE;
			} else {
				continue;
			}
		}

		rc = fn(vmd, data);
		if (rc) {
			break;
		}
	}

	vmm_mutex_unlock(&vmctrl.lock);

	return rc;
}
VMM_EXPORT_SYMBOL(vmm_vmsg_domain_iterate);

struct vmsg_domain_find_ctrl {
	const char *name;
	struct vmm_vmsg_domain *domain;
};

static int vmsg_domain_find(struct vmm_vmsg_domain *domain, void *data)
{
	struct vmsg_domain_find_ctrl *c = data;

	if ((strcmp(domain->name, c->name) == 0)) {
		c->domain = domain;
		return 1;
	}

	return 0;
}

struct vmm_vmsg_domain *vmm_vmsg_domain_find(const char *name)
{
	struct vmsg_domain_find_ctrl c;

	if (!name) {
		return NULL;
	}

	c.name = name;
	c.domain = NULL;
	vmm_vmsg_domain_iterate(NULL, &c, vmsg_domain_find);

	return c.domain;
}
VMM_EXPORT_SYMBOL(vmm_vmsg_domain_find);

static int vmsg_domain_count(struct vmm_vmsg_domain *domain, void *data)
{
	u32 *count_ptr = data;

	(*count_ptr)++;

	return 0;
}

u32 vmm_vmsg_domain_count(void)
{
	u32 retval = 0;

	vmm_vmsg_domain_iterate(NULL, &retval, vmsg_domain_count);

	return retval;
}
VMM_EXPORT_SYMBOL(vmm_vmsg_domain_count);

int vmm_vmsg_domain_node_iterate(struct vmm_vmsg_domain *domain,
				 struct vmm_vmsg_node *start, void *data,
				 int (*fn)(struct vmm_vmsg_node *, void *))
{
	int rc = VMM_OK;
	bool start_found = (start) ? FALSE : TRUE;
	struct vmm_vmsg_node *vmn;

	if (!domain || !fn) {
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&domain->node_lock);

	list_for_each_entry(vmn, &domain->node_list, domain_head) {
		if (!start_found) {
			if (start && start == vmn) {
				start_found = TRUE;
			} else {
				continue;
			}
		}

		rc = fn(vmn, data);
		if (rc) {
			break;
		}
	}

	vmm_mutex_unlock(&domain->node_lock);

	return rc;
}

const char *vmm_vmsg_domain_get_name(struct vmm_vmsg_domain *domain)
{
	return (domain) ? domain->name : NULL;
}
VMM_EXPORT_SYMBOL(vmm_vmsg_domain_get_name);

struct vmm_vmsg_node *vmm_vmsg_node_create(const char *name, u32 addr,
					u32 max_data_len,
					struct vmm_vmsg_node_ops *ops,
					struct vmm_vmsg_domain *domain,
					void *priv)
{
	bool found;
	int id_min, id_max, a;
	struct vmm_vmsg_event event;
	struct vmm_vmsg_node *vmn, *new_vmn;

	if (!name || !ops) {
		return NULL;
	}
	if (!domain) {
		domain = vmctrl.default_domain;
	}

	vmm_mutex_lock(&vmctrl.lock);

	found = FALSE;
	list_for_each_entry(vmn, &vmctrl.node_list, head) {
		if (strcmp(vmn->name, name) == 0) {
			found = TRUE;
			break;
		}
	}
	if (found) {
		vmm_mutex_unlock(&vmctrl.lock);
		return NULL;
	}

	new_vmn = vmm_zalloc(sizeof(*new_vmn));
	if (!new_vmn) {
		vmm_mutex_unlock(&vmctrl.lock);
		return NULL;
	}

	if (addr == VMM_VMSG_NODE_ADDR_ANY) {
		id_min = VMM_VMSG_NODE_ADDR_MIN;
		id_max = 0;
	} else if (addr > VMM_VMSG_NODE_ADDR_MIN) {
		id_min = addr;
		id_max = addr + 1;
	} else {
		vmm_free(new_vmn);
		vmm_mutex_unlock(&vmctrl.lock);
		return NULL;
	}

	a = ida_simple_get(&vmctrl.node_ida, id_min, id_max, 0);
	if (a < 0) {
		vmm_free(new_vmn);
		vmm_mutex_unlock(&vmctrl.lock);
		return NULL;
	}

	new_vmn->addr = a;
	INIT_LIST_HEAD(&new_vmn->head);
	INIT_LIST_HEAD(&new_vmn->domain_head);
	strncpy(new_vmn->name, name, sizeof(new_vmn->name));
	new_vmn->max_data_len = max_data_len;
	new_vmn->priv = priv;
	arch_atomic_write(&new_vmn->is_ready, 0);
	new_vmn->domain = domain;
	new_vmn->ops = ops;

	list_add_tail(&new_vmn->head, &vmctrl.node_list);

	vmm_mutex_lock(&domain->node_lock);
	list_add_tail(&new_vmn->domain_head, &domain->node_list);
	vmm_mutex_unlock(&domain->node_lock);

	vmm_mutex_unlock(&vmctrl.lock);

	event.data = new_vmn;
	vmm_blocking_notifier_call(&vmctrl.notifier_chain,
				   VMM_VMSG_EVENT_CREATE_NODE,
				   &event);

	return new_vmn;
}
VMM_EXPORT_SYMBOL(vmm_vmsg_node_create);

int vmm_vmsg_node_destroy(struct vmm_vmsg_node *node)
{
	int err;
	struct vmm_vmsg_domain *domain;
	struct vmm_vmsg_event event;

	if (!node) {
		return VMM_EINVALID;
	}
	domain = node->domain;

	err = vmsg_node_peer_down(node);
	if (err) {
		return err;
	}

	event.data = node;
	vmm_blocking_notifier_call(&vmctrl.notifier_chain,
				   VMM_VMSG_EVENT_DESTROY_NODE,
				   &event);

	vmm_mutex_lock(&vmctrl.lock);

	vmm_mutex_lock(&domain->node_lock);
	list_del(&node->domain_head);
	vmm_mutex_unlock(&domain->node_lock);

	list_del(&node->head);

	ida_simple_remove(&vmctrl.node_ida, node->addr);

	vmm_free(node);

	vmm_mutex_unlock(&vmctrl.lock);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_vmsg_node_destroy);

int vmm_vmsg_node_iterate(struct vmm_vmsg_node *start, void *data,
			  int (*fn)(struct vmm_vmsg_node *, void *))
{
	int rc = VMM_OK;
	bool start_found = (start) ? FALSE : TRUE;
	struct vmm_vmsg_node *vmn;

	if (!fn) {
		return VMM_EINVALID;
	}

	vmm_mutex_lock(&vmctrl.lock);

	list_for_each_entry(vmn, &vmctrl.node_list, head) {
		if (!start_found) {
			if (start && start == vmn) {
				start_found = TRUE;
			} else {
				continue;
			}
		}

		rc = fn(vmn, data);
		if (rc) {
			break;
		}
	}

	vmm_mutex_unlock(&vmctrl.lock);

	return rc;
}
VMM_EXPORT_SYMBOL(vmm_vmsg_node_iterate);

struct vmsg_node_find_ctrl {
	const char *name;
	struct vmm_vmsg_node *node;
};

static int vmsg_node_find(struct vmm_vmsg_node *node, void *data)
{
	struct vmsg_node_find_ctrl *c = data;

	if ((strcmp(node->name, c->name) == 0)) {
		c->node = node;
		return 1;
	}

	return 0;
}

struct vmm_vmsg_node *vmm_vmsg_node_find(const char *name)
{
	struct vmsg_node_find_ctrl c;

	if (!name) {
		return NULL;
	}

	c.name = name;
	c.node = NULL;
	vmm_vmsg_node_iterate(NULL, &c, vmsg_node_find);

	return c.node;
}
VMM_EXPORT_SYMBOL(vmm_vmsg_node_find);

static int vmsg_node_count(struct vmm_vmsg_node *node, void *data)
{
	u32 *count_ptr = data;

	(*count_ptr)++;

	return 0;
}

u32 vmm_vmsg_node_count(void)
{
	u32 retval = 0;

	vmm_vmsg_node_iterate(NULL, &retval, vmsg_node_count);

	return retval;
}
VMM_EXPORT_SYMBOL(vmm_vmsg_node_count);

int vmm_vmsg_node_send(struct vmm_vmsg_node *node, struct vmm_vmsg *msg)
{
	if (!node || !msg) {
		return VMM_EINVALID;
	}

	return vmsg_node_send(node, msg, false);
}
VMM_EXPORT_SYMBOL(vmm_vmsg_node_send);

int vmm_vmsg_node_send_fast(struct vmm_vmsg_node *node, struct vmm_vmsg *msg)
{
	int rc;

	if (!node || !msg) {
		return VMM_EINVALID;
	}

	rc = vmsg_node_send(node, msg, true);
	if (rc == VMM_EAGAIN) {
		return vmsg_node_send(node, msg, false);
	}

	return rc;
}
VMM_EXPORT_SYMBOL(vmm_vmsg_node_send_fast);

int vmm_vmsg_node_start_work(struct vmm_vmsg_node *node,
			     void *data, int (*fn) (void *))
{
	if (!node || !fn) {
		return VMM_EINVALID;
	}

	return vmsg_node_start_work(node, data, fn);
}
VMM_EXPORT_SYMBOL(vmm_vmsg_node_start_work);

int vmm_vmsg_node_stop_work(struct vmm_vmsg_node *node,
			    void *data, int (*fn) (void *))
{
	if (!node || !fn) {
		return VMM_EINVALID;
	}

	return vmsg_node_stop_work(node, data, fn);
}
VMM_EXPORT_SYMBOL(vmm_vmsg_node_stop_work);

void vmm_vmsg_node_ready(struct vmm_vmsg_node *node)
{
	if (!node)
		return;

	BUG_ON(vmsg_node_peer_up(node));
}
VMM_EXPORT_SYMBOL(vmm_vmsg_node_ready);

void vmm_vmsg_node_notready(struct vmm_vmsg_node *node)
{
	if (!node)
		return;

	BUG_ON(vmsg_node_peer_down(node));
}
VMM_EXPORT_SYMBOL(vmm_vmsg_node_notready);

bool vmm_vmsg_node_is_ready(struct vmm_vmsg_node *node)
{
	if (node) {
		return arch_atomic_read(&node->is_ready) ? TRUE : FALSE;
	}

	return FALSE;
}
VMM_EXPORT_SYMBOL(vmm_vmsg_node_is_ready);

const char *vmm_vmsg_node_get_name(struct vmm_vmsg_node *node)
{
	return (node) ? node->name : NULL;
}
VMM_EXPORT_SYMBOL(vmm_vmsg_node_get_name);

u32 vmm_vmsg_node_get_addr(struct vmm_vmsg_node *node)
{
	return (node) ? node->addr : VMM_VMSG_NODE_ADDR_ANY;
}
VMM_EXPORT_SYMBOL(vmm_vmsg_node_get_addr);

u32 vmm_vmsg_node_get_max_data_len(struct vmm_vmsg_node *node)
{
	return (node) ? node->max_data_len : 0x0;
}
VMM_EXPORT_SYMBOL(vmm_vmsg_node_get_max_data_len);

struct vmm_vmsg_domain *vmm_vmsg_node_get_domain(struct vmm_vmsg_node *node)
{
	return (node) ? node->domain : NULL;
}
VMM_EXPORT_SYMBOL(vmm_vmsg_node_get_domain);

static int __init vmm_vmsg_init(void)
{
	memset(&vmctrl, 0, sizeof(vmctrl));

	INIT_MUTEX(&vmctrl.lock);
	INIT_LIST_HEAD(&vmctrl.domain_list);
	INIT_LIST_HEAD(&vmctrl.node_list);
	INIT_IDA(&vmctrl.node_ida);
	BLOCKING_INIT_NOTIFIER_CHAIN(&vmctrl.notifier_chain);

	vmctrl.default_domain = vmm_vmsg_domain_create("vmsg_default",
						       16, NULL);
	if (!vmctrl.default_domain) {
		return VMM_ENOMEM;
	}

	return VMM_OK;
}

static void __exit vmm_vmsg_exit(void)
{
	/* Nothing to do here. */
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
