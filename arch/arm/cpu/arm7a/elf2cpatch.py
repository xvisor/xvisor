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
# @file elf2cpatch.py
# @version 1.0
# @author Anup Patel (anup@brainfault.org)
# @brief Script to generate cpatch script from guest OS ELF
# */

# Each sensitive non-priviledged ARM instruction is converted to a hypercall
# The hypercall in ARM instruction set is SVC <imm24> instruction.
#
# We encode sensitive non-priviledged instructions in <imm24> operand of SVC
# instruction. Each encoded instruction will have its own unique inst_id.
# the fields of instruction will be encoded as inst_field. The inst_field
# for each encoded sensitive non-priviledged instruction will be diffrent.
#
# Limitations:
# 1. LDM/STM: In LDM (exception return), LDM (user register) and 
#	      STM (user register) instructions possible values of Rn are 
#	      r0, r4, r8, or sp.
# 2. LDRBT/STRBT: In LDRBT/STRBT Rt are r0, r1, r2, r3, r4, r5, r6, and r7
# 3. LDRT/STRT: In LDRT/STRT Rt are r0, r1, r2, r3, r4, r5, r6, and r7
#
# The above limitations are introduced based on the instruction usage in 
# Linux and NetBSD

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

dumpcmd = os.environ.get("CROSS_COMPILE") + "objdump -D " + options.filename

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

# Heuritic based line filtering to avoid overwriting constants 
for lnum, l in enumerate(lines):
	if not(vlnums[lnum]):
		continue
	w = l.split(" ")
	if len(w)>4:
		op = w[2]
		a1 = w[3].replace(",", "")
		c0 = w[len(w)-3]
		c1 = w[len(w)-2]
		c2 = w[len(w)-1]
		if op=="ldr" and c0==";" and c2.startswith("<") and c2.endswith(">"):
			sym = c2
			sym = sym.replace("<", "")
			sym = sym.replace(">", "")
			syms = sym.split("+")
			if not(sym2base.has_key(syms[0])):
				continue
			addr = sym2base[syms[0]]
			if len(syms)>1:
				if syms[1].startswith("0x"):
					addr += int(syms[1], 16)
				else:
					addr += int(syms[1], 16)
			if not(addr2lnum.has_key(addr)):
				continue
			ln = addr2lnum[addr]
			while ln < len(vlnums):
				if vlnums[ln]:
					vlnums[ln] = False
					ln += 1
				else:
					break
			if a1!="pc":
				if w[1].startswith("0x"):
					addr = int(w[1], 16)
				else:
					addr = int(w[1], 16)
				if not(addr2lnum.has_key(addr)):
					continue
				ln = addr2lnum[addr];
				while ln < len(vlnums):
					if vlnums[ln]:
						vlnums[ln] = False
						ln += 1
					else:
						break

#for ln, l in enumerate(lines):
#	if vsymdec[ln]:
#		print ""
#	if vlnums[ln] or vsymdec[ln]:
#		print l

# CPS
#	Syntax:
# 		cpsie <iflags> {,#mode} 
# 		cpsid <iflags> {,#mode} 
# 		cps #<mode> 
#	Fields:
#		imod = bits[19:18]
#		M = bits[17:17]
#		A = bits[8:8]
#		I = bits[7:7]
#		F = bits[6:6]
#		mode = bits[4:0]
#	Hypercall Fields:
#		inst_cond[31:28] = cond
#		inst_op[27:24] = 0xf
#		inst_id[23:20] = 0
#		inst_subid[19:17] = 0
#		inst_fields[16:15] = imod
#		inst_fields[14:14] = M
#		inst_fields[13:13] = A
#		inst_fields[12:12] = I
#		inst_fields[11:11] = F
#		inst_fields[10:6] = mode
def convert_cps_inst(hxstr):
	hx = int(hxstr, 16)
	inst_id = 0
	inst_subid = 0
	cond = 0xe
	imod = (hx >> 18) & 0x3
	M = (hx >> 17) & 0x1
	A = (hx >> 8) & 0x1
	I = (hx >> 7) & 0x1
	F = (hx >> 6) & 0x1
	mode = hx & 0x1F
	rethx = 0x0F000000
	rethx = rethx | (cond << 28)
	rethx = rethx | (inst_id << 20)
	rethx = rethx | (inst_subid << 17)
	rethx = rethx | (imod << 15)
	rethx = rethx | (M << 14)
	rethx = rethx | (A << 13)
	rethx = rethx | (I << 12)
	rethx = rethx | (F << 11)
	rethx = rethx | (mode << 6)
	return rethx

