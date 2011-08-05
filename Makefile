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
# @brief toplevel makefile to build VMM source code
# */

# Find out source & build directories
src_dir=$(CURDIR)
ifdef O
 build_dir=$(shell readlink -f $(O))
else
 build_dir=$(CURDIR)/build
endif

# Check if verbosity is ON for build process
VERBOSE_DEFAULT    := 0
CMD_PREFIX_DEFAULT := @
ifdef VERBOSE
	ifeq ("$(origin VERBOSE)", "command line")
		VB := $(VERBOSE)
	else
		VB := $(VERBOSE_DEFAULT)
	endif
else
	VB := $(VERBOSE_DEFAULT)
endif
ifeq ($(VB), 1)
	V :=
else
	V := $(CMD_PREFIX_DEFAULT)
endif

# Name & Version
export PROJECT_NAME = Xvisor (eXtensible Versatile hypervISOR)
export PROJECT_VERSION = 0.1
export CONFIG_DIR=$(build_dir)/tmpconf
export CONFIG_FILE=$(build_dir)/.config
export DEPENDENCY_FILE=$(build_dir)/.deps

# Openconf settings
export OPENCONF_PROJECT = $(PROJECT_NAME)
export OPENCONF_VERSION = $(PROJECT_VERSION)
export OPENCONF_INPUT = openconf.cfg
export OPENCONF_CONFIG = $(CONFIG_FILE)
export OPENCONF_TMPDIR = $(CONFIG_DIR)
export OPENCONF_AUTOCONFIG = openconf.conf
export OPENCONF_AUTOHEADER = openconf.h

# Include configuration file if present
-include $(CONFIG_FILE)
CONFIG_ARCH:=$(shell echo $(CONFIG_ARCH))
CONFIG_CPU:=$(shell echo $(CONFIG_CPU))
CONFIG_BOARD:=$(shell echo $(CONFIG_BOARD))

TESTDIR:=tests/$(CONFIG_CPU)/$(CONFIG_BOARD)/basic/

# Setup path of directories
export core_dir=$(CURDIR)/core
export cpu_dir=$(CURDIR)/arch/$(CONFIG_ARCH)/cpu/$(CONFIG_CPU)
export cpu_common_dir=$(CURDIR)/arch/$(CONFIG_ARCH)/cpu/common
export board_dir=$(CURDIR)/arch/$(CONFIG_ARCH)/board/$(CONFIG_BOARD)
export board_common_dir=$(CURDIR)/arch/$(CONFIG_ARCH)/board/common
export tools_dir=$(CURDIR)/tools
export drivers_dir=$(CURDIR)/drivers
export emulators_dir=$(CURDIR)/emulators

# Setup list of objects for compilation
cpu-object-mks=$(shell if [ -d $(cpu_dir) ]; then find $(cpu_dir) -iname "objects.mk"; fi)
cpu-common-object-mks=$(shell if [ -d $(cpu_common_dir) ]; then find $(cpu_common_dir) -iname "objects.mk"; fi)
board-object-mks=$(shell if [ -d $(board_dir) ]; then find $(board_dir) -iname "objects.mk"; fi)
board-common-object-mks=$(shell if [ -d $(board_common_dir) ]; then find $(board_common_dir) -iname "objects.mk"; fi)
core-object-mks=$(shell if [ -d $(core_dir) ]; then find $(core_dir) -iname "objects.mk"; fi)
drivers-object-mks=$(shell if [ -d $(drivers_dir) ]; then find $(drivers_dir) -iname "objects.mk"; fi)
emulators-object-mks=$(shell if [ -d $(emulators_dir) ]; then find $(emulators_dir) -iname "objects.mk"; fi)
include $(cpu-object-mks) 
include $(cpu-common-object-mks) 
include $(board-object-mks) 
include $(board-common-object-mks) 
include $(core-object-mks) 
include $(drivers-object-mks)
include $(emulators-object-mks)
objs-y=$(foreach obj,$(cpu-objs-y),$(build_dir)/arch/$(CONFIG_ARCH)/cpu/$(CONFIG_CPU)/$(obj))
objs-y+=$(foreach obj,$(cpu-common-objs-y),$(build_dir)/arch/$(CONFIG_ARCH)/cpu/common/$(obj))
objs-y+=$(foreach obj,$(board-objs-y),$(build_dir)/arch/$(CONFIG_ARCH)/board/$(CONFIG_BOARD)/$(obj))
objs-y+=$(foreach obj,$(board-common-objs-y),$(build_dir)/arch/$(CONFIG_ARCH)/board/common/$(obj))
objs-y+=$(foreach obj,$(core-objs-y),$(build_dir)/core/$(obj))
objs-y+=$(foreach obj,$(drivers-objs-y),$(build_dir)/drivers/$(obj))
objs-y+=$(foreach obj,$(emulators-objs-y),$(build_dir)/emulators/$(obj))

