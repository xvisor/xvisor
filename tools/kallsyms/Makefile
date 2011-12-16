#/**
# Copyright (c) 2011 Jean-Christophe Dubois.
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
# @author Jean-Christophe Dubois <jcd@tribudubois.net>
# @brief toplevel makefile for Beagleboard Nandflash ECC tool
# */

src_dir=$(CURDIR)
ifdef O
 build_dir=$(shell readlink -f $(O))
else
 build_dir=$(CURDIR)/build
endif

targets-y=$(build_dir)/kallsyms
kallsyms-deps-y =
kallsyms-objs-y =$(build_dir)/kallsyms.o

host-cc=gcc
host-cflags=-g
host-ld=gcc
host-ldflags=

.PHONY: all
all: $(targets-y)

$(kallsyms-objs-y): $(kallsyms-deps-y)

$(build_dir)/kallsyms: $(kallsyms-objs-y)
	@mkdir -p `dirname $@`
	@echo " (host-ld)   $(subst $(build_dir)/,,$@)"
	@$(host-ld) $(host-ldflags) $(kallsyms-objs-y) -o $@

$(build_dir)/%.o: $(src_dir)/%.c
	@mkdir -p `dirname $@`
	@echo " (host-cc)   $(subst $(build_dir)/,,$@)"
	@$(host-cc) $(host-cflags) -I`dirname $<` -c $< -o $@

.PHONY: clean
clean:

