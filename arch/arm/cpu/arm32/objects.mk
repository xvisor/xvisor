#/**
# Copyright (c) 2011 Pranav Sawargaonkar.
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
# @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
# @author Anup Patel (anup@brainfault.org)
# @brief list of ARM32 cpu objects.
# */

# This selects which instruction set is used.
# Note that GCC does not numerically define an architecture version
# macro, but instead defines a whole series of macros which makes
# testing for a specific architecture or later rather impossible.
arch-$(CONFIG_ARMV7A) += -D__ARM_ARCH_VERSION__=7 -mno-thumb-interwork -march=armv7-a
ifdef CONFIG_ARMV6K
arch-$(CONFIG_ARMV6) += -D__ARM_ARCH_VERSION__=6 -mno-thumb-interwork -march=armv6k
else
arch-$(CONFIG_ARMV6) += -D__ARM_ARCH_VERSION__=6 -mno-thumb-interwork -march=armv6
endif
arch-$(CONFIG_ARMV5) += -D__ARM_ARCH_VERSION__=5 -mno-thumb-interwork -march=armv5te

# Target processor specific tunning options
tune-y	= 

# Need -Uarm for gcc < 3.x
cpu-cppflags+=-DCPU_TEXT_START=0xFF000000
cpu-cflags += -msoft-float -marm -Uarm $(arch-y) $(tune-y)
ifeq ($(CONFIG_ARM32_STACKTRACE), y)
cpu-cflags += -fno-omit-frame-pointer -mapcs -mno-sched-prolog
endif
cpu-asflags += -marm $(arch-y) $(tune-y)
cpu-ldflags += -msoft-float

cpu-objs-y += cpu_entry.o
cpu-objs-y += cpu_mmu.o
cpu-objs-y += cpu_atomic.o
cpu-objs-y += cpu_atomic64.o

cpu-objs-$(CONFIG_CPU_ARM926T)+= cpu_proc_arm926.o
cpu-objs-$(CONFIG_ARMV6)+= cpu_proc_v6.o
cpu-objs-$(CONFIG_ARMV7A)+= cpu_proc_v7.o

cpu-objs-$(CONFIG_ARMV5)+= cpu_cache_v5.o
cpu-objs-$(CONFIG_ARMV6)+= cpu_cache_v6.o
cpu-objs-$(CONFIG_ARMV7A)+= cpu_cache_v7.o

cpu-objs-y+= cpu_init.o
cpu-objs-y+= cpu_delay.o
cpu-objs-y+= cpu_string.o
cpu-objs-y+= cpu_elf.o
cpu-objs-$(CONFIG_ARM32_STACKTRACE)+= cpu_stacktrace.o
cpu-objs-$(CONFIG_SMP)+= cpu_smp.o
cpu-objs-$(CONFIG_SMP)+= cpu_locks.o
cpu-objs-y+= cpu_interrupts.o
cpu-objs-y+= cpu_vcpu_helper.o
cpu-objs-y+= cpu_vcpu_coproc.o
cpu-objs-y+= cpu_vcpu_cp15.o
cpu-objs-y+= cpu_vcpu_mem.o
cpu-objs-y+= cpu_vcpu_irq.o
cpu-objs-y+= cpu_vcpu_hypercall_arm.o
cpu-objs-y+= cpu_vcpu_hypercall_thumb.o

