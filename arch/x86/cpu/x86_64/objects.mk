#/**
# Copyright (c) 2012 Himanshu Chauhan.
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
# @version 1.0
# @author Himanshu Chauhan (hschauhan@nulltrace.org)
# @brief list of x86_64 object files.
# */

cpu-cflags +=-finline-functions -O0 -mcmodel=large
cpu-cppflags +=-DCPU_TEXT_LMA=0x200000

cpu-objs-y+= start.o

#These commented out files are what we will need to
#implement. Not all of them. But its good to have the
#list so that we know what is to be done.
#
cpu-objs-y+= cpu_atomic.o
cpu-objs-y+= cpu_atomic64.o
#ifdef CONFIG_SMP
#cpu-objs-y+= cpu_locks.o
#endif
cpu-objs-y+= cpu_main.o
cpu-objs-y+= cpu_hacks.o
cpu-objs-y+= cpu_elf.o
cpu-objs-y+= cpu_interrupts.o
cpu-objs-y+= cpu_vcpu_irq.o
cpu-objs-y+= cpu_vcpu_helper.o
cpu-objs-y+= cpu_mmu.o
cpu-objs-y+= dumpstack.o
cpu-objs-y+= dumpstack_64.o
cpu-objs-y+= stacktrace.o
cpu-objs-y+= cpu_interrupt_handlers.o
cpu-objs-$(CONFIG_LOCAL_APIC) += cpu_apic.o
