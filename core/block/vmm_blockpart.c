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
 * @file vmm_blockpart.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for block device partition managment
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_threads.h>
#include <vmm_completion.h>
#include <vmm_modules.h>
#include <block/vmm_blockpart.h>
#include <libs/mathlib.h>

#define MODULE_DESC			"Block Device Partition Managment"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		VMM_BLOCKPART_IPRIORITY
#define	MODULE_INIT			vmm_blockpart_init
#define	MODULE_EXIT			vmm_blockpart_exit

enum blockpart_work_type {
	BLOCKPART_WORK_UNKNOWN=0,
	BLOCKPART_WORK_PARSE=1,
};

struct blockpart_work {
	struct dlist head;
	enum blockpart_work_type type;
	struct vmm_blockdev *bdev;
};

struct blockpart_ctrl {
	vmm_spinlock_t mngr_list_lock;
	struct dlist mngr_list;
	vmm_spinlock_t work_list_lock;
	struct dlist work_list;
	struct vmm_completion work_avail;
	u32 work_count;
	struct vmm_thread *work_thread;
	struct vmm_notifier_block client;
};

static struct blockpart_ctrl bpctrl;

static u32 blockpart_count_work(void)
{
	u32 ret = 0;
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&bpctrl.work_list_lock, flags);
	ret = bpctrl.work_count;
	vmm_spin_unlock_irqrestore(&bpctrl.work_list_lock, flags);

	return ret;
}

static struct blockpart_work *blockpart_pop_work(void)
{
	irq_flags_t flags;
	struct blockpart_work *w = NULL;

	vmm_spin_lock_irqsave(&bpctrl.work_list_lock, flags);

	if (!list_empty(&bpctrl.work_list)) {
		w = list_first_entry(&bpctrl.work_list,
				     struct blockpart_work, head);
		list_del(&w->head);
		bpctrl.work_count--;
	}

	vmm_spin_unlock_irqrestore(&bpctrl.work_list_lock, flags);

	return w;
}

static void blockpart_add_work(enum blockpart_work_type type, 
				struct vmm_blockdev *bdev)
{
	bool found;
	irq_flags_t flags;
	struct blockpart_work *w;

	if (!bdev) {
		return;
	}

	vmm_spin_lock_irqsave(&bpctrl.work_list_lock, flags);

	found = FALSE;
	list_for_each_entry(w, &bpctrl.work_list, head) {
		if ((w->type == type) && (w->bdev == bdev)) {
			found = TRUE;
			break;
		}
	}
	if (!found) {
		w = vmm_zalloc(sizeof(struct blockpart_work));
		if (w) {
			INIT_LIST_HEAD(&w->head);
			w->type = type;
			w->bdev = bdev;
			list_add_tail(&w->head, &bpctrl.work_list);
			bpctrl.work_count++;
		}
	}

	vmm_spin_unlock_irqrestore(&bpctrl.work_list_lock, flags);
}

static void blockpart_del_work(enum blockpart_work_type type, 
				struct vmm_blockdev *bdev)
{
	irq_flags_t flags;
	struct blockpart_work *w;

	if (!bdev) {
		return;
	}

	vmm_spin_lock_irqsave(&bpctrl.work_list_lock, flags);

	list_for_each_entry(w, &bpctrl.work_list, head) {
		if ((w->type == type) && (w->bdev == bdev)) {
			list_del(&w->head);
			bpctrl.work_count--;
			vmm_free(w);
			break;
		}
	}

	vmm_spin_unlock_irqrestore(&bpctrl.work_list_lock, flags);
}

