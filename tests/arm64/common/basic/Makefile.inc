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
# @brief common makefile to build basic firmware
# */

# Include top-level configuration file if present
-include $(build_dir)/.config

ifdef CROSS_COMPILE
FW_CROSS_COMPILE=$(CROSS_COMPILE)
else
FW_CROSS_COMPILE=aarch64-none-linux-gnu-
endif
FW_CPPFLAGS=-I. -I$(common_dir) -DTEXT_START=$(board_text_start)
FW_CFLAGS=$(FW_CPPFLAGS) -g -Wall -Werror -nostdlib -mgeneral-regs-only -mlittle-endian
FW_ASFLAGS=$(FW_CPPFLAGS) -g -Wall -Werror -nostdlib -mgeneral-regs-only -mlittle-endian -D__ASSEMBLY__
FW_LDFLAGS=$(FW_CFLAGS) -Wl,--build-id=none
FW_AS=$(FW_CROSS_COMPILE)gcc
FW_CC=$(FW_CROSS_COMPILE)gcc
FW_CPP=$(FW_CROSS_COMPILE)cpp
FW_OBJCOPY=$(FW_CROSS_COMPILE)objcopy

FIRMWARE_OBJS=$(obj_dir)/arm_entry.o \
		$(obj_dir)/arm_mmu.o

FIRMWARE_OBJS+=$(board_objs)

FIRMWARE_OBJS+=$(obj_dir)/arm_main.o \
		$(obj_dir)/arm_heap.o \
		$(obj_dir)/arm_irq.o \
		$(obj_dir)/arm_stdio.o \
		$(obj_dir)/arm_string.o \
		$(obj_dir)/dhry_1.o \
		$(obj_dir)/dhry_2.o \
		$(obj_dir)/dhry_port.o \
		$(obj_dir)/libfdt/fdt.o \
		$(obj_dir)/libfdt/fdt_ro.o \
		$(obj_dir)/libfdt/fdt_rw.o \
		$(obj_dir)/libfdt/fdt_strerror.o \
		$(obj_dir)/libfdt/fdt_support.o \
		$(obj_dir)/libfdt/fdt_sw.o \
		$(obj_dir)/libfdt/fdt_wip.o

FIRMWARE_CPPFLAGS=-DFIRMWARE_BOOT
FIRMWARE_CFLAGS=$(FIRMWARE_CPPFLAGS)
FIRMWARE_ASFLAGS=$(FIRMWARE_CPPFLAGS)
FIRMWARE_LINK_SCRIPT=$(obj_dir)/firmware.lnk
FIRMWARE_LDFLAGS=-Wl,-T$(FIRMWARE_LINK_SCRIPT) $(FIRMWARE_CPPFLAGS)

FIRMWARE_COMMON_DEPS=$(common_dir)/arm_asm_macro.h \
                $(common_dir)/arm_math.h \
                $(common_dir)/arm_defines.h \
                $(common_dir)/arm_types.h \
                $(common_dir)/arm_board.h \
                $(common_dir)/arm_heap.h \
                $(common_dir)/arm_inline_asm.h \
                $(common_dir)/arm_io.h \
                $(common_dir)/arm_irq.h \
                $(common_dir)/arm_mmu.h \
                $(common_dir)/arm_stdio.h \
                $(common_dir)/arm_string.h \
                $(common_dir)/libfdt/fdt.h \
                $(common_dir)/libfdt/fdt_support.h \
                $(common_dir)/libfdt/libfdt.h \
                $(common_dir)/libfdt/libfdt_env.h \
                $(common_dir)/libfdt/libfdt_internal.h

FIRMWARE_TARGETS = $(obj_dir)/firmware.bin

.PHONY: all
all: $(FIRMWARE_TARGETS)

$(obj_dir)/firmware.bin: $(obj_dir)/firmware.elf
	@mkdir -p `dirname $@`
	@echo " (OBJCOPY)   $(subst $(obj_dir)/,,$@)"
	@$(FW_OBJCOPY) -O binary $< $@

$(obj_dir)/firmware.elf: $(FIRMWARE_OBJS) $(FIRMWARE_LINK_SCRIPT)
	@mkdir -p `dirname $@`
	@echo " (LD)        $(subst $(obj_dir)/,,$@)"
	@$(FW_CC) $(FIRMWARE_OBJS) $(FW_LDFLAGS) $(FIRMWARE_LDFLAGS) -o $@

$(obj_dir)/%.lnk: $(common_dir)/%.ld
	@mkdir -p `dirname $@`
	@echo " (CPP)       $(subst $(obj_dir)/,,$@)"
	@$(FW_CPP) $(FW_CPPFLAGS) $(FIRMWARE_CPPFLAGS) $< | grep -v "\#" > $@

$(obj_dir)/%.o: %.c $(FIRMWARE_COMMON_DEPS)
	@mkdir -p `dirname $@`
	@echo " (CC)        $(subst $(obj_dir)/,,$@)"
	@$(FW_CC) $(FW_CFLAGS) $(FIRMWARE_CFLAGS) -c $< -o $@

$(obj_dir)/%.o: $(common_dir)/%.c $(FIRMWARE_COMMON_DEPS)
	@mkdir -p `dirname $@`
	@echo " (CC)        $(subst $(obj_dir)/,,$@)"
	@$(FW_CC) $(FW_CFLAGS) $(FIRMWARE_CFLAGS) -c $< -o $@

$(obj_dir)/%.o: %.S $(FIRMWARE_COMMON_DEPS)
	@mkdir -p `dirname $@`
	@echo " (AS)        $(subst $(obj_dir)/,,$@)"
	@$(FW_AS) $(FW_ASFLAGS) $(FIRMWARE_ASFLAGS) -c $< -o $@

$(obj_dir)/%.o: $(common_dir)/%.S $(FIRMWARE_COMMON_DEPS)
	@mkdir -p `dirname $@`
	@echo " (AS)        $(subst $(obj_dir)/,,$@)"
	@$(FW_AS) $(FW_ASFLAGS) $(FIRMWARE_ASFLAGS) -c $< -o $@

.PHONY: clean
clean:
	@echo " (RM)        firmware.elf*"
	@rm -f $(obj_dir)/firmware.elf*
	@echo " (RM)        firmware.bin*"
	@rm -f $(obj_dir)/firmware.bin*
	@echo " (RM)        $(obj_dir)"
	@rm -rf $(obj_dir)