# MRS
#	Syntax:
# 		mrs<c> <Rd>, <spec_reg> 
#	Fields:
#		cond = bits[31:28]
#		R = bits[22:22]
#		Rd = bits[15:12]
#	Hypercall Fields:
#		inst_cond[31:28] = cond
#		inst_op[27:24] = 0xf
#		inst_id[23:20] = 0
#		inst_subid[19:17] = 1
#		inst_fields[16:13] = Rd
#		inst_fields[12:12] = R
def convert_mrs_inst(hxstr):
	hx = int(hxstr, 16)
	inst_id = 0
	inst_subid = 1
	cond = (hx >> 28) & 0xF
	R = (hx >> 22) & 0x1
	Rd = (hx >> 12) & 0xF
	rethx = 0x0F000000
	rethx = rethx | (cond << 28)
	rethx = rethx | (inst_id << 20)
	rethx = rethx | (inst_subid << 17)
	rethx = rethx | (Rd << 13)
	rethx = rethx | (R << 12)
	return rethx

# MSR (immediate)
#	Syntax:
# 		msr<c> <spec_reg>, #<const>
#	Fields:
#		cond = bits[31:28]
#		R = bits[22:22]
#		mask = bits[19:16]
#		imm12 = bits[11:0]
#	Hypercall Fields:
#		inst_cond[31:28] = cond
#		inst_op[27:24] = 0xf
#		inst_id[23:20] = 0
#		inst_subid[19:17] = 2
#		inst_fields[16:13] = mask
#		inst_fields[12:1] = imm12
#		inst_fields[0:0] = R
def convert_msr_i_inst(hxstr):
	hx = int(hxstr, 16)
	inst_id = 0
	inst_subid = 2
	cond = (hx >> 28) & 0xF
	R = (hx >> 22) & 0x1
	mask = (hx >> 16) & 0xF
	imm12 = (hx >> 0) & 0xFFF
	rethx = 0x0F000000
	rethx = rethx | (cond << 28)
	rethx = rethx | (inst_id << 17)
	rethx = rethx | (mask << 13)
	rethx = rethx | (imm12 << 1)
	rethx = rethx | (R << 0)
	return rethx

# MSR (register)
#	Syntax:
# 		msr<c> <spec_reg>, <Rn>
#	Fields:
#		cond = bits[31:28]
#		R = bits[22:22]
#		mask = bits[19:16]
#		Rn = bits[3:0]
#	Hypercall Fields:
#		inst_cond[31:28] = cond
#		inst_op[27:24] = 0xf
#		inst_id[23:20] = 0
#		inst_subid[19:17] = 3
#		inst_fields[16:13] = mask
#		inst_fields[12:9] = Rn
#		inst_fields[8:8] = R
def convert_msr_r_inst(hxstr):
	hx = int(hxstr, 16)
	inst_id = 0
	inst_subid = 3
	cond = (hx >> 28) & 0xF
	R = (hx >> 22) & 0x1
	mask = (hx >> 16) & 0xF
	Rn = (hx >> 0) & 0xF
	rethx = 0x0F000000
	rethx = rethx | (cond << 28)
	rethx = rethx | (inst_id << 20)
	rethx = rethx | (inst_subid << 17)
	rethx = rethx | (mask << 13)
	rethx = rethx | (Rn << 9)
	rethx = rethx | (R << 8)
	return rethx

# RFE
#	Syntax:
# 		rfe<amode><c> <Rn>{!}
#	Fields:
#		P = bits[24:24]
#		U = bits[23:23]
#		W = bits[21:21]
#		Rn = bits[19:16]
#	Hypercall Fields:
#		inst_cond[31:28] = cond
#		inst_op[27:24] = 0xf
#		inst_id[23:20] = 0
#		inst_subid[19:17] = 4
#		inst_fields[16:16] = P
#		inst_fields[15:15] = U
#		inst_fields[14:14] = W
#		inst_fields[13:10] = Rn
def convert_rfe_inst(hxstr):
	hx = int(hxstr, 16)
	inst_id = 0
	inst_subid = 4 
	cond = 0xF
	P = (hx >> 24) & 0x1
	U = (hx >> 23) & 0x1
	W = (hx >> 21) & 0x1
	Rn = (hx >> 16) & 0xF
	rethx = 0x0F000000
	rethx = rethx | (cond << 28)
	rethx = rethx | (inst_id << 20)
	rethx = rethx | (inst_subid << 17)
	rethx = rethx | (P << 16)
	rethx = rethx | (U << 15)
	rethx = rethx | (W << 14)
	rethx = rethx | (Rn << 10)
	return rethx

