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
# @file Makefile.inc
# @author Anup Patel (anup@brainfault.org)
# @brief common RISC-V makefile to build basic firmware
# */

ARCH_CPPFLAGS=$(board_cppflags) -DFIRMWARE_BOOT
ARCH_CPPFLAGS+=-DTEXT_START=$(board_text_start)
ifeq ($(board_32bit),y)
ARCH_CFLAGS=$(board_cflags) -fno-omit-frame-pointer -fno-optimize-sibling-calls -mno-save-restore -mstrict-align -mabi=ilp32 -march=rv32imafdc -mcmodel=medany
ARCH_ASFLAGS=$(board_asflags) -fno-omit-frame-pointer -fno-optimize-sibling-calls -mno-save-restore -mstrict-align -mabi=ilp32 -march=rv32imafdc -mcmodel=medany
else
ARCH_CFLAGS=$(board_cflags) -fno-omit-frame-pointer -fno-optimize-sibling-calls -mno-save-restore -mstrict-align -mabi=lp64 -march=rv64imafdc -mcmodel=medany
ARCH_ASFLAGS=$(board_asflags) -fno-omit-frame-pointer -fno-optimize-sibling-calls -mno-save-restore -mstrict-align -mabi=lp64 -march=rv64imafdc -mcmodel=medany
endif
ARCH_LDSCRIPT=$(obj_dir)/firmware.lnk
ARCH_LDFLAGS=$(board_ldflags)
ifeq ($(board_32bit),y)
ARCH_LDFLAGS+=-static-libgcc -lgcc
endif

ARCH_OBJ_DIR=$(obj_dir)
ARCH_BASIC_DIR=$(basic_dir)
ARCH_DIR=$(arch_dir)

ARCH_OBJS= $(ARCH_OBJ_DIR)/arch_entry.o \
	   $(ARCH_OBJ_DIR)/arch_cache.o \
	   $(ARCH_OBJ_DIR)/arch_irq.o \
	   $(ARCH_OBJ_DIR)/arch_linux.o \
	   $(ARCH_OBJ_DIR)/arch_mmu.o \
	   $(board_objs)

ARCH_DEPS= $(ARCH_DIR)/arch_asm.h \
           $(ARCH_DIR)/arch_defines.h \
           $(ARCH_DIR)/arch_io.h \
           $(ARCH_DIR)/arch_sbi.h \
           $(ARCH_DIR)/arch_types.h \
           $(ARCH_DIR)/arch_math.h

# Include common makefile for basic firmware
include $(ARCH_BASIC_DIR)/Makefile.inc
