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
 * @file riscv_sbi.h
 * @author Anup Patel (anup.patel@wdc.com)
 * @brief Supervisor binary interface (SBI) defines
 */

#ifndef __RISCV_SBI_H__
#define __RISCV_SBI_H__

#include <vmm_const.h>

/* SBI Extension IDs */
#define	SBI_EXT_0_1_SET_TIMER			0x0
#define SBI_EXT_0_1_CONSOLE_PUTCHAR		0x1
#define SBI_EXT_0_1_CONSOLE_GETCHAR		0x2
#define SBI_EXT_0_1_CLEAR_IPI			0x3
#define SBI_EXT_0_1_SEND_IPI			0x4
#define SBI_EXT_0_1_REMOTE_FENCE_I		0x5
#define SBI_EXT_0_1_REMOTE_SFENCE_VMA		0x6
#define SBI_EXT_0_1_REMOTE_SFENCE_VMA_ASID	0x7
#define SBI_EXT_0_1_SHUTDOWN			0x8
#define SBI_EXT_BASE				0x10
#define SBI_EXT_TIME				0x54494D45
#define SBI_EXT_IPI				0x735049
#define SBI_EXT_RFENCE				0x52464E43

/* SBI function IDs for BASE extension */
#define SBI_EXT_BASE_GET_SPEC_VERSION		0x0
#define SBI_EXT_BASE_GET_IMP_ID			0x1
#define	SBI_EXT_BASE_GET_IMP_VERSION		0x2
#define	SBI_EXT_BASE_PROBE_EXT			0x3
#define	SBI_EXT_BASE_GET_MVENDORID		0x4
#define	SBI_EXT_BASE_GET_MARCHID		0x5
#define	SBI_EXT_BASE_GET_MIMPID			0x6

#define SBI_SPEC_VERSION_MAJOR_SHIFT		24
#define SBI_SPEC_VERSION_MAJOR_MASK		0x7f
#define SBI_SPEC_VERSION_MINOR_MASK		0xffffff

/* SBI function IDs for TIME extension */
#define SBI_EXT_TIME_SET_TIMER			0x0

/* SBI function IDs for IPI extension */
#define SBI_EXT_IPI_SEND_IPI			0x0

/* SBI function IDs for RFENCE extension */
#define	SBI_EXT_RFENCE_REMOTE_FENCE_I		0x0
#define SBI_EXT_RFENCE_REMOTE_SFENCE_VMA	0x1
#define SBI_EXT_RFENCE_REMOTE_SFENCE_VMA_ASID	0x2
#define SBI_EXT_RFENCE_REMOTE_HFENCE_GVMA	0x3
#define SBI_EXT_RFENCE_REMOTE_HFENCE_GVMA_VMID	0x4
#define SBI_EXT_RFENCE_REMOTE_HFENCE_VVMA	0x5
#define SBI_EXT_RFENCE_REMOTE_HFENCE_VVMA_ASID	0x6

#define SBI_EXT_VENDOR_START			0x09000000
#define SBI_EXT_VENDOR_END			0x09FFFFFF

/* SBI return error codes */
#define SBI_SUCCESS		0
#define SBI_ERR_FAILURE		-1
#define SBI_ERR_NOT_SUPPORTED	-2
#define SBI_ERR_INVALID_PARAM   -3
#define SBI_ERR_DENIED		-4
#define SBI_ERR_INVALID_ADDRESS -5

#endif
