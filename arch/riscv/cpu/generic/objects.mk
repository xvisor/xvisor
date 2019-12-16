#/**
# Copyright (c) 2018 Anup Patel.
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
# @brief list of RISC-V cpu objects.
# */

# This selects which instruction set is used.
# Note that GCC does not numerically define an architecture version
# macro, but instead defines a whole series of macros which makes
# testing for a specific architecture or later rather impossible.

ifeq ($(CONFIG_64BIT),y)
arch-cflags-y += -mabi=lp64
march-y = rv64im
else
arch-ldflags-y += -static-libgcc -lgcc
arch-cflags-y += -mabi=ilp32
march-y = rv32im
endif

ifeq ($(CONFIG_RISCV_ISA_A),y)
	arch-a-y = a
endif
ifeq ($(CONFIG_RISCV_ISA_C),y)
	arch-c-y = c
endif

arch-cflags-y += -fno-omit-frame-pointer -fno-optimize-sibling-calls
arch-cflags-y += -mno-save-restore -mstrict-align

ifeq ($(CONFIG_CMODEL_MEDLOW),y)
	arch-cflags-y += -mcmodel=medlow
endif
ifeq ($(CONFIG_CMODEL_MEDANY),y)
	arch-cflags-y += -mcmodel=medany
endif

cpu-cppflags+=-DTEXT_START=0x10000000
cpu-cflags += $(arch-cflags-y) -march=$(march-y)$(arch-a-y)$(arch-c-y)
cpu-cflags += -fno-strict-aliasing -O2
cpu-asflags += $(arch-cflags-y) -march=$(march-y)$(arch-a-y)fd$(arch-c-y)
cpu-ldflags += $(arch-ldflags-y)

cpu-objs-y+= cpu_entry.o
cpu-objs-y+= cpu_entry_helper.o
cpu-objs-y+= cpu_proc.o
cpu-objs-y+= cpu_tlb.o
cpu-objs-y+= cpu_sbi.o
cpu-objs-y+= cpu_init.o
cpu-objs-y+= cpu_mmu_initial_pgtbl.o
cpu-objs-y+= cpu_mmu.o
cpu-objs-y+= cpu_delay.o
cpu-objs-$(CONFIG_MODULES)+= cpu_elf.o
cpu-objs-$(CONFIG_RISCV_STACKTRACE)+= cpu_stacktrace.o
cpu-objs-$(CONFIG_SMP)+= cpu_locks.o
cpu-objs-y+= cpu_atomic.o
cpu-objs-y+= cpu_atomic64.o
cpu-objs-y+= cpu_exception.o
cpu-objs-y+= cpu_vcpu_helper.o
cpu-objs-y+= cpu_vcpu_csr.o
cpu-objs-y+= cpu_vcpu_fp.o
cpu-objs-y+= cpu_vcpu_irq.o
cpu-objs-y+= cpu_vcpu_sbi.o
cpu-objs-y+= cpu_vcpu_switch.o
cpu-objs-y+= cpu_vcpu_timer.o
cpu-objs-y+= cpu_vcpu_trap.o
cpu-objs-y+= cpu_vcpu_unpriv.o
