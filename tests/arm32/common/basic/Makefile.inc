#/**
# Copyright (c) 2012 Anup Patel.
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
# @brief common arm32 makefile to build basic firmware
# */

board_usr_stack_size ?= 4096

ARCH_CPPFLAGS=$(board_cppflags) -DFIRMWARE_BOOT
ifeq ($(board_smp), y)
ARCH_CPPFLAGS+=-DBOARD_SMP
endif
ifeq ($(board_secure_extn),y)
ARCH_CPPFLAGS+=-DARM_SECURE_EXTN_IMPLEMENTED
endif
ARCH_CPPFLAGS+=-DTEXT_START=$(board_text_start) -DUSR_STACK_SIZE=$(board_usr_stack_size)
ARCH_CFLAGS=$(board_cflags) -msoft-float -marm
ARCH_ASFLAGS=$(board_asflags) -msoft-float -marm
ARCH_LDSCRIPT=$(obj_dir)/firmware.lnk
ARCH_LDFLAGS=$(board_ldflags)

ifeq ($(board_arch),v5)
ARCH_CPPFLAGS+=-DARM_ARCH_v5
ARCH_CFLAGS+=-march=armv5te
ARCH_ASFLAGS+=-march=armv5te
endif
ifeq ($(board_arch),v6)
ARCH_CPPFLAGS+=-DARM_ARCH_v6
ARCH_CFLAGS+=-march=armv6
ARCH_ASFLAGS+=-march=armv6
endif
ifeq ($(board_arch),v7)
ARCH_CPPFLAGS+=-DARM_ARCH_v7
ARCH_CFLAGS+=-march=armv7-a
ARCH_ASFLAGS+=-march=armv7-a
endif
ifeq ($(board_arch),v7ve)
ARCH_CPPFLAGS+=-DARM_ARCH_v7ve
ARCH_CFLAGS+=-march=armv7ve
ARCH_ASFLAGS+=-march=armv7ve
endif

ARCH_OBJ_DIR=$(obj_dir)
ARCH_BASIC_DIR=$(basic_dir)
ARCH_DIR=$(arch_dir)

ifeq ($(board_arch),v5)
ARCH_OBJS= $(ARCH_OBJ_DIR)/arch_entry_v5.o \
	   $(ARCH_OBJ_DIR)/arch_cache_v5.o \
	   $(ARCH_OBJ_DIR)/arch_mmu_v5.o
endif

ifeq ($(board_arch),v6)
ARCH_OBJS= $(ARCH_OBJ_DIR)/arch_entry_v6.o \
	   $(ARCH_OBJ_DIR)/arch_cache_v6.o \
	   $(ARCH_OBJ_DIR)/arch_mmu_v6.o
endif

ifeq ($(board_arch),v7)
ARCH_OBJS= $(ARCH_OBJ_DIR)/arch_entry_v7.o \
	   $(ARCH_OBJ_DIR)/arch_cache_v7.o \
	   $(ARCH_OBJ_DIR)/arch_mmu_v7.o
endif

ifeq ($(board_arch),v7ve)
ARCH_OBJS= $(ARCH_OBJ_DIR)/arch_entry_v7.o \
	   $(ARCH_OBJ_DIR)/arch_cache_v7.o \
	   $(ARCH_OBJ_DIR)/arch_mmu_v7.o
endif

ARCH_OBJS+= $(ARCH_OBJ_DIR)/arch_irq.o \
	    $(ARCH_OBJ_DIR)/arch_linux.o \
	    $(ARCH_OBJ_DIR)/arch_math.o \
	    $(board_objs)

ARCH_DEPS= $(ARCH_DIR)/arm_asm_macro.h \
           $(ARCH_DIR)/arm_defines.h \
           $(ARCH_DIR)/arm_inline_asm.h \
           $(ARCH_DIR)/arch_math.h \
           $(ARCH_DIR)/arch_types.h \
           $(ARCH_DIR)/arch_io.h

# Include common makefile for basic firmware
include $(ARCH_BASIC_DIR)/Makefile.inc
