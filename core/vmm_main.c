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
 * @brief main source file to start, stop and reset hypervisor
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
#include <vmm_delay.h>
#include <vmm_manager.h>
#include <vmm_scheduler.h>
#include <vmm_loadbal.h>
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

/* Optional includes */
#include <drv/rtc.h>

void __noreturn vmm_hang(void)
{
	while (1) ;
}

static void system_init_work(struct vmm_work *work)
{
#define BOOTCMD_WIDTH		256
	int ret;
	char bcmd[BOOTCMD_WIDTH];
	const char *str;
	u32 c, freed;
	struct vmm_chardev *cdev;
#if defined(CONFIG_RTC)
	struct rtc_device *rdev;
#endif
	struct vmm_devtree_node *node, *node1;

	/* Initialize command manager */
	vmm_printf("Initialize Command Manager\n");
	ret = vmm_cmdmgr_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

	/* Initialize device driver framework */
	vmm_printf("Initialize Device Driver Framework\n");
	ret = vmm_devdrv_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

	/* Initialize device emulation framework */
	vmm_printf("Initialize Device Emulation Framework\n");
	ret = vmm_devemu_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

	/* Initialize character device framework */
	vmm_printf("Initialize Character Device Framework\n");
	ret = vmm_chardev_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

	/* Initialize virtual serial port framework */
	vmm_printf("Initialize Virtual Serial Port Framework\n");
	ret = vmm_vserial_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

#if defined(CONFIG_SMP)
	/* Poll for all present CPUs to become online */
	/* Note: There is a timeout of 1 second */
	/* Note: The modules might use SMP IPIs or might have per-cpu context 
	 * so, we do this before vmm_modules_init() in-order to make sure that 
	 * correct number of online CPUs are visible to all modules.
	 */
	ret = 1000;
	while(ret--) {
		int all_cpu_online = 1;

		for_each_present_cpu(c) {
			if (!vmm_cpu_online(c)) {
				all_cpu_online = 0;
			}
		}

		if (all_cpu_online) {
			break;
		}

		vmm_mdelay(1);
	}
#endif

	/* Initialize hypervisor modules */
	vmm_printf("Initialize Hypervisor Modules\n");
	ret = vmm_modules_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

	/* Initialize cpu final */
	vmm_printf("Initialize CPU Final\n");
	ret = arch_cpu_final_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

	/* Intialize board final */
	vmm_printf("Initialize Board Final\n");
	ret = arch_board_final_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

	/* Print status of present host CPUs */
	for_each_present_cpu(c) {
		if (vmm_cpu_online(c)) {
			vmm_printf("CPU%d: Online\n", c);
		} else {
			vmm_printf("CPU%d: Possible\n", c);
		}
	}
	vmm_printf("Brought Up %d CPUs\n", vmm_num_online_cpus());

	/* Free init memory */
	vmm_printf("Freeing init memory: ");
	freed = vmm_host_free_initmem();
	vmm_printf("%dK\n", freed);

	/* Process attributes in chosen node */
	node = vmm_devtree_getnode(VMM_DEVTREE_PATH_SEPARATOR_STRING
				   VMM_DEVTREE_CHOSEN_NODE_NAME);
	if (node) {
		/* Find character device based on console attribute */
		str = vmm_devtree_attrval(node, VMM_DEVTREE_CONSOLE_ATTR_NAME);
		if (!(cdev = vmm_chardev_find(str))) {
			if ((node1 = vmm_devtree_getnode(str))) {
				cdev = vmm_chardev_find(node1->name);
			}
		}
		/* Set chosen console device as stdio device */
		if (cdev) {
			vmm_printf("Change stdio device to %s\n", cdev->name);
			vmm_stdio_change_device(cdev);
		}

#if defined(CONFIG_RTC)
		/* Find rtc device based on rtc_device attribute */
		str = vmm_devtree_attrval(node, VMM_DEVTREE_RTCDEV_ATTR_NAME);
		if (!(rdev = rtc_device_find(str))) {
			if ((node1 = vmm_devtree_getnode(str))) {
				rdev = rtc_device_find(node1->name);
			}
		}
		/* Syncup wallclock time with chosen rtc device */
		if (rdev) {
			ret = rtc_device_sync_wallclock(rdev);
			vmm_printf("Syncup wallclock using %s", rdev->name);
			if (ret) {
				vmm_printf("(error %d)", ret);
			}
			vmm_printf("\n");
		}
#endif

		/* Execute boot commands */
		str = vmm_devtree_attrval(node, VMM_DEVTREE_BOOTCMD_ATTR_NAME);
		if (str) {
			c = vmm_devtree_attrlen(node, VMM_DEVTREE_BOOTCMD_ATTR_NAME);
			while (c) {
#if defined(CONFIG_VERBOSE_MODE)
				/* Print boot command */
				vmm_printf("bootcmd: %s\n", str);
#endif
				/* Execute boot command */
				strlcpy(bcmd, str, sizeof(bcmd));
				cdev = vmm_stdio_device();
				vmm_cmdmgr_execute_cmdstr(cdev, bcmd, NULL);
				/* Next boot command */
				c -= strlen(str) + 1;
				str += strlen(str) + 1;
			}
		}
	}
}

