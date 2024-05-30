#/**
# Copyright (c) 2010 Anup Patel.
# All rights reserved.
#
# Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
# Modified by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
# to improve the device tree dependency generation.
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
MINOR = 3
RELEASE = 2

# Select Make Options:
# o  Do not use make's built-in rules
# o  Do not print "Entering directory ...";
MAKEFLAGS += -r --no-print-directory

# Find out source, build and install directories
src_dir=$(CURDIR)
ifdef O
 build_dir=$(shell readlink -f $(O))
else
 build_dir=$(CURDIR)/build
endif
ifeq ($(build_dir),$(CURDIR))
$(error Build directory is same as source directory.)
endif
ifdef I
 install_dir=$(shell readlink -f $(I))
else
 install_dir=$(CURDIR)/install
endif
ifeq ($(install_dir),$(CURDIR))
$(error Install directory is same as source directory.)
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
	override V :=
else
	override V := $(CMD_PREFIX_DEFAULT)
endif

# Name & Version
export PROJECT_NAME = Xvisor (eXtensible Versatile hypervISOR)
export PROJECT_VERSION = $(MAJOR).$(MINOR).$(RELEASE)
export CONFIG_DIR=$(build_dir)/openconf
export CONFIG_FILE=$(CONFIG_DIR)/.config

# GIT describe
export GITDESC=$(shell if [ -d $(src_dir)/.git ]; then git describe 2> /dev/null; fi)

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
export arch_common_dir=$(CURDIR)/arch/common
export cpu_dir=$(CURDIR)/arch/$(CONFIG_ARCH)/cpu/$(CONFIG_CPU)
export cpu_common_dir=$(CURDIR)/arch/$(CONFIG_ARCH)/cpu/common
export dts_dir=$(CURDIR)/arch/$(CONFIG_ARCH)/dts
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
ifdef CROSS_COMPILE
CC		=	$(CROSS_COMPILE)gcc
CPP		=	$(CROSS_COMPILE)cpp
AR		=	$(CROSS_COMPILE)ar
LD		=	$(CROSS_COMPILE)ld
NM		=	$(CROSS_COMPILE)nm
OBJCOPY		=	$(CROSS_COMPILE)objcopy
else
CC		?=	gcc
CPP		?=	cpp
AR		?=	ar
LD		?=	ld
NM		?=	nm
OBJCOPY		?=	objcopy
endif
AS		=	$(CC)
DTC		=	dtc

# Check whether the compiler supports --no-warn-rwx-segments
CC_SUPPORT_WARN_RWX_SEGMENTS := $(shell $(CC) -nostdlib -Wl,--no-warn-rwx-segments -x c /dev/null -o /dev/null 2>&1 | grep -- "--no-warn-rwx-segments" >/dev/null && echo n || echo y)

# Setup compilation flags
cppflags=-include $(OPENCONF_TMPDIR)/$(OPENCONF_AUTOHEADER)
cppflags+=-include $(core_dir)/include/vmm_openconf.h
cppflags+=-DCONFIG_MAJOR=$(MAJOR)
cppflags+=-DCONFIG_MINOR=$(MINOR)
cppflags+=-DCONFIG_RELEASE=$(RELEASE)
ifneq ($(GITDESC),)
cppflags+=-DCONFIG_GITDESC="\"$(GITDESC)\""
endif
cppflags+=-I$(cpu_dir)/include
cppflags+=-I$(cpu_common_dir)/include
cppflags+=-I$(board_dir)/include
cppflags+=-I$(board_common_dir)/include
cppflags+=-I$(arch_dir)/include
cppflags+=-I$(arch_common_dir)/include
cppflags+=-I$(core_dir)/include
cppflags+=-I$(commands_dir)/include
cppflags+=-I$(daemons_dir)/include
cppflags+=-I$(drivers_dir)/include
cppflags+=-I$(emulators_dir)/include
cppflags+=-I$(libs_dir)/include
cppflags+=$(cpu-cppflags)
cppflags+=$(board-cppflags)
cppflags+=$(libs-cppflags-y)
cflags=-g -Wall -nostdlib --sysroot=$(drivers_dir)/include -fno-builtin -fno-stack-protector -D__VMM__
cflags+=$(board-cflags)
cflags+=$(cpu-cflags)
cflags+=$(libs-cflags-y)
cflags+=$(cppflags)
ifdef CONFIG_PROFILE
cflags+=-finstrument-functions
endif
asflags=-g -Wall -nostdlib -D__ASSEMBLY__
asflags+=$(board-asflags)
asflags+=$(cpu-asflags)
asflags+=$(libs-asflags-y)
asflags+=$(cppflags)
arflags=rcs
ldflags=-g -Wall -nostdlib -Wl,--build-id=none
ifeq ($(CC_SUPPORT_WARN_RWX_SEGMENTS),y)
ldflags+=-Wl,--no-warn-rwx-segments
endif
ldflags+=$(board-ldflags)
ldflags+=$(cpu-ldflags)
ldflags+=$(libs-ldflags-y)
mergeflags=-r
mergeflags+=$(cpu-mergeflags)
dataflags=-r -b binary

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

