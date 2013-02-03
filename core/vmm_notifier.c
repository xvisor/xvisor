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
 * @file vmm_notifier.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of notifier chain managment.
 * 
 * The notifer chain managment is highly inspired from Linux notifiers.
 *
 * The linux notifier implementation can be found at:
 * <linux_source>/kernel/notifier.c
 *
 * Linux notifier source is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_notifier.h>

/*
 *  Notifier chain core routines.
 */

static int notifier_chain_register(struct vmm_notifier_block **nl,
				   struct vmm_notifier_block *n)
{
	while ((*nl) != NULL) {
		if (n->priority > (*nl)->priority)
			break;
		nl = &((*nl)->next);
	}
	n->next = *nl;
	*nl = n;
	return VMM_OK;
}

static int notifier_chain_cond_register(struct vmm_notifier_block **nl,
					struct vmm_notifier_block *n)
{
	while ((*nl) != NULL) {
		if ((*nl) == n)
			return VMM_OK;
		if (n->priority > (*nl)->priority)
			break;
		nl = &((*nl)->next);
	}
	n->next = *nl;
	*nl = n;
	return VMM_OK;
}

static int notifier_chain_unregister(struct vmm_notifier_block **nl,
				     struct vmm_notifier_block *n)
{
	while ((*nl) != NULL) {
		if ((*nl) == n) {
			*nl = n->next;
			return VMM_OK;
		}
		nl = &((*nl)->next);
	}
	return VMM_ENOENT;
}

static int notifier_call_chain(struct vmm_notifier_block **nl,
				unsigned long val, void *v,
				int nr_to_call,	int *nr_calls)
{
	int ret = NOTIFY_DONE;
	struct vmm_notifier_block *nb, *next_nb;

	nb = *nl;

	while (nb && nr_to_call) {
		next_nb = nb->next;

		ret = nb->notifier_call(nb, val, v);

		if (nr_calls)
			(*nr_calls)++;

		if ((ret & NOTIFY_STOP_MASK) == NOTIFY_STOP_MASK)
			break;
		nb = next_nb;
		nr_to_call--;
	}

	return ret;
}

/*
 *  Atomic notifier chain routines.
 */

int vmm_atomic_notifier_register(struct vmm_atomic_notifier_chain *nc,
				 struct vmm_notifier_block *n)
{
	irq_flags_t flags;
	int ret;

	vmm_spin_lock_irqsave(&nc->lock, flags);
	ret = notifier_chain_register(&nc->head, n);
	vmm_spin_unlock_irqrestore(&nc->lock, flags);

	return ret;
}

int vmm_atomic_notifier_unregister(struct vmm_atomic_notifier_chain *nc,
				   struct vmm_notifier_block *n)
{
	irq_flags_t flags;
	int ret;

	vmm_spin_lock_irqsave(&nc->lock, flags);
	ret = notifier_chain_unregister(&nc->head, n);
	vmm_spin_unlock_irqrestore(&nc->lock, flags);

	return ret;
}

int __vmm_atomic_notifier_call(struct vmm_atomic_notifier_chain *nc,
				unsigned long val, void *v,
				int nr_to_call, int *nr_calls)
{
	irq_flags_t flags;
	int ret;

	vmm_spin_lock_irqsave(&nc->lock, flags);
	ret = notifier_call_chain(&nc->head, val, v, nr_to_call, nr_calls);
	vmm_spin_unlock_irqrestore(&nc->lock, flags);

	return ret;
}

int vmm_atomic_notifier_call(struct vmm_atomic_notifier_chain *nc,
			     unsigned long val, void *v)
{
	return __vmm_atomic_notifier_call(nc, val, v, -1, NULL);
}

/*
 *  Blocking notifier chain routines.
 */

int vmm_blocking_notifier_register(struct vmm_blocking_notifier_chain *nc,
				   struct vmm_notifier_block *n)
{
	int ret;

	vmm_semaphore_down(&nc->rwsem);
	ret = notifier_chain_register(&nc->head, n);
	vmm_semaphore_up(&nc->rwsem);

	return ret;
}

int vmm_blocking_notifier_cond_register(struct vmm_blocking_notifier_chain *nc,
					struct vmm_notifier_block *n)
{
	int ret;

	vmm_semaphore_down(&nc->rwsem);
	ret = notifier_chain_cond_register(&nc->head, n);
	vmm_semaphore_up(&nc->rwsem);

	return ret;
}

int vmm_blocking_notifier_unregister(struct vmm_blocking_notifier_chain *nc,
				     struct vmm_notifier_block *n)
{
	int ret;

	vmm_semaphore_down(&nc->rwsem);
	ret = notifier_chain_unregister(&nc->head, n);
	vmm_semaphore_up(&nc->rwsem);

	return ret;
}

int __vmm_blocking_notifier_call(struct vmm_blocking_notifier_chain *nc,
				 unsigned long val, void *v,
				 int nr_to_call, int *nr_calls)
{
	int ret = NOTIFY_DONE;

	vmm_semaphore_down(&nc->rwsem);
	ret = notifier_call_chain(&nc->head, val, v, nr_to_call, nr_calls);
	vmm_semaphore_up(&nc->rwsem);

	return ret;
}

int vmm_blocking_notifier_call(struct vmm_blocking_notifier_chain *nc,
				unsigned long val, void *v)
{
	return __vmm_blocking_notifier_call(nc, val, v, -1, NULL);
}

/*
 *  Raw notifier chain routines.
 */

int vmm_raw_notifier_register(struct vmm_raw_notifier_chain *nc,
			      struct vmm_notifier_block *n)
{
	return notifier_chain_register(&nc->head, n);
}

int vmm_raw_notifier_unregister(struct vmm_raw_notifier_chain *nc,
				struct vmm_notifier_block *n)
{
	return notifier_chain_unregister(&nc->head, n);
}

int __vmm_raw_notifier_call(struct vmm_raw_notifier_chain *nc,
			    unsigned long val, void *v,
			    int nr_to_call, int *nr_calls)
{
	return notifier_call_chain(&nc->head, val, v, nr_to_call, nr_calls);
}

int vmm_raw_notifier_call(struct vmm_raw_notifier_chain *nc,
			  unsigned long val, void *v)
{
	return __vmm_raw_notifier_call(nc, val, v, -1, NULL);
}

