/*
 * Copyright (c) 2010-2020 Himanshu Chauhan.
 * All rights reserved.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef _CPU_REGISTERS_H
#define _CPU_REGISTERS_H

#define	ZERO		$0
#define	AT		$1
#define	V0		$2
#define	V1		$3
#define	A0		$4
#define	A1		$5
#define	A2		$6
#define	A3		$7
#define	T0		$8
#define	T1		$9
#define	T2		$10
#define	T3		$11
#define	T4		$12
#define	T5		$13
#define	T6		$14
#define	T7		$15
#define	T8		$24
#define	T9		$25
#define	S0		$16
#define	S1		$17
#define	S2		$18
#define	S3		$19
#define	S4		$20
#define	S5		$21
#define	S6		$22
#define	S7		$23
#define	K0		$26
#define	K1		$27
#define	GP		$28
#define	SP		$29
#define	S8		$30
#define	FP		$30
#define	RA		$31

#define ZERO_IDX	0
#define AT_IDX		1
#define V0_IDX		2
#define V1_IDX		3
#define A0_IDX		4
#define A1_IDX		5
#define A2_IDX		6
#define A3_IDX		7
#define T0_IDX		8
#define T1_IDX		9
#define T2_IDX		10
#define T3_IDX		11
#define T4_IDX		12
#define T5_IDX		13
#define T6_IDX		14
#define T7_IDX		15
#define S0_IDX		16
#define S1_IDX		17
#define S2_IDX		18
#define S3_IDX		19
#define S4_IDX		20
#define S5_IDX		21
#define S6_IDX		22
#define S7_IDX		23
#define T8_IDX		24
#define T9_IDX		25
#define K0_IDX		26
#define K1_IDX		27
#define GP_IDX		28
#define SP_IDX		29
#define S8_IDX		30
#define RA_IDX		31
#define U_CP0_EPC_IDX	32
#define U_CP0_STATUS_IDX 33
#define U_CP0_ENTRYHI_IDX 34

#define	CPU_GPR_COUNT	32
#define CPU_UREG_COUNT	(CPU_GPR_COUNT + 3)
#define	CPU_TLB_COUNT	16

#define	CP0_INDEX	$0
#define	CP0_RANDOM	$1
#define	CP0_ENTRYLO0	$2
#define	CP0_ENTRYLO1	$3
#define	CP0_CONTEXT	$4
#define	CP0_PAGEMASK	$5
#define	CP0_WIRED	$6
#define	CP0_HWRENA	$7
#define	CP0_BADVADDR	$8
#define	CP0_COUNT	$9
#define	CP0_ENTRYHI	$10
#define	CP0_COMPARE	$11
#define	CP0_STATUS	$12
#define	CP0_INTCTL	$12,1
#define	CP0_SRSCTL	$12,2
#define	CP0_SRSMAP	$12,3
#define	CP0_CAUSE	$13
#define	CP0_EPC		$14
#define	CP0_PRID	$15
#define	CP0_EBASE	$15,1
#define	CP0_CONFIG	$16
#define	CP0_CONFIG1	$16,1
#define	CP0_CONFIG2	$16,2
#define	CP0_CONFIG3	$16,3
#define	CP0_LLADDR	$17
#define	CP0_WATCHLO	$18
#define	CP0_WATCHHI	$19
#define	CP0_DEBUG	$23
#define	CP0_DEPC	$24
#define	CP0_PERFCTL	$25,0
#define	CP0_PERFCNT	$25,1
#define	CP0_ECC		$26
#define	CP0_CACHEERR	$27
#define	CP0_TAGLO	$28
#define	CP0_DATALO	$28,1
#define	CP0_TAGHI	$29
#define	CP0_DATAHI	$29,1
#define	CP0_ERRORPC	$30

#define	CP0_INDEX_IDX		0
#define	CP0_RANDOM_IDX		1
#define	CP0_ENTRYLO0_IDX	2
#define	CP0_ENTRYLO1_IDX	3
#define	CP0_CONTEXT_IDX		4
#define	CP0_PAGEMASK_IDX	5
#define	CP0_WIRED_IDX		6
#define	CP0_HWRENA_IDX		7
#define	CP0_BADVADDR_IDX	8
#define	CP0_COUNT_IDX		9
#define	CP0_ENTRYHI_IDX		10
#define	CP0_COMPARE_IDX		11
#define	CP0_STATUS_IDX		12
#define	CP0_INTCTL_IDX		13
#define	CP0_SRSCTL_IDX		14
#define	CP0_SRSMAP_IDX		15
#define	CP0_CAUSE_IDX		16
#define	CP0_EPC_IDX		17
#define	CP0_PRID_IDX		18
#define	CP0_EBASE_IDX		19
#define	CP0_CONFIG_IDX		20
#define	CP0_CONFIG1_IDX		21
#define	CP0_CONFIG2_IDX		22
#define	CP0_CONFIG3_IDX		23
#define	CP0_LLADDR_IDX		24
#define	CP0_WATCHLO_IDX		25
#define	CP0_WATCHHI_IDX		26
#define	CP0_DEBUG_IDX		27
#define	CP0_DEPC_IDX		28
#define	CP0_PERFCTL_IDX		29
#define	CP0_PERFCNT_IDX		30
#define	CP0_ECC_IDX		31
#define	CP0_CACHEERR_IDX	32
#define	CP0_TAGLO_IDX		33
#define	CP0_DATALO_IDX		34
#define	CP0_TAGHI_IDX		35
#define	CP0_DATAHI_IDX		36
#define	CP0_ERRORPC_IDX		37

#define	CP0_REG_COUNT		38

#define	S_EntryHiVPN2 	13 /* VPN/2 (R/W) */
#define	ST0_CU0		0x10000000

#define CP0_STATUS_UM_SHIFT	4

#endif /* _MIPS_REGISTERS_H */
