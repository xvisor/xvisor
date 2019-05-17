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

FW_CROSS_COMPILE=$(CROSS_COMPILE)
FW_CPPFLAGS=$(ARCH_CPPFLAGS) -I. -I$(ARCH_BASIC_DIR) -I$(ARCH_DIR)
FW_CFLAGS=$(ARCH_CFLAGS) $(FW_CPPFLAGS) -g -Wall -Werror -nostdlib 
FW_ASFLAGS=$(ARCH_ASFLAGS) $(FW_CPPFLAGS) -g -Wall -Werror -nostdlib -D__ASSEMBLY__
FW_LDFLAGS=$(ARCH_LDFLAGS) $(FW_CFLAGS) -Wl,--build-id=none -Wl,-T$(ARCH_LDSCRIPT)

ifdef CROSS_COMPILE
CC		=	$(CROSS_COMPILE)gcc
CPP		=	$(CROSS_COMPILE)cpp
OBJCOPY		=	$(CROSS_COMPILE)objcopy
else
CC		?=	gcc
CPP		?=	cpp
OBJCOPY		?=	objcopy
endif
AS		=	$(CC)
DTC		=	dtc

FW_OBJ_DIR= $(ARCH_OBJ_DIR)
FW_BASIC_DIR= $(ARCH_BASIC_DIR)
FW_ARCH_DIR= $(ARCH_DIR)

FW_DEPS+= $(ARCH_DEPS) \
	  $(FW_BASIC_DIR)/arch_board.h \
	  $(FW_BASIC_DIR)/arch_cache.h \
	  $(FW_BASIC_DIR)/arch_irq.h \
	  $(FW_BASIC_DIR)/arch_linux.h \
	  $(FW_BASIC_DIR)/arch_mmu.h \
	  $(FW_BASIC_DIR)/basic_heap.h \
	  $(FW_BASIC_DIR)/basic_irq.h \
	  $(FW_BASIC_DIR)/basic_stdio.h \
	  $(FW_BASIC_DIR)/basic_string.h \
	  $(FW_BASIC_DIR)/dhry.h \
	  $(FW_BASIC_DIR)/dhry_port.h \
	  $(FW_BASIC_DIR)/libfdt/fdt.h \
	  $(FW_BASIC_DIR)/libfdt/fdt_support.h \
	  $(FW_BASIC_DIR)/libfdt/libfdt.h \
	  $(FW_BASIC_DIR)/libfdt/libfdt_env.h \
	  $(FW_BASIC_DIR)/libfdt/libfdt_internal.h

FW_OBJS+= $(ARCH_OBJS) \
	  $(FW_OBJ_DIR)/basic_heap.o \
	  $(FW_OBJ_DIR)/basic_irq.o \
	  $(FW_OBJ_DIR)/basic_main.o \
	  $(FW_OBJ_DIR)/basic_stdio.o \
	  $(FW_OBJ_DIR)/basic_string.o \
	  $(FW_OBJ_DIR)/dhry_1.o \
	  $(FW_OBJ_DIR)/dhry_2.o \
	  $(FW_OBJ_DIR)/dhry_port.o \
	  $(FW_OBJ_DIR)/libfdt/fdt.o \
	  $(FW_OBJ_DIR)/libfdt/fdt_ro.o \
	  $(FW_OBJ_DIR)/libfdt/fdt_rw.o \
	  $(FW_OBJ_DIR)/libfdt/fdt_strerror.o \
	  $(FW_OBJ_DIR)/libfdt/fdt_support.o \
	  $(FW_OBJ_DIR)/libfdt/fdt_sw.o \
	  $(FW_OBJ_DIR)/libfdt/fdt_wip.o

CPATCH32=$(build_dir)/tools/cpatch/cpatch32
ELF2CPATCH=$(top_dir)/arch/arm/cpu/arm32/elf2cpatch.py

CMD_PATCH_ELF = CROSS_COMPILE=$(FW_CROSS_COMPILE) \
		$(ELF2CPATCH) -f $@ | $(CPATCH32) $@ 0

FW_TARGETS = $(FW_OBJ_DIR)/firmware.elf
FW_TARGETS += $(FW_OBJ_DIR)/firmware.bin

ifeq ($(shell test -e $(CPATCH32) && echo -n yes),yes)
FW_TARGETS += $(FW_OBJ_DIR)/firmware.elf.patched
FW_TARGETS += $(FW_OBJ_DIR)/firmware.bin.patched
endif

