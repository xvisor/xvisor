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
# @file tools.mk
# @version 1.0
# @author Anup Patel (anup@brainfault.org)
# @brief List of tools to build
# */

tools-y=$(build_dir)/tools/dtc/dtc
tools-y+=$(build_dir)/tools/cpatch/cpatch32
tools-y+=$(build_dir)/tools/bbflash/bb_nandflash_ecc
tools-y+=$(build_dir)/tools/kallsyms/kallsyms