# SRS
#	Syntax:
# 		srs{amode}<c> sp{!}, #mode
#	Fields:
#		P = bits[24:24]
#		U = bits[23:23]
#		W = bits[21:21]
#		mode = bits[5:0]
#	Hypercall Fields:
#		inst_cond[31:28] = cond
#		inst_op[27:24] = 0xf
#		inst_id[23:20] = 0
#		inst_subid[19:17] = 5
#		inst_fields[16:16] = P
#		inst_fields[15:15] = U
#		inst_fields[14:14] = W
#		inst_fields[13:10] = mode
def convert_srs_inst(hxstr):
	hx = int(hxstr, 16)
	inst_id = 0
	inst_subid = 5
	cond = 0xF
	P = (hx >> 24) & 0x1
	U = (hx >> 23) & 0x1
	W = (hx >> 21) & 0x1
	mode = (hx >> 16) & 0xF
	rethx = 0x0F000000
	rethx = rethx | (cond << 28)
	rethx = rethx | (inst_id << 20)
	rethx = rethx | (inst_subid << 17)
	rethx = rethx | (P << 16)
	rethx = rethx | (U << 15)
	rethx = rethx | (W << 14)
	rethx = rethx | (mode << 10)
	return rethx

# LDM (exception return or user register)
#	Syntax:
# 		ldm<amode><c> <Rn>{!}, <registers_with_or_without_pc>^
#	Fields:
#		cond = bits[31:28]
#		P = bits[24:24]
#		U = bits[23:23]
#		W = bits[21:21]
#		Rn = bits[19:16]
#		reg_list = bits[15:0]
#	Hypercall Fields:
#		inst_cond[31:28] = cond
#		inst_op[27:24] = 0xf
#		inst_id[23:20] = 1 (if P==0)
#		inst_id[23:20] = 2 (if P==1)
#		inst_fields[19:18] = 0 (if Rn=[r0-r3] then Rn=r0)
#		inst_fields[19:18] = 1 (if Rn=[r4-r7] then Rn=r4)
#		inst_fields[19:18] = 2 (if Rn=[r8-r11] then Rn=r8)
#		inst_fields[19:18] = 3 (if Rn=[r12-r15] then Rn=r13 or Rn=sp)
#		inst_fields[17:17] = U
#		inst_fields[16:16] = W
#		inst_fields[15:0] = reg_list
def convert_ldm_ue_inst(hxstr):
	hx = int(hxstr, 16)
	cond = (hx >> 28) & 0xF
	P = (hx >> 24) & 0x1
	U = (hx >> 23) & 0x1
	W = (hx >> 21) & 0x1
	Rn = (hx >> 16) & 0xF
	if (0<=Rn and Rn<4):
		Rn = 0 # Use Rn=r0
	elif (4<=Rn and Rn<8):
		Rn = 1 # Use Rn=r4
	elif (8<=Rn and Rn<12):
		Rn = 2 # Use Rn=r8
	else:
		Rn = 3 # Use Rn=sp
	inst_id = 1 + P
	reg_list = (hx >> 0) & 0xFFFF
	rethx = 0x0F000000
	rethx = rethx | (cond << 28)
	rethx = rethx | (inst_id << 20)
	rethx = rethx | (Rn << 18)
	rethx = rethx | (U << 17)
	rethx = rethx | (W << 16)
	rethx = rethx | (reg_list << 0)
	return rethx

# STM (user registers)
#	Syntax:
# 		stm<amode><c> <Rn>, <registers>^
#	Fields:
#		cond = bits[31:28]
#		P = bits[24:24]
#		U = bits[23:23]
#		Rn = bits[19:16]
#		reg_list = bits[15:0]
#	Hypercall Fields:
#		inst_cond[31:28] = cond
#		inst_op[27:24] = 0xf
#		inst_id[23:20] = 3
#		inst_fields[19:18] = 0 (if Rn=[r0-r3] then Rn=r0)
#		inst_fields[19:18] = 1 (if Rn=[r4-r7] then Rn=r4)
#		inst_fields[19:18] = 2 (if Rn=[r8-r11] then Rn=r8)
#		inst_fields[19:18] = 3 (if Rn=[r12-r15] then Rn=r13 or Rn=sp)
#		inst_fields[17:17] = P
#		inst_fields[18:18] = U
#		inst_fields[15:0] = reg_list
def convert_stm_u_inst(hxstr):
	hx = int(hxstr, 16)
	cond = (hx >> 28) & 0xF
	P = (hx >> 24) & 0x1
	U = (hx >> 23) & 0x1
	Rn = (hx >> 16) & 0xF
	reg_list = (hx >> 0) & 0x7FFF
	if (0<=Rn and Rn<4):
		Rn = 0 # Use Rn=r0
	elif (4<=Rn and Rn<8):
		Rn = 1 # Use Rn=r4
	elif (8<=Rn and Rn<12):
		Rn = 2 # Use Rn=r8
	else:
		Rn = 3 # Use Rn=sp
	inst_id = 3
	rethx = 0x0F000000
	rethx = rethx | (cond << 28)
	rethx = rethx | (inst_id << 20)
	rethx = rethx | (Rn << 18)
	rethx = rethx | (P << 17)
	rethx = rethx | (U << 16)
	rethx = rethx | (reg_list << 0)
	return rethx

