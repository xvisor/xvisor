#/**
# Copyright (c) 2014 Himanshu Chauhan
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
# @author Himanshu Chauhan <hschauhan@nulltrace.org>
# @brief list of cryptographic hashes
# */

libs-objs-$(CONFIG_CRYPTO_HASH_MD5)+= crypto/hashes/md5.o
libs-objs-$(CONFIG_CRYPTO_HASH_SHA256)+= crypto/hashes/sha256.o
