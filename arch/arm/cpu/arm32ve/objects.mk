#/**
# Copyright (c) 2012 Anup Patel.
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
# @brief list of ARM32VE cpu objects.
# */

# This selects which instruction set is used.
# Note that GCC does not numerically define an architecture version
# macro, but instead defines a whole series of macros which makes
# testing for a specific architecture or later rather impossible.
arch-$(CONFIG_ARMV7A_VE) += -march=armv7ve -mno-thumb-interwork

# This selects how we optimise for the processor.
tune-$(CONFIG_CPU_CORTEX_A15) += -mcpu=cortex-a15
tune-$(CONFIG_CPU_CORTEX_A7) += -mcpu=cortex-a7
tune-$(CONFIG_CPU_GENERIC_V7_VE) += -mcpu=cortex-a15

# Need -Uarm for gcc < 3.x
cpu-cppflags+=-DTEXT_START=0x10000000
cpu-cflags += -msoft-float -marm -Uarm $(arch-y) $(tune-y)
cpu-cflags += -fno-strict-aliasing -O2
ifeq ($(CONFIG_ARM32VE_STACKTRACE), y)
cpu-cflags += -fno-omit-frame-pointer -mapcs -mno-sched-prolog
endif
cpu-asflags += -marm $(arch-y) $(tune-y)
cpu-ldflags += -msoft-float

cpu-objs-y+= cpu_entry.o
cpu-objs-y+= cpu_proc.o
cpu-objs-y+= cpu_cache.o

cpu-objs-y+= cpu_init.o
cpu-objs-y+= cpu_delay.o
cpu-objs-y+= cpu_memcpy.o
cpu-objs-y+= cpu_memset.o
cpu-objs-$(CONFIG_MODULES)+= cpu_elf.o
cpu-objs-$(CONFIG_ARM32VE_STACKTRACE)+= cpu_stacktrace.o
cpu-objs-y+= cpu_atomic.o
cpu-objs-y+= cpu_atomic64.o
cpu-objs-y+= cpu_interrupts.o
cpu-objs-y+= cpu_vcpu_switch.o
cpu-objs-y+= cpu_vcpu_helper.o
cpu-objs-y+= cpu_vcpu_excep.o
cpu-objs-y+= cpu_vcpu_inject.o
cpu-objs-y+= cpu_vcpu_emulate.o
cpu-objs-y+= cpu_vcpu_coproc.o
cpu-objs-y+= cpu_vcpu_cp14.o
cpu-objs-y+= cpu_vcpu_cp15.o
cpu-objs-y+= cpu_vcpu_vfp.o
cpu-objs-y+= cpu_vcpu_mem.o
cpu-objs-y+= cpu_vcpu_irq.o

