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
# @file objects.mk
# @author Anup Patel (anup@brainfault.org)
# @brief list of threads test objects to be build
# */

libs-objs-$(CONFIG_WBOXTEST_THREADS) += wboxtest/threads/kern1.o
libs-objs-$(CONFIG_WBOXTEST_THREADS) += wboxtest/threads/kern2.o
libs-objs-$(CONFIG_WBOXTEST_THREADS) += wboxtest/threads/kern3.o
libs-objs-$(CONFIG_WBOXTEST_THREADS) += wboxtest/threads/kern4.o
libs-objs-$(CONFIG_WBOXTEST_THREADS) += wboxtest/threads/mutex2.o
libs-objs-$(CONFIG_WBOXTEST_THREADS) += wboxtest/threads/mutex3.o
libs-objs-$(CONFIG_WBOXTEST_THREADS) += wboxtest/threads/mutex4.o
libs-objs-$(CONFIG_WBOXTEST_THREADS) += wboxtest/threads/mutex5.o
libs-objs-$(CONFIG_WBOXTEST_THREADS) += wboxtest/threads/mutex6.o
libs-objs-$(CONFIG_WBOXTEST_THREADS) += wboxtest/threads/mutex7.o
libs-objs-$(CONFIG_WBOXTEST_THREADS) += wboxtest/threads/mutex8.o
libs-objs-$(CONFIG_WBOXTEST_THREADS) += wboxtest/threads/mutex9.o
libs-objs-$(CONFIG_WBOXTEST_THREADS) += wboxtest/threads/semaphore1.o
libs-objs-$(CONFIG_WBOXTEST_THREADS) += wboxtest/threads/semaphore2.o
libs-objs-$(CONFIG_WBOXTEST_THREADS) += wboxtest/threads/semaphore3.o
libs-objs-$(CONFIG_WBOXTEST_THREADS) += wboxtest/threads/waitqueue1.o
libs-objs-$(CONFIG_WBOXTEST_THREADS) += wboxtest/threads/waitqueue2.o
libs-objs-$(CONFIG_WBOXTEST_THREADS) += wboxtest/threads/waitqueue3.o
