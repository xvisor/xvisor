#/**
# Copyright (c) 2010 Himanshu Chauhan.
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
# @author Himanshu Chauhan (hschauhan@nulltrace.org)
# @brief list of MIPS cpu objects.
# */

ifeq ($(CROSS_COMPILE),"")
CROSS_COMPILE=mips-linux-gnu-
endif

cpu-cflags +=-finline-functions -O0
cpu-ldflags += -static-libgcc -lgcc

cpu-objs-y+= start.o
cpu-objs-y+= cpu_atomic.o
ifdef CONFIG_SMP
cpu-objs-y+= cpu_locks.o
endif
cpu-objs-y+= cpu_main.o
cpu-objs-y+= cpu_timer.o
cpu-objs-y+= cpu_interrupts.o
cpu-objs-y+= cpu_host_aspace.o
cpu-objs-y+= cpu_vcpu_irq.o
cpu-objs-y+= cpu_vcpu_helper.o
cpu-objs-y+= cpu_mmu.o
cpu-objs-y+= cpu_vcpu_mmu.o
cpu-objs-y+= cpu_genex.o
cpu-objs-y+= cpu_vcpu_emulate.o

