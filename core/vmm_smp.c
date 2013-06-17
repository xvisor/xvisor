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
 * @file vmm_smp.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Symetric Multiprocessor Mamagment APIs Implementation
 */

#include <vmm_error.h>
#include <vmm_smp.h>
#include <vmm_stdio.h>
#include <vmm_timer.h>
#include <libs/list.h>
#include <arch_locks.h>

#define MAX_SMP_IPI_PER_CPU		(CONFIG_CPU_COUNT)

struct smp_ipi_call {
	void (*func)(void *, void *, void *);
	void *arg0;
	void *arg1;
	void *arg2;
};

struct smp_ipi_ctrl {
	arch_spinlock_t lock;
	u32 head;
	u32 tail;
	u32 avail;
	struct smp_ipi_call queue[MAX_SMP_IPI_PER_CPU];
} __cacheline_aligned_in_smp;

static struct smp_ipi_ctrl smp_ipi[CONFIG_CPU_COUNT];

static void smp_ipi_submit(u32 cpu, struct smp_ipi_call *ipic)
{
	struct smp_ipi_call *ipic_p;
	struct smp_ipi_ctrl *ctrl;

	if ((cpu >= CONFIG_CPU_COUNT) || !ipic) {
		return;
	}

	ctrl = &smp_ipi[cpu];

	arch_spin_lock(&ctrl->lock);

	if (ctrl->avail >= MAX_SMP_IPI_PER_CPU) {
		WARN(1, "CPU%d: IPI queue full\n", cpu);
		arch_spin_unlock(&ctrl->lock);
		return;
	}

	ipic_p = &ctrl->queue[ctrl->head];
	ipic_p->func = ipic->func;
	ipic_p->arg0 = ipic->arg0;
	ipic_p->arg1 = ipic->arg1;
	ipic_p->arg2 = ipic->arg2;

	ctrl->head++;
	if (ctrl->head >= MAX_SMP_IPI_PER_CPU) {
		ctrl->head = 0;
	}

	ctrl->avail++;

	arch_spin_unlock(&ctrl->lock);
}

static u32 smp_ipi_pending_count(u32 cpu)
{
	u32 ret;
	struct smp_ipi_ctrl *ctrl;

	if (cpu >= CONFIG_CPU_COUNT) {
		return 0;
	}

	ctrl = &smp_ipi[cpu];

	arch_spin_lock(&ctrl->lock);
	ret = ctrl->avail;
	arch_spin_unlock(&ctrl->lock);

	return ret;
}

void vmm_smp_ipi_exec(void)
{
	u32 cpu = vmm_smp_processor_id();
	struct smp_ipi_call *ipic;
	struct smp_ipi_ctrl *ctrl;

	if (cpu >= CONFIG_CPU_COUNT) {
		return;
	}

	ctrl = &smp_ipi[cpu];

	arch_spin_lock(&ctrl->lock);

	while (ctrl->avail) {
		ipic = &ctrl->queue[ctrl->tail];

		if (ipic->func) {
			ipic->func(ipic->arg0, ipic->arg1, ipic->arg2);
		}
		ipic->func = NULL;

		ctrl->tail++;
		if (ctrl->tail >= MAX_SMP_IPI_PER_CPU) {
			ctrl->tail = 0;
		}

		ctrl->avail--;
	}

	arch_spin_unlock(&ctrl->lock);
}

void vmm_smp_ipi_async_call(const struct vmm_cpumask *dest,
			     void (*func)(void *, void *, void *),
			     void *arg0, void *arg1, void *arg2)
{
	u32 c, trig_count, cpu = vmm_smp_processor_id();
	struct vmm_cpumask trig_mask = VMM_CPU_MASK_NONE;
	struct smp_ipi_call ipic;

	if (!dest || !func) {
		return;
	}

	trig_count = 0;
	for_each_cpu(c, dest) {
		if (!vmm_cpu_online(c)) {
			continue;
		}

		if (c == cpu) {
			func(arg0, arg1, arg2);
		} else {
			ipic.func = func;
			ipic.arg0 = arg0;
			ipic.arg1 = arg1;
			ipic.arg2 = arg2;
			smp_ipi_submit(c, &ipic);
			vmm_cpumask_set_cpu(c, &trig_mask);
			trig_count++;
		}
	}

	if (trig_count) {
		arch_smp_ipi_trigger(&trig_mask);
	}
}

void vmm_smp_ipi_sync_call(const struct vmm_cpumask *dest,
			   u32 timeout_msecs,
			   void (*func)(void *, void *, void *),
			   void *arg0, void *arg1, void *arg2)
{
	u64 timeout_tstamp;
	u32 c, check_count, trig_count, cpu = vmm_smp_processor_id();
	struct vmm_cpumask trig_mask = VMM_CPU_MASK_NONE;
	struct smp_ipi_call ipic;

	if (!dest || !func) {
		return;
	}

	trig_count = 0;
	for_each_cpu(c, dest) {
		if (!vmm_cpu_online(c)) {
			continue;
		}

		if (c == cpu) {
			func(arg0, arg1, arg2);
		} else {
			ipic.func = func;
			ipic.arg0 = arg0;
			ipic.arg1 = arg1;
			ipic.arg2 = arg2;
			smp_ipi_submit(c, &ipic);
			vmm_cpumask_set_cpu(c, &trig_mask);
			trig_count++;
		}
	}

	if (trig_count) {
		arch_smp_ipi_trigger(&trig_mask);

		timeout_tstamp = vmm_timer_timestamp();
		timeout_tstamp += (u64)timeout_msecs * 1000000ULL;
		while (vmm_timer_timestamp() < timeout_tstamp) {
			check_count = 0;
			for_each_cpu(c, &trig_mask) {
				if (!smp_ipi_pending_count(c)) {
					check_count++;
				}
			}

			if (check_count == trig_count) {
				break;
			}
		}
	}
}

int __cpuinit vmm_smp_ipi_init(void)
{
	int rc;
	u32 c, q, cpu = vmm_smp_processor_id();

	/* Initialize IPI list */
	if (!cpu) {
		for (c = 0; c < CONFIG_CPU_COUNT; c++) {
			ARCH_SPIN_LOCK_INIT(&smp_ipi[c].lock);
			smp_ipi[c].head = 0;
			smp_ipi[c].tail = 0;
			smp_ipi[c].avail = 0;
			for (q = 0; q < MAX_SMP_IPI_PER_CPU; q++) {
				smp_ipi[c].queue[q].func = NULL;
				smp_ipi[c].queue[q].arg0 = NULL;
				smp_ipi[c].queue[q].arg1 = NULL;
				smp_ipi[c].queue[q].arg2 = NULL;
			}
		}
	}

	/* Arch specific IPI initialization */
	if ((rc = arch_smp_ipi_init())) {
		return rc;
	}

	return VMM_OK;
}

