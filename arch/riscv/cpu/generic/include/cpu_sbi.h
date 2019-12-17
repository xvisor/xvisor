/**
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
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
 * @file cpu_sbi.h
 * @author Anup Patel (anup.patel@wdc.com)
 * @brief Supervisor binary interface (SBI) helper functions header
 */

#ifndef __CPU_SBI_H__
#define __CPU_SBI_H__

#include <vmm_types.h>

#define SBI_SPEC_VERSION_DEFAULT	0x1

struct vmm_cpumask;

struct sbiret {
	long error;
	long value;
};

struct sbiret sbi_ecall(int ext, int fid, unsigned long arg0,
			unsigned long arg1, unsigned long arg2,
			unsigned long arg3, unsigned long arg4,
			unsigned long arg5);

/**
 * Convert SBI spec error code into Xvisor error code
 *
 * @return VMM_Exxx error code OR VMM_OK
 */
int sbi_err_map_xvisor_errno(int err);

/**
 * Convert logical CPU mask to hardware HART mask
 *
 * @param cmask input logical CPU mask
 * @param hmask output hardware HART mask
 */
void sbi_cpumask_to_hartmask(const struct vmm_cpumask *cmask,
			     struct vmm_cpumask *hmask);

/**
 * Writes given character to the console device.
 *
 * @param ch The data to be written to the console.
 */
void sbi_console_putchar(int ch);

/**
 * Reads a character from console device.
 *
 * @return the character read from console
 */
int sbi_console_getchar(void);

/**
 * Remove all the harts from executing supervisor code.
 */
void sbi_shutdown(void);

/**
 * Clear any pending IPIs for the calling HART.
 */
void sbi_clear_ipi(void);

/**
 * Send IPIs to a set of target HARTs.
 *
 * @param hart_mask mask representing set of target HARTs
 */
void sbi_send_ipi(const unsigned long *hart_mask);

/**
 * Program the timer for next timer event.
 *
 * @param stime_value Timer value after which next timer event should fire.
 */
void sbi_set_timer(u64 stime_value);

/**
 * Send FENCE_I to a set of target HARTs.
 *
 * @param hart_mask mask representing set of target HARTs
 */
void sbi_remote_fence_i(const unsigned long *hart_mask);

/**
 * Send SFENCE_VMA to a set of target HARTs.
 *
 * @param hart_mask mask representing set of target HARTs
 * @param start virtual address start
 * @param size virtual address size
 */
void sbi_remote_sfence_vma(const unsigned long *hart_mask,
			   unsigned long start,
			   unsigned long size);

/**
 * Send SFENCE_VMA_ASID to a set of target HARTs.
 *
 * @param hart_mask mask representing set of target HARTs
 * @param start virtual address start
 * @param size virtual address size
 * @param asid address space ID
 */
void sbi_remote_sfence_vma_asid(const unsigned long *hart_mask,
				unsigned long start,
				unsigned long size,
				unsigned long asid);

/**
 * Send HFENCE_GVMA to a set of target HARTs.
 *
 * @param hart_mask mask representing set of target HARTs
 * @param start guest physical address start
 * @param size guest physical address size
 */
void sbi_remote_hfence_gvma(const unsigned long *hart_mask,
			    unsigned long start,
			    unsigned long size);

/**
 * Send HFENCE_GVMA_VMID to a set of target HARTs.
 *
 * @param hart_mask mask representing set of target HARTs
 * @param start guest physical address start
 * @param size guest physical address size
 * @param vmid virtual machine ID
 */
void sbi_remote_hfence_gvma_vmid(const unsigned long *hart_mask,
				 unsigned long start,
				 unsigned long size,
				 unsigned long vmid);

/**
 * Send HFENCE_VVMA to a set of target HARTs.
 *
 * @param hart_mask mask representing set of target HARTs
 * @param start virtual address start
 * @param size virtual address size
 */
void sbi_remote_hfence_vvma(const unsigned long *hart_mask,
			    unsigned long start,
			    unsigned long size);

/**
 * Send HFENCE_VVMA_ASID to a set of target HARTs.
 *
 * @param hart_mask mask representing set of target HARTs
 * @param start virtual address start
 * @param size virtual address size
 * @param asid address space ID
 */
void sbi_remote_hfence_vvma_asid(const unsigned long *hart_mask,
				 unsigned long start,
				 unsigned long size,
				 unsigned long asid);

/**
 * Check given SBI extension is supported or not.
 *
 * @param ext extension ID
 * @return >= 0 for supported AND VMM_ENOTSUPP for not-supported
 */
int sbi_probe_extension(long ext);

/**
 * Check underlying SBI implementation is v0.1 only
 *
 * @return 1 for SBI v0.1 AND 0 for higer version
 */
int sbi_spec_is_0_1(void);

/**
 * Check underlying SBI implementation has v0.2 RFENCE
 *
 * @return 1 for supported AND 0 for not-supported
 */
int sbi_has_0_2_rfence(void);

/**
 * Get SBI spec major version
 *
 * @return major version number
 */
unsigned long sbi_major_version(void);

/**
 * Get SBI spec minor version
 *
 * @return minor version number
 */
unsigned long sbi_minor_version(void);

/**
 * Initialize SBI library
 *
 * @return VMM_OK on success AND VMM_Exxx error code on failure
 */
int sbi_init(void);

#endif