# SUBS PC, LR and related instructions
#	Syntax:
# 		subs<c> pc, lr, #<const>
# 		<opc1>s<c> pc, <Rn>, #<const>
# 		<opc1>S<c> pc, <Rn>, <Rm> {,<shift>}
# 		<opc2>s<c> pc, #<const>
# 		<opc2>S<c> pc, <Rm> {,<shift>}
#	Fields:
#		cond = bits[31:28]
#		opcode = bits[24:21]
#		Rn = bits[19:16]
#		imm12 = bits[11:0]
#		imm5 = bits[11:7]
#		type = bits[6:5]
#		Rm = bits[3:0]
#	Hypercall Fields:
#		inst_cond[31:28] = cond
#		inst_op[27:24] = 0xf
#		inst_id[23:20] = 4 (if bits[25:25]==0)
#		inst_id[23:20] = 5 (if bits[25:25]==1)
#		inst_fields[19:16] = opcode
#		inst_fields[15:12] = Rn
#		inst_fields[11:0] = imm12
#		inst_fields[11:7] = imm5
#		inst_fields[6:5] = type
#		inst_fields[3:0] = Rm
def convert_subs_rel_inst(hxstr):
	hx = int(hxstr, 16)
	cond = (hx >> 28) & 0xF
	opcode = (hx >> 21) & 0xF
	Rn = (hx >> 16) & 0xF
	imm12 = (hx >> 0) & 0xFFF
	imm5 = (hx >> 7) & 0x1F
	typ = (hx >> 5) & 0x3
	Rm = (hx >> 0) & 0xF
	inst_id = 4 + ((hx >> 25) & 0x1)
	rethx = 0x0F000000
	rethx = rethx | (cond << 28)
	rethx = rethx | (inst_id << 20)
	rethx = rethx | (opcode << 16)
	rethx = rethx | (Rn << 12)
	if (inst_id==5):
		rethx = rethx | (imm12 << 0)
	else:
		rethx = rethx | (imm5 << 7)
		rethx = rethx | (typ << 5)
		rethx = rethx | (Rm << 0)
	return rethx

# LDRT
#	Syntax:
# 		ldrt<c> <Rt>, [<Rn> {, #<imm>}]
# 		ldrt<c> <Rt>, [<Rn>] {, #+/-<imm>}
# 		ldrt<c> <Rt>, [<Rn>], +/-<Rm> {, <shift>}
#	Fields:
#		cond = bits[31:28]
#		U = bits[23:23]
#		Rn = bits[19:16]
#		Rt = bits[15:12]
#		imm12 = bits[11:0]
#		imm5 = bits[11:7]
#		type = bits[6:5]
#		Rm = bits[3:0]
#	Hypercall Fields:
#		inst_cond[31:28] = cond
#		inst_op[27:24] = 0xf
#		inst_id[23:20] = 6 (if bits[25:25]==0)
#		inst_id[23:20] = 7 (if bits[25:25]==1)
#		inst_fields[19:19] = U
#		inst_fields[18:15] = Rn
#		inst_fields[14:12] = Rt
#		inst_fields[11:0] = imm12
#		inst_fields[11:7] = imm5
#		inst_fields[6:5] = type
#		inst_fields[3:0] = Rm
def convert_ldrt_inst(hxstr,is_strt):
	hx = int(hxstr, 16)
	cond = (hx >> 28) & 0xF
	U = (hx >> 23) & 0x1
	Rn = (hx >> 16) & 0xF
	Rt = (hx >> 12) & 0xF
	Rt = Rt & 0x7 # Hack Rt can be [r0-r7]
	imm12 = (hx >> 0) & 0xFFF
	imm5 = (hx >> 7) & 0x1F
	typ = (hx >> 5) & 0x3
	Rm = (hx >> 0) & 0xF
	inst_id = 6 + ((hx >> 25) & 0x1)
	rethx = 0x0F000000
	rethx = rethx | (cond << 28)
	rethx = rethx | (inst_id << 20)
	rethx = rethx | (U << 19)
	rethx = rethx | (Rn << 15)
	rethx = rethx | (Rt << 12)
	if (((hx >> 25) & 0x1) == 0):
		rethx = rethx | (imm12 << 0)
	else:
		rethx = rethx | (imm5 << 7)
		rethx = rethx | (typ << 5)
		rethx = rethx | (Rm << 0)
	return rethx

