#/**
# Copyright (c) 2013 Sukanto Ghosh.
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
# @file Makefile.inc
# @author Sukanto Ghosh (sukantoghosh@gmail.com)
# @brief common arm64 makefile to build basic firmware
# */

ARCH_CPPFLAGS=$(board_cppflags) -DFIRMWARE_BOOT
ARCH_CPPFLAGS+=-DTEXT_START=$(board_text_start)
ARCH_CFLAGS=$(board_cflags) -mgeneral-regs-only -mlittle-endian
ARCH_ASFLAGS=$(board_asflags) -mgeneral-regs-only -mlittle-endian
ARCH_LDSCRIPT=$(obj_dir)/firmware.lnk
ARCH_LDFLAGS=$(board_ldflags)

ARCH_OBJ_DIR=$(obj_dir)
ARCH_BASIC_DIR=$(basic_dir)
ARCH_DIR=$(arch_dir)

ARCH_OBJS= $(ARCH_OBJ_DIR)/arch_entry.o \
	   $(ARCH_OBJ_DIR)/arch_cache.o \
	   $(ARCH_OBJ_DIR)/arch_irq.o \
	   $(ARCH_OBJ_DIR)/arch_linux.o \
	   $(ARCH_OBJ_DIR)/arch_mmu.o \
	   $(board_objs)

ARCH_DEPS= $(ARCH_DIR)/arm_asm_macro.h \
           $(ARCH_DIR)/arm_defines.h \
           $(ARCH_DIR)/arm_inline_asm.h \
           $(ARCH_DIR)/arch_io.h \
           $(ARCH_DIR)/arch_types.h \
           $(ARCH_DIR)/arch_math.h

# Include common makefile for basic firmware
include $(ARCH_BASIC_DIR)/Makefile.inc
