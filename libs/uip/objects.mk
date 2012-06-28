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
# @brief list of uip objects to be built.
# */

#libs-cppflags-$(CONFIG_UIP)+= -I$(libs_dir)/uip

libs-objs-$(CONFIG_UIP)+= uip/uip-daemon.o
libs-objs-$(CONFIG_UIP)+= uip/uip-netport.o
libs-objs-$(CONFIG_UIP)+= uip/uip-arp.o
libs-objs-$(CONFIG_UIP)+= uip/uip.o
libs-objs-$(CONFIG_UIP)+= uip/psock.o
libs-objs-$(CONFIG_UIP)+= uip/timer.o
libs-objs-$(CONFIG_UIP)+= uip/uip-fw.o
libs-objs-$(CONFIG_UIP)+= uip/uip-neighbor.o
libs-objs-$(CONFIG_UIP)+= uip/uip-split.o
