#/**
# Copyright (c) 2011 Anup Patel.
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
# @brief list of common objects.
# */

board-common-objs-$(CONFIG_ARM_GIC)+=gic.o
board-common-objs-$(CONFIG_ARM_TIMER_SP804)+=sp804_timer.o
board-common-objs-$(CONFIG_ARM_TWD)+=smp_twd.o
board-common-objs-$(CONFIG_ARM_PL190)+=pl190.o
board-common-objs-$(CONFIG_ICST)+=icst.o
