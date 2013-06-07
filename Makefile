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
# @brief toplevel makefile to build VMM source code
# */

# Current Version
MAJOR = 0
MINOR = 2
RELEASE = 2

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
export PROJECT_VERSION = $(MAJOR).$(MINOR).$(RELEASE)
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

# Setup path of directories
export arch_dir=$(CURDIR)/arch
export cpu_dir=$(CURDIR)/arch/$(CONFIG_ARCH)/cpu/$(CONFIG_CPU)
export cpu_common_dir=$(CURDIR)/arch/$(CONFIG_ARCH)/cpu/common
export board_dir=$(CURDIR)/arch/$(CONFIG_ARCH)/board/$(CONFIG_BOARD)
export board_common_dir=$(CURDIR)/arch/$(CONFIG_ARCH)/board/common
export tools_dir=$(CURDIR)/tools
export core_dir=$(CURDIR)/core
export libs_dir=$(CURDIR)/libs
export commands_dir=$(CURDIR)/commands
export daemons_dir=$(CURDIR)/daemons
export drivers_dir=$(CURDIR)/drivers
export emulators_dir=$(CURDIR)/emulators

# Setup list of tools for compilation
include $(tools_dir)/tools.mk

# Setup list of targets for compilation
targets-y+=$(build_dir)/vmm.elf
targets-y+=$(build_dir)/vmm.bin
targets-y+=$(build_dir)/system.map

# Setup compilation environment
cpp=$(CROSS_COMPILE)cpp
cppflags=-include $(OPENCONF_TMPDIR)/$(OPENCONF_AUTOHEADER)
cppflags+=-DCONFIG_MAJOR=$(MAJOR)
cppflags+=-DCONFIG_MINOR=$(MINOR)
cppflags+=-DCONFIG_RELEASE=$(RELEASE)
cppflags+=-I$(cpu_dir)/include
cppflags+=-I$(cpu_common_dir)/include
cppflags+=-I$(board_dir)/include
cppflags+=-I$(board_common_dir)/include
cppflags+=-I$(core_dir)/include
cppflags+=-I$(commands_dir)/include
cppflags+=-I$(daemons_dir)/include
cppflags+=-I$(drivers_dir)/include
cppflags+=-I$(emulators_dir)/include
cppflags+=-I$(libs_dir)/include
cppflags+=-I$(arch_dir)/include
cppflags+=$(cpu-cppflags)
cppflags+=$(board-cppflags)
cppflags+=$(libs-cppflags-y)
cc=$(CROSS_COMPILE)gcc
cflags=-g -Wall -nostdlib -fno-builtin -D__VMM__
cflags+=$(board-cflags) 
cflags+=$(cpu-cflags) 
cflags+=$(libs-cflags-y) 
cflags+=$(cppflags)
ifdef CONFIG_PROFILE
cflags+=-finstrument-functions
endif
as=$(CROSS_COMPILE)gcc
asflags=-g -Wall -nostdlib -D__ASSEMBLY__ 
asflags+=$(board-asflags) 
asflags+=$(cpu-asflags) 
asflags+=$(libs-asflags-y) 
asflags+=$(cppflags)
ar=$(CROSS_COMPILE)ar
arflags=rcs
ld=$(CROSS_COMPILE)gcc
ldflags=-g -Wall -nostdlib -Wl,--build-id=none
ldflags+=$(board-ldflags) 
ldflags+=$(cpu-ldflags) 
ldflags+=$(libs-ldflags-y) 
merge=$(CROSS_COMPILE)ld
mergeflags=-r
objcopy=$(CROSS_COMPILE)objcopy
nm=$(CROSS_COMPILE)nm

# If only "modules" is specified as make goals then
# define __VMM_MODULES__ in cflags.
# Also, when "modules" is a make goal then no other
# goal can be specified along with it.
ifneq ($(filter modules,$(MAKECMDGOALS)),)
ifeq ($(filter-out modules,$(MAKECMDGOALS)),)
cflags+=-D__VMM_MODULES__
else
$(error Invalid make targets. Cannot use target modules with any other target.)
endif
endif

# Setup functions for compilation
merge_objs = $(V)mkdir -p `dirname $(1)`; \
	     echo " (merge)     $(subst $(build_dir)/,,$(1))"; \
	     $(merge) $(mergeflags) $(2) -o $(1)
