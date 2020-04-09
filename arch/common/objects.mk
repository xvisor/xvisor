#/**
# Copyright (c) 2020 Anup Patel.
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
# @brief list of common architecture objects.
# */

arch-common-objs-$(CONFIG_ARCH_GENERIC_BOARD)+=generic_board.o
arch-common-objs-$(CONFIG_ARCH_GENERIC_DEFTERM)+=generic_defterm.o
arch-common-objs-$(CONFIG_ARCH_GENERIC_DEFTERM_EARLY)+=generic_defterm_early.o
arch-common-objs-$(CONFIG_ARCH_GENERIC_DEVTREE)+=generic_devtree.o
arch-common-objs-$(CONFIG_ARCH_GENERIC_MMU)+=generic_mmu.o
arch-common-objs-$(CONFIG_ARCH_GENERIC_SMP_IPI)+=generic_smp_ipi.o
