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
#include <vmm_percpu.h>
#include <vmm_smp.h>
#include <vmm_delay.h>
#include <vmm_stdio.h>
#include <vmm_timer.h>
#include <vmm_completion.h>
#include <vmm_manager.h>
#include <libs/fifo.h>

#define SMP_IPI_MAX_SYNC_PER_CPU	(CONFIG_CPU_COUNT)
#define SMP_IPI_MAX_ASYNC_PER_CPU	(CONFIG_MAX_VCPU_COUNT)

#define SMP_IPI_WAIT_TRY_COUNT		100
#define SMP_IPI_WAIT_UDELAY		1000

#define IPI_VCPU_STACK_SZ 		CONFIG_THREAD_STACK_SIZE
#define IPI_VCPU_PRIORITY 		VMM_VCPU_DEF_PRIORITY
#define IPI_VCPU_TIMESLICE 		VMM_VCPU_DEF_TIME_SLICE

struct smp_ipi_call {
	void (*func)(void *, void *, void *);
	void *arg0;
	void *arg1;
	void *arg2;
};

struct smp_ipi_ctrl {
	struct fifo *sync_fifo;
	struct fifo *async_fifo;
	struct vmm_completion ipi_avail;
	struct vmm_vcpu *ipi_vcpu;
};

static DEFINE_PER_CPU(struct smp_ipi_ctrl, ictl);

static void smp_ipi_sync_submit(u32 cpu, struct smp_ipi_call *ipic)
{
	int try;
	struct smp_ipi_ctrl *ictlp = &per_cpu(ictl, cpu);

	if (!ipic || !ipic->func) {
		return;
	}

	try = SMP_IPI_WAIT_TRY_COUNT;
	while (!fifo_enqueue(ictlp->sync_fifo, ipic, FALSE) && try) {
		vmm_udelay(SMP_IPI_WAIT_UDELAY);
		try--;
	}

	if (!try) {
		WARN(1, "CPU%d: IPI sync fifo full\n", cpu);
	}
}

static void smp_ipi_async_submit(u32 cpu, struct smp_ipi_call *ipic)
{
	int try;
	struct smp_ipi_ctrl *ictlp = &per_cpu(ictl, cpu);

	if (!ipic || !ipic->func) {
		return;
	}

	try = SMP_IPI_WAIT_TRY_COUNT;
	while (!fifo_enqueue(ictlp->async_fifo, ipic, FALSE) && try) {
		vmm_udelay(SMP_IPI_WAIT_UDELAY);
		try--;
	}

	if (!try) {
		WARN(1, "CPU%d: IPI async fifo full\n", cpu);
	}
}

static u32 smp_ipi_sync_pending_count(u32 cpu)
{
	struct smp_ipi_ctrl *ictlp = &per_cpu(ictl, cpu);

	return fifo_avail(ictlp->sync_fifo);
}

static void smp_ipi_main(void)
{
	u32 avail;
	struct smp_ipi_call ipic;
	struct smp_ipi_ctrl *ictlp = &this_cpu(ictl);

	while (1) {
		vmm_completion_wait(&ictlp->ipi_avail);

		/* Process async IPIs */
		avail = fifo_avail(ictlp->async_fifo);
		while (avail && fifo_dequeue(ictlp->async_fifo, &ipic)) {
			if (ipic.func) {
				ipic.func(ipic.arg0, ipic.arg1, ipic.arg2);
			}
			avail--;
		}
	}
}

void vmm_smp_ipi_exec(void)
{
#if 0
	u32 avail;
#endif
	struct smp_ipi_call ipic;
	struct smp_ipi_ctrl *ictlp = &this_cpu(ictl);

	/* Process Sync IPIs */
	while (fifo_dequeue(ictlp->sync_fifo, &ipic)) {
		if (ipic.func) {
			ipic.func(ipic.arg0, ipic.arg1, ipic.arg2);
		}
	}

#if 0
	avail = fifo_avail(ictlp->async_fifo);
	while (avail) {
		if (fifo_dequeue(ictlp->async_fifo, &ipic)) {
			if (ipic.func) {
				ipic.func(ipic.arg0, ipic.arg1, ipic.arg2);
			}
		}
		avail--;
	}
#else
	/* Signal IPI available event */
	vmm_completion_complete(&ictlp->ipi_avail);
#endif
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
		if (c == cpu) {
			func(arg0, arg1, arg2);
		} else {
			if (!vmm_cpu_online(c)) {
				continue;
			}

			ipic.func = func;
			ipic.arg0 = arg0;
			ipic.arg1 = arg1;
			ipic.arg2 = arg2;
			smp_ipi_async_submit(c, &ipic);
			vmm_cpumask_set_cpu(c, &trig_mask);
			trig_count++;
		}
	}

	if (trig_count) {
		arch_smp_ipi_trigger(&trig_mask);
	}
}

