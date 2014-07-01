/**
 * Copyright (c) 2013 Jean-Christophe Dubois
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
 * @file vmm_loadbal.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for hypervisor load balancer
 */

#include <vmm_error.h>
#include <vmm_timer.h>
#include <vmm_manager.h>
#include <vmm_threads.h>
#include <vmm_mutex.h>
#include <vmm_completion.h>
#include <vmm_loadbal.h>

#define LOADBAL_PRIORITY 		VMM_VCPU_DEF_PRIORITY
#define LOADBAL_TIMESLICE 		VMM_VCPU_DEF_TIME_SLICE
#define LOADBAL_PERIOD			(CONFIG_LOADBAL_PERIOD_SECS * \
					 1000000000ULL)

struct vmm_loadbal_ctrl {
	struct vmm_mutex curr_algo_lock;
	struct vmm_loadbal_algo *curr_algo;
	struct vmm_mutex algo_list_lock;
	struct dlist algo_list;
	struct vmm_completion loadbal_cmpl;
	struct vmm_thread *loadbal_thread;
};

static bool lbctrl_init_done = FALSE;
static struct vmm_loadbal_ctrl lbctrl;

u32 vmm_loadbal_good_hcpu(u8 priority)
{
	u32 ret;

	if (!lbctrl_init_done ||
	    !vmm_timer_started() ||
	    (VMM_VCPU_MAX_PRIORITY < priority) ||
	    (priority < VMM_VCPU_MIN_PRIORITY)) {
		return vmm_smp_processor_id();
	}

	vmm_mutex_lock(&lbctrl.curr_algo_lock);

	if (lbctrl.curr_algo && lbctrl.curr_algo->good_hcpu) {
		ret = lbctrl.curr_algo->good_hcpu(lbctrl.curr_algo, priority);
	} else {
		ret = vmm_smp_processor_id();
	}

	vmm_mutex_unlock(&lbctrl.curr_algo_lock);

	return ret;
}

static int loadbal_main(void *data)
{
	u64 tstamp;

	while (1) {
		tstamp = LOADBAL_PERIOD;
		vmm_completion_wait_timeout(&lbctrl.loadbal_cmpl, &tstamp);

		vmm_mutex_lock(&lbctrl.curr_algo_lock);

		if (lbctrl.curr_algo && lbctrl.curr_algo->balance) {
			lbctrl.curr_algo->balance(lbctrl.curr_algo);
		}

		vmm_mutex_unlock(&lbctrl.curr_algo_lock);
	}

	return VMM_OK;
}

struct vmm_loadbal_algo *vmm_loadbal_current_algo(void)
{
	struct vmm_loadbal_algo *ret;

	vmm_mutex_lock(&lbctrl.curr_algo_lock);
	ret = lbctrl.curr_algo;
	vmm_mutex_unlock(&lbctrl.curr_algo_lock);

	return ret;
}

static struct vmm_loadbal_algo *__loadbal_best_algo(void)
{
	u32 best_rating;
	struct vmm_loadbal_algo *algo, *best_algo;

	best_rating = 0;
	best_algo = NULL;
	list_for_each_entry(algo, &lbctrl.algo_list, head) {
		if (best_rating < algo->rating) {
			best_rating = algo->rating;
			best_algo = algo;
		}
	}

	return best_algo;
}

int vmm_loadbal_register_algo(struct vmm_loadbal_algo *lbalgo)
{
	int rc = VMM_OK;
	bool found;
	struct vmm_loadbal_algo *algo, *best_algo;

	/* Sanity checks */
	if (!lbalgo) {
		return VMM_EFAIL;
	}

	/* Lock algo list */
	vmm_mutex_lock(&lbctrl.algo_list_lock);

	/* Registered algo instance should not be present in algo list */
	found = FALSE;
	list_for_each_entry(algo, &lbctrl.algo_list, head) {
		if (algo == lbalgo) {
			found = TRUE;
			break;
		}
	}
	if (found) {
		vmm_mutex_unlock(&lbctrl.algo_list_lock);
		return VMM_EEXIST;
	}

	/* Add registered algo instance to algo list */
	INIT_LIST_HEAD(&lbalgo->head);
	list_add_tail(&lbalgo->head, &lbctrl.algo_list);

	/* Find best algo */
	best_algo = __loadbal_best_algo();

	/* Update current algo */
	vmm_mutex_lock(&lbctrl.curr_algo_lock);
	if (best_algo && lbctrl.curr_algo != best_algo) {
		if (best_algo->start) {
			rc = best_algo->start(best_algo);
		}
		if (rc == VMM_OK) {
			if (lbctrl.curr_algo && lbctrl.curr_algo->stop) {
				lbctrl.curr_algo->stop(lbctrl.curr_algo);
			}
			lbctrl.curr_algo = best_algo;
		}
	}
	vmm_mutex_unlock(&lbctrl.curr_algo_lock);

	/* Unlock algo list */
	vmm_mutex_unlock(&lbctrl.algo_list_lock);

	return rc;
}

