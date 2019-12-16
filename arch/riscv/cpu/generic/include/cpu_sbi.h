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

void sbi_console_putchar(int ch);

int sbi_console_getchar(void);

void sbi_set_timer(u64 stime_value);

void sbi_shutdown(void);

void sbi_clear_ipi(void);

void sbi_send_ipi(const unsigned long *hart_mask);

void sbi_remote_fence_i(const unsigned long *hart_mask);

void sbi_remote_sfence_vma(const unsigned long *hart_mask,
			   unsigned long start,
			   unsigned long size);

void sbi_remote_sfence_vma_asid(const unsigned long *hart_mask,
				unsigned long start,
				unsigned long size,
				unsigned long asid);

#endif