# STRT
#	Syntax:
# 		strt<c> <Rt>, [<Rn> {, #<imm>}]
# 		strt<c> <Rt>, [<Rn>] {, #+/-<imm>}
# 		strt<c> <Rt>, [<Rn>], +/-<Rm> {, <shift>}
#	Fields:
#		cond = bits[31:28]
#		U = bits[23:23]
#		Rn = bits[19:16]
#		Rt = bits[15:12]
#		imm12 = bits[11:0]
#		imm5 = bits[11:7]
#		type = bits[6:5]
#		Rm = bits[3:0]
#	Hypercall Fields:
#		inst_cond[31:28] = cond
#		inst_op[27:24] = 0xf
#		inst_id[23:20] = 8 (if bits[25:25]==0)
#		inst_id[23:20] = 9 (if bits[25:25]==1)
#		inst_fields[19:19] = U
#		inst_fields[18:15] = Rn
#		inst_fields[14:12] = Rt
#		inst_fields[11:0] = imm12
#		inst_fields[11:7] = imm5
#		inst_fields[6:5] = type
#		inst_fields[3:0] = Rm
def convert_strt_inst(hxstr):
	hx = int(hxstr, 16)
	cond = (hx >> 28) & 0xF
	U = (hx >> 23) & 0x1
	Rn = (hx >> 16) & 0xF
	Rt = (hx >> 12) & 0xF
	Rt = Rt & 0x7 # Hack Rt can be [r0-r7]
	imm12 = (hx >> 0) & 0xFFF
	imm5 = (hx >> 7) & 0x1F
	typ = (hx >> 5) & 0x3
	Rm = (hx >> 0) & 0xF
	inst_id = 8 + ((hx >> 25) & 0x1)
	rethx = 0x0F000000
	rethx = rethx | (cond << 28)
	rethx = rethx | (inst_id << 20)
	rethx = rethx | (U << 19)
	rethx = rethx | (Rn << 15)
	rethx = rethx | (Rt << 12)
	if (((hx >> 25) & 0x1) == 0):
		rethx = rethx | (imm12 << 0)
	else:
		rethx = rethx | (imm5 << 7)
		rethx = rethx | (typ << 5)
		rethx = rethx | (Rm << 0)
	return rethx

# LDRBT
#	Syntax:
# 		ldrbt<c> <Rt>, [<Rn> {, #<imm>}]
# 		ldrbt<c> <Rt>, [<Rn>] {, #+/-<imm>}
# 		ldrbt<c> <Rt>, [<Rn>], +/-<Rm> {, <shift>}
#	Fields:
#		cond = bits[31:28]
#		U = bits[23:23]
#		Rn = bits[19:16]
#		Rt = bits[15:12]
#		imm12 = bits[11:0]
#		imm5 = bits[11:7]
#		type = bits[6:5]
#		Rm = bits[3:0]
#	Hypercall Fields:
#		inst_cond[31:28] = cond
#		inst_op[27:24] = 0xf
#		inst_id[23:20] = 10 (if bits[25:25]==0)
#		inst_id[23:20] = 11 (if bits[25:25]==1)
#		inst_fields[19:19] = U
#		inst_fields[18:15] = Rn
#		inst_fields[14:12] = Rt
#		inst_fields[11:0] = imm12
#		inst_fields[11:7] = imm5
#		inst_fields[6:5] = type
#		inst_fields[3:0] = Rm
def convert_ldrbt_inst(hxstr):
	hx = int(hxstr, 16)
	cond = (hx >> 28) & 0xF
	U = (hx >> 23) & 0x1
	Rn = (hx >> 16) & 0xF
	Rt = (hx >> 12) & 0xF
	Rt = Rt & 0x7 # Hack Rt can be [r0-r7]
	imm12 = (hx >> 0) & 0xFFF
	imm5 = (hx >> 7) & 0x1F
	typ = (hx >> 5) & 0x3
	Rm = (hx >> 0) & 0xF
	inst_id = 10 + ((hx >> 25) & 0x1)
	rethx = 0x0F000000
	rethx = rethx | (cond << 28)
	rethx = rethx | (inst_id << 20)
	rethx = rethx | (U << 19)
	rethx = rethx | (Rn << 15)
	rethx = rethx | (Rt << 12)
	if (((hx >> 25) & 0x1) == 0):
		rethx = rethx | (imm12 << 0)
	else:
		rethx = rethx | (imm5 << 7)
		rethx = rethx | (typ << 5)
		rethx = rethx | (Rm << 0)
	return rethx