.PHONY: all
all: $(FW_TARGETS)

.PHONY: install
install: all
	@mkdir -p $(install_dir);
	@for f in $(FW_TARGETS) ; do \
	echo " (INSTALL)  " `echo $$f | sed -e "s@$(FW_OBJ_DIR)/@@"`; \
	cp -f $$f $(install_dir); \
	done

$(FW_OBJ_DIR)/firmware.bin.patched: $(FW_OBJ_DIR)/firmware.elf.patched
	@mkdir -p `dirname $@`
	@echo " (OBJCOPY)   $(subst $(FW_OBJ_DIR)/,,$@)"
	@$(OBJCOPY) -O binary $< $@

$(FW_OBJ_DIR)/firmware.elf.patched: $(FW_OBJ_DIR)/firmware.elf
	@mkdir -p `dirname $@`
	@cp $< $@
	@echo " (PATCH)     $(subst $(FW_OBJ_DIR)/,,$@)"
	@$(call CMD_PATCH_ELF)

$(FW_OBJ_DIR)/firmware.bin: $(FW_OBJ_DIR)/firmware.elf
	@mkdir -p `dirname $@`
	@echo " (OBJCOPY)   $(subst $(FW_OBJ_DIR)/,,$@)"
	@$(OBJCOPY) -O binary $< $@

$(FW_OBJ_DIR)/firmware.elf: $(FW_OBJS) $(ARCH_LDSCRIPT)
	@mkdir -p `dirname $@`
	@echo " (LD)        $(subst $(FW_OBJ_DIR)/,,$@)"
	@$(CC) $(FW_OBJS) $(FW_LDFLAGS) -o $@

$(FW_OBJ_DIR)/%.lnk: $(FW_ARCH_DIR)/%.ld
	@mkdir -p `dirname $@`
	@echo " (CPP)       $(subst $(FW_OBJ_DIR)/,,$@)"
	@$(CPP) $(FW_CPPFLAGS) -x c $< | grep -v "\#" > $@

$(FW_OBJ_DIR)/%.o: %.c $(FW_DEPS)
	@mkdir -p `dirname $@`
	@echo " (CC)        $(subst $(FW_OBJ_DIR)/,,$@)"
	@$(CC) $(FW_CFLAGS) -c $< -o $@

$(FW_OBJ_DIR)/%.o: $(FW_BASIC_DIR)/%.c $(FW_DEPS)
	@mkdir -p `dirname $@`
	@echo " (CC)        $(subst $(FW_OBJ_DIR)/,,$@)"
	@$(CC) $(FW_CFLAGS) -c $< -o $@

$(FW_OBJ_DIR)/%.o: $(FW_ARCH_DIR)/%.c $(FW_DEPS)
	@mkdir -p `dirname $@`
	@echo " (CC)        $(subst $(FW_OBJ_DIR)/,,$@)"
	@$(CC) $(FW_CFLAGS) -c $< -o $@

$(FW_OBJ_DIR)/%.o: %.S $(FW_DEPS)
	@mkdir -p `dirname $@`
	@echo " (AS)        $(subst $(FW_OBJ_DIR)/,,$@)"
	@$(AS) $(FW_ASFLAGS) -c $< -o $@

$(FW_OBJ_DIR)/%.o: $(FW_BASIC_DIR)/%.S $(FW_DEPS)
	@mkdir -p `dirname $@`
	@echo " (AS)        $(subst $(FW_OBJ_DIR)/,,$@)"
	@$(AS) $(FW_ASFLAGS) -c $< -o $@

$(FW_OBJ_DIR)/%.o: $(FW_ARCH_DIR)/%.S $(FW_DEPS)
	@mkdir -p `dirname $@`
	@echo " (AS)        $(subst $(FW_OBJ_DIR)/,,$@)"
	@$(AS) $(FW_ASFLAGS) -c $< -o $@

.PHONY: clean
clean:
	@echo " (RM)        firmware.elf*"
	@rm -f $(FW_OBJ_DIR)/firmware.elf*
	@echo " (RM)        firmware.bin*"
	@rm -f $(FW_OBJ_DIR)/firmware.bin*
	@echo " (RM)        $(FW_OBJ_DIR)"
	@rm -rf $(FW_OBJ_DIR)
