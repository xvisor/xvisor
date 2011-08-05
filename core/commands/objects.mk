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
# @version 1.0
# @author Anup Patel (anup@brainfault.org)
# @brief list of command to be build
# */

core-objs-y+= commands/vmm_cmd_version.o
core-objs-y+= commands/vmm_cmd_reset.o
core-objs-y+= commands/vmm_cmd_shutdown.o
core-objs-y+= commands/vmm_cmd_devtree.o
core-objs-y+= commands/vmm_cmd_vcpu.o
core-objs-y+= commands/vmm_cmd_guest.o
core-objs-y+= commands/vmm_cmd_hyperthreads.o
core-objs-y+= commands/vmm_cmd_chardev.o
core-objs-y+= commands/vmm_cmd_blockdev.o
core-objs-y+= commands/vmm_cmd_vserial.o
core-objs-y+= commands/vmm_cmd_stdio.o
core-objs-$(CONFIG_MM_BUDDY)+= commands/vmm_cmd_buddy.o
core-objs-$(CONFIG_NET)+= commands/vmm_cmd_ping.o
core-objs-y+= commands/vmm_cmd_elf.o
