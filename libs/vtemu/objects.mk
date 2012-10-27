#/**
# Copyright (c) 2012 Anup Patel.
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
# @file objects.mk
# @author Anup Patel (anup@brainfault.org)
# @brief list of vtemu objects to be build
# */

libs-objs-$(CONFIG_VTEMU)+= vtemu/vtemu_lib.o

vtemu_lib-y+= vtemu.o
vtemu_lib-y+= vtemu_font.o
vtemu_lib-$(CONFIG_VTEMU_FONT_6x11)+= vtemu_font_6x11.o
vtemu_lib-$(CONFIG_VTEMU_FONT_7x14)+= vtemu_font_7x14.o
vtemu_lib-$(CONFIG_VTEMU_FONT_8x8)+= vtemu_font_8x8.o
vtemu_lib-$(CONFIG_VTEMU_FONT_8x16)+= vtemu_font_8x16.o
vtemu_lib-$(CONFIG_VTEMU_FONT_10x18)+= vtemu_font_10x18.o
vtemu_lib-$(CONFIG_VTEMU_FONT_ACORN_8x8)+= vtemu_font_acorn_8x8.o
vtemu_lib-$(CONFIG_VTEMU_FONT_MINI_8x8)+= vtemu_font_mini_4x6.o
vtemu_lib-$(CONFIG_VTEMU_FONT_PEARL_8x8)+= vtemu_font_pearl_8x8.o
vtemu_lib-$(CONFIG_VTEMU_FONT_SUN8x16)+= vtemu_font_sun8x16.o
vtemu_lib-$(CONFIG_VTEMU_FONT_SUN12x22)+= vtemu_font_sun12x22.o

%/vtemu_lib.o: $(foreach obj,$(vtemu_lib-y),%/$(obj))
	$(call merge_objs,$@,$^)

%/vtemu_lib.dep: $(foreach dep,$(vtemu_lib-y:.o=.dep),%/$(dep))
	$(call merge_deps,$@,$^)