# Setup list of deps files for compilation
deps-y=$(objs-y:.o=.dep)

# Setup list of tools for compilation
include $(tools_dir)/tools.mk

# Setup list of targets for compilation
targets-y=$(build_dir)/vmm.elf
targets-y+=$(build_dir)/vmm.bin

# Setup compilation environment
cpp=$(CROSS_COMPILE)cpp
cppflags=-include $(OPENCONF_TMPDIR)/$(OPENCONF_AUTOHEADER)
cppflags+=-I$(cpu_dir)/include
cppflags+=-I$(cpu_common_dir)/include
cppflags+=-I$(board_dir)/include
cppflags+=-I$(board_common_dir)/include
cppflags+=-I$(core_dir)/include
cppflags+=-I$(drivers_dir)/include
cppflags+=-I$(emulators_dir)/include
cppflags+=$(cpu-cppflags)
cppflags+=$(board-cppflags)
cc=$(CROSS_COMPILE)gcc
cflags=-g -Wall -nostdlib 
cflags+=$(board-cflags) 
cflags+=$(cpu-cflags) 
cflags+=$(cppflags)
as=$(CROSS_COMPILE)gcc
asflags=-g -Wall -nostdlib -D__ASSEMBLY__ 
asflags+=$(board-asflags) 
asflags+=$(cpu-asflags) 
asflags+=$(cppflags)
ar=$(CROSS_COMPILE)ar
arflasgs=rcs
ld=$(CROSS_COMPILE)gcc
ldflags=-g -Wall -nostdlib 
ldflags+=$(board-ldflags) 
ldflags+=$(cpu-ldflags) 
ldflags+=-Wl,-T$(build_dir)/linker.ld
objcopy=$(CROSS_COMPILE)objcopy

# Default rule "make"
.PHONY: all
all: $(CONFIG_FILE) $(DEPENDENCY_FILE) $(tools-y) $(targets-y)

# Generate and Include dependency rules
-include $(DEPENDENCY_FILE)
$(DEPENDENCY_FILE): $(CONFIG_FILE) $(deps-y)
	$(V)cat $(deps-y) > $(DEPENDENCY_FILE)

# Include additional rules for tools
include $(tools_dir)/rules.mk

# Rules to build .S and .c files
$(build_dir)/vmm.bin: $(build_dir)/vmm.elf
	$(V)mkdir -p `dirname $@`
	$(if $(V), @echo " (objcopy)   $(subst $(build_dir)/,,$@)")
	$(V)$(objcopy) -O binary $< $@

$(build_dir)/vmm.elf: $(build_dir)/linker.ld $(objs-y)
	$(V)mkdir -p `dirname $@`
	$(if $(V), @echo " (ld)        $(subst $(build_dir)/,,$@)")
	$(V)$(ld) $(objs-y) $(ldflags) -o $@

$(build_dir)/linker.ld: $(cpu_dir)/linker.ld
	$(V)mkdir -p `dirname $@`
	$(if $(V), @echo " (cpp)       $(subst $(build_dir)/,,$@)")
	$(V)$(cpp) $(cppflags) $< | grep -v "\#" > $@

