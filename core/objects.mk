#/**
# Copyright (c) 2010 Anup Patel.
# All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# @file objects.mk
# @author Anup Patel (anup@brainfault.org)
# @brief list of core objects to be build
# */

core-objs-y+= vmm_main.o
core-objs-y+= vmm_heap.o
core-objs-y+= vmm_stdio.o
core-objs-y+= vmm_cpumask.o
core-objs-y+= vmm_devtree.o
core-objs-y+= vmm_devtree_irq.o
core-objs-y+= vmm_devtree_reg.o
core-objs-y+= vmm_devres.o
core-objs-y+= vmm_devdrv.o
core-objs-y+= vmm_devemu.o
core-objs-y+= vmm_resource.o
core-objs-y+= vmm_host_irq.o
core-objs-y+= vmm_host_extirq.o
core-objs-y+= vmm_host_ram.o
core-objs-y+= vmm_host_vapool.o
core-objs-y+= vmm_host_aspace.o
core-objs-y+= vmm_percpu.o
core-objs-$(CONFIG_SMP)+= vmm_smp.o
core-objs-$(CONFIG_SMP)+= vmm_loadbal.o
core-objs-y+= vmm_clocksource.o
core-objs-y+= vmm_clockchip.o
core-objs-y+= vmm_timer.o
core-objs-y+= vmm_delay.o
core-objs-y+= vmm_vcpu_irq.o
core-objs-y+= vmm_guest_aspace.o
core-objs-y+= vmm_manager.o
core-objs-y+= vmm_scheduler.o
core-objs-y+= vmm_threads.o
core-objs-y+= vmm_waitqueue.o
core-objs-y+= vmm_completion.o
core-objs-y+= vmm_semaphore.o
core-objs-y+= vmm_mutex.o
core-objs-y+= vmm_notifier.o
core-objs-y+= vmm_workqueue.o
core-objs-y+= vmm_cmdmgr.o
core-objs-y+= vmm_wallclock.o
core-objs-y+= vmm_chardev.o
core-objs-y+= vmm_modules.o
core-objs-y+= vmm_params.o
core-objs-$(CONFIG_PROFILE)+= vmm_profiler.o
core-objs-$(CONFIG_IOMMU)+= vmm_iommu.o
core-objs-y+= vmm_extable.o
