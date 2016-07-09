#/**
# Copyright (c) 2010 Anup Patel.
# All rights reserved.
#
# Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
# Modified by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
# to improve the device tree dependency generation, and to port the Linux
# device tree source preprocessing rule.
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

$(build_dir)/tools/dtc/bin/dtc: $(CURDIR)/tools/dtc/Makefile
	$(V)mkdir -p `dirname $@`
	$(if $(V), @echo " (make)      $(subst $(build_dir)/,,$@)")
	$(V)$(MAKE) -C $(CURDIR)/tools/dtc LINK.c=gcc CC=gcc LD=gcc AR=ar PREFIX=$(build_dir)/tools/dtc install

dtsflags = $(cppflags) -nostdinc -nostdlib -fno-builtin -D__DTS__ -x assembler-with-cpp

$(build_dir)/%.dep: $(src_dir)/%.dts $(build_dir)/tools/dtc/bin/dtc
	$(V)mkdir -p `dirname $@`
	$(if $(V), @echo " (dtc-dep)   $(subst $(build_dir)/,,$@)")
	$(V)$(cpp) $(dtsflags) $< | $(build_dir)/tools/dtc/bin/dtc -Wno-unit_address_vs_reg -d $@ -I dts -O dtb -i `dirname $<` -o /dev/null
	$(V)sed -i "s|/dev/null|$(subst .dep,.dtb,$@)|g" $@
	$(V)sed -i "s|<stdin>|$<|g" $@
	$(V)$(cc) $(dtsflags) -MT $(subst .dep,.dtb,$@) -MM $< >> $@

$(build_dir)/%.dtb: $(src_dir)/%.dts $(build_dir)/tools/dtc/bin/dtc
	$(V)mkdir -p `dirname $@`
	$(if $(V), @echo " (dtc)       $(subst $(build_dir)/,,$@)")
	$(V)$(cpp) $(dtsflags) $< | $(build_dir)/tools/dtc/bin/dtc -Wno-unit_address_vs_reg -p 0x100 -I dts -O dtb -i `dirname $<` -o $@

$(build_dir)/%.S: $(src_dir)/%.dts $(build_dir)/tools/dtc/bin/dtc
	$(V)mkdir -p `dirname $@`
	$(if $(V), @echo " (dtc)       $(subst $(build_dir)/,,$@)")
	$(V)echo '.section ".devtree"' > $@
	$(V)$(cpp) $(dtsflags) $< | $(build_dir)/tools/dtc/bin/dtc -Wno-unit_address_vs_reg -I dts -O asm -i `dirname $<` >> $@

$(build_dir)/%.dep: $(src_dir)/%.data
	$(V)mkdir -p `dirname $@`
	$(if $(V), @echo " (d2c-dep)   $(subst $(build_dir)/,,$@)")
	$(V)echo "$(@:.dep=.c): $<" > $@
	$(V)echo "$(@:.dep=.o): $(@:.dep=.c)" >> $@

$(build_dir)/%.c: $(src_dir)/%.data
	$(V)mkdir -p `dirname $@`
	$(if $(V), @echo " (d2c)       $(subst $(build_dir)/,,$@)")
	$(V)$(src_dir)/tools/scripts/d2c.py $(subst $(src_dir)/,,$<) > $@

$(build_dir)/%.dep: $(build_dir)/%.data
	$(V)mkdir -p `dirname $@`
	$(if $(V), @echo " (d2c-dep)   $(subst $(build_dir)/,,$@)")
	$(V)echo "$(@:.dep=.c): $<" > $@
	$(V)echo "$(@:.dep=.o): $(@:.dep=.c)" >> $@

$(build_dir)/%.c: $(build_dir)/%.data
	$(V)mkdir -p `dirname $@`
	$(if $(V), @echo " (d2c)       $(subst $(build_dir)/,,$@)")
	$(V)(cd $(build_dir) && $(src_dir)/tools/scripts/d2c.py $(subst $(build_dir)/,,$<) > $@ && cd $(src_dir))

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

