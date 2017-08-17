# -*- makefile-gnu -*-
# Copyright (c) 2016 Philipp Ittershagen.
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
# @author Philipp Ittershagen <pit@shgn.de>
# @brief Makefile to build an Xvisor FreeRTOS guest image
#

# support VERBOSE=yes
Q := $(if $(filter-out yesPlease,$(strip $(VERBOSE))Please),@)

# --------- directory settings

basedir  = ../../../..
builddir = $(basedir)/build

srcdir          = $(basedir)/tests/arm32/vexpress-a9/freertos
common_srcdir   = $(srcdir)/../../common/basic
freertos_srcdir = $(srcdir)/FreeRTOS/Source

objdir          = $(patsubst $(basedir)/%,$(builddir)/%, $(srcdir))

# board features
BOARD_SMP = y
BOARD_SECURE_EXTN = n
BOARD_FDT_SUPPORT = n

BOARD_TEXT_START     = 0x48000000
BOARD_USR_STACK_SIZE = 16384

# --------- source files

# FreeRTOS heap allocator implementation choice
FREERTOS_HEAP_IMPL = $(freertos_srcdir)/portable/MemMang/heap_2.c

# platform-independent FreeRTOS source files
FREERTOS_GLOBS = $(addprefix $(freertos_srcdir)/, *.c)

FREERTOS_SRCS = $(wildcard $(FREERTOS_GLOBS)) $(FREERTOS_HEAP_IMPL)

# sources from the common firmware directory (relative to $common_srcdir)
COMMON_SRCS = arm_string.c arm_math.c arm_stdio.c arm_irq.c pic/gic.c	\
              timer/sp804.c serial/pl01x.c arm_cache_v7.S sys/vminfo.c

# FreeRTOS specific glue and application source (relative to $srcdir)
OTHER_SRCS = port/port.c port/portASM.S ../basic/arm_board.c	\
             arm_entry_v7.S glue.c main.c

CC	= $(CROSS_COMPILE)gcc
CPP	= $(CROSS_COMPILE)cpp
LD	= $(CROSS_COMPILE)ld
AS	= $(CROSS_COMPILE)as
OBJCOPY = $(CROSS_COMPILE)objcopy

RM      = rm
MKDIR_P = mkdir -p

ELF2CPATCH = $(basedir)/arch/arm/cpu/arm32/elf2cpatch.py
CPATCH32   = $(builddir)/tools/cpatch/cpatch32

# --------- compiler/linker flags

LINK_SCRIPT = $(common_srcdir)/firmware.ld

CPPFLAGS = -I$(srcdir)/../basic -I$(common_srcdir)			\
           -I$(freertos_srcdir)/include -I$(srcdir) -I$(srcdir)/port	\
           -DTEXT_START=$(BOARD_TEXT_START)				\
           -DUSR_STACK_SIZE=$(BOARD_USR_STACK_SIZE)

ifeq ($(BOARD_SMP), y)
CPPFLAGS += -DBOARD_SMP
endif
ifeq ($(BOARD_SECURE_EXTN),y)
CPPFLAGS += -DARM_SECURE_EXTN_IMPLEMENTED
endif
ifeq ($(BOARD_FDT_SUPPORT),y)
CPPFLAGS += -DBOARD_FDT_SUPPORT
endif

CFLAGS  = -mtune=cortex-a9 -march=armv7-a -Wall -Werror  \
          -nostdlib -msoft-float -marm
ASFLAGS = -D__ASSEMBLY__ $(CFLAGS)
LDFLAGS = -Wl,-T$(LINK_SCRIPT_GEN) -nostdlib -Wl,--build-id=none
LDLIBS  = -lgcc -Wl,-T$(LINK_SCRIPT_GEN)

# --------- binary and elf targets

TARGET_NAME  = freertos
BIN_TARGETS  = $(objdir)/$(TARGET_NAME).bin $(objdir)/$(TARGET_NAME).patched.bin
TARGETS      = $(BIN_TARGETS) $(BIN_TARGETS:%.bin=%.elf)

