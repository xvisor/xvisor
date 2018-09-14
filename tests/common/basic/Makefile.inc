#/**
# Copyright (c) 2018 Anup Patel.
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
# @brief common makefile to build basic firmware
# */

FIRMWARE_COMMON_DEPS+= $(common_dir)/basic_stdio.h \
			$(common_dir)/basic_string.h \
			$(common_dir)/libfdt/fdt.h \
			$(common_dir)/libfdt/fdt_support.h \
			$(common_dir)/libfdt/libfdt.h \
			$(common_dir)/libfdt/libfdt_env.h \
			$(common_dir)/libfdt/libfdt_internal.h

FIRMWARE_OBJS+= $(obj_dir)/basic_stdio.o \
		$(obj_dir)/basic_string.o \
		$(obj_dir)/libfdt/fdt.o \
		$(obj_dir)/libfdt/fdt_ro.o \
		$(obj_dir)/libfdt/fdt_rw.o \
		$(obj_dir)/libfdt/fdt_strerror.o \
		$(obj_dir)/libfdt/fdt_support.o \
		$(obj_dir)/libfdt/fdt_sw.o \
		$(obj_dir)/libfdt/fdt_wip.o

CPATCH32=$(build_dir)/tools/cpatch/cpatch32
ELF2CPATCH=$(top_dir)/arch/arm/cpu/arm32/elf2cpatch.py

cmd_patch_elf = CROSS_COMPILE=$(FW_CROSS_COMPILE) \
		$(ELF2CPATCH) -f $@ | $(CPATCH32) $@ 0

FIRMWARE_TARGETS = $(obj_dir)/firmware.elf
FIRMWARE_TARGETS += $(obj_dir)/firmware.bin

ifeq ($(shell test -e $(CPATCH32) && echo -n yes),yes)
FIRMWARE_TARGETS += $(obj_dir)/firmware.elf.patched
FIRMWARE_TARGETS += $(obj_dir)/firmware.bin.patched
endif

.PHONY: all
all: $(FIRMWARE_TARGETS)

.PHONY: install
install: all
	@mkdir -p $(install_dir);
	@for f in $(FIRMWARE_TARGETS) ; do \
	echo " (INSTALL)  " `echo $$f | sed -e "s@$(obj_dir)/@@"`; \
	cp -f $$f $(install_dir); \
	done

$(obj_dir)/firmware.bin.patched: $(obj_dir)/firmware.elf.patched
	@mkdir -p `dirname $@`
	@echo " (OBJCOPY)   $(subst $(obj_dir)/,,$@)"
	@$(FW_OBJCOPY) -O binary $< $@

$(obj_dir)/firmware.elf.patched: $(obj_dir)/firmware.elf
	@mkdir -p `dirname $@`
	@cp $< $@
	@echo " (PATCH)     $(subst $(obj_dir)/,,$@)"
	@$(call cmd_patch_elf)

$(obj_dir)/firmware.bin: $(obj_dir)/firmware.elf
	@mkdir -p `dirname $@`
	@echo " (OBJCOPY)   $(subst $(obj_dir)/,,$@)"
	@$(FW_OBJCOPY) -O binary $< $@

$(obj_dir)/firmware.elf: $(FIRMWARE_OBJS) $(FIRMWARE_LINK_SCRIPT)
	@mkdir -p `dirname $@`
	@echo " (LD)        $(subst $(obj_dir)/,,$@)"
	@$(FW_CC) $(FIRMWARE_OBJS) $(FW_LDFLAGS) $(FIRMWARE_LDFLAGS) -o $@

$(obj_dir)/%.lnk: $(common_arch_dir)/%.ld
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

$(obj_dir)/%.o: $(common_arch_dir)/%.c $(FIRMWARE_COMMON_DEPS)
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

$(obj_dir)/%.o: $(common_arch_dir)/%.S $(FIRMWARE_COMMON_DEPS)
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