define dynamic_flags
-I$(shell dirname $(2)) -DVMM_MODNAME=$(subst -,_,$(shell basename $(1) .o))
endef

# Setup functions for compilation
merge_objs = $(V)mkdir -p `dirname $(1)`; \
	     echo " (merge)     $(subst $(build_dir)/,,$(1))"; \
	     $(LD) $(mergeflags) $(2) -o $(1)
merge_deps = $(V)mkdir -p `dirname $(1)`; \
	     echo " (merge-dep) $(subst $(build_dir)/,,$(1))"; \
	     cat $(2) > $(1)
copy_file =  $(V)mkdir -p `dirname $(1)`; \
	     echo " (copy)      $(subst $(build_dir)/,,$(1))"; \
	     cp -f $(2) $(1)
compile_cpp = $(V)mkdir -p `dirname $(1)`; \
	     echo " (cpp)       $(subst $(build_dir)/,,$(1))"; \
	     $(CPP) $(cppflags) -x c $(2) | grep -v "\#" > $(1)
compile_cc_dep = $(V)mkdir -p `dirname $(1)`; \
	     echo " (cc-dep)    $(subst $(build_dir)/,,$(1))"; \
	     echo -n `dirname $(1)`/ > $(1) && \
	     $(CC) $(cflags) $(call dynamic_flags,$(1),$(2))   \
	       -MM $(2) >> $(1) || rm -f $(1)
compile_cc = $(V)mkdir -p `dirname $(1)`; \
	     echo " (cc)        $(subst $(build_dir)/,,$(1))"; \
	     $(CC) $(cflags) $(call dynamic_flags,$(1),$(2)) -c $(2) -o $(1)
compile_as_dep = $(V)mkdir -p `dirname $(1)`; \
	     echo " (as-dep)    $(subst $(build_dir)/,,$(1))"; \
	     echo -n `dirname $(1)`/ > $(1) && \
	     $(AS) $(asflags) $(call dynamic_flags,$(1),$(2))  \
	       -MM $(2) >> $(1) || rm -f $(1)
compile_as = $(V)mkdir -p `dirname $(1)`; \
	     echo " (as)        $(subst $(build_dir)/,,$(1))"; \
	     $(AS) $(asflags) $(call dynamic_flags,$(1),$(2)) -c $(2) -o $(1)
compile_ld = $(V)mkdir -p `dirname $(1)`; \
	     echo " (ld)        $(subst $(build_dir)/,,$(1))"; \
	     $(CC) $(3) $(ldflags) -Wl,-T$(2) -o $(1)
compile_nm = $(V)mkdir -p `dirname $(1)`; \
	     echo " (nm)        $(subst $(build_dir)/,,$(1))"; \
	     $(NM) -n $(2) | grep -v '\( [aNUw] \)\|\(__crc_\)\|\( \$$[adt]\)' > $(1)
compile_objcopy = $(V)mkdir -p `dirname $(1)`; \
	     echo " (objcopy)   $(subst $(build_dir)/,,$(1))"; \
	     $(OBJCOPY) -O binary $(2) $(1)
inst_file =  $(V)mkdir -p `dirname $(1)`; \
	     echo " (install)   $(subst $(install_dir)/,,$(1))"; \
	     cp -f $(2) $(1)