int vmm_loadbal_unregister_algo(struct vmm_loadbal_algo *lbalgo)
{
	int rc = VMM_OK;
	bool found;
	struct vmm_loadbal_algo *algo, *best_algo;

	/* Sanity checks */
	if (!lbalgo || !lbalgo->balance) {
		return VMM_EFAIL;
	}

	/* Lock algo list */
	vmm_mutex_lock(&lbctrl.algo_list_lock);

	/* Unregistered algo instance should be present in algo list */
	found = FALSE;
	list_for_each_entry(algo, &lbctrl.algo_list, head) {
		if (algo == lbalgo) {
			found = TRUE;
			break;
		}
	}
	if (!found) {
		vmm_mutex_unlock(&lbctrl.algo_list_lock);
		return VMM_ENOTAVAIL;
	}

	/* Update current algo */
	vmm_mutex_lock(&lbctrl.curr_algo_lock);
	if (lbctrl.curr_algo == lbalgo) {
		if (lbctrl.curr_algo->stop) {
			lbctrl.curr_algo->stop(lbctrl.curr_algo);
		}
		lbctrl.curr_algo = NULL;
	}
	vmm_mutex_unlock(&lbctrl.curr_algo_lock);

	/* Remove from algo list */
	list_del(&lbalgo->head);

	/* Find best algo */
	best_algo = __loadbal_best_algo();

	/* Update current algo */
	vmm_mutex_lock(&lbctrl.curr_algo_lock);
	if (best_algo && lbctrl.curr_algo != best_algo) {
		if (best_algo->start) {
			rc = best_algo->start(best_algo);
		}
		if (rc == VMM_OK) {
			if (lbctrl.curr_algo && lbctrl.curr_algo->stop) {
				lbctrl.curr_algo->stop(lbctrl.curr_algo);
			}
			lbctrl.curr_algo = best_algo;
		}
	}
	vmm_mutex_unlock(&lbctrl.curr_algo_lock);

	/* Unlock algo list */
	vmm_mutex_unlock(&lbctrl.algo_list_lock);

	return rc;
}

int __init vmm_loadbal_init(void)
{
	int rc;

	/* Initialize loadbal current algo */
	INIT_MUTEX(&lbctrl.curr_algo_lock);
	lbctrl.curr_algo = NULL;

	/* Initialize loadbal algo list */
	INIT_MUTEX(&lbctrl.algo_list_lock);
	INIT_LIST_HEAD(&lbctrl.algo_list);

	/* Initialize loadbal completion */
	INIT_COMPLETION(&lbctrl.loadbal_cmpl);

	/* Create loadbal thread with default time slice */
	lbctrl.loadbal_thread = vmm_threads_create("loadbal",
						   loadbal_main, NULL,
						   LOADBAL_PRIORITY,
						   LOADBAL_TIMESLICE);
	if (!lbctrl.loadbal_thread) {
		return VMM_EFAIL;
	}

	/* Set loadbal thread affinity to this cpu */
	if ((rc = vmm_threads_set_affinity(lbctrl.loadbal_thread,
				vmm_cpumask_of(vmm_smp_processor_id())))) {
		return rc;
	}

	/* Start loadbal thread */
	if ((rc = vmm_threads_start(lbctrl.loadbal_thread))) {
		return rc;
	}

	/* Mark loadbal initialization to be complete */
	lbctrl_init_done = TRUE;

	return VMM_OK;
}