# STRBT
#	Syntax:
# 		strbt<c> <Rt>, [<Rn> {, #<imm>}]
# 		strbt<c> <Rt>, [<Rn>] {, #+/-<imm>}
# 		strbt<c> <Rt>, [<Rn>], +/-<Rm> {, <shift>}
#	Fields:
#		cond = bits[31:28]
#		U = bits[23:23]
#		Rn = bits[19:16]
#		Rt = bits[15:12]
#		imm12 = bits[11:0]
#		imm5 = bits[11:7]
#		type = bits[6:5]
#		Rm = bits[3:0]
#	Hypercall Fields:
#		inst_cond[31:28] = cond
#		inst_op[27:24] = 0xf
#		inst_id[23:20] = 12 (if bits[25:25]==0)
#		inst_id[23:20] = 13 (if bits[25:25]==1)
#		inst_fields[19:19] = U
#		inst_fields[18:15] = Rn
#		inst_fields[14:12] = Rt
#		inst_fields[11:0] = imm12
#		inst_fields[11:7] = imm5
#		inst_fields[6:5] = type
#		inst_fields[3:0] = Rm
def convert_strbt_inst(hxstr):
	hx = int(hxstr, 16)
	cond = (hx >> 28) & 0xF
	U = (hx >> 23) & 0x1
	Rn = (hx >> 16) & 0xF
	Rt = (hx >> 12) & 0xF
	Rt = Rt & 0x7 # Hack Rt can be [r0-r7]
	imm12 = (hx >> 0) & 0xFFF
	imm5 = (hx >> 7) & 0x1F
	typ = (hx >> 5) & 0x3
	Rm = (hx >> 0) & 0xF
	inst_id = 12 + ((hx >> 25) & 0x1)
	rethx = 0x0F000000
	rethx = rethx | (cond << 28)
	rethx = rethx | (inst_id << 20)
	rethx = rethx | (U << 19)
	rethx = rethx | (Rn << 15)
	rethx = rethx | (Rt << 12)
	if (((hx >> 25) & 0x1) == 0):
		rethx = rethx | (imm12 << 0)
	else:
		rethx = rethx | (imm5 << 7)
		rethx = rethx | (typ << 5)
		rethx = rethx | (Rm << 0)
	return rethx

# LDRHT
#	Syntax:
# 		ldrht<c> <Rt>, [<Rn> {, #<imm>}]
# 		ldrht<c> <Rt>, [<Rn>] {, #+/-<imm>}
# 		ldrht<c> <Rt>, [<Rn>], +/-<Rm>
#	Fields:
#		cond = bits[31:28]
#		U = bits[23:23]
#		Rn = bits[19:16]
#		Rt = bits[15:12]
#		imm4H = bits[11:8]
#		imm4L = bits[3:0]
#		Rm = bits[3:0]
#	Hypercall Fields:
#		inst_cond[31:28] = cond
#		inst_op[27:24] = 0xf
#		inst_id[23:20] = 14
#		inst_subid[19:17] = 0 (if bits[22:22]==0)
#		inst_subid[19:17] = 1 (if bits[22:22]==1)
#		inst_fields[16:13] = Rn
#		inst_fields[12:9] = Rt
#		inst_fields[8:8] = U
#		inst_fields[7:4] = imm4H
#		inst_fields[3:0] = imm4L
#		inst_fields[7:4] = Rm
def convert_ldrht_inst(hxstr):
	hx = int(hxstr, 16)
	cond = (hx >> 28) & 0xF
	U = (hx >> 23) & 0x1
	Rn = (hx >> 16) & 0xF
	Rt = (hx >> 12) & 0xF
	imm4H = (hx >> 8) & 0xF
	imm4L = (hx >> 0) & 0xF
	Rm = (hx >> 0) & 0xF
	inst_id = 14
	inst_subid = 0 + ((hx >> 22) & 0x1)
	rethx = 0x0F000000
	rethx = rethx | (cond << 28)
	rethx = rethx | (inst_id << 20)
	rethx = rethx | (inst_subid << 17)
	rethx = rethx | (Rn << 13)
	rethx = rethx | (Rt << 9)
	rethx = rethx | (U << 8)
	if (((hx >> 22) & 0x1) == 1):
		rethx = rethx | (imm4H << 4)
		rethx = rethx | (imm4L << 0)
	else:
		rethx = rethx | (Rm << 4)
	return rethx

# LDRSBT
#	Syntax:
# 		ldrsbt<c> <Rt>, [<Rn> {, #<imm>}]
# 		ldrsbt<c> <Rt>, [<Rn>] {, #+/-<imm>}
# 		ldrsbt<c> <Rt>, [<Rn>], +/-<Rm>
#	Fields:
#		cond = bits[31:28]
#		U = bits[23:23]
#		Rn = bits[19:16]
#		Rt = bits[15:12]
#		imm4H = bits[11:8]
#		imm4L = bits[3:0]
#		Rm = bits[3:0]
#	Hypercall Fields:
#		inst_cond[31:28] = cond
#		inst_op[27:24] = 0xf
#		inst_id[23:20] = 14
#		inst_subid[19:17] = 2 (if bits[22:22]==0)
#		inst_subid[19:17] = 3 (if bits[22:22]==1)
#		inst_fields[16:13] = Rn
#		inst_fields[12:9] = Rt
#		inst_fields[8:8] = U
#		inst_fields[7:4] = imm4H
#		inst_fields[3:0] = imm4L
#		inst_fields[7:4] = Rm
def convert_ldrsbt_inst(hxstr):
	hx = int(hxstr, 16)
	cond = (hx >> 28) & 0xF
	U = (hx >> 23) & 0x1
	Rn = (hx >> 16) & 0xF
	Rt = (hx >> 12) & 0xF
	imm4H = (hx >> 8) & 0xF
	imm4L = (hx >> 0) & 0xF
	Rm = (hx >> 0) & 0xF
	inst_id = 14
	inst_subid = 2 + ((hx >> 22) & 0x1)
	rethx = 0x0F000000
	rethx = rethx | (cond << 28)
	rethx = rethx | (inst_id << 20)
	rethx = rethx | (inst_subid << 17)
	rethx = rethx | (Rn << 13)
	rethx = rethx | (Rt << 9)
	rethx = rethx | (U << 8)
	if (((hx >> 22) & 0x1) == 1):
		rethx = rethx | (imm4H << 4)
		rethx = rethx | (imm4L << 0)
	else:
		rethx = rethx | (Rm << 4)
	return rethx

