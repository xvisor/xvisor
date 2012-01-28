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
# @brief list of command objects to be build
# */

commands-objs-$(CONFIG_CMD_VERSION)+= cmd_version.o
commands-objs-$(CONFIG_CMD_RESET)+= cmd_reset.o
commands-objs-$(CONFIG_CMD_SHUTDOWN)+= cmd_shutdown.o
commands-objs-$(CONFIG_CMD_VAPOOL)+= cmd_vapool.o
commands-objs-$(CONFIG_CMD_RAM)+= cmd_ram.o
commands-objs-$(CONFIG_CMD_DEVTREE)+= cmd_devtree.o
commands-objs-$(CONFIG_CMD_VCPU)+= cmd_vcpu.o
commands-objs-$(CONFIG_CMD_GUEST)+= cmd_guest.o
commands-objs-$(CONFIG_CMD_MEMORY)+= cmd_memory.o
commands-objs-$(CONFIG_CMD_THREAD)+= cmd_thread.o
commands-objs-$(CONFIG_CMD_CHARDEV)+= cmd_chardev.o
commands-objs-$(CONFIG_CMD_BLOCKDEV)+= cmd_blockdev.o
commands-objs-$(CONFIG_CMD_VSERIAL)+= cmd_vserial.o
commands-objs-$(CONFIG_CMD_STDIO)+= cmd_stdio.o
commands-objs-$(CONFIG_CMD_BUDDY)+= cmd_buddy.o
commands-objs-$(CONFIG_CMD_PING)+= cmd_ping.o
commands-objs-$(CONFIG_CMD_PROFILE)+= cmd_profile.o
commands-objs-$(CONFIG_CMD_THREADTEST)+= cmd_threadtest.o

