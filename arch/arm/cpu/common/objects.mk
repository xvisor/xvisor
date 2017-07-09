#/**
# Copyright (c) 2012 Sukanto Ghosh.
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
# @brief list of common objects.
# */

cpu-common-objs-y+=emulate_arm.o
cpu-common-objs-y+=emulate_thumb.o
cpu-common-objs-y+=emulate_psci.o
cpu-common-objs-$(CONFIG_ARM_LOCKS)+=arm_locks.o
cpu-common-objs-$(CONFIG_ARM_VGIC)+=vgic.o
cpu-common-objs-$(CONFIG_ARM_VGIC)+=vgic_v2.o
cpu-common-objs-$(CONFIG_ARM_VGIC)+=vgic_v3.o
cpu-common-objs-$(CONFIG_ARM_GENERIC_TIMER)+=generic_timer.o
cpu-common-objs-$(CONFIG_ARM_MMU_LPAE)+=mmu_lpae.o
cpu-common-objs-$(CONFIG_ARM_MMU_LPAE)+=mmu_lpae_entry_ttbl.o

