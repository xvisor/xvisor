#/**
# Copyright (c) 2016 Anup Patel.
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
# @file vstelnet.mk
# @author Anup Patel (anup@brainfault.org)
# @brief list of vsdaemon objects to be build
# */

libs-objs-$(CONFIG_VSDAEMON) += vsdaemon/vsdaemon.o
libs-objs-$(CONFIG_VSDAEMON_CHARDEV) += vsdaemon/vsdaemon_chardev.o
libs-objs-$(CONFIG_VSDAEMON_MTERM) += vsdaemon/vsdaemon_mterm.o
libs-objs-$(CONFIG_VSDAEMON_TELNET) += vsdaemon/vsdaemon_telnet.o
