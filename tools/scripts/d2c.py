#!/usr/bin/python
#/**
# Copyright (c) 2013 Anup Patel.
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
# @file d2c.py
# @author Anup Patel (anup@brainfault.org)
# @brief Create C source file from binary data file
#
# The purpose here is to take a binary file and output it
# as an array of bytes suitable for compiling and linking
# with a C/C++ program.
#
# Example usage: ./2c.py somefile.data > somefile.c
# */

import sys
import re

if __name__ == "__main__":
	try:
		filename = sys.argv[1]
	except IndexError:
		print "Usage: %s <filename>" % sys.argv[0]
		raise SystemExit
    
	contentFile = open(filename, "rb");
	varname = filename;

	varname = re.sub('[. -/]', '_', varname);
	varszname = varname + "_size";
	varname = varname + "_start";

	print "const char %s[] = {" % varname,

	filesize = 0;
	while True:
		byte = contentFile.read(1)

		if not byte:
			break

		filesize = filesize + 1
		print "0x%02x," % ord(byte),

	print "};\n"

	print "const unsigned long %s = %d;" % (varszname, filesize);

