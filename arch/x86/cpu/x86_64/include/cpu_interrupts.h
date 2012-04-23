/**
 * Copyright (c) 2010-20 Himanshu Chauhan.
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
 * @file cpu_interrupts.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief header file for cpu interrupts
 */
#ifndef _CPU_INTERRUPTS_H__
#define _CPU_INTERRUPTS_H__

#include <vmm_types.h>
#include <arch_regs.h>

/* Interrupt Descriptor Table */

/* Segment Selector and Offset (SSO) */
union _sso {
	u32 val;
	struct {
		u32 offset:16;
		u32 selector:16;
	} bits;
} __packed;


/* offset and type */
union _ot {
	u32 val;
	struct {
		u32 ist:3;
		u32 rz:5;
		u32 type:4;
		u32 z:1; /* should be zero */
		u32 dpl:2;
		u32 present:1;
		u32 offset:16;
	} bits;
} __packed;

/* offset 32-63 bits */
union _off {
	u32 va;
	struct {
		u32 offset;
	} bits;
} __packed;

/* Trap and Interrupt Gate */
struct gate_descriptor {
	union _sso sso;
	union _ot ot;
	union _off off;
	u32 reserved;
} __packed;

struct idt64_ptr {
	u16 idt_limit;
	u64 idt_base;
} __packed;

/*
 * These values are just flag bits not the actual value of
 * type to be written to register.
 */
#define IDT_GATE_TYPE_INTERRUPT		(0x01UL << 0)
#define IDT_GATE_TYPE_TRAP		(0x01UL << 1)
#define IDT_GATE_TYPE_CALL		(0x01UL << 2)

/* IA-32e mode types */
#define _GATE_TYPE_LDT			0x2
#define _GATE_TYPE_TSS_AVAILABLE	0x9
#define _GATE_TYPE_TSS_BUSY		0xB
#define _GATE_TYPE_CALL			0xC
#define _GATE_TYPE_INTERRUPT		0xE
#define _GATE_TYPE_TRAP			0xF

/* Task state segment:
 * We need one because, x86 requires at least one TSS be present
 * and we want to use interrupt stack table. In IA32e mode task
 * switching isn't supported by the processor. Instead Intel chose
 * to reuse the TSS as IST in 64-bit mode. Another hack?
 */
struct tss_64 {
	u32 resvd_0;
	u32 rsp0_lo;
	u32 rsp0_hi;
	u32 rsp1_lo;
	u32 rsp1_hi;
	u32 rsp2_lo;
	u32 rsp2_hi;
	u32 resvd_1;
	u32 resvd_2;
	u32 ist1_lo;
	u32 ist1_hi;
	u32 ist2_lo;
	u32 ist2_hi;
	u32 ist3_lo;
	u32 ist3_hi;
	u32 ist4_lo;
	u32 ist4_hi;
	u32 ist5_lo;
	u32 ist5_hi;
	u32 ist6_lo;
	u32 ist6_hi;
	u32 ist7_lo;
	u32 ist7_hi;
	u32 resvd_3;
	u32 resvd_4;
	u32 map_base;
} __packed;

union tss_desc_base_limit {
	u32 val;
	struct {
		u32 tss_base1:16;
		u32 tss_limit:16;
	} bits;
} __packed;

union tss_desc_base_type {
	u32 val;
	struct {
		u32 tss_base2:8;
		u32 type:4;
		u32 resvd1:1;
		u32 dpl:2;
		u32 present:1;
		u32 limit:4;
		u32 avl:1;
		u32 resvd0:2;
		u32 granularity:1;
		u32 tss_base3:8;
	} bits;
} __packed;

union tss_desc_base {
	u32 val;
	struct {
		u32 tss_base4;
	} bits;
} __packed;

struct tss64_desc {
	union tss_desc_base_limit tbl;
	union tss_desc_base_type tbt;
	union tss_desc_base tb;
	u32 reserved;
} __packed;

#endif /* _CPU_INTERRYPTS_H__ */