merge_deps = $(V)mkdir -p `dirname $(1)`; \
	     echo " (merge-dep) $(subst $(build_dir)/,,$(1))"; \
	     cat $(2) > $(1)
copy_file =  $(V)mkdir -p `dirname $(1)`; \
	     echo " (copy)      $(subst $(build_dir)/,,$(1))"; \
	     cp -f $(2) $(1)
compile_cpp = $(V)mkdir -p `dirname $(1)`; \
	     echo " (cpp)       $(subst $(build_dir)/,,$(1))"; \
	     $(cpp) $(cppflags) $(2) | grep -v "\#" > $(1)
compile_cc_dep = $(V)mkdir -p `dirname $(1)`; \
	     echo " (cc-dep)    $(subst $(build_dir)/,,$(1))"; \
	     echo -n `dirname $(1)`/ > $(1); \
	     $(cc) $(cflags) -I`dirname $(2)` -MM $(2) >> $(1)
compile_cc = $(V)mkdir -p `dirname $(1)`; \
	     echo " (cc)        $(subst $(build_dir)/,,$(1))"; \
	     $(cc) $(cflags) -DVMM_MODNAME=\"$(shell basename $(1))\" -I`dirname $<` -c $(2) -o $(1)
compile_as_dep = $(V)mkdir -p `dirname $(1)`; \
	     echo " (as-dep)    $(subst $(build_dir)/,,$(1))"; \
	     echo -n `dirname $(1)`/ > $(1); \
	     $(as) $(asflags) -I`dirname $(2)` -MM $(2) >> $(1)
compile_as = $(V)mkdir -p `dirname $(1)`; \
	     echo " (as)        $(subst $(build_dir)/,,$(1))"; \
	     $(as) $(asflags) -DVMM_MODNAME=\"$(shell basename $(1))\" -I`dirname $<` -c $(2) -o $(1)
compile_ld = $(V)mkdir -p `dirname $(1)`; \
	     echo " (ld)        $(subst $(build_dir)/,,$(1))"; \
	     $(ld) $(3) $(ldflags) -Wl,-T$(2) -o $(1)
compile_nm = $(V)mkdir -p `dirname $(1)`; \
	     echo " (nm)        $(subst $(build_dir)/,,$(1))"; \
	     $(nm) -n $(2) | grep -v '\( [aNUw] \)\|\(__crc_\)\|\( \$[adt]\)' > $(1)
compile_objcopy = $(V)mkdir -p `dirname $(1)`; \
	     echo " (objcopy)   $(subst $(build_dir)/,,$(1))"; \
	     $(objcopy) -O binary $(2) $(1)

# Setup list of objects.mk files
cpu-object-mks=$(shell if [ -d $(cpu_dir) ]; then find $(cpu_dir) -iname "objects.mk" | sort -r; fi)
cpu-common-object-mks=$(shell if [ -d $(cpu_common_dir) ]; then find $(cpu_common_dir) -iname "objects.mk" | sort -r; fi)
board-object-mks=$(shell if [ -d $(board_dir) ]; then find $(board_dir) -iname "objects.mk" | sort -r; fi)
board-common-object-mks=$(shell if [ -d $(board_common_dir) ]; then find $(board_common_dir) -iname "objects.mk" | sort -r; fi)
core-object-mks=$(shell if [ -d $(core_dir) ]; then find $(core_dir) -iname "objects.mk" | sort -r; fi)
libs-object-mks=$(shell if [ -d $(libs_dir) ]; then find $(libs_dir) -iname "objects.mk" | sort -r; fi)
commands-object-mks=$(shell if [ -d $(commands_dir) ]; then find $(commands_dir) -iname "objects.mk" | sort -r; fi)
daemons-object-mks=$(shell if [ -d $(daemons_dir) ]; then find $(daemons_dir) -iname "objects.mk" | sort -r; fi)
drivers-object-mks=$(shell if [ -d $(drivers_dir) ]; then find $(drivers_dir) -iname "objects.mk" | sort -r; fi)
emulators-object-mks=$(shell if [ -d $(emulators_dir) ]; then find $(emulators_dir) -iname "objects.mk" | sort -r; fi)

# Default rule "make" should always be first rule
.PHONY: all
all:

# Include all object.mk files
include $(cpu-object-mks) 
include $(cpu-common-object-mks) 
include $(board-object-mks) 
include $(board-common-object-mks) 
include $(core-object-mks) 
include $(libs-object-mks) 
include $(commands-object-mks) 
include $(daemons-object-mks)
include $(drivers-object-mks)
include $(emulators-object-mks)