# LDRSHT
#	Syntax:
# 		ldrsht<c> <Rt>, [<Rn> {, #<imm>}]
# 		ldrsht<c> <Rt>, [<Rn>] {, #+/-<imm>}
# 		ldrsht<c> <Rt>, [<Rn>], +/-<Rm>
#	Fields:
#		cond = bits[31:28]
#		U = bits[23:23]
#		Rn = bits[19:16]
#		Rt = bits[15:12]
#		imm4H = bits[11:8]
#		imm4L = bits[3:0]
#		Rm = bits[3:0]
#	Hypercall Fields:
#		inst_cond[31:28] = cond
#		inst_op[27:24] = 0xf
#		inst_id[23:20] = 14
#		inst_subid[19:17] = 4 (if bits[22:22]==0)
#		inst_subid[19:17] = 5 (if bits[22:22]==1)
#		inst_fields[16:13] = Rn
#		inst_fields[12:9] = Rt
#		inst_fields[8:8] = U
#		inst_fields[7:4] = imm4H
#		inst_fields[3:0] = imm4L
#		inst_fields[7:4] = Rm
def convert_ldrsht_inst(hxstr):
	hx = int(hxstr, 16)
	cond = (hx >> 28) & 0xF
	U = (hx >> 23) & 0x1
	Rn = (hx >> 16) & 0xF
	Rt = (hx >> 12) & 0xF
	imm4H = (hx >> 8) & 0xF
	imm4L = (hx >> 0) & 0xF
	Rm = (hx >> 0) & 0xF
	inst_id = 14
	inst_subid = 4 + ((hx >> 22) & 0x1)
	rethx = 0x0F000000
	rethx = rethx | (cond << 28)
	rethx = rethx | (inst_id << 20)
	rethx = rethx | (inst_subid << 17)
	rethx = rethx | (Rn << 13)
	rethx = rethx | (Rt << 9)
	rethx = rethx | (U << 8)
	if (((hx >> 22) & 0x1) == 1):
		rethx = rethx | (imm4H << 4)
		rethx = rethx | (imm4L << 0)
	else:
		rethx = rethx | (Rm << 4)
	return rethx

