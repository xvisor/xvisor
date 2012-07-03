#/**
# Copyright (c) 2010 Anup Patel.
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
# @brief toplevel makefile to build ARM test code
# */

# Determine the build directory
top_dir=$(CURDIR)/../../../..
ifdef O
 build_dir=$(shell readlink -f $(O))
else
 build_dir=$(top_dir)/build
endif

# Include top-level configuration file if present
-include $(build_dir)/.config

obj_dir=$(build_dir)/tests/arm32/pb-a8/basic
common_dir=$(top_dir)/tests/arm32/common/basic

ifdef CROSS_COMPILE
ARM_CROSS_COMPILE=$(CROSS_COMPILE)
else
ARM_CROSS_COMPILE=arm-none-eabi-
endif
ARM_CPPFLAGS=-I. -I$(common_dir) -DTEXT_START=0x70010000
ARM_CFLAGS=-g -Wall -Werror -nostdlib -msoft-float -march=armv7-a -marm $(ARM_CPPFLAGS)
ARM_ASFLAGS=-g -Wall -Werror -nostdlib -msoft-float -march=armv7-a -marm $(ARM_CPPFLAGS) -D__ASSEMBLY__
ARM_LDFLAGS=$(ARM_CFLAGS)
ARM_AS=$(ARM_CROSS_COMPILE)gcc
ARM_CC=$(ARM_CROSS_COMPILE)gcc
ARM_CPP=$(ARM_CROSS_COMPILE)cpp
ARM_OBJCOPY=$(ARM_CROSS_COMPILE)objcopy

ARM_TEST_OBJS=$(obj_dir)/arm_entry.o \
		$(obj_dir)/arm_main.o \
		$(obj_dir)/arm_board.o \
		$(obj_dir)/arm_heap.o \
		$(obj_dir)/arm_irq.o \
		$(obj_dir)/arm_stdio.o \
		$(obj_dir)/arm_string.o \
		$(obj_dir)/arm_math.o \
		$(obj_dir)/arm_mmu.o \
		$(obj_dir)/arm_gic.o \
		$(obj_dir)/arm_timer.o \
		$(obj_dir)/arm_pl01x.o \
		$(obj_dir)/dhry_1.o \
		$(obj_dir)/dhry_2.o \
		$(obj_dir)/dhry_port.o

ARM_TEST_CPPFLAGS=-DARM_TEST_BOOT
ARM_TEST_CFLAGS=$(ARM_TEST_CPPFLAGS)
ARM_TEST_ASFLAGS=$(ARM_TEST_CPPFLAGS)
ARM_TEST_LINK_SCRIPT=$(obj_dir)/arm_test.lnk
ARM_TEST_LDFLAGS=-Wl,-T$(ARM_TEST_LINK_SCRIPT) $(ARM_TEST_CPPFLAGS)

ARM_COMMON_DEPS=arm_asm_macro.h $(common_dir)/arm_math.h $(common_dir)/arm_defines.h $(common_dir)/arm_types.h

CPATCH32=$(build_dir)/tools/cpatch/cpatch32
ELF2CPATCH=$(top_dir)/arch/arm/cpu/arm32/elf2cpatch.py

cmd_patch_elf = CROSS_COMPILE=$(ARM_CROSS_COMPILE) \
		$(ELF2CPATCH) -f $@ .text | $(CPATCH32) $@ 0

ARM_TEST_TARGETS = $(obj_dir)/arm_test.bin
ifdef CONFIG_ARM32
ARM_TEST_TARGETS += $(obj_dir)/arm_test.bin.patched
endif

.PHONY: all
all: $(ARM_TEST_TARGETS)

$(obj_dir)/arm_test.bin.patched: $(obj_dir)/arm_test.elf.patched
	@mkdir -p `dirname $@`
	@echo " (OBJCOPY)   $(subst $(obj_dir)/,,$@)"
	@$(ARM_OBJCOPY) -O binary $< $@

$(obj_dir)/arm_test.elf.patched: $(obj_dir)/arm_test.elf
	@mkdir -p `dirname $@`
	@cp $< $@
	@echo " (PATCH)     $(subst $(obj_dir)/,,$@)"
	@$(call cmd_patch_elf)

$(obj_dir)/arm_test.bin: $(obj_dir)/arm_test.elf
	@mkdir -p `dirname $@`
	@echo " (OBJCOPY)   $(subst $(obj_dir)/,,$@)"
	@$(ARM_OBJCOPY) -O binary $< $@

$(obj_dir)/arm_test.elf: $(ARM_TEST_OBJS) $(ARM_TEST_LINK_SCRIPT)
	@mkdir -p `dirname $@`
	@echo " (LD)        $(subst $(obj_dir)/,,$@)"
	@$(ARM_CC) $(ARM_TEST_OBJS) $(ARM_LDFLAGS) $(ARM_TEST_LDFLAGS) -o $@

$(obj_dir)/%.lnk: $(common_dir)/%.ld
	@mkdir -p `dirname $@`
	@echo " (CPP)       $(subst $(obj_dir)/,,$@)"
	@$(ARM_CPP) $(ARM_CPPFLAGS) $(ARM_TEST_CPPFLAGS) $< | grep -v "\#" > $@

$(obj_dir)/%.o: %.c $(ARM_COMMON_DEPS)
	@mkdir -p `dirname $@`
	@echo " (CC)        $(subst $(obj_dir)/,,$@)"
	@$(ARM_CC) $(ARM_CFLAGS) $(ARM_TEST_CFLAGS) -c $< -o $@

$(obj_dir)/%.o: %.S $(ARM_COMMON_DEPS)
	@mkdir -p `dirname $@`
	@echo " (AS)        $(subst $(obj_dir)/,,$@)"
	@$(ARM_AS) $(ARM_ASFLAGS) $(ARM_TEST_ASFLAGS) -c $< -o $@

$(obj_dir)/%.o: $(common_dir)/%.c $(ARM_COMMON_DEPS)
	@mkdir -p `dirname $@`
	@echo " (CC)        $(subst $(obj_dir)/,,$@)"
	@$(ARM_CC) $(ARM_CFLAGS) $(ARM_TEST_CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	@echo " (RM)        arm_test.elf*"
	@rm -f $(obj_dir)/arm_test.elf*
	@echo " (RM)        arm_test.bin*"
	@rm -f $(obj_dir)/arm_test.bin*
	@echo " (RM)        $(obj_dir)"
	@rm -rf $(obj_dir)

