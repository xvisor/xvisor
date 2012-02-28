/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file vmm_config.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief board specific configuration of VMM
 */
#ifndef _VMM_CONFIG_H__
#define _VMM_CONFIG_H__

/** CPU frequency in MHz */
#define VMM_CPU_FREQ_MHZ				100

/** Delay of VMM ticks in microseconds */
#define VMM_CPU_TICK_DELAY_MICROSECS 			1000

/** Counter Jiffies */
#define VMM_COUNTER_JIFFIES				(VMM_CPU_FREQ_MHZ * VMM_CPU_TICK_DELAY_MICROSECS)

/** Total VMM Guests */
#define VMM_GUEST_COUNT					2

/** Maximum number of VCPUs per Guest */
#define VMM_GUEST_MAX_VCPU				1

/** Total VMM VCPUs */
#define VMM_VCPU_COUNT					2

/** Starting program counter for each VCPU */
#define VMM_VCPU_START_PC				0xFFFFFFFC

/** Maximum number of scheduler ticks per VCPU */
#define VMM_VCPU_MAX_TICK				20

/** Desired Timer frequency for each VCPU in MHz */
#define VMM_VCPU_TIMER_FREQ_MHZ				10

/** Number of interrupts for each VCPU */
#define VMM_VCPU_INTERRUPT_COUNT			8

/** Priority levels for each VCPU */
#define VMM_VCPU_INTERRUPT_PRIORITIES			{ 0, 1, 2, 2, \
							2, 2, 2, 2, \
							2, 2, 2, 2, \
							0, 2, 2, 2 }

/**
 * Intialization info specific to each guest.
 * 							<BOOT ADDR>,   <SIZE>
 * 							(64-bit),    (64-bit)
 */
#define VMM_GUEST_INITINFO				{ { 0xFFFFFFFFCULL }, \
							{ 0xFFFFFFFFCULL } }

/**
 * Intialization info specific to each vcpu and vcpu-to-guest mappings
 * 							<GUEST>,
 * 							[0-255],
 */
#define VMM_VCPU_INITINFO				{ { 0 } , \
							  { 1 } }

/** Maximum number of regions per addrespace */
#define VMM_ADDRSPACE_MAX_REGION			16

/** Address space region count */
#define VMM_ADDRSPACE_REGION_INITINFO_COUNT		6

/**
 * Intialization info specific to each address space regions.
 * 							<GUEST_NUM>, <GUEST PHYS_ADDR>,  <HOST ACTU_ADDR>,     <SIZE>
 * 							[0-255],    (64-bit), 	      (64-bit),           (64-bit)
 */
#define VMM_ADDRSPACE_REGION_INITINFO			{ { 0,       0x000000000ULL,   	  0x001000000ULL,   	0x06000000 }, \
							  { 0,       0xFFF000000ULL,   	  0x007000000ULL,   	0x01000000 }, \
							  { 0,       0x4ef600000ULL,   	  0x4ef600000ULL,   	0x00000500 }, \
							  { 1,       0x000000000ULL,   	  0x008000000ULL,   	0x06000000 }, \
							  { 1,       0xFFF000000ULL,   	  0x00E000000ULL,   	0x01000000 }, \
							  { 1,       0x4ef600000ULL,   	  0x4ef600400ULL,   	0x00000500 } }
#endif