# STRHT
#	Syntax:
# 		strht<c> <Rt>, [<Rn> {, #<imm>}]
# 		strht<c> <Rt>, [<Rn>] {, #+/-<imm>}
# 		strht<c> <Rt>, [<Rn>], +/-<Rm>
#	Fields:
#		cond = bits[31:28]
#		U = bits[23:23]
#		Rn = bits[19:16]
#		Rt = bits[15:12]
#		imm4H = bits[11:8]
#		imm4L = bits[3:0]
#		Rm = bits[3:0]
#	Hypercall Fields:
#		inst_cond[31:28] = cond
#		inst_op[27:24] = 0xf
#		inst_id[23:20] = 14
#		inst_subid[19:17] = 6 (if bits[22:22]==0)
#		inst_subid[19:17] = 7 (if bits[22:22]==1)
#		inst_fields[16:13] = Rn
#		inst_fields[12:9] = Rt
#		inst_fields[8:8] = U
#		inst_fields[7:4] = imm4H
#		inst_fields[3:0] = imm4L
#		inst_fields[7:4] = Rm
def convert_strht_inst(hxstr):
	hx = int(hxstr, 16)
	cond = (hx >> 28) & 0xF
	U = (hx >> 23) & 0x1
	Rn = (hx >> 16) & 0xF
	Rt = (hx >> 12) & 0xF
	imm4H = (hx >> 8) & 0xF
	imm4L = (hx >> 0) & 0xF
	Rm = (hx >> 0) & 0xF
	inst_id = 14
	inst_subid = 6 + ((hx >> 22) & 0x1)
	rethx = 0x0F000000
	rethx = rethx | (cond << 28)
	rethx = rethx | (inst_id << 20)
	rethx = rethx | (inst_subid << 17)
	rethx = rethx | (Rn << 13)
	rethx = rethx | (Rt << 9)
	rethx = rethx | (U << 8)
	if (((hx >> 22) & 0x1) == 1):
		rethx = rethx | (imm4H << 4)
		rethx = rethx | (imm4L << 0)
	else:
		rethx = rethx | (Rm << 4)
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
		if (len(w)==4):
			if (w[2]=="cps" or w[2]=="cpsie" or w[2]=="cpsid"):
				print "\t#", w[2], w[3]
				print "\twrite32,0x%x,0x%08x" % (addr, convert_cps_inst(w[1]))
			elif (w[2].startswith("rfeda") or 
				w[2].startswith("rfedb") or 
				w[2].startswith("rfeia") or 
				w[2].startswith("rfeib") or 
				w[2].startswith("rfe")):
				print "\t#", w[2], w[3]
				print "\twrite32,0x%x,0x%08x" % (addr, convert_rfe_inst(w[1]))				
		elif len(w)>=5: 
			if (w[2].startswith("mrs")):
				print "\t#", w[2], w[3], w[4]
				print "\twrite32,0x%x,0x%08x" % (addr, convert_mrs_inst(w[1]))
			elif (w[2].startswith("msr")):
				print "\t#", w[2], w[3], w[4]
				# Check bit[25] to findout 
				# whether instruction is immediate or literal
				if int(w[1], 16) & 0x02000000:
					print "\twrite32,0x%x,0x%08x" % (addr, convert_msr_i_inst(w[1]))
				else:
					print "\twrite32,0x%x,0x%08x" % (addr, convert_msr_r_inst(w[1]))
			elif (w[2].startswith("srsda") or 
				w[2].startswith("srsdb") or 
				w[2].startswith("srsia") or 
				w[2].startswith("srsib") or 
				w[2].startswith("srs")):
				print "\t#", w[2], w[3], w[4]
				print "\twrite32,0x%x,0x%08x" % (addr, convert_srs_inst(w[1]))
			elif ((w[2].startswith("ldmda") or 
				w[2].startswith("ldmdb") or 
				w[2].startswith("ldmia") or 
				w[2].startswith("ldmib") or 
				w[2].startswith("ldm")) and
				w[4].startswith("{") and
				w[len(w)-1].endswith("}^")):
				print "\t#", w[2], w[3], w[4]
				print "\twrite32,0x%x,0x%08x" % (addr, convert_ldm_ue_inst(w[1]))
			elif ((w[2].startswith("stmda") or 
				w[2].startswith("stmdb") or 
				w[2].startswith("stmia") or 
				w[2].startswith("stmib") or 
				w[2].startswith("stm")) and
				w[4].startswith("{") and
				w[len(w)-1].endswith("}^")):
				print "\t#", w[2], w[3], w[4]
				print "\twrite32,0x%x,0x%08x" % (addr, convert_stm_u_inst(w[1]))
			elif (w[2].startswith("ldrbt")):
				print "\t#", w[2], w[3], w[4]
				print "\twrite32,0x%x,0x%08x" % (addr, convert_ldrbt_inst(w[1]))
			elif (w[2].startswith("ldrht")):
				print "\t#", w[2], w[3], w[4]
				print "\twrite32,0x%x,0x%08x" % (addr, convert_ldrht_inst(w[1]))
			elif (w[2].startswith("ldrsbt")):
				print "\t#", w[2], w[3], w[4]
				print "\twrite32,0x%x,0x%08x" % (addr, convert_ldrsbt_inst(w[1]))
			elif (w[2].startswith("ldrsht")):
				print "\t#", w[2], w[3], w[4]
				print "\twrite32,0x%x,0x%08x" % (addr, convert_ldrsht_inst(w[1]))
			elif (w[2].startswith("ldrt")):
				print "\t#", w[2], w[3], w[4]
				print "\twrite32,0x%x,0x%08x" % (addr, convert_ldrt_inst(w[1]))
			elif (w[2].startswith("strbt")):
				print "\t#", w[2], w[3], w[4]
				print "\twrite32,0x%x,0x%08x" % (addr, convert_strbt_inst(w[1]))
			elif (w[2].startswith("strht")):
				print "\t#", w[2], w[3], w[4]
				print "\twrite32,0x%x,0x%08x" % (addr, convert_strht_inst(w[1]))
			elif (w[2].startswith("strt")):
				print "\t#", w[2], w[3], w[4]
				print "\twrite32,0x%x,0x%08x" % (addr, convert_strt_inst(w[1]))
			elif ((int(w[1], 16) & 0x0C10F000)==0x0010F000):
				print "\t#", w[2], w[3], w[4]
				print "\twrite32,0x%x,0x%08x" % (addr, convert_subs_rel_inst(w[1]))

