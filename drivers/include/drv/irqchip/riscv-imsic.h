/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2022 Anup Patel.
 */
#ifndef __RISCV_IMSIC_H__
#define __RISCV_IMSIC_H__

#include <linux/types.h>

#define IMSIC_MMIO_PAGE_SHIFT		12
#define IMSIC_MMIO_PAGE_SZ		(1UL << IMSIC_MMIO_PAGE_SHIFT)
#define IMSIC_MMIO_PAGE_LE		0x00
#define IMSIC_MMIO_PAGE_BE		0x04

#define IMSIC_MIN_ID			63
#define IMSIC_MAX_ID			2048

#define IMSIC_EIDELIVERY		0x70

#define IMSIC_EITHRESHOLD		0x72

#define IMSIC_EIP0			0x80
#define IMSIC_EIP63			0xbf
#define IMSIC_EIPx_BITS		32

#define IMSIC_EIE0			0xc0
#define IMSIC_EIE63			0xff
#define IMSIC_EIEx_BITS		32

#define IMSIC_FIRST			IMSIC_EIDELIVERY
#define IMSIC_LAST			IMSIC_EIE63

#define IMSIC_MMIO_SETIPNUM_LE		0x00
#define IMSIC_MMIO_SETIPNUM_BE		0x04

struct imsic_global_config {
	/*
	 * MSI Target Address Scheme
	 *
	 * XLEN-1                                                12     0
	 * |                                                     |     |
	 * -------------------------------------------------------------
	 * |xxxxxx|Group Index|xxxxxxxxxxx|HART Index|Guest Index|  0  |
	 * -------------------------------------------------------------
	 */

	/* Bits representing Guest index, HART index, and Group index */
	u32 guest_index_bits;
	u32 hart_index_bits;
	u32 group_index_bits;
	u32 group_index_shift;

	/* Global base address matching all target MSI addresses */
	physical_addr_t base_addr;

	/* Number of interrupt identities */
	u32 nr_ids;
};

struct imsic_local_config {
	physical_addr_t msi_pa;
	void *msi_va;
};

#ifdef CONFIG_RISCV_IMSIC

extern const struct imsic_global_config *imsic_get_global_config(void);

extern const struct imsic_local_config *imsic_get_local_config(
							unsigned int cpu);

#else

static inline const struct imsic_global_config *imsic_get_global_config(void)
{
	return NULL;
}

static inline const struct imsic_local_config *imsic_get_local_config(
							unsigned int cpu)
{
	return NULL;
}

#endif

#endif
