/**
 * Copyright (c) 2013 Sukanto Ghosh.
 * All rights reserved.
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
 * @file arm_features.h
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief common header file for ARM CPU features
 */
#ifndef _ARM_FEATURES_H__
#define _ARM_FEATURES_H__

/* CPUID related macors & defines */
#define ARM_CPUID_ARM1026     0x4106a262
#define ARM_CPUID_ARM926      0x41069265
#define ARM_CPUID_ARM946      0x41059461
#define ARM_CPUID_TI915T      0x54029152
#define ARM_CPUID_TI925T      0x54029252
#define ARM_CPUID_SA1100      0x4401A11B
#define ARM_CPUID_SA1110      0x6901B119
#define ARM_CPUID_PXA250      0x69052100
#define ARM_CPUID_PXA255      0x69052d00
#define ARM_CPUID_PXA260      0x69052903
#define ARM_CPUID_PXA261      0x69052d05
#define ARM_CPUID_PXA262      0x69052d06
#define ARM_CPUID_PXA270      0x69054110
#define ARM_CPUID_PXA270_A0   0x69054110
#define ARM_CPUID_PXA270_A1   0x69054111
#define ARM_CPUID_PXA270_B0   0x69054112
#define ARM_CPUID_PXA270_B1   0x69054113
#define ARM_CPUID_PXA270_C0   0x69054114
#define ARM_CPUID_PXA270_C5   0x69054117
#define ARM_CPUID_ARM1136     0x4117b363
#define ARM_CPUID_ARM1136_R2  0x4107b362
#define ARM_CPUID_ARM11MPCORE 0x410fb022
#define ARM_CPUID_CORTEXA8    0x410fc080
#define ARM_CPUID_CORTEXA9    0x410fc090
#define ARM_CPUID_CORTEXA15   0x412fc0f1
#define ARM_CPUID_CORTEXM3    0x410fc231
#define ARM_CPUID_ARMV8	      0x000f0000
#define ARM_CPUID_ANY         0xffffffff

/* VCPU Feature related enumeration */
enum arm_features {
	ARM_FEATURE_VFP,
	ARM_FEATURE_AUXCR,  /* ARM1026 Auxiliary control register.  */
	ARM_FEATURE_XSCALE, /* Intel XScale extensions.  */
	ARM_FEATURE_IWMMXT, /* Intel iwMMXt extension.  */
	ARM_FEATURE_V6,
	ARM_FEATURE_V6K,
	ARM_FEATURE_V7,
	ARM_FEATURE_THUMB2,
	ARM_FEATURE_MPU,    /* Only has Memory Protection Unit, not full MMU.  */
	ARM_FEATURE_VFP3,
	ARM_FEATURE_VFP_FP16,
	ARM_FEATURE_NEON,
	ARM_FEATURE_THUMB_DIV, /* divide supported in Thumb encoding */
	ARM_FEATURE_M, /* Microcontroller profile.  */
	ARM_FEATURE_OMAPCP, /* OMAP specific CP15 ops handling.  */
	ARM_FEATURE_THUMB2EE,
	ARM_FEATURE_V7MP,    /* v7 Multiprocessing Extensions */
	ARM_FEATURE_V8,
	ARM_FEATURE_V4T,
	ARM_FEATURE_V5,
	ARM_FEATURE_STRONGARM,
	ARM_FEATURE_VAPA, /* cp15 VA to PA lookups */
	ARM_FEATURE_ARM_DIV, /* divide supported in ARM encoding */
	ARM_FEATURE_VFP4, /* VFPv4 (implies that NEON is v2) */
	ARM_FEATURE_GENERIC_TIMER,
	ARM_FEATURE_MVFR, /* Media and VFP Feature Registers 0 and 1 */
	ARM_FEATURE_DUMMY_C15_REGS, /* RAZ/WI all of cp15 crn=15 */
	ARM_FEATURE_CACHE_TEST_CLEAN, /* 926/1026 style test-and-clean ops */
	ARM_FEATURE_CACHE_DIRTY_REG, /* 1136/1176 cache dirty status register */
	ARM_FEATURE_CACHE_BLOCK_OPS, /* v6 optional cache block operations */
	ARM_FEATURE_MPIDR, /* has cp15 MPIDR */
	ARM_FEATURE_PXN, /* has Privileged Execute Never bit */
	ARM_FEATURE_LPAE, /* has Large Physical Address Extension */
	ARM_FEATURE_TRUSTZONE, 
};

#endif

