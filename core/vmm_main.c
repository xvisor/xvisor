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
#include <vmm_scheduler.h>
#include <vmm_hyperthreads.h>
#include <vmm_mterm.h>
#include <vmm_devemu.h>
#include <vmm_vserial.h>
#include <vmm_modules.h>
#include <vmm_devdrv.h>
#include <vmm_chardev.h>
#include <vmm_blockdev.h>
#include <vmm_netdev.h>

void vmm_hang(void)
{
	while (1) ;
}

void vmm_init(void)
{
	int ret;

	/* Initialize Heap */
	ret = vmm_heap_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize Device Tree */
	ret = vmm_devtree_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize host virtual address space */
	ret = vmm_host_aspace_init();
	if (ret) {
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
	vmm_printf("%s Version %d.%d (%s %s)\n",
		   VMM_PROJECT_NAME, VMM_PROJECT_VER_MAJOR,
		   VMM_PROJECT_VER_MINOR, __DATE__, __TIME__);
	vmm_printf("\n");

	/* Print intial messages that we missed */
	vmm_printf("Initialize Heap\n");
	vmm_printf("Initialize Device Tree\n");
	vmm_printf("Initialize Host Address Space\n");
	vmm_printf("Initialize CPU Early\n");
	vmm_printf("Initialize Board Early\n");

	/* Initialize host interrupts */
	vmm_printf("Initialize Host Interrupt Subsystem\n");
	ret = vmm_host_irq_init();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

	/* Initialize scheduler */
	vmm_printf("Initialize Scheduler\n");
	ret = vmm_scheduler_init();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

	/* Initialize hyperthreading framework */
	vmm_printf("Initialize Hyperthreading Framework\n");
	ret = vmm_hyperthreading_init();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

	/* Initialize device driver framework */
	vmm_printf("Initialize Device Driver Framework\n");
	ret = vmm_devdrv_init();
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
	vmm_printf("Initialize Networking Framework\n");
	ret = vmm_netdev_init();
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
	vmm_printf("Initialize Virtual Serial Port Framework\n");
	ret = vmm_vserial_init();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

	/* Initialize managment terminal */
	vmm_printf("Initialize Managment Terminal\n");
	ret = vmm_mterm_init();
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

	/* Probe device emulators */
	ret = vmm_devemu_probe();
	if (ret) {
		vmm_hang();
	}
}

void vmm_exit(void)
{
	/* FIXME: Stop all functionality and exit */
}

void vmm_start(void)
{
	int ret;

	/* Start mterm */
	vmm_printf("Start Managment Terminal\n");
	ret = vmm_mterm_start();
	if (ret) {
		vmm_printf("Error %d\n", ret);
		vmm_hang();
	}

	/* Start scheduler */
	vmm_printf("Start Scheduler\n");
	vmm_scheduler_start();

	/* Wait here till scheduler gets invoked by timer */
	vmm_hang();
}
