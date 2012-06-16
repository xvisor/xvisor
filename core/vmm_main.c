/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_main.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief main file for core code
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_devtree.h>
#include <vmm_stdio.h>
#include <vmm_version.h>
#include <vmm_host_aspace.h>
#include <vmm_host_irq.h>
#include <vmm_smp.h>
#include <vmm_percpu.h>
#include <vmm_clocksource.h>
#include <vmm_clockchip.h>
#include <vmm_timer.h>
#include <vmm_manager.h>
#include <vmm_scheduler.h>
#include <vmm_threads.h>
#include <vmm_profiler.h>
#include <vmm_devdrv.h>
#include <vmm_devemu.h>
#include <vmm_workqueue.h>
#include <vmm_cmdmgr.h>
#include <vmm_wallclock.h>
#include <vmm_chardev.h>
#include <vmm_vserial.h>
#include <vmm_modules.h>
#include <arch_cpu.h>
#include <arch_board.h>

void vmm_hang(void)
{
	while (1) ;
}

void vmm_init(void)
{
	int ret;
	u32 c, freed, cpu = vmm_smp_processor_id();
	struct dlist *l;
	struct vmm_devtree_node *gnode, *gsnode;
	struct vmm_guest *guest = NULL;

	/* Mark this CPU present */
	vmm_set_cpu_present(cpu, TRUE);

	/* Print version string */
	vmm_printf("\n");
	vmm_printf("%s v%d.%d.%d (%s %s)\n", VMM_NAME, 
		   VMM_VERSION_MAJOR, VMM_VERSION_MINOR, VMM_VERSION_RELEASE,
		   __DATE__, __TIME__);
	vmm_printf("\n");

	/* Initialize host virtual address space */
	vmm_printf("Initialize Host Address Space\n");
	ret = vmm_host_aspace_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize heap */
	vmm_printf("Initialize Heap Management\n");
	ret = vmm_heap_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize CPU early */
	vmm_printf("Initialize CPU Early\n");
	ret = arch_cpu_early_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize Board early */
	vmm_printf("Initialize Board Early\n");
	ret = arch_board_early_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize per-cpu area */
	vmm_printf("Initialize PerCPU Areas\n");
	ret = vmm_percpu_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize device tree */
	vmm_printf("Initialize Device Tree\n");
	ret = vmm_devtree_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize host interrupts */
	vmm_printf("Initialize Host Interrupt Subsystem\n");
	ret = vmm_host_irq_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize standerd input/output */
	vmm_printf("Initialize Standard I/O Subsystem\n");
	ret = vmm_stdio_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize clocksource manager */
	vmm_printf("Initialize Clocksource Manager\n");
	ret = vmm_clocksource_init();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

	/* Initialize clockchip manager */
	vmm_printf("Initialize Clockchip Manager\n");
	ret = vmm_clockchip_init();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

	/* Initialize hypervisor timer */
	vmm_printf("Initialize Hypervisor Timer\n");
	ret = vmm_timer_init();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

	/* Initialize hypervisor manager */
	vmm_printf("Initialize Hypervisor Manager\n");
	ret = vmm_manager_init();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

	/* Initialize hypervisor scheduler */
	vmm_printf("Initialize Hypervisor Scheduler\n");
	ret = vmm_scheduler_init();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

#if defined(CONFIG_SMP)
	/* Initialize secondary CPUs */
	vmm_printf("Initialize Secondary CPUs\n");
	ret = arch_smp_prepare_cpus();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

	/* Start each possible secondary CPUs */
	for_each_possible_cpu(c) {
		ret = arch_smp_start_cpu(c);
		if (ret) {
			vmm_printf("Failed to start CPU%d\n", ret);
		}
	}
#endif

	/* Initialize hypervisor threads */
	vmm_printf("Initialize Hypervisor Threads\n");
	ret = vmm_threads_init();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

#ifdef CONFIG_PROFILE
	/* Intialize hypervisor profiler */
	vmm_printf("Initialize Hypervisor Profiler\n");
	ret = vmm_profiler_init();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}
