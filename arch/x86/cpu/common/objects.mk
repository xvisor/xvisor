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
# @brief list of x86 common object files.
# */

cpu-cflags +=-finline-functions -O0 -mcmodel=large

cpu-common-objs-y += stacktrace.o
cpu-common-objs-y += dumpstack.o
cpu-common-objs-y += cpu_features.o
cpu-common-objs-y += cpu_extables.o
cpu-common-objs-$(CONFIG_LOCAL_APIC) += cpu_apic.o
cpu-common-objs-$(CONFIG_ACPI) += acpi.o
cpu-common-objs-$(CONFIG_VEXT) += vm/vm.o
cpu-common-objs-$(CONFIG_VEXT) += vm/cpu_inst_decode.o