inst_file_list =  $(V)mkdir -p $(1); \
	     for f in $(2) ; do \
	     echo " (install)  " `echo $$f | sed -e "s@$(3)/@@"`; \
	     cp -f $$f $(1); \
	     done

# Setup list of objects.mk files
cpu-object-mks=$(shell if [ -d $(cpu_dir) ]; then find $(cpu_dir) -iname "objects.mk" | sort -r; fi)
cpu-common-object-mks=$(shell if [ -d $(cpu_common_dir) ]; then find $(cpu_common_dir) -iname "objects.mk" | sort -r; fi)
dts-object-mks=$(shell if [ -d $(dts_dir) ]; then find $(dts_dir) -iname "objects.mk" | sort -r; fi)
board-object-mks=$(shell if [ -d $(board_dir) ]; then find $(board_dir) -iname "objects.mk" | sort -r; fi)
board-common-object-mks=$(shell if [ -d $(board_common_dir) ]; then find $(board_common_dir) -iname "objects.mk" | sort -r; fi)
arch-common-object-mks=$(shell if [ -d $(arch_common_dir) ]; then find $(arch_common_dir) -iname "objects.mk" | sort -r; fi)
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
include $(dts-object-mks)
include $(board-object-mks)
include $(board-common-object-mks)
include $(arch-common-object-mks)
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
arch-common-y=$(foreach obj,$(arch-common-objs-y),$(build_dir)/arch/common/$(obj))
core-y=$(foreach obj,$(core-objs-y),$(build_dir)/core/$(obj))
libs-y=$(foreach obj,$(libs-objs-y),$(build_dir)/libs/$(obj))
commands-y=$(foreach obj,$(commands-objs-y),$(build_dir)/commands/$(obj))
daemons-y=$(foreach obj,$(daemons-objs-y),$(build_dir)/daemons/$(obj))
drivers-y=$(foreach obj,$(drivers-objs-y),$(build_dir)/drivers/$(obj))
emulators-y=$(foreach obj,$(emulators-objs-y),$(build_dir)/emulators/$(obj))
dtbs-y=$(foreach dtb,$(arch-dtbs-y),$(build_dir)/arch/$(CONFIG_ARCH)/dts/$(dtb))

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
deps-y+=$(arch-common-y:.o=.dep)
deps-y+=$(core-y:.o=.dep)
deps-y+=$(libs-y:.o=.dep)
deps-y+=$(commands-y:.o=.dep)
deps-y+=$(daemons-y:.o=.dep)
deps-y+=$(drivers-y:.o=.dep)
deps-y+=$(emulators-y:.o=.dep)
deps-y+=$(dtbs-y:.dtb=.dep)

# Setup list of deps files for module objects
deps-y+=$(core-m:.o=.dep)
deps-y+=$(libs-m:.o=.dep)
deps-y+=$(commands-m:.o=.dep)
deps-y+=$(daemons-m:.o=.dep)
deps-y+=$(drivers-m:.o=.dep)
deps-y+=$(emulators-m:.o=.dep)

# Setup list of all built-in objects
all-y=$(build_dir)/arch/arch.o
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
all: $(CONFIG_FILE) $(tools-y) $(targets-y) $(dtbs-y)

.PHONY: dtbs
dtbs: $(CONFIG_FILE) $(dtbs-y)

.PHONY: modules
modules: $(CONFIG_FILE) $(all-m)

.PHONY: install
install: all
	$(call inst_file_list,$(install_dir),$(targets-y),$(build_dir))
	$(call inst_file_list,$(install_dir),$(dtbs-y),$(build_dir))

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

$(build_dir)/arch/arch.o: $(cpu-y) $(cpu-common-y) $(board-y) $(board-common-y) $(arch-common-y)
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
	$(call compile_cc,$@,$<)

$(build_dir)/%.xo: $(build_dir)/%.o
	$(call copy_file,$@,$^)

# Include built-in and module objects dependency files
# Dependency files should only be included after default Makefile rule
# They should not be included for any "xxxconfig", "xxxclean", or "cscope" rule
NO_DEP_TARGETS := %config %clean cscope
ifeq ($(filter $(NO_DEP_TARGETS),$(MAKECMDGOALS)),)
 -include $(deps-y)
