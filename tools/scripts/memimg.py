#!/usr/bin/python
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
# @file memimg.py
# @author Anup Patel (anup@brainfault.org)
# @brief Create memory image from a set of input files of any format
# */

import os
import sys
from optparse import OptionParser

usage = "Usage: %prog [options] <file1_path>@<file1_addr> [<file2_path>@<file2_addr> ...]"
parser = OptionParser(usage=usage)
parser.add_option("-a", "--memaddr", dest="memaddr", type="int",
                  help="Memory address where output image will be loaded")
parser.add_option("-o", "--outimg", dest="outimg",
                  help="Output image path", metavar="FILE")
parser.add_option("-q", "--quiet",
                  action="store_false", dest="verbose", default=True,
                  help="Don't print status messages to stdout")

(options, files) = parser.parse_args()

if not options.memaddr:
	memaddr = 0;
else:
	memaddr = options.memaddr;

outimg = "";
if not options.outimg:
	print "Error: No output file specified";
	sys.exit();
else:
	outimg = options.outimg;

if len(files)==0:
	print "Error: No input files to read";
	sys.exit();

file_paths = [];
file_offsets = [];

for fnum, f in enumerate(files):
	fs = f.split("@");
	if len(fs) < 2:
		print "Warning: Invalid input file %s. Ignoring." %(f);
		continue;
	if int(fs[1], 16) < memaddr:
		print "Warning: Incorrect file address 0x%x for file %s. Ignoring." % (int(fs[1], 16), fs[0]);
		continue;
	if os.path.isfile(fs[0]):
		file_paths.append(fs[0]); 
		file_offsets.append(int(fs[1], 16) - memaddr);
	else:
		print "Warning: File %s does not exists. Ingnoring." % (fs[0]);

outfile = open(outimg, "wb+");
if not outfile:
	print "Error: Unable to open output image %s." % (outimg);
	sys.exit();

for fnum, f in enumerate(file_paths):
	outfile.seek(file_offsets[fnum]);
	infile = open(file_paths[fnum], "rb");
	try:
		inbytes = infile.read(16);
		while inbytes:
			outfile.write(inbytes);
			inbytes = infile.read(16);
	finally:
		infile.close();

outfile.close();