$(build_dir)/%.dep: $(src_dir)/%.S
	$(V)mkdir -p `dirname $@`
	$(if $(V), @echo " (as-dep)    $(subst $(build_dir)/,,$@)")
	$(V)echo -n `dirname $@`/ > $@
	$(V)$(as) $(asflags) -I`dirname $<` -MM $< >> $@

$(build_dir)/%.dep: $(src_dir)/%.c
	$(V)mkdir -p `dirname $@`
	$(if $(V), @echo " (cc-dep)    $(subst $(build_dir)/,,$@)")
	$(V)echo -n `dirname $@`/ > $@
	$(V)$(cc) $(cflags) -I`dirname $<` -MM $< >> $@

$(build_dir)/%.o: $(src_dir)/%.S
	$(V)mkdir -p `dirname $@`
	$(if $(V), @echo " (as)        $(subst $(build_dir)/,,$@)")
	$(V)$(as) $(asflags) -I`dirname $<` -c $< -o $@

$(build_dir)/%.o: $(build_dir)/%.S
	$(V)mkdir -p `dirname $@`
	$(if $(V), @echo " (as)        $(subst $(build_dir)/,,$@)")
	$(V)$(as) $(asflags) -I`dirname $<` -c $< -o $@

$(build_dir)/%.o: $(src_dir)/%.c
	$(V)mkdir -p `dirname $@`
	$(if $(V), @echo " (cc)        $(subst $(build_dir)/,,$@)")
	$(V)$(cc) $(cflags) -I`dirname $<` -c $< -o $@

$(build_dir)/%.o: $(build_dir)/%.c
	$(V)mkdir -p `dirname $@`
	$(if $(V), @echo " (cc)        $(subst $(build_dir)/,,$@)")
	$(V)$(cc) $(cflags) -I`dirname $<` -c $< -o $@

# Rule for "make clean"
.PHONY: clean
clean: $(CONFIG_FILE)
ifeq ($(build_dir),$(CURDIR)/build)
	$(if $(V), @echo " (rm)        $(build_dir)")
	$(V)rm -rf $(build_dir)
endif
	$(V)$(MAKE) -C $(src_dir)/tools/dtc clean

.PHONY: tests
tests:
	$(V)$(MAKE) -C $(src_dir)/$(TESTDIR)

# Rule for "make distclean"
.PHONY: distclean
distclean:
	$(V)$(MAKE) -C $(src_dir)/tools/openconf clean

# Include config file rules
-include $(CONFIG_FILE).cmd

# Rule for "make menuconfig"
.PHONY: menuconfig
menuconfig:
	$(V)mkdir -p $(OPENCONF_TMPDIR)
	$(V)$(MAKE) -C tools/openconf menuconfig
	./tools/openconf/mconf $(OPENCONF_INPUT)

# Rule for "make oldconfig"
.PHONY: oldconfig
oldconfig:
	$(V)mkdir -p $(OPENCONF_TMPDIR)
	$(V)$(MAKE) -C tools/openconf oldconfig
	$(V)cp $(src_dir)/arch/$(ARCH)/board/$(BOARD)/defconfig $(OPENCONF_CONFIG)
	./tools/openconf/conf -o $(OPENCONF_INPUT)

# Rule for "make defconfig"
.PHONY: defconfig
defconfig:
	$(V)mkdir -p $(OPENCONF_TMPDIR)
	$(V)$(MAKE) -C tools/openconf defconfig
	$(V)cp $(src_dir)/arch/$(ARCH)/board/$(BOARD)/defconfig $(OPENCONF_CONFIG)
	./tools/openconf/conf -s $(OPENCONF_INPUT)

.PHONY: tags
tags:
	$(V)ctags -R --c++-kinds=+p --fields=+iaS --extra=+q .
	$(V)echo "Generating tags ..."

.PHONY: cscope
cscope:
	$(V)echo "Generating cscope database ..."
	$(V)find ./ -name "*.[CHSchs]" > cscope.files
	$(V)cscope -bqk
