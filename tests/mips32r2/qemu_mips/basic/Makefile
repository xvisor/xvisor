#/**
# Copyright (c) 2010 Himanshu Chauhan.
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
# @author Himanshu Chauhan (hschauhan@nulltrace.org)
# @brief toplevel makefile to build MIPS test code
# */

# Include top-level configuration file if present
top_dir=$(CURDIR)/../../../..
ifdef O
 build_dir=$(shell readlink -f $(O))
else
 build_dir=$(top_dir)/build
endif
-include $(build_dir)/.config
obj_dir=$(build_dir)/tests/mips32r2/qemu-mips/basic

ifdef CROSS_COMPILE
MIPS_CROSS_COMPILE=$(CROSS_COMPILE)
else
MIPS_CROSS_COMPILE=mips-linux-gnu-
endif
MIPS_CPPFLAGS=-I. -DTEXT_START=0x00000000
MIPS_CFLAGS=-g -Wall -Werror -nostdlib -march=mips32r2 $(MIPS_CPPFLAGS)
MIPS_ASFLAGS=-g -Wall -Werror -nostdlib -march=mips32r2 $(MIPS_CPPFLAGS) -D__ASSEMBLY__ -DTEXT_START=0x00000000
MIPS_LDFLAGS=$(MIPS_CFLAGS) -static-libgcc -lgcc -DTEXT_START=0x00000000
MIPS_AS=$(MIPS_CROSS_COMPILE)gcc
MIPS_CC=$(MIPS_CROSS_COMPILE)gcc
MIPS_CPP=$(MIPS_CROSS_COMPILE)cpp
MIPS_OBJCOPY=$(MIPS_CROSS_COMPILE)objcopy

MIPS_TEST_OBJS=$(obj_dir)/mips32r2_entry.o
MIPS_TEST_CPPFLAGS=-DMIPS_TEST_BOOT
MIPS_TEST_CFLAGS=$(MIPS_TEST_CPPFLAGS)
MIPS_TEST_ASFLAGS=$(MIPS_TEST_CPPFLAGS)
MIPS_TEST_LINK_SCRIPT=$(CURDIR)/linker.ld
MIPS_TEST_LDFLAGS=-Wl,-T$(MIPS_TEST_LINK_SCRIPT) $(MIPS_TEST_CPPFLAGS)

MIPS_COMMON_DEPS=mips32r2_asm_macros.h mips32r2_regs.h

.PHONY: all
all: $(build_dir)/pflash.img

$(build_dir)/pflash.img: $(build_dir)/vmm.elf \
			 $(obj_dir)/mips32r2_test.bin \
			 $(build_dir)/u-boot.bin
	@echo " (FLASHIMG)    $(subst $(obj_dir)/,,$@)"
	@dd if=/dev/zero of=$(build_dir)/pflash.img bs=4M count=1
	@dd if=$(build_dir)/u-boot.bin of=$(build_dir)/pflash.img bs=1 \
		conv=notrunc
	@dd if=$(build_dir)/vmm.elf of=$(build_dir)/pflash.img bs=1 \
		conv=notrunc seek=1M
	@dd if=$(obj_dir)/mips32r2_test.bin of=$(build_dir)/pflash.img \
		bs=1 conv=notrunc seek=2M

# Generate the TEST code
$(obj_dir)/mips32r2_test.bin: $(obj_dir)/mips32r2_test.elf
	@mkdir -p `dirname $@`
	@echo " (OBJCOPY)   $(subst $(obj_dir)/,,$@)"
	@$(MIPS_OBJCOPY) -O binary $< $@

$(obj_dir)/mips32r2_test.elf: $(MIPS_TEST_OBJS) $(MIPS_TEST_LINK_SCRIPT)
	@mkdir -p `dirname $@`
	@echo " (LD)        $(subst $(obj_dir)/,,$@)"
	@$(MIPS_CC) $(MIPS_TEST_OBJS) $(MIPS_LDFLAGS) $(MIPS_TEST_LDFLAGS) -o $@

$(MIPS_TEST_OBJS): mips32r2_entry.S $(MIPS_COMMON_DEPS)
	@mkdir -p `dirname $@`
	@echo " (AS)        $(subst $(obj_dir)/,,$@)"
	@$(MIPS_AS) $(MIPS_ASFLAGS) $(MIPS_TEST_ASFLAGS) -c $< -o $@

$(build_dir)/u-boot.bin:
	@echo " (CP)        U-Boot for Qemu-MIPS"
	@cp u-boot.bin $(build_dir)/

.PHONY: clean
clean:
	@echo " (RM)        mips32r2_test.elf*"
	@rm -f $(obj_dir)/mips32r2_test.elf*
	@echo " (RM)        mips32r2_test.bin*"
	@rm -f $(obj_dir)/mips32r2_test.bin*
	@echo " (RM)        qemu.img"
	@rm -f $(obj_dir)/qemu.img
	@echo " (RM)        $(obj_dir)"
	@rm -rf $(obj_dir)

