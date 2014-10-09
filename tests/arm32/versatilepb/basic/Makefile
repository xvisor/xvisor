#/**
# Copyright (c) 2012 Jean-Christophe Dubois.
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
# @author Jean-Christophe Dubois (jcd@tribudubois.net)
# @brief toplevel makefile to build firmware
# */

# Determine the build directory
top_dir=$(CURDIR)/../../../..
ifdef O
 build_dir=$(shell readlink -f $(O))
else
 build_dir=$(top_dir)/build
endif

obj_dir=$(build_dir)/tests/arm32/versatilepb/basic
common_dir=$(top_dir)/tests/arm32/common/basic

board_arch       = v5
board_text_start = 0x01000000
board_objs       = $(obj_dir)/arm_board.o \
                   $(obj_dir)/pic/pl190.o \
                   $(obj_dir)/timer/sp804.o \
                   $(obj_dir)/serial/pl01x.o
board_fdt_support = y

board_cppflags   =
board_cflags     =
board_asflags    =
board_ldflags    =

# Include common makefile for basic firmware
include $(common_dir)/Makefile.inc