static int blockpart_thread_main(void *udata)
{
	bool parsed;
	int rc, i, j, cnt, wcnt;
	struct blockpart_work *w;
	struct vmm_blockpart_manager *m;

	while (1) {
		vmm_completion_wait(&bpctrl.work_avail);

		wcnt = blockpart_count_work();
		for (i = 0; i < wcnt; i++) {
			w = blockpart_pop_work();
			if (!w) {
				continue;
			}

			switch(w->type) {
			case BLOCKPART_WORK_PARSE:
				parsed = FALSE;
				cnt = vmm_blockpart_manager_count();
				for (j = 0; j < cnt; j++) {
					m = vmm_blockpart_manager_get(j);
					if (!m || !m->parse_part) {
						continue;
					}
					rc = m->parse_part(w->bdev);
					if (rc) {
						continue;
					}
					parsed = TRUE;
					w->bdev->part_manager_sign = m->sign;
					break;
				}
				if (!parsed) {
					blockpart_add_work(w->type, w->bdev);
				}
				break;
			default:
				break;
			};

			vmm_free(w);
		}
	};

	return VMM_OK;
}

static void blockpart_signal_one_work(void)
{
	vmm_completion_complete(&bpctrl.work_avail);
}

static void blockpart_signal_all_work(void)
{
	irq_flags_t flags;
	struct blockpart_work *w;

	vmm_spin_lock_irqsave(&bpctrl.work_list_lock, flags);

	list_for_each_entry(w, &bpctrl.work_list, head) {
		vmm_completion_complete(&bpctrl.work_avail);
	}

	vmm_spin_unlock_irqrestore(&bpctrl.work_list_lock, flags);
}

static int blockpart_blk_notification(struct vmm_notifier_block *nb,
				      unsigned long evt, void *data)
{
	u32 i, cnt;
	int ret = NOTIFY_OK;
	struct vmm_blockdev_event *e = data;
	struct vmm_blockpart_manager *m;

	/* Raw block device with no parent should only be parsed */
	if (e->bdev->parent) {
		return NOTIFY_DONE;
	}

	switch (evt) {
	case VMM_BLOCKDEV_EVENT_REGISTER:
		blockpart_add_work(BLOCKPART_WORK_PARSE, e->bdev);
		blockpart_signal_one_work();
		break;
	case VMM_BLOCKDEV_EVENT_UNREGISTER:
		blockpart_del_work(BLOCKPART_WORK_PARSE, e->bdev);
		cnt = vmm_blockpart_manager_count();
		for (i = 0; i < cnt; i++) {
			m = vmm_blockpart_manager_get(i);
			if (!m || !m->cleanup_part) {
				continue;
			}
			if (m->sign == e->bdev->part_manager_sign) {
				m->cleanup_part(e->bdev);
				break;
			}
		}
		break;
	default:
		ret = NOTIFY_DONE;
		break;
	}

	return ret;
}

int vmm_blockpart_manager_register(struct vmm_blockpart_manager *mngr)
{
	bool found;
	irq_flags_t flags;
	struct vmm_blockpart_manager *mngrt;

	if (!mngr) {
		return VMM_EFAIL;
	}

	mngrt = NULL;
	found = FALSE;

	vmm_spin_lock_irqsave(&bpctrl.mngr_list_lock, flags);

	list_for_each_entry(mngrt, &bpctrl.mngr_list, head) {
		if (mngrt->sign == mngr->sign) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		vmm_spin_unlock_irqrestore(&bpctrl.mngr_list_lock, flags);
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&mngr->head);
	list_add_tail(&mngr->head, &bpctrl.mngr_list);

	vmm_spin_unlock_irqrestore(&bpctrl.mngr_list_lock, flags);

	/* Some blockpart work might not have been processed due to 
	 * unavailability of appropriate partition manager.
	 * To solve, we give dummy wakeup signal for each available work.
	 */
	blockpart_signal_all_work();

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_blockpart_manager_register);

