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
libs-objs-y+= common/scatterlist.o
libs-objs-y+= common/stringlib.o
libs-objs-y+= common/mathlib.o
libs-objs-y+= common/stacktrace.o
libs-objs-y+= common/smoothsort.o
libs-objs-y+= common/list_sort.o

