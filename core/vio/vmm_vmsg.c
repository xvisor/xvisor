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
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_percpu.h>
#include <vmm_spinlocks.h>
#include <vmm_completion.h>
#include <vmm_threads.h>
#include <vmm_host_aspace.h>
#include <vmm_modules.h>
#include <vio/vmm_vmsg.h>
#include <libs/idr.h>
#include <libs/stringlib.h>
#include <libs/mempool.h>

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

struct vmsg_worker;

struct vmsg_work {
	struct dlist head;
	struct vmsg_worker *worker;
	struct vmm_vmsg_domain *domain;
	struct vmm_vmsg *msg;
	char name[VMM_FIELD_NAME_SIZE];
	u32 addr;
	int (*func) (struct vmsg_work *work);
	void (*free) (struct vmsg_work *work);
};

struct vmsg_worker {
	struct mempool *work_pool;
	struct vmm_thread *thread;
	struct vmm_completion bh_avail;
	vmm_spinlock_t bh_lock;
	struct dlist work_list;
	struct dlist lazy_list;
};

static DEFINE_PER_CPU(struct vmsg_worker, vworker);

static void vmsg_free_pool_work(struct vmsg_work *work)
{
	mempool_free(work->worker->work_pool, work);
}

static void vmsg_free_heap_work(struct vmsg_work *work)
{
	vmm_free(work);
}

static int vmsg_enqueue_work(struct vmm_vmsg_domain *domain,
			     struct vmm_vmsg *msg,
			     const char *name, u32 addr,
			     int (*func) (struct vmsg_work *))
{
	irq_flags_t flags;
	struct vmsg_work *work;
	struct vmsg_worker *worker = &this_cpu(vworker);

	if (!domain || !func) {
		return VMM_EINVALID;
	}

	work = mempool_malloc(worker->work_pool);
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
	work->worker = worker;
	work->domain = domain;
	work->msg = msg;
	strncpy(work->name, name, sizeof(work->name));
	work->addr = addr;
	work->func = func;

	if (work->msg) {
		vmm_vmsg_ref(work->msg);
	}

	vmm_spin_lock_irqsave(&worker->bh_lock, flags);
	list_add_tail(&work->head, &worker->work_list);
	vmm_spin_unlock_irqrestore(&worker->bh_lock, flags);

	vmm_completion_complete(&worker->bh_avail);

	return VMM_OK;
}

static int vmsg_enqueue_lazy(struct vmm_vmsg_node_lazy *lazy)
{
	irq_flags_t flags;
	struct vmsg_worker *worker = &this_cpu(vworker);

	if (!lazy) {
		return VMM_EINVALID;
	}

	vmm_spin_lock_irqsave(&worker->bh_lock, flags);
	list_add_tail(&lazy->head, &worker->lazy_list);
	vmm_spin_unlock_irqrestore(&worker->bh_lock, flags);

	vmm_completion_complete(&worker->bh_avail);

	return VMM_OK;
}

static int vmsg_dequeue(struct vmsg_worker *worker,
			struct vmm_vmsg_node_lazy **lazyp,
			struct vmsg_work **workp)
{
	irq_flags_t flags;

	if (!worker || !lazyp || !workp) {
		return VMM_EINVALID;
	}

	vmm_spin_lock_irqsave(&worker->bh_lock, flags);

	if (list_empty(&worker->lazy_list) && list_empty(&worker->work_list)) {
		vmm_spin_unlock_irqrestore(&worker->bh_lock, flags);
		vmm_completion_wait(&worker->bh_avail);
		vmm_spin_lock_irqsave(&worker->bh_lock, flags);
	}

	if (!list_empty(&worker->lazy_list)) {
		*lazyp = list_entry(list_pop(&worker->lazy_list),
				    struct vmm_vmsg_node_lazy, head);
	}

	if (!list_empty(&worker->work_list)) {
		*workp = list_entry(list_pop(&worker->work_list),
				    struct vmsg_work, head);
	}

	vmm_spin_unlock_irqrestore(&worker->bh_lock, flags);

	return VMM_OK;
}

