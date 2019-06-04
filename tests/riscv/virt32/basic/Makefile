#/**
# Copyright (c) 2019 Anup Patel.
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
# @file Makefile
# @author Anup Patel (anup@brainfault.org)
# @brief toplevel makefile to build RISC-V code
# */

# Determine the build directory
top_dir=$(CURDIR)/../../../..
ifdef O
 build_dir=$(shell readlink -f $(O))
else
 build_dir=$(top_dir)/build
endif
ifdef I
 install_dir=$(shell readlink -f $(I))
else
 install_dir=$(top_dir)/install
endif

obj_dir=$(build_dir)/tests/riscv/virt32/basic
basic_dir=$(top_dir)/tests/common/basic
arch_dir=$(top_dir)/tests/riscv/common/basic

board_text_start = 0x80000000
board_32bit=y
board_objs       = $(obj_dir)/arch_board.o \
                   $(obj_dir)/display/simplefb.o \
		   $(obj_dir)/pic/riscv_intc.o \
                   $(obj_dir)/serial/uart8250.o \
                   $(obj_dir)/sys/vminfo.o \
		   $(obj_dir)/timer/riscv_timer.o

# Include common makefile for basic test
include $(arch_dir)/Makefile.inc
