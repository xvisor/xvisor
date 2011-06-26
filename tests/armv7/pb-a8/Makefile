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
# @version 1.0
# @author Anup Patel (anup@brainfault.org)
# @brief toplevel makefile to build ARM test code
# */

ifdef CROSS_COMPILE
ARM_CROSS_COMPILE=$(CROSS_COMPILE)
else
ARM_CROSS_COMPILE=arm-none-eabi-
endif
ARM_CPPFLAGS=-I. -DTEXT_START=0x100000
ARM_CFLAGS=-g -Wall -Werror -nostdlib -mcpu=cortex-a8 $(ARM_CPPFLAGS)
ARM_ASFLAGS=-g -Wall -Werror -nostdlib -mcpu=cortex-a8 $(ARM_CPPFLAGS) -D__ASSEMBLY__
ARM_LDFLAGS=$(ARM_CFLAGS) -static-libgcc -lgcc
ARM_AS=$(ARM_CROSS_COMPILE)gcc
ARM_CC=$(ARM_CROSS_COMPILE)gcc
ARM_CPP=$(ARM_CROSS_COMPILE)cpp
ARM_OBJCOPY=$(ARM_CROSS_COMPILE)objcopy

ARM_TEST_OBJ_DIR=./test_objs
ARM_TEST_OBJS=$(ARM_TEST_OBJ_DIR)/arm_entry.o \
		$(ARM_TEST_OBJ_DIR)/arm_main.o \
		$(ARM_TEST_OBJ_DIR)/arm_interrupts.o \
		$(ARM_TEST_OBJ_DIR)/arm_pl01x.o
ARM_TEST_CPPFLAGS=-DARM_TEST_BOOT
ARM_TEST_CFLAGS=$(ARM_TEST_CPPFLAGS)
ARM_TEST_ASFLAGS=$(ARM_TEST_CPPFLAGS)
ARM_TEST_LINK_SCRIPT=$(ARM_TEST_OBJ_DIR)/arm_test.lnk
ARM_TEST_LDFLAGS=-Wl,-T$(ARM_TEST_LINK_SCRIPT) $(ARM_TEST_CPPFLAGS)

ARM_COMMON_DEPS=arm_asm_macro.h arm_regs.h arm_types.h

.PHONY: all
all: arm_test.bin 

# Generate ARM_TEST code
arm_test.bin: arm_test.elf
	@echo "   (OBJCOPY) $@"
	@$(ARM_OBJCOPY) -O binary $< $@

arm_test.elf: $(ARM_TEST_OBJS) $(ARM_TEST_LINK_SCRIPT)
	@echo "   (LD)      $@"
	@$(ARM_CC) $(ARM_TEST_OBJS) $(ARM_LDFLAGS) $(ARM_TEST_LDFLAGS) -o $@ 

$(ARM_TEST_OBJ_DIR)/%.lnk: %.ld
	@mkdir -p `dirname $@`
	@echo "   (CPP)     $@"
	@$(ARM_CPP) $(ARM_CPPFLAGS) $(ARM_TEST_CPPFLAGS) $< | grep -v "\#" > $@

$(ARM_TEST_OBJ_DIR)/%.o: %.c $(ARM_COMMON_DEPS)
	@mkdir -p `dirname $@`
	@echo "   (CC)      $@"
	@$(ARM_CC) $(ARM_CFLAGS) $(ARM_TEST_CFLAGS) -c $< -o $@

$(ARM_TEST_OBJ_DIR)/%.o: %.S $(ARM_COMMON_DEPS)
	@mkdir -p `dirname $@`
	@echo "   (AS)      $@"
	@$(ARM_AS) $(ARM_ASFLAGS) $(ARM_TEST_ASFLAGS) -c $< -o $@

.PHONY: clean
clean:
	@echo "   (RM)      arm_test.elf"
	@rm -f arm_test.elf
	@echo "   (RM)      arm_test.bin"
	@rm -f arm_test.bin
	@echo "   (RM)      $(ARM_TEST_OBJ_DIR)"
	@rm -rf $(ARM_TEST_OBJ_DIR)

