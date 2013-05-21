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
# @file rules.mk
# @author Anup Patel (anup@brainfault.org)
# @brief Rules to build & use tools
# */

$(build_dir)/tools/dtc/dtc: $(CURDIR)/tools/dtc/Makefile
	$(V)mkdir -p `dirname $@`
	$(if $(V), @echo " (make)      $(subst $(build_dir)/,,$@)")
	$(V)$(MAKE) -C $(CURDIR)/tools/dtc O=$(build_dir)/tools/dtc

$(build_dir)/%.dep: $(src_dir)/%.dts
	$(V)mkdir -p `dirname $@`
	$(if $(V), @echo " (dtc-dep)   $(subst $(build_dir)/,,$@)")
	$(V)echo "$(@:.dep=.S): $<" > $@

$(build_dir)/%.S: $(src_dir)/%.dts $(build_dir)/tools/dtc/dtc
	$(V)mkdir -p `dirname $@`
	$(if $(V), @echo " (dtc)       $(subst $(build_dir)/,,$@)")
	$(V)$(build_dir)/tools/dtc/dtc -I dts -O asm $< -o $@
	$(V)sed -i '1 i .section ".devtree"' $@

ifdef CONFIG_CPATCH

$(build_dir)/tools/cpatch/cpatch32: $(CURDIR)/tools/cpatch/Makefile
	$(V)mkdir -p `dirname $@`
	$(if $(V), @echo " (make)      $(subst $(build_dir)/,,$@)")
	$(V)$(MAKE) -C $(CURDIR)/tools/cpatch O=$(build_dir)/tools/cpatch

endif

ifdef CONFIG_BBFLASH

$(build_dir)/tools/bbflash/bb_nandflash_ecc: $(CURDIR)/tools/bbflash/Makefile
	$(V)mkdir -p `dirname $@`
	$(if $(V), @echo " (make)      $(subst $(build_dir)/,,$@)")
	$(V)$(MAKE) -C $(CURDIR)/tools/bbflash O=$(build_dir)/tools/bbflash

endif

$(build_dir)/tools/kallsyms/kallsyms: $(CURDIR)/tools/kallsyms/Makefile
	$(V)mkdir -p `dirname $@`
	$(if $(V), @echo " (make)      $(subst $(build_dir)/,,$@)")
	$(V)$(MAKE) -C $(CURDIR)/tools/kallsyms O=$(build_dir)/tools/kallsyms

$(build_dir)/%.S: $(build_dir)/%.map $(build_dir)/tools/kallsyms/kallsyms
	$(V)mkdir -p `dirname $@`
	$(if $(V), @echo " (kallsyms)  $(subst $(build_dir)/,,$@)")
	$(V)$(build_dir)/tools/kallsyms/kallsyms --all-symbols < $< > $@