# Setup list of built-in objects
cpu-y=$(foreach obj,$(cpu-objs-y),$(build_dir)/arch/$(CONFIG_ARCH)/cpu/$(CONFIG_CPU)/$(obj))
cpu-common-y=$(foreach obj,$(cpu-common-objs-y),$(build_dir)/arch/$(CONFIG_ARCH)/cpu/common/$(obj))
board-y=$(foreach obj,$(board-objs-y),$(build_dir)/arch/$(CONFIG_ARCH)/board/$(CONFIG_BOARD)/$(obj))
board-common-y=$(foreach obj,$(board-common-objs-y),$(build_dir)/arch/$(CONFIG_ARCH)/board/common/$(obj))
core-y=$(foreach obj,$(core-objs-y),$(build_dir)/core/$(obj))
libs-y=$(foreach obj,$(libs-objs-y),$(build_dir)/libs/$(obj))
commands-y=$(foreach obj,$(commands-objs-y),$(build_dir)/commands/$(obj))
daemons-y=$(foreach obj,$(daemons-objs-y),$(build_dir)/daemons/$(obj))
drivers-y=$(foreach obj,$(drivers-objs-y),$(build_dir)/drivers/$(obj))
emulators-y=$(foreach obj,$(emulators-objs-y),$(build_dir)/emulators/$(obj))
dtbs-y=$(foreach dtb,$(board-dtbs-y),$(build_dir)/arch/$(CONFIG_ARCH)/board/$(CONFIG_BOARD)/$(dtb))

# Setup list of module objects
core-m=$(foreach obj,$(core-objs-m),$(build_dir)/core/$(obj))
libs-m=$(foreach obj,$(libs-objs-m),$(build_dir)/libs/$(obj))
commands-m=$(foreach obj,$(commands-objs-m),$(build_dir)/commands/$(obj))
daemons-m=$(foreach obj,$(daemons-objs-m),$(build_dir)/daemons/$(obj))
drivers-m=$(foreach obj,$(drivers-objs-m),$(build_dir)/drivers/$(obj))
emulators-m=$(foreach obj,$(emulators-objs-m),$(build_dir)/emulators/$(obj))

# Setup list of deps files for built-in objects
deps-y=$(cpu-y:.o=.dep)
deps-y+=$(cpu-common-y:.o=.dep)
deps-y+=$(board-y:.o=.dep)
deps-y+=$(board-common-y:.o=.dep)
deps-y+=$(core-y:.o=.dep)
deps-y+=$(libs-y:.o=.dep)
deps-y+=$(commands-y:.o=.dep)
deps-y+=$(daemons-y:.o=.dep)
deps-y+=$(drivers-y:.o=.dep)
deps-y+=$(emulators-y:.o=.dep)

# Setup list of deps files for module objects
deps-y+=$(core-m:.o=.dep)
deps-y+=$(libs-m:.o=.dep)
deps-y+=$(commands-m:.o=.dep)
deps-y+=$(daemons-m:.o=.dep)
deps-y+=$(drivers-m:.o=.dep)
deps-y+=$(emulators-m:.o=.dep)

# Setup list of all built-in objects
all-y=$(build_dir)/arch/$(CONFIG_ARCH)/cpu/cpu.o
all-y+=$(build_dir)/arch/$(CONFIG_ARCH)/board/board.o
all-y+=$(build_dir)/core/core.o
ifneq ($(words $(libs-y)), 0)
all-y+=$(build_dir)/libs/libs.o
endif
ifneq ($(words $(commands-y)), 0)
all-y+=$(build_dir)/commands/commands.o
endif
ifneq ($(words $(daemons-y)), 0)
all-y+=$(build_dir)/daemons/daemons.o
endif
ifneq ($(words $(drivers-y)), 0)
all-y+=$(build_dir)/drivers/drivers.o
endif
ifneq ($(words $(emulators-y)), 0)
all-y+=$(build_dir)/emulators/emulators.o
endif

# Setup list of all module objects
all-m+=$(core-m:.o=.xo)
all-m+=$(libs-m:.o=.xo)
all-m+=$(commands-m:.o=.xo)
all-m+=$(daemons-m:.o=.xo)
all-m+=$(drivers-m:.o=.xo)
all-m+=$(emulators-m:.o=.xo)