void vmm_init(void)
{
	int ret;
#if defined(CONFIG_SMP)
	u32 c;
#endif
	u32 cpu = vmm_smp_processor_id();
	struct vmm_work sysinit;

	/* Mark this CPU possible & present */
	vmm_set_cpu_possible(cpu, TRUE);
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
	vmm_printf("Initialize Host IRQ\n");
	ret = vmm_host_irq_init();
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

	/* Initialize standerd input/output */
	vmm_printf("Initialize Standard I/O\n");
	ret = vmm_stdio_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

	/* Initialize clocksource manager */
	vmm_printf("Initialize Clocksource Manager\n");
	ret = vmm_clocksource_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

	/* Initialize clockchip manager */
	vmm_printf("Initialize Clockchip Manager\n");
	ret = vmm_clockchip_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

	/* Initialize hypervisor timer */
	vmm_printf("Initialize Hypervisor Timer\n");
	ret = vmm_timer_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

	/* Initialize soft delay */
	vmm_printf("Initialize Soft Delay\n");
	ret = vmm_delay_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

	/* Initialize hypervisor manager */
	vmm_printf("Initialize Hypervisor Manager\n");
	ret = vmm_manager_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

	/* Initialize hypervisor scheduler */
	vmm_printf("Initialize Hypervisor Scheduler\n");
	ret = vmm_scheduler_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

	/* Initialize hypervisor threads */
	vmm_printf("Initialize Hypervisor Threads\n");
	ret = vmm_threads_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

#ifdef CONFIG_PROFILE
	/* Intialize hypervisor profiler */
	vmm_printf("Initialize Hypervisor Profiler\n");
	ret = vmm_profiler_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}
#endif

#if defined(CONFIG_SMP)
	/* Initialize inter-processor interrupts */
	vmm_printf("Initialize Inter Processor Interrupts\n")
	ret = vmm_smp_ipi_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize secondary CPUs */
	vmm_printf("Initialize Secondary CPUs\n");
	ret = arch_smp_init_cpus();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

	/* Prepare secondary CPUs */
	ret = arch_smp_prepare_cpus(vmm_num_possible_cpus());
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

	/* Start each present secondary CPUs */
	for_each_present_cpu(c) {
		if (c == cpu) {
			continue;
		}
		ret = arch_smp_start_cpu(c);
		if (ret) {
			vmm_printf("Failed to start CPU%d\n", ret);
		}
	}

	/* Initialize hypervisor load balancer */
	vmm_printf("Initialize Hypervisor Load Balancer\n");
	ret = vmm_loadbal_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}
#endif

	/* Initialize workqueue framework */
	vmm_printf("Initialize Workqueue Framework\n");
	ret = vmm_workqueue_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

	/* Initialize wallclock */
	vmm_printf("Initialize Wallclock Subsystem\n");
	ret = vmm_wallclock_init();
	if (ret) {
		vmm_panic("Error %d\n", ret);
	}

	/* Schedule system initialization work */
	INIT_WORK(&sysinit, &system_init_work);
	vmm_workqueue_schedule_work(NULL, &sysinit);

	/* Start timer (Must be last step) */
	vmm_timer_start();

	/* Wait here till scheduler gets invoked by timer */
	vmm_hang();
}

#if defined(CONFIG_SMP)
void vmm_init_secondary(void)
{
	int ret;

	/* Initialize host virtual address space */
	ret = vmm_host_aspace_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize host interrupts */
	ret = vmm_host_irq_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize clockchip manager */
	ret = vmm_clockchip_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize hypervisor timer */
	ret = vmm_timer_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize soft delay */
	ret = vmm_delay_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize hypervisor scheduler */
	ret = vmm_scheduler_init();
	if (ret) {
		vmm_hang();
	}

	/* Initialize inter-processor interrupts */
	ret = vmm_smp_ipi_init();
	if (ret) {
		vmm_hang();
	}

	/* Start timer (Must be last step) */
	vmm_timer_start();

	/* Wait here till scheduler gets invoked by timer */
	vmm_hang();
}
#endif

static void system_stop(void)
{
	/* Stop scheduler */
	vmm_printf("Stopping Hypervisor Timer\n");
	vmm_timer_stop();

	/* FIXME: Do other cleanup stuff. */
}

static int (*system_reset)() = NULL;

void vmm_register_system_reset(int (*callback)())
{
	system_reset = callback;
}

void vmm_reset(void)
{
	int rc;

	/* Stop the system */
	system_stop();

	/* Issue system reset */
	if (!system_reset) {
		vmm_printf("Error: no system reset callback.\n");
		vmm_printf("Please reset system manually ...\n");
	} else {
		vmm_printf("Issuing System Reset\n");
		if ((rc = system_reset())) {
			vmm_printf("Error: reset failed (error %d)\n", rc);
		}
	}

	/* Wait here. Nothing else to do. */
	vmm_hang();
}

static int (*system_shutdown)() = NULL;

void vmm_register_system_shutdown(int (*callback)())
{
	system_shutdown = callback;
}

void vmm_shutdown(void)
{
	int rc;

	/* Stop the system */
	system_stop();

	/* Issue system shutdown */
	if (!system_shutdown) {
		vmm_printf("Error: no system shutdown callback.\n");
		vmm_printf("Please shutdown system manually ...\n");
	} else {
		vmm_printf("Issuing System Shutdown\n");
		if ((rc = system_shutdown())) {
			vmm_printf("Error: shutdown failed (error %d)\n", rc);
		}
	}

	/* Wait here. Nothing else to do. */
	vmm_hang();
}
