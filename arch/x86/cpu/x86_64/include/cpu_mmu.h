/*
 * Copyright (c) 2010-2020 Himanshu Chauhan.
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
 * @author: Himanshu Chauhan <hschauhan@nulltrace.org>
 * @brief: MMU related definition and structures.
 */

#ifndef __CPU_MMU_H_
#define __CPU_MMU_H_

/*
 * Bit width and mask for 4 tree levels used in
 * virtual address mapping. 4 tree levels, 9 bit
 * each cover 36-bit of virtual address and reset
 * of the lower 12-bits out of total 48 bits are
 * used as is for page offset.
 *
 *   63-48   47-39  38-30  29-21  20-12   11-0
 * +---------------------------------------------+
 * | UNSED | PML4 | PGDP | PGDI | PGTI | PG OFSET|
 * +---------------------------------------------+
 */
#define PGTREE_BIT_WIDTH	9
#define PGTREE_MASK		~((0x01UL << PGTREE_BIT_WIDTH) - 1)

#define PML4_SHIFT		39
#define PML4_MASK		(PGTREE_MASK << PML4_SHIFT)

#define PGDP_SHIFT		30
#define PGDP_MASK		(PGTREE_MASK << PGDP_SHIFT)

#define PGDI_SHIFT		21
#define PGDI_MASK		(PGTREE_MASK << PGDI_SHIFT)

#define PGTI_SHIFT		12
#define PGTI_MASK		(PGTREE_MASK << PGTI_SHIFT)

#define PAGE_SHIFT		12
#define PAGE_SIZE		(0x01 << PAGE_SHIFT)
#define PAGE_MASK		~(PAGE_SIZE - 1)

#if !defined (__ASSEMBLY__)
#define VIRT_TO_PML4(__virt)	(((u64)__virt & ~(PML4_MASK)) >> PML4_SHIFT)
#define VIRT_TO_PGDP(__virt)	(((u64)__virt & ~(PGDP_MASK)) >> PGDP_SHIFT)
#define VIRT_TO_PGDI(__virt)	(((u64)__virt & ~(PGDI_MASK)) >> PGDI_SHIFT)
#define VIRT_TO_PGTI(__virt)	(((u64)__virt & ~(PGTI_MASK)) >> PGTI_SHIFT)
#define VIRT_TO_PGOFF(__virt)	(((u64)__virt & ~(PAGE_MASK)) >> PAGE_SHIFT)
#else
.macro VIRT_TO_PML4 __virt
	and $(~(PML4_MASK)), \__virt
        shr $PML4_SHIFT, \_virt
.endm

.macro VIRT_TO_PGDP __virt
	and $(~(PGDP_MASK)), \__virt
        shr $PGDP_SHIFT, \_virt
.endm

.macro VIRT_TO_PGDI __virt
	and $(~(PGDI_MASK)), \__virt
        shr $PGDI_SHIFT, \_virt
.endm

.macro VIRT_TO_PGTI __virt
	and $(~(PGTI_MASK)), \__virt
        shr $PGTI_SHIFT, \_virt
.endm
#endif

#endif /* __CPU_MMU_H_ */
