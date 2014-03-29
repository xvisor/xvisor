#/**
# Copyright (c) 2013 Sukanto Ghosh.
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
# @author Sukanto Ghosh (sukantoghosh@gmail.com)
# @brief list of ARM64 cpu objects.
# */

# This selects which instruction set is used.
# Note that GCC does not numerically define an architecture version
# macro, but instead defines a whole series of macros which makes
# testing for a specific architecture or later rather impossible.
arch-$(CONFIG_ARMV8) += -mgeneral-regs-only -mlittle-endian

cpu-cppflags+=-DTEXT_START=0x10000000
cpu-cflags += $(arch-y) $(tune-y)
cpu-asflags += $(arch-y) $(tune-y)
cpu-ldflags +=

cpu-objs-y+= cpu_entry.o
cpu-objs-y+= cpu_proc.o
cpu-objs-y+= cpu_cache.o
cpu-objs-y+= cpu_init.o
cpu-objs-y+= cpu_delay.o
cpu-objs-y+= cpu_elf.o
cpu-objs-$(CONFIG_ARM64_STACKTRACE)+= cpu_stacktrace.o
cpu-objs-$(CONFIG_SMP)+= cpu_locks.o
cpu-objs-y+= cpu_atomic.o
cpu-objs-y+= cpu_atomic64.o
cpu-objs-y+= cpu_interrupts.o
cpu-objs-y+= cpu_vcpu_helper.o
cpu-objs-y+= cpu_vcpu_coproc.o
cpu-objs-y+= cpu_vcpu_excep.o
cpu-objs-y+= cpu_vcpu_inject.o
cpu-objs-y+= cpu_vcpu_emulate.o
cpu-objs-y+= cpu_vcpu_mem.o
cpu-objs-y+= cpu_vcpu_vfp.o
cpu-objs-y+= cpu_vcpu_sysregs.o
cpu-objs-y+= cpu_vcpu_irq.o

