#!/usr/bin/python
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
# @file elf2cpatch.py
# @author Anup Patel (anup@brainfault.org)
# @brief Script to generate cpatch script from guest OS ELF
# */

# We need to trap the guest OS on interrupt return so we replace
# exception return instructions with HVC calls:
# 
# MOVS PC, LR  =>  HVC #0
#

import os
import sys
from optparse import OptionParser

usage = "Usage: %prog [options] <section1> [<section2> <section3> ...]"
parser = OptionParser(usage=usage)
parser.add_option("-f", "--file", dest="filename",
                  help="Input ARM ELF32 file", metavar="FILE")
parser.add_option("-q", "--quiet",
                  action="store_false", dest="verbose", default=True,
                  help="Don't print status messages to stdout")

(options, secs) = parser.parse_args()

if not options.filename:
	print "Error: No input ARM ELF32 file"
	sys.exit()

if len(secs)==0:
	print "Error: No sections to scan"
	sys.exit()

dumpcmd = os.environ.get("CROSS_COMPILE") + "objdump -d " + options.filename

# Initialize data structures
lines = [];
lsyms = [];
lsecs = [];
vlnums = [];
vsymdec = [];
addr2lnum = {};
sym2base = {};

# Populate data structures
sec = ""
sec_valid = 0
lnum = 0
sym = ""
base = 0
p = os.popen(dumpcmd,"r")
while 1:
	l = p.readline()
	if not l: break
	l = l.strip(" ");
	l = l.replace("\n","");
	l = l.replace("\t"," ");
	while l.count("  ")>0:
		l = l.replace("  "," ")
	w = l.split(" ");
	if len(w)>3 and w[0]=="Disassembly" and w[1]=="of" and w[2]=="section":
		w[3] = w[3].replace(":", "")
		sec = ""
		sec_valid = 0
		for i, s in enumerate(secs):
			if w[3]==s:
				sec = s
				sec_valid = 1
				break
	elif sec_valid==1:
		if len(w)>2:
			addr = base | int(w[0].replace(":",""), 16)
			lines.append(l)
			lsecs.append(sec)
			lsyms.append(sym)
			addr2lnum[addr] = lnum
			vlnums.append(True)
			vsymdec.append(False)
			lnum += 1
		elif len(w)==2:
			if not(w[1].startswith("<") and w[1].endswith(">:")):
				continue
			base = int(w[0].replace(":",""), 16)
			w[1] = w[1].replace("<","")
			w[1] = w[1].replace(">:","")
			sym = w[1]
			lines.append(l)
			sym2base[sym] = base
			lsecs.append(sec)
			lsyms.append(sym)
			vlnums.append(False)
			vsymdec.append(True)
			lnum += 1


# MOVS PC, LR (SUBS PC, LR and related instructions)
#	Syntax:
# 		MOVS<c> pc, lr, #<const>
#	Fields:
#		cond = bits[31:28]
#		opcode = bits[24:21]
#		Rn = bits[19:16]
#		imm5 = bits[11:7]
#		type = bits[6:5]
#		Rm = bits[3:0]
#	Hypercall Fields:
#		inst_cond[31:28] = cond
#		inst_op[27:24] = 0x1
#		inst_op[23:20] = 0x4
#		inst_fields[19:8] = 0x0
#		inst_op[7:4] = 0x7
#		inst_fields[3:0] = 0x0
def convert_movs_pc_lr_inst(hxstr):
	hx = int(hxstr, 16)
	cond = (hx >> 28) & 0xF
	rethx = 0x01400070
	rethx = rethx | (cond << 28)
	return rethx

psec = ""
for ln, l in enumerate(lines):
	if vlnums[ln]:
		sec = lsecs[ln];
		if sec!=psec:
			print "section," + sec
			psec = sec
		w = l.split(" ")
		if len(w)<3:
			continue
		w[0] = w[0].replace(":", "")
		addr = int(w[0], 16)
		if (len(w)>1):
			if (w[1]=="Address"):
				continue
		if len(w)>=5: 
			if (w[2]=="movs" and w[3]=="pc," and w[4]=="lr"):
				print "\t#", w[2], w[3], w[4]
				print "\twrite32,0x%x,0x%08x" % (addr, convert_movs_pc_lr_inst(w[1]))