#endif

	/* Initialize device driver framework */
	vmm_printf("Initialize Device Driver Framework\n");
	ret = vmm_devdrv_init();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

	/* Initialize device emulation framework */
	vmm_printf("Initialize Device Emulation Framework\n");
	ret = vmm_devemu_init();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

	/* Initialize workqueue framework */
	vmm_printf("Initialize Workqueue Framework\n");
	ret = vmm_workqueue_init();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

	/* Initialize command manager */
	vmm_printf("Initialize Command Manager\n");
	ret = vmm_cmdmgr_init();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

	/* Initialize wall-clock */
	vmm_printf("Initialize Wall-Clock Subsystem\n");
	ret = vmm_wallclock_init();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

	/* Initialize character device framework */
	vmm_printf("Initialize Character Device Framework\n");
	ret = vmm_chardev_init();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

	/* Initialize character device framework */
	vmm_printf("Initialize Virtual Serial Port Framework\n");
	ret = vmm_vserial_init();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

	/* Initialize modules */
	ret = vmm_modules_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize cpu final */
	vmm_printf("Initialize CPU Final\n");
	ret = arch_cpu_final_init();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

	/* Intialize board final */
	vmm_printf("Initialize Board Final\n");
	ret = arch_board_final_init();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

	/* Free init memory (Must be third last step) */
	vmm_printf("Freeing init memory: ");
	freed = vmm_host_free_initmem();
	vmm_printf("%dK\n", freed);

	/* Populate guest instances (Must be second last step) */
	vmm_printf("Creating Pre-Configured Guests\n");
	gsnode = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				     VMM_DEVTREE_GUESTINFO_NODE_NAME);
	if (!gsnode) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}
	list_for_each(l, &gsnode->child_list) {
		gnode = list_entry(l, struct vmm_devtree_node, head);
#if defined(CONFIG_VERBOSE_MODE)
		vmm_printf("Creating %s\n", gnode->name);
#endif
		guest = vmm_manager_guest_create(gnode);
		if (!guest) {
			vmm_printf("%s: failed to create %s\n", 
					__func__, gnode->name);
		}
	}

	/* Print status of host CPUs */
	for_each_possible_cpu(c) {
		if (vmm_cpu_online(c)) {
			vmm_printf("CPU%d: Online\n", c);
		} else if (vmm_cpu_present(c)) {
			vmm_printf("CPU%d: Present\n", c);
		} else {
			vmm_printf("CPU%d: Possible\n", c);
		}
	}
	vmm_printf("Brought Up %d CPUs\n", vmm_num_online_cpus());

	/* Start timer (Must be last step) */
	vmm_timer_start();

	/* Wait here till scheduler gets invoked by timer */
	vmm_hang();
}

#if defined(CONFIG_SMP)
void vmm_init_secondary(void)
{
	int ret;
	u32 cpu = vmm_smp_processor_id();

	/* Mark this CPU present */
	vmm_set_cpu_present(cpu, TRUE);

	/* Initialize clockchip manager */
	ret = vmm_clockchip_init();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

	/* Initialize hypervisor timer */
	ret = vmm_timer_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize hypervisor scheduler */
	ret = vmm_scheduler_init();
	if (ret) {
		vmm_hang();
	}

	/* Start timer (Must be last step) */
	vmm_timer_start();

	/* Wait here till scheduler gets invoked by timer */
	vmm_hang();
}
#endif

void vmm_reset(void)
{
	int rc;

	/* Stop scheduler */
	vmm_printf("Stopping Hypervisor Timer\n");
	vmm_timer_stop();

	/* FIXME: Do other cleanup stuff. */

	/* Issue board reset */
	vmm_printf("Issuing Board Reset\n");
	if ((rc = arch_board_reset())) {
		vmm_panic("Error: Board reset failed.\n");
	}

	/* Wait here. Nothing else to do. */
	vmm_hang();
}

void vmm_shutdown(void)
{
	int rc;

	/* Stop scheduler */
	vmm_printf("Stopping Hypervisor Timer Subsytem\n");
	vmm_timer_stop();

	/* FIXME: Do other cleanup stuff. */

	/* Issue board shutdown */
	vmm_printf("Issuing Board Shutdown\n");
	if ((rc = arch_board_shutdown())) {
		vmm_panic("Error: Board shutdown failed.\n");
	}

	/* Wait here. Nothing else to do. */
	vmm_hang();
}
