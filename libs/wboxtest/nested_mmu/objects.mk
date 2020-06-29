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
# @brief list of nested MMU test objects to be build
# */

libs-objs-$(CONFIG_WBOXTEST_NESTED_MMU) += wboxtest/nested_mmu/s2_page_rdwr.o
libs-objs-$(CONFIG_WBOXTEST_NESTED_MMU) += wboxtest/nested_mmu/s2_page_rdonly.o
libs-objs-$(CONFIG_WBOXTEST_NESTED_MMU) += wboxtest/nested_mmu/s2_page_nordwr.o
libs-objs-$(CONFIG_WBOXTEST_NESTED_MMU) += wboxtest/nested_mmu/s2_hugepage_rdwr.o
libs-objs-$(CONFIG_WBOXTEST_NESTED_MMU) += wboxtest/nested_mmu/s2_hugepage_rdonly.o
libs-objs-$(CONFIG_WBOXTEST_NESTED_MMU) += wboxtest/nested_mmu/s2_hugepage_nordwr.o
libs-objs-$(CONFIG_WBOXTEST_NESTED_MMU) += wboxtest/nested_mmu/s1_page_s2_page_rdwr.o
libs-objs-$(CONFIG_WBOXTEST_NESTED_MMU) += wboxtest/nested_mmu/s1_page_s2_page_rdonly.o
libs-objs-$(CONFIG_WBOXTEST_NESTED_MMU) += wboxtest/nested_mmu/s1_page_s2_page_nordwr.o
libs-objs-$(CONFIG_WBOXTEST_NESTED_MMU) += wboxtest/nested_mmu/s1_page_s2_hugepage_rdwr.o
libs-objs-$(CONFIG_WBOXTEST_NESTED_MMU) += wboxtest/nested_mmu/s1_page_s2_hugepage_rdonly.o
libs-objs-$(CONFIG_WBOXTEST_NESTED_MMU) += wboxtest/nested_mmu/s1_page_s2_hugepage_nordwr.o
libs-objs-$(CONFIG_WBOXTEST_NESTED_MMU) += wboxtest/nested_mmu/s1_hugepage_s2_page_rdwr.o
libs-objs-$(CONFIG_WBOXTEST_NESTED_MMU) += wboxtest/nested_mmu/s1_hugepage_s2_page_rdonly.o
libs-objs-$(CONFIG_WBOXTEST_NESTED_MMU) += wboxtest/nested_mmu/s1_hugepage_s2_page_nordwr.o
libs-objs-$(CONFIG_WBOXTEST_NESTED_MMU) += wboxtest/nested_mmu/s1_hugepage_s2_hugepage_rdwr.o
libs-objs-$(CONFIG_WBOXTEST_NESTED_MMU) += wboxtest/nested_mmu/s1_hugepage_s2_hugepage_rdonly.o
libs-objs-$(CONFIG_WBOXTEST_NESTED_MMU) += wboxtest/nested_mmu/s1_hugepage_s2_hugepage_nordwr.o