int vmm_smp_ipi_sync_call(const struct vmm_cpumask *dest,
			   u32 timeout_msecs,
			   void (*func)(void *, void *, void *),
			   void *arg0, void *arg1, void *arg2)
{
	int rc = VMM_OK;
	u64 timeout_tstamp;
	u32 c, check_count, trig_count, cpu = vmm_smp_processor_id();
	struct vmm_cpumask trig_mask = VMM_CPU_MASK_NONE;
	struct smp_ipi_call ipic;

	if (!dest || !func) {
		return VMM_EFAIL;
	}

	trig_count = 0;
	for_each_cpu(c, dest) {
		if (c == cpu) {
			func(arg0, arg1, arg2);
		} else {
			if (!vmm_cpu_online(c)) {
				continue;
			}

			ipic.func = func;
			ipic.arg0 = arg0;
			ipic.arg1 = arg1;
			ipic.arg2 = arg2;
			smp_ipi_sync_submit(c, &ipic);
			vmm_cpumask_set_cpu(c, &trig_mask);
			trig_count++;
		}
	}

	if (trig_count) {
		arch_smp_ipi_trigger(&trig_mask);

		rc = VMM_ETIMEDOUT;
		timeout_tstamp = vmm_timer_timestamp();
		timeout_tstamp += (u64)timeout_msecs * 1000000ULL;
		while (vmm_timer_timestamp() < timeout_tstamp) {
			check_count = 0;
			for_each_cpu(c, &trig_mask) {
				if (!smp_ipi_sync_pending_count(c)) {
					check_count++;
				}
			}

			if (check_count == trig_count) {
				rc = VMM_OK;
			}

			vmm_udelay(SMP_IPI_WAIT_UDELAY);
		}
	}

	return rc;
}

int __cpuinit vmm_smp_ipi_init(void)
{
	int rc;
	char vcpu_name[VMM_FIELD_NAME_SIZE];
	u32 cpu = vmm_smp_processor_id();
	struct smp_ipi_ctrl *ictlp = &this_cpu(ictl);

	/* Initialize Sync IPI FIFO */
	ictlp->sync_fifo = fifo_alloc(sizeof(struct smp_ipi_call), 
					   SMP_IPI_MAX_SYNC_PER_CPU);
	if (!ictlp->sync_fifo) {
		rc = VMM_ENOMEM;
		goto fail;
	}

	/* Initialize Async IPI FIFO */
	ictlp->async_fifo = fifo_alloc(sizeof(struct smp_ipi_call), 
					   SMP_IPI_MAX_ASYNC_PER_CPU);
	if (!ictlp->async_fifo) {
		rc = VMM_ENOMEM;
		goto fail_free_sync;
	}

	/* Initialize IPI available completion event */
	INIT_COMPLETION(&ictlp->ipi_avail);

	/* Create IPI bottom-half VCPU. (Per Host CPU) */
	vmm_snprintf(vcpu_name, sizeof(vcpu_name), "ipi/%d", cpu);
	ictlp->ipi_vcpu = vmm_manager_vcpu_orphan_create(vcpu_name,
						(virtual_addr_t)&smp_ipi_main,
						IPI_VCPU_STACK_SZ,
						IPI_VCPU_PRIORITY, 
						IPI_VCPU_TIMESLICE);
	if (!ictlp->ipi_vcpu) {
		rc = VMM_EFAIL;
		goto fail_free_async;
	}

	/* The IPI orphan VCPU need to stay on this cpu */
	if ((rc = vmm_manager_vcpu_set_affinity(ictlp->ipi_vcpu,
						vmm_cpumask_of(cpu)))) {
		goto fail_free_vcpu;
	}

	/* Kick IPI orphan VCPU */
	if ((rc = vmm_manager_vcpu_kick(ictlp->ipi_vcpu))) {
		goto fail_free_vcpu;
	}

	/* Arch specific IPI initialization */
	if ((rc = arch_smp_ipi_init())) {
		goto fail_stop_vcpu;
	}

	return VMM_OK;

fail_stop_vcpu:
	vmm_manager_vcpu_halt(ictlp->ipi_vcpu);
fail_free_vcpu:
	vmm_manager_vcpu_orphan_destroy(ictlp->ipi_vcpu);
fail_free_async:
	fifo_free(ictlp->async_fifo);
fail_free_sync:
	fifo_free(ictlp->sync_fifo);
fail:
	return rc;
}

