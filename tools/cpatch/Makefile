#/**
# Copyright (c) 2011 Anup Patel.
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
# @brief toplevel makefile for Code Patcher
# */

src_dir=$(CURDIR)
ifdef O
 build_dir=$(shell readlink -f $(O))
else
 build_dir=$(CURDIR)/build
endif

targets-y=$(build_dir)/cpatch32
cpatch32-deps-y =$(src_dir)/../../libs/common/elf.h $(src_dir)/libelf32.h
cpatch32-objs-y =$(build_dir)/libelf32.o
cpatch32-objs-y+=$(build_dir)/cpatch32.o

host-cc=gcc
host-cflags=-g -I$(src_dir)/../../libs/common
host-ld=gcc
host-ldflags=

.PHONY: all
all: $(targets-y)

$(cpatch32-objs-y): $(cpatch32-deps-y)

$(build_dir)/cpatch32: $(cpatch32-objs-y)
	@mkdir -p `dirname $@`
	@echo " (host-ld)   $(subst $(build_dir)/,,$@)"
	@$(host-ld) $(host-ldflags) $(cpatch32-objs-y) -o $@

$(build_dir)/%.o: $(src_dir)/%.c
	@mkdir -p `dirname $@`
	@echo " (host-cc)   $(subst $(build_dir)/,,$@)"
	@$(host-cc) $(host-cflags) -I`dirname $<` -c $< -o $@

.PHONY: clean
clean:

