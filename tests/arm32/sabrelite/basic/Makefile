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
# @file Makefile
# @author Sukanto Ghosh (sukantoghosh@gmail.com)
# @brief toplevel makefile to build firmware
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

obj_dir=$(build_dir)/tests/arm32/sabrelite/basic
basic_dir=$(top_dir)/tests/common/basic
arch_dir=$(top_dir)/tests/arm32/common/basic

board_arch       = v7
board_text_start = 0x14000000
board_objs       = $(obj_dir)/arch_board.o \
                   $(obj_dir)/pic/gic.o \
                   $(obj_dir)/timer/imx_gpt.o \
                   $(obj_dir)/serial/imx.o \
                   $(obj_dir)/sys/vminfo.o
board_smp	 = y

board_cppflags   =
board_cflags     = -mtune=cortex-a9
board_asflags    = -mtune=cortex-a9
board_ldflags    =

# Include common arch makefile for basic firmware
include $(arch_dir)/Makefile.inc

