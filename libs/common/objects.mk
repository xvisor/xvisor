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
# @author Jean-Christophe Dubois (jcd@tribudubois.net)
# @brief list of common objects to be build
# */

libs-objs-y+= common/bcd.o
libs-objs-y+= common/bitops.o
libs-objs-y+= common/bitmap.o
libs-objs-y+= common/ctype.o
libs-objs-y+= common/scatterlist.o
libs-objs-y+= common/stringlib.o
libs-objs-y+= common/mathlib.o
libs-objs-y+= common/stacktrace.o
libs-objs-y+= common/smoothsort.o
libs-objs-y+= common/list_sort.o
libs-objs-y+= common/fifo.o
libs-objs-y+= common/lifo.o
libs-objs-y+= common/rbtree.o
libs-objs-y+= common/radix-tree.o
libs-objs-y+= common/buddy.o
libs-objs-y+= common/mempool.o
libs-objs-y+= common/libfdt.o
libs-objs-y+= common/bitrev.o

libs-objs-$(CONFIG_LIBAUTH)+= common/libauth.o
libs-objs-$(CONFIG_LIBAUTH_DEFAULT_USER)+= common/libauth_passwd.o

ifdef CONFIG_LIBAUTH_DEFAULT_USER

ifdef CONFIG_LIBAUTH_USE_MD5_PASSWD
hash_bin:=md5sum
endif
ifdef CONFIG_LIBAUTH_USE_SHA256_PASSWD
hash_bin:=sha256sum
endif

$(build_dir)/libs/common/libauth_passwd.data: $(CONFIG_FILE)
	$(V)mkdir -p `dirname $@`
	$(V)echo -n ${CONFIG_LIBAUTH_DEFAULT_PASSWD} | ${hash_bin} | awk '{ printf "%s", $$1 }' >> $@

$(build_dir)/libauth_passwd.o: $(build_dir)/libauth_passwd.c
	$(call compile_cc,$@,$<)

endif

libs-objs-$(CONFIG_GENALLOC)+= common/genalloc.o

