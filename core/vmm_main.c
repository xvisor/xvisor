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
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief main file for core code
 */

#include <vmm_error.h>
#include <vmm_cpu.h>
#include <vmm_board.h>
#include <vmm_heap.h>
#include <vmm_devtree.h>
#include <vmm_stdio.h>
#include <vmm_version.h>
#include <vmm_host_aspace.h>
#include <vmm_host_irq.h>
#include <vmm_timer.h>
#include <vmm_manager.h>
#include <vmm_scheduler.h>
#include <vmm_threads.h>
#include <vmm_cmdmgr.h>
#include <vmm_devdrv.h>
#include <vmm_devemu.h>
#include <vmm_vserial.h>
#include <vmm_modules.h>
#include <vmm_chardev.h>
#include <vmm_blockdev.h>
#include <vmm_netdev.h>
#include <vmm_profiler.h>

void vmm_hang(void)
{
	while (1) ;
}

void vmm_init(void)
{
	int ret;
	u32 freed;
	struct dlist *l;
	vmm_devtree_node_t *gnode, *gsnode;
	vmm_guest_t *guest = NULL;

	/* Initialize host virtual address space */
	ret = vmm_host_aspace_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize heap */
	ret = vmm_heap_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize device tree */
	ret = vmm_devtree_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize host interrupts */
	ret = vmm_host_irq_init();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

	/* Initialize CPU early */
	ret = vmm_cpu_early_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize Board early */
	ret = vmm_board_early_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize standerd input/output */
	ret = vmm_stdio_init();
	if (ret) {
		vmm_hang();
	}

	/* Print version string */
	vmm_printf("\n");
	vmm_printf("%s Version %d.%d.%d (%s %s)\n",
		   VMM_PROJECT_NAME, VMM_PROJECT_VER_MAJOR,
		   VMM_PROJECT_VER_MINOR, VMM_PROJECT_VER_RELEASE,
		   __DATE__, __TIME__);
	vmm_printf("\n");

	/* Print initial messages that we missed */
	vmm_printf("Initialize Host Address Space\n");
	vmm_printf("Initialize Heap Managment\n");
	vmm_printf("Initialize Device Tree\n");
	vmm_printf("Initialize Host Interrupt Subsystem\n");
	vmm_printf("Initialize CPU Early\n");
	vmm_printf("Initialize Board Early\n");
	vmm_printf("Initialize Standard I/O Subsystem\n");

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

	/* Initialize character device framework */
	vmm_printf("Initialize Character Device Framework\n");
	ret = vmm_chardev_init();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

	/* Initialize block device framework */
	vmm_printf("Initialize Block Device Framework\n");
	ret = vmm_blockdev_init();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

	/* Initialize network device framework */
	vmm_printf("Initialize Networking Device Framework\n");
	ret = vmm_netdev_init();
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

	/* Initialize command manager */
	vmm_printf("Initialize Command Manager\n");
	ret = vmm_cmdmgr_init();
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
	ret = vmm_cpu_final_init();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

	/* Intialize board final */
	vmm_printf("Initialize Board Final\n");
	ret = vmm_board_final_init();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

	/* Free init memory (Must be third last step) */
	vmm_printf("Freeing init memory: ");
	freed = vmm_host_free_initmem();
	vmm_printf("%dK\n", freed);

	/* Populate guest instances (Must be second last step) */
	vmm_printf("Creating Pre-Configured Guest Instances\n");
	gsnode = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPRATOR_STRING
				     VMM_DEVTREE_GUESTINFO_NODE_NAME);
	if (!gsnode) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}
	list_for_each(l, &gsnode->child_list) {
		gnode = list_entry(l, vmm_devtree_node_t, head);
#if defined(CONFIG_VERBOSE_MODE)
		vmm_printf("Creating %s\n", gnode->name);
#endif
		guest = vmm_manager_guest_create(gnode);
		if (!guest) {
			vmm_printf("%s: failed to create %s\n", 
					__func__, gnode->name);
		}
	}

	/* Start timer (Must be last step) */
	vmm_printf("Starting Hypervisor Timer\n");
	vmm_timer_start();

	/* Wait here till scheduler gets invoked by timer */
	vmm_hang();
}

void vmm_reset(void)
{
	int rc;

	/* Stop scheduler */
	vmm_printf("Stopping Hypervisor Timer\n");
	vmm_timer_stop();

	/* FIXME: Do other cleanup stuff. */

	/* Issue board reset */
	vmm_printf("Issuing Board Reset\n");
	if ((rc = vmm_board_reset())) {
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
	if ((rc = vmm_board_shutdown())) {
		vmm_panic("Error: Board shutdown failed.\n");
	}

	/* Wait here. Nothing else to do. */
	vmm_hang();
}