int vmm_blockpart_manager_unregister(struct vmm_blockpart_manager *mngr)
{
	bool found;
	irq_flags_t flags;
	struct vmm_blockpart_manager *mngrt;

	if (!mngr) {
		return VMM_EFAIL;
	}

	vmm_spin_lock_irqsave(&bpctrl.mngr_list_lock, flags);

	if (list_empty(&bpctrl.mngr_list)) {
		vmm_spin_unlock_irqrestore(&bpctrl.mngr_list_lock, flags);
		return VMM_EFAIL;
	}

	mngrt = NULL;
	found = FALSE;
	list_for_each_entry(mngrt, &bpctrl.mngr_list, head) {
		if (mngrt->sign == mngr->sign) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		vmm_spin_unlock_irqrestore(&bpctrl.mngr_list_lock, flags);
		return VMM_ENOTAVAIL;
	}

	list_del(&mngr->head);

	vmm_spin_unlock_irqrestore(&bpctrl.mngr_list_lock, flags);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_blockpart_manager_unregister);

struct vmm_blockpart_manager *vmm_blockpart_manager_get(int index)
{
	bool found;
	irq_flags_t flags;
	struct vmm_blockpart_manager *mngrt;

	if (index < 0) {
		return NULL;
	}

	vmm_spin_lock_irqsave(&bpctrl.mngr_list_lock, flags);

	mngrt = NULL;
	found = FALSE;

	list_for_each_entry(mngrt, &bpctrl.mngr_list, head) {
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	vmm_spin_unlock_irqrestore(&bpctrl.mngr_list_lock, flags);

	if (!found) {
		return NULL;
	}

	return mngrt;
}
VMM_EXPORT_SYMBOL(vmm_blockpart_manager_get);

u32 vmm_blockpart_manager_count(void)
{
	u32 retval = 0;
	irq_flags_t flags;
	struct vmm_blockpart_manager *mngrt;

	vmm_spin_lock_irqsave(&bpctrl.mngr_list_lock, flags);

	list_for_each_entry(mngrt, &bpctrl.mngr_list, head) {
		retval++;
	}

	vmm_spin_unlock_irqrestore(&bpctrl.mngr_list_lock, flags);

	return retval;
}
VMM_EXPORT_SYMBOL(vmm_blockpart_manager_count);

static int __init vmm_blockpart_init(void)
{
	int rc, b, count;
	struct vmm_blockdev *bdev;

	/* Initialize manager list lock */
	INIT_SPIN_LOCK(&bpctrl.mngr_list_lock);

	/* Initialize manager list */
	INIT_LIST_HEAD(&bpctrl.mngr_list);

	/* Initialize work list lock */
	INIT_SPIN_LOCK(&bpctrl.work_list_lock);

	/* Initialize work list */
	INIT_LIST_HEAD(&bpctrl.work_list);

	/* Initialize work available completion */
	INIT_COMPLETION(&bpctrl.work_avail);

	/* Initialize work count */
	bpctrl.work_count = 0;

	/* Register client for block device notifications */
	bpctrl.client.notifier_call = &blockpart_blk_notification;
	bpctrl.client.priority = 0;
	rc = vmm_blockdev_register_client(&bpctrl.client);
	if (rc) {
		return rc;
	}

	/* Create blockpart work thread */
	bpctrl.work_thread = vmm_threads_create("partd", 
						blockpart_thread_main, NULL, 
						VMM_THREAD_DEF_PRIORITY,
						VMM_THREAD_DEF_TIME_SLICE);
	if (!bpctrl.work_thread) {
		return VMM_EFAIL;
	}

	/* We may have block device alread created so 
	 * add raw block device with no parent for parsing partitions 
	 */
	count = vmm_blockdev_count();
	for (b = 0; b < count; b++) {
		bdev = vmm_blockdev_get(b);
		if (!bdev || bdev->parent) {
			continue;
		}
		blockpart_add_work(BLOCKPART_WORK_PARSE, bdev);
		blockpart_signal_one_work();
	}

	/* Start blockpart work thread */
	vmm_threads_start(bpctrl.work_thread);

	return VMM_OK;
}

static void __exit vmm_blockpart_exit(void)
{
	/* Start blockpart workerthread */
	vmm_threads_stop(bpctrl.work_thread);

	/* Destroy blockpart work thread */
	vmm_threads_destroy(bpctrl.work_thread);

	/* Unregister client for block device notifications */
	vmm_blockdev_unregister_client(&bpctrl.client);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
