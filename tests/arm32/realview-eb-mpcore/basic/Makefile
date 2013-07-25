#/**
# Copyright (c) 2013 Anup Patel.
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
# @brief toplevel makefile to build firmware
# */

# Determine the build directory
top_dir=$(CURDIR)/../../../..
ifdef O
 build_dir=$(shell readlink -f $(O))
else
 build_dir=$(top_dir)/build
endif

obj_dir=$(build_dir)/tests/arm32/realview-eb-mpcore/basic
common_dir=$(top_dir)/tests/arm32/common/basic

board_arch       = v6
board_text_start = 0x00010000
board_objs       = $(obj_dir)/arm_board.o \
                   $(obj_dir)/pic/gic.o \
                   $(obj_dir)/timer/sp804.o \
                   $(obj_dir)/serial/pl01x.o
board_smp        = y

# Include common makefile for basic firmware
include $(common_dir)/Makefile.inc