# Preserve all intermediate files
.SECONDARY:

# Default rule "make"
.PHONY: all
all: $(CONFIG_FILE) $(DEPENDENCY_FILE) $(tools-y) $(targets-y)

.PHONY: modules
modules: $(CONFIG_FILE) $(DEPENDENCY_FILE) $(all-m)

dtbs: $(dtbs-y)

# Generate and include built-in dependency rules
-include $(DEPENDENCY_FILE)
$(DEPENDENCY_FILE): $(CONFIG_FILE) $(deps-y)
	$(V)cat $(deps-y) > $(DEPENDENCY_FILE)

# Include additional rules for tools
include $(tools_dir)/rules.mk

$(build_dir)/vmm.bin: $(build_dir)/vmm.elf
	$(call compile_objcopy,$@,$<)

$(build_dir)/vmm.elf: $(build_dir)/linker.ld $(all-y) $(build_dir)/system.o
	$(call compile_ld,$@,$(build_dir)/linker.ld,$(all-y) $(build_dir)/system.o)

$(build_dir)/system.map: $(build_dir)/vmm_tmp.elf
	$(call compile_nm,$@,$<)

$(build_dir)/vmm_tmp.elf: $(build_dir)/linker.ld $(all-y) $(build_dir)/system_tmp1.o
	$(call compile_ld,$@,$(build_dir)/linker.ld,$(all-y) $(build_dir)/system_tmp1.o)

$(build_dir)/system_tmp1.map: $(build_dir)/vmm_tmp1.elf
	$(call compile_nm,$@,$<)

$(build_dir)/vmm_tmp1.elf: $(build_dir)/linker.ld $(all-y)
	$(call compile_ld,$@,$(build_dir)/linker.ld,$(all-y))

$(build_dir)/linker.ld: $(cpu_dir)/linker.ld
	$(call compile_cpp,$@,$<)

$(build_dir)/arch/$(CONFIG_ARCH)/cpu/cpu.o: $(cpu-y) $(cpu-common-y)
	$(call merge_objs,$@,$^)

$(build_dir)/arch/$(CONFIG_ARCH)/board/board.o: $(board-y) $(board-common-y)
	$(call merge_objs,$@,$^)

$(build_dir)/core/core.o: $(core-y)
	$(call merge_objs,$@,$^)

$(build_dir)/libs/libs.o: $(libs-y)
	$(call merge_objs,$@,$^)

$(build_dir)/commands/commands.o: $(commands-y)
	$(call merge_objs,$@,$^)

$(build_dir)/daemons/daemons.o: $(daemons-y)
	$(call merge_objs,$@,$^)

$(build_dir)/drivers/drivers.o: $(drivers-y)
	$(call merge_objs,$@,$^)

$(build_dir)/emulators/emulators.o: $(emulators-y)
	$(call merge_objs,$@,$^)

$(build_dir)/%.dep: $(src_dir)/%.S
	$(call compile_as_dep,$@,$<)

$(build_dir)/%.dep: $(src_dir)/%.c
	$(call compile_cc_dep,$@,$<)

$(build_dir)/%.o: $(src_dir)/%.S
	$(call compile_as,$@,$<)

$(build_dir)/%.o: $(build_dir)/%.S
	$(call compile_as,$@,$<)

$(build_dir)/%.o: $(src_dir)/%.c
	$(call compile_cc,$@,$<)

$(build_dir)/%.o: $(build_dir)/%.c
	$(call compile_cp,$@,$<)

$(build_dir)/%.xo: $(build_dir)/%.o
	$(call copy_file,$@,$^)

# Rule for "make clean"
.PHONY: clean
clean:
ifeq ($(build_dir),$(CURDIR)/build)
	$(if $(V), @echo " (rm)        $(build_dir)")
	$(V)rm -rf $(build_dir)
endif
	$(V)$(MAKE) -C $(src_dir)/tools/dtc clean

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
	./tools/openconf/conf -o $(OPENCONF_INPUT)

# Rule for "make xxx-defconfig"
%-defconfig:
	$(V)mkdir -p $(OPENCONF_TMPDIR)
	$(V)$(MAKE) -C tools/openconf defconfig
	./tools/openconf/conf -D $(src_dir)/arch/$(ARCH)/configs/$@ $(OPENCONF_INPUT)
	./tools/openconf/conf -s $(OPENCONF_INPUT)