# assemble final list of source and object files
SRCS = $(sort $(FREERTOS_SRCS)                              \
              $(addprefix $(common_srcdir)/,$(COMMON_SRCS)) \
              $(addprefix $(srcdir)/,$(OTHER_SRCS)))
OBJS = $(patsubst $(basedir)/%,$(builddir)/%.o,$(basename $(SRCS)))

LINK_SCRIPT_GEN = $(patsubst $(basedir)/%,$(builddir)/%.lnk,$(basename $(LINK_SCRIPT)))

ALL_DEPS = $(OBJS:%.o=%.d)

all: $(TARGETS)
PHONY += all

$(objdir)/%.bin: $(objdir)/%.elf
	$(call cmd-objcopy,$@,$<)

$(objdir)/%.patched.elf: $(objdir)/%.elf
	$(call cmd-patch-elf,$@,$<)

$(objdir)/$(TARGET_NAME).elf: $(LINK_SCRIPT_GEN)

$(objdir)/$(TARGET_NAME).elf: $(OBJS)
	$(call cmd-ld,$@,$^)

$(builddir)/%.lnk: $(basedir)/%.ld
	$(call cmd-cpp,$@,$<)

$(builddir)/%.o: $(basedir)/%.c
	$(call cmd-cc,$@,$<)

$(builddir)/%.o: $(basedir)/%.S
	$(call cmd-as,$@,$<)

$(builddir)/%.d: $(basedir)/%.c
	$(call cmd-cc-dep,$@,$<)

$(builddir)/%.d: $(basedir)/%.S
	$(call cmd-cc-dep,$@,$<)

CLEAN_TARGETS = clean-deps clean-objs clean-targets clean-gen
PHONY += $(CLEAN_TARGETS)

clean-deps:
	$(call cmd-delete,$(OBJS:%.o=%.d))

clean-objs:
	$(call cmd-delete,$(OBJS))

clean-targets:
	$(call cmd-delete,$(TARGETS))

clean-gen:
	$(call cmd-delete,$(LINK_SCRIPT_GEN))

clean: $(CLEAN_TARGETS)


-include $(ALL_DEPS)


## convenience functions

cmd-pp = @echo "($(1))	$(subst $(builddir)/,,$(2))"

define cmd-patch-elf
	$(call cmd-pp,patch,$(1))
	$(Q)CROSS_COMPILE=$(CROSS_COMPILE) && \
	cp $(2) $(1) && \
	$(ELF2CPATCH) -f $(1) | $(CPATCH32) $(1) 0 >/dev/null
endef

define cmd-cc
	$(Q)$(MKDIR_P) $(dir $@)
	$(call cmd-pp,cc,$(1))
	$(Q)$(CC) -c $(CPPFLAGS) $(CFLAGS) $(2) -o $(1)
endef

define cmd-as
	$(Q)$(MKDIR_P) $(dir $@)
	$(call cmd-pp,as,$(1))
	$(Q)$(CC) -c $(CPPFLAGS) $(ASFLAGS) $(2) -o $(1)
endef

define cmd-cpp
	$(Q)$(MKDIR_P) $(dir $@)
	$(call cmd-pp,cpp,$(1))
	$(Q)$(CPP) $(CPPFLAGS) $(2) | grep -v "\#" > $(1)
endef

define cmd-ld
	$(Q)$(MKDIR_P) $(dir $@)
	$(call cmd-pp,ld,$(1))
	$(Q)$(CC) $(LDFLAGS) -o $(1) $(2) $(LDLIBS)
endef

define cmd-objcopy
	$(Q)$(MKDIR_P) $(dir $@)
	$(call cmd-pp,copy,$(1))
	$(Q)$(OBJCOPY) -O binary $(2) $(1)
endef

define cmd-cc-dep
	$(Q)$(MKDIR_P) $(dir $@)
	$(call cmd-pp,dep,$(1))
	$(Q)echo -n `dirname $(1)`/ > $(1) && \
	$(CPP) $(CPPFLAGS) -MM $(2) >> $(1) || rm -f $(1)
endef

# empty line required
define cmd-delete
	$(call cmd-pp,clean,$(1))
	$(Q)rm -f $(1)

endef


.PHONY: $(PHONY)
.SUFFIXES:

# Taf!

