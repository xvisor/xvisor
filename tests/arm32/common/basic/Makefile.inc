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

FW_CROSS_COMPILE=$(CROSS_COMPILE)
FW_CPPFLAGS=-I. -I$(common_dir) -I$(common_arch_dir) -DTEXT_START=$(board_text_start) -DUSR_STACK_SIZE=$(board_usr_stack_size)
FW_CFLAGS=-g -Wall -Werror -nostdlib -msoft-float -marm
FW_ASFLAGS=-g -Wall -Werror -nostdlib -msoft-float -marm -D__ASSEMBLY__
ifeq ($(board_arch),v5)
FW_CPPFLAGS+=-DARM_ARCH_v5
FW_CFLAGS+=-march=armv5te $(FW_CPPFLAGS)
FW_ASFLAGS+=-march=armv5te $(FW_CPPFLAGS)
endif
ifeq ($(board_arch),v6)
FW_CPPFLAGS+=-DARM_ARCH_v6
FW_CFLAGS+=-march=armv6 $(FW_CPPFLAGS)
FW_ASFLAGS+=-march=armv6 $(FW_CPPFLAGS)
endif
ifeq ($(board_arch),v7)
FW_CPPFLAGS+=-DARM_ARCH_v7
FW_CFLAGS+=-march=armv7-a $(FW_CPPFLAGS)
FW_ASFLAGS+=-march=armv7-a $(FW_CPPFLAGS)
endif
ifeq ($(board_arch),v7ve)
FW_CPPFLAGS+=-DARM_ARCH_v7ve
FW_CFLAGS+=-march=armv7ve $(FW_CPPFLAGS)
FW_ASFLAGS+=-march=armv7ve $(FW_CPPFLAGS)
endif
FW_LDFLAGS=$(FW_CFLAGS) -Wl,--build-id=none
FW_AS=$(FW_CROSS_COMPILE)gcc
FW_CC=$(FW_CROSS_COMPILE)gcc
FW_CPP=$(FW_CROSS_COMPILE)cpp
FW_OBJCOPY=$(FW_CROSS_COMPILE)objcopy

ifeq ($(board_arch),v5)
FIRMWARE_OBJS=$(obj_dir)/arm_entry_v5.o \
		$(obj_dir)/arm_cache_v5.o \
		$(obj_dir)/arm_mmu_v5.o
endif

ifeq ($(board_arch),v6)
FIRMWARE_OBJS=$(obj_dir)/arm_entry_v6.o \
		$(obj_dir)/arm_cache_v6.o \
		$(obj_dir)/arm_mmu_v6.o
endif

ifeq ($(board_arch),v7)
FIRMWARE_OBJS=$(obj_dir)/arm_entry_v7.o \
		$(obj_dir)/arm_cache_v7.o \
		$(obj_dir)/arm_mmu_v7.o
endif

ifeq ($(board_arch),v7ve)
FIRMWARE_OBJS=$(obj_dir)/arm_entry_v7.o \
		$(obj_dir)/arm_cache_v7.o \
		$(obj_dir)/arm_mmu_v7.o
endif

FIRMWARE_OBJS+=$(board_objs)

FIRMWARE_OBJS+=$(obj_dir)/arm_main.o \
		$(obj_dir)/arm_heap.o \
		$(obj_dir)/arm_irq.o \
		$(obj_dir)/arch_math.o \
		$(obj_dir)/dhry_1.o \
		$(obj_dir)/dhry_2.o \
		$(obj_dir)/dhry_port.o

FIRMWARE_CPPFLAGS=-DFIRMWARE_BOOT
ifeq ($(board_smp), y)
FIRMWARE_CPPFLAGS+=-DBOARD_SMP
endif
ifeq ($(board_secure_extn),y)
FIRMWARE_CPPFLAGS+=-DARM_SECURE_EXTN_IMPLEMENTED
endif
FIRMWARE_CPPFLAGS+=$(board_cppflags)
FIRMWARE_CFLAGS=$(FIRMWARE_CPPFLAGS) $(board_cflags)
FIRMWARE_ASFLAGS=$(FIRMWARE_CPPFLAGS) $(board_asflags)
FIRMWARE_LINK_SCRIPT=$(obj_dir)/firmware.lnk
FIRMWARE_LDFLAGS=-Wl,-T$(FIRMWARE_LINK_SCRIPT) $(FIRMWARE_CPPFLAGS) $(board_ldflags)

FIRMWARE_COMMON_DEPS=$(common_arch_dir)/arm_asm_macro.h \
                $(common_arch_dir)/arch_math.h \
                $(common_arch_dir)/arm_defines.h \
                $(common_arch_dir)/arch_types.h \
                $(common_arch_dir)/arm_board.h \
                $(common_arch_dir)/arm_heap.h \
                $(common_arch_dir)/arm_inline_asm.h \
                $(common_arch_dir)/arm_cache.h \
                $(common_arch_dir)/arch_io.h \
                $(common_arch_dir)/arm_irq.h \
                $(common_arch_dir)/arm_mmu.h

# Include common makefile for basic firmware
include $(common_dir)/Makefile.inc
