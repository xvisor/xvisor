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
#include <libs/fifo.h>

#define MAX_SMP_IPI_PER_CPU		(CONFIG_MAX_VCPU_COUNT)

struct smp_ipi_call {
	void (*func)(void *, void *, void *);
	void *arg0;
	void *arg1;
	void *arg2;
};

static struct fifo *smp_ipi_fifo[CONFIG_CPU_COUNT];

static void smp_ipi_submit(u32 cpu, struct smp_ipi_call *ipic)
{
	if ((cpu >= CONFIG_CPU_COUNT) || !ipic || !ipic->func) {
		return;
	}

	if (!fifo_enqueue(smp_ipi_fifo[cpu], ipic, FALSE)) {
		WARN(1, "CPU%d: IPI queue full\n", cpu);
		return;
	}
}

static u32 smp_ipi_pending_count(u32 cpu)
{
	if (cpu >= CONFIG_CPU_COUNT) {
		return 0;
	}

	return fifo_avail(smp_ipi_fifo[cpu]);
}

void vmm_smp_ipi_exec(void)
{
	u32 cpu = vmm_smp_processor_id();
	struct smp_ipi_call ipic;
	struct fifo *ififo;

	if (cpu >= CONFIG_CPU_COUNT) {
		return;
	}

	ififo = smp_ipi_fifo[cpu];

	while (fifo_dequeue(ififo, &ipic)) {
		if (ipic.func) {
			ipic.func(ipic.arg0, ipic.arg1, ipic.arg2);
		}
	}
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
			smp_ipi_submit(c, &ipic);
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
				return VMM_OK;
			}
		}
		return VMM_ETIMEDOUT;
	} else {
		return VMM_OK;
	}
}

int __cpuinit vmm_smp_ipi_init(void)
{
	int rc;
	u32 c, cpu = vmm_smp_processor_id();

	/* Initialize IPI FIFOs */
	if (!cpu) {
		for (c = 0; c < CONFIG_CPU_COUNT; c++) {
			smp_ipi_fifo[c] = 
				fifo_alloc(sizeof(struct smp_ipi_call), 
					   MAX_SMP_IPI_PER_CPU);
			if (!smp_ipi_fifo[c]) {
				return VMM_ENOMEM;
			}
		}
	}

	/* Arch specific IPI initialization */
	if ((rc = arch_smp_ipi_init())) {
		return rc;
	}

	return VMM_OK;
}