static void vmsg_force_stop_lazy(struct vmm_vmsg_node_lazy *lazy_orig)
{
	u32 cpu;
	bool done = FALSE;
	irq_flags_t flags;
	struct vmsg_worker *worker;
	struct vmm_vmsg_node_lazy *lazy, *lazy1;

	for_each_online_cpu(cpu) {
		worker = &per_cpu(vworker, cpu);

		vmm_spin_lock_irqsave(&worker->bh_lock, flags);

		list_for_each_entry_safe(lazy, lazy1, &worker->lazy_list, head) {
			if (lazy == lazy_orig) {
				list_del(&lazy->head);
				arch_atomic_write(&lazy->sched_count, 0);
				done = TRUE;
				break;
			}
		}

		vmm_spin_unlock_irqrestore(&worker->bh_lock, flags);

		if (done) {
			break;
		}
	}
}

static int vmsg_worker_main(void *data)
{
	int rc;
	irq_flags_t f;
	struct vmsg_work *work;
	struct vmm_vmsg_node_lazy *lazy;
	struct vmsg_worker *worker = data;

	while (1) {
		lazy = NULL;
		work = NULL;
		rc = vmsg_dequeue(worker, &lazy, &work);
		if (rc) {
			continue;
		}

		if (work) {
			/* Call work function */
			rc = work->func(work);
			if (rc == VMM_EAGAIN) {
				vmm_spin_lock_irqsave(&worker->bh_lock, f);
				list_add_tail(&work->head, &worker->work_list);
				vmm_spin_unlock_irqrestore(&worker->bh_lock, f);

				vmm_completion_complete(&worker->bh_avail);

				continue;
			}

			/* Free-up msg */
			if (work->msg) {
				vmm_vmsg_dref(work->msg);
				work->msg = NULL;
			}

			/* Free-up work */
			work->free(work);
		}

		if (lazy) {
			/* Call lazy xfer function */
			lazy->xfer(lazy->node, lazy->arg, lazy->budget);

			/* Add back to netswitch bh queue if required */
			if (arch_atomic_sub_return(&lazy->sched_count, 1) > 0) {
				vmm_spin_lock_irqsave(&worker->bh_lock, f);
				list_add_tail(&lazy->head, &worker->lazy_list);
				vmm_spin_unlock_irqrestore(&worker->bh_lock, f);

				vmm_completion_complete(&worker->bh_avail);
			}
		}
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
	int err = VMM_OK;
	struct vmm_vmsg_domain *domain = node->domain;

	DPRINTF("%s: node=%s\n", __func__, node->name);

	if (arch_atomic_cmpxchg(&node->is_ready, 1, 0)) {
		err = vmsg_enqueue_work(domain, NULL,
					node->name, node->addr,
					vmsg_node_peer_down_func);
		if (err) {
			vmm_printf("%s: node=%s error=%d\n",
				   __func__, node->name, err);
			return err;
		}
	}

	return err;
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
		err = vmsg_enqueue_work(domain, NULL,
					node->name, node->addr,
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

	return vmsg_enqueue_work(node->domain, msg,
				 node->name, node->addr,
				 vmsg_node_send_func);
}

static int vmsg_node_start_lazy(struct vmm_vmsg_node_lazy *lazy)
{
	int rc = VMM_EBUSY;
	struct vmm_vmsg_node *node;
	long sched_count;

	if (!lazy || !lazy->node) {
		return VMM_EINVALID;
	}
	node = lazy->node;

	DPRINTF("%s: node=%s lazy=0x%p\n", __func__, node->name, lazy);

	sched_count = arch_atomic_add_return(&lazy->sched_count, 1);
	if (sched_count == 1) {
		rc = vmsg_enqueue_lazy(lazy);
		if (rc) {
			vmm_printf("%s: node=%s lazy bh enqueue failed.\n",
				   __func__, node->name);
		} else {
			rc = VMM_OK;
		}
	}

	return rc;
}

static int vmsg_node_stop_lazy(struct vmm_vmsg_node_lazy *lazy)
{
	if (!lazy || !lazy->node) {
		return VMM_EINVALID;
	}

	DPRINTF("%s: node=%s lazy=0x%p\n",
		__func__, lazy->node->name, lazy);

	vmsg_force_stop_lazy(lazy);

	return VMM_OK;
}

struct vmm_vmsg_domain *vmm_vmsg_domain_create(const char *name, void *priv)
{
	bool found;
	struct vmm_vmsg_event event;
	struct vmm_vmsg_domain *vmd, *new_vmd;

	if (!name) {
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
	INIT_MUTEX(&new_vmd->node_lock);
	INIT_LIST_HEAD(&new_vmd->node_list);

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

int vmm_vmsg_node_start_lazy(struct vmm_vmsg_node_lazy *lazy)
{
	if (!lazy || !lazy->node || !lazy->xfer) {
		return VMM_EINVALID;
	}

	return vmsg_node_start_lazy(lazy);
}
VMM_EXPORT_SYMBOL(vmm_vmsg_node_start_lazy);

int vmm_vmsg_node_stop_lazy(struct vmm_vmsg_node_lazy *lazy)
{
	if (!lazy || !lazy->node || !lazy->xfer) {
		return VMM_EINVALID;
	}

	return vmsg_node_stop_lazy(lazy);
}
VMM_EXPORT_SYMBOL(vmm_vmsg_node_stop_lazy);

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

static void vmsg_create_workers(void *arg0, void *arg1, void *arg3)
{
	int ret;
	char name[VMM_FIELD_NAME_SIZE];
	u32 cpu = vmm_smp_processor_id();
	struct vmsg_worker *worker = &this_cpu(vworker);

	worker->thread = NULL;
	worker->work_pool = NULL;
	INIT_COMPLETION(&worker->bh_avail);
	INIT_SPIN_LOCK(&worker->bh_lock);
	INIT_LIST_HEAD(&worker->work_list);
	INIT_LIST_HEAD(&worker->lazy_list);

	worker->work_pool = mempool_ram_create(sizeof(struct vmsg_work),
					       8, VMM_PAGEPOOL_NORMAL);
	if (!worker->work_pool) {
		vmm_printf("%s: cpu=%d failed to create work pool\n",
			   __func__, cpu);
		return;
	}

	vmm_snprintf(name, sizeof(name), "vmsg/%d", cpu);
	worker->thread = vmm_threads_create(name, vmsg_worker_main, worker,
					    VMM_THREAD_DEF_PRIORITY,
					    VMM_THREAD_DEF_TIME_SLICE);
	if (!worker->thread) {
		vmm_printf("%s: cpu=%d failed to create thread\n",
			   __func__, cpu);
		mempool_destroy(worker->work_pool);
		return;
	}

	ret = vmm_threads_set_affinity(worker->thread, vmm_cpumask_of(cpu));
	if (ret) {
		vmm_printf("%s: cpu=%d failed to set thread affinity\n",
			   __func__, cpu);
		vmm_threads_destroy(worker->thread);
		mempool_destroy(worker->work_pool);
		return;
	}

	ret = vmm_threads_start(worker->thread);
	if (ret) {
		vmm_printf("%s: cpu=%d failed to start thread\n",
			   __func__, cpu);
		vmm_threads_destroy(worker->thread);
		mempool_destroy(worker->work_pool);
		return;
	}
}

static int __init vmm_vmsg_init(void)
{
	memset(&vmctrl, 0, sizeof(vmctrl));

	INIT_MUTEX(&vmctrl.lock);
	INIT_LIST_HEAD(&vmctrl.domain_list);
	INIT_LIST_HEAD(&vmctrl.node_list);
	INIT_IDA(&vmctrl.node_ida);
	BLOCKING_INIT_NOTIFIER_CHAIN(&vmctrl.notifier_chain);

	vmctrl.default_domain = vmm_vmsg_domain_create("vmsg_default", NULL);
	if (!vmctrl.default_domain) {
		return VMM_ENOMEM;
	}

	vmm_smp_ipi_async_call(cpu_online_mask, vmsg_create_workers,
			       NULL, NULL, NULL);

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