endif

# Rule for "make clean"
.PHONY: clean
clean:
	$(V)mkdir -p $(build_dir)
	$(if $(V), @echo " (rm)        $(build_dir)/*.c")
	$(V)find $(build_dir) -type f -name "*.c" -exec rm -rf {} +
	$(if $(V), @echo " (rm)        $(build_dir)/*.s")
	$(V)find $(build_dir) -type f -name "*.s" -exec rm -rf {} +
	$(if $(V), @echo " (rm)        $(build_dir)/*.S")
	$(V)find $(build_dir) -type f -name "*.S" -exec rm -rf {} +
	$(if $(V), @echo " (rm)        $(build_dir)/*.o")
	$(V)find $(build_dir) -type f -name "*.o" -exec rm -rf {} +
	$(if $(V), @echo " (rm)        $(build_dir)/*.elf")
	$(V)find $(build_dir) -type f -name "*.elf" -exec rm -rf {} +
	$(if $(V), @echo " (rm)        $(build_dir)/*.bin")
	$(V)find $(build_dir) -type f -name "*.bin" -exec rm -rf {} +

# Rule for "make distclean"
.PHONY: distclean
distclean:
	$(V)mkdir -p $(build_dir)
	$(if $(V), @echo " (rm)        $(build_dir)/*.dep")
	$(V)find $(build_dir) -type f -name "*.dep" -exec rm -rf {} +
ifeq ($(build_dir),$(CURDIR)/build)
	$(if $(V), @echo " (rm)        $(build_dir)")
	$(V)rm -rf $(build_dir)
endif
ifeq ($(install_dir),$(CURDIR)/install)
	$(if $(V), @echo " (rm)        $(install_dir)")
	$(V)rm -rf $(install_dir)
endif
	$(V)$(MAKE) -C $(src_dir)/tools/openconf clean
	$(if $(V), @echo " (rm)        $(CURDIR)/cscope*")
	$(V)rm -f $(CURDIR)/cscope*

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
	./tools/openconf/conf -s $(OPENCONF_INPUT)

# Rule for "make savedefconfig"
.PHONY: savedefconfig
savedefconfig:
	$(V)mkdir -p $(OPENCONF_TMPDIR)
	$(V)$(MAKE) -C tools/openconf savedefconfig
	./tools/openconf/conf -S $(OPENCONF_TMPDIR)/defconfig $(OPENCONF_INPUT)

# Rule for "make xxx-defconfig"
%-defconfig:
	$(V)mkdir -p $(OPENCONF_TMPDIR)
	$(V)$(MAKE) -C tools/openconf defconfig
	./tools/openconf/conf -D $(src_dir)/arch/$(ARCH)/configs/$@ $(OPENCONF_INPUT)
	./tools/openconf/conf -s $(OPENCONF_INPUT)

.PHONY: cscope
SUPPORTED_ARCHS := arm x86 riscv
IGNORED_ARCHS := $(if $(CONFIG_ARCH),$(filter-out $(CONFIG_ARCH),$(SUPPORTED_ARCHS)),)
cscope:
	$(V)find $(CURDIR) \
		$(foreach IGNORED_ARCH,$(IGNORED_ARCHS),-not -path '$(CURDIR)/*/$(IGNORED_ARCH)/*') \
		$(if $(findstring arm,$(IGNORED_ARCHS)),-not -path '$(CURDIR)/*/arm32/*' -not -path '$(CURDIR)/*/arm64/*',) \
		-name "*.[chS]" -print > $(CURDIR)/cscope.files
ifneq ("$(wildcard $(OPENCONF_TMPDIR)/$(OPENCONF_AUTOHEADER))","")
	$(V)echo "$(OPENCONF_TMPDIR)/$(OPENCONF_AUTOHEADER)" >> $(CURDIR)/cscope.files
else
	$(warning cscope will be partially complete, please consider trying again after "make menuconfig".)
endif
	$(V)cscope -bkq -i $(CURDIR)/cscope.files -f $(CURDIR)/cscope.out
