/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file bitmap.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Abstraction and functions for bitmap handling.
 *
 * This header file is largely adapted from linux-xxx/include/linux/bitmap.h
 *
 * The original code is licensed under the GPL.
 */

#ifndef __BITMAP_H__
#define __BITMAP_H__

#include <vmm_types.h>
#include <vmm_string.h>
#include <vmm_macros.h>
#include <bitops.h>

/*
 * bitmaps provide bit arrays that consume one or more unsigned
 * longs.  The bitmap interface and available operations are listed
 * here, in bitmap.h
 *
 * Function implementations generic to all architectures are in
 * libs/common/bitmap.c. 
 */

/*
 * The available bitmap operations and their rough meaning in the
 * case that the bitmap is a single unsigned long are thus:
 *
 * Note that nbits should be always a compile time evaluable constant.
 * Otherwise many inlines will generate horrible code.
 *
 * bitmap_zero(dst, nbits)			*dst = 0UL
 * bitmap_fill(dst, nbits)			*dst = ~0UL
 * bitmap_copy(dst, src, nbits)			*dst = *src
 * bitmap_and(dst, src1, src2, nbits)		*dst = *src1 & *src2
 * bitmap_or(dst, src1, src2, nbits)		*dst = *src1 | *src2
 * bitmap_xor(dst, src1, src2, nbits)		*dst = *src1 ^ *src2
 * bitmap_andnot(dst, src1, src2, nbits)	*dst = *src1 & ~(*src2)
 * bitmap_complement(dst, src, nbits)		*dst = ~(*src)
 * bitmap_equal(src1, src2, nbits)		Are *src1 and *src2 equal?
 * bitmap_intersects(src1, src2, nbits) 	Do *src1 and *src2 overlap?
 * bitmap_subset(src1, src2, nbits)		Is *src1 a subset of *src2?
 * bitmap_empty(src, nbits)			Are all bits zero in *src?
 * bitmap_full(src, nbits)			Are all bits set in *src?
 * bitmap_weight(src, nbits)			Hamming Weight: number set bits
 * bitmap_set(dst, pos, nbits)			Set specified bit area
 * bitmap_clear(dst, pos, nbits)		Clear specified bit area
 * bitmap_shift_right(dst, src, n, nbits)	*dst = *src >> n
 * bitmap_shift_left(dst, src, n, nbits)	*dst = *src << n
 * bitmap_find_free_region(bitmap, bits, order)	Find and allocate bit region
 * bitmap_release_region(bitmap, pos, order)	Free specified bit region
 * bitmap_allocate_region(bitmap, pos, order)	Allocate specified bit region
 */

#define BITMAP_LAST_WORD_MASK(nbits)					\
(									\
	((nbits) % BITS_PER_LONG) ?					\
		(1UL<<((nbits) % BITS_PER_LONG))-1 : ~0UL		\
)

#define small_const_nbits(nbits) \
	(__builtin_constant_p(nbits) && (nbits) <= BITS_PER_LONG)

#define DECLARE_BITMAP(name, nbits)	unsigned long name[BITS_TO_LONGS(nbits)]
#define DEFINE_BITMAP(name)		extern unsigned long name[]

static inline unsigned long bitmap_estimate_size(int nbits)
{
	return (BITS_TO_LONGS(nbits) * sizeof(unsigned long));
}

/*
 * bitmap.c provides these functions:
 */
extern int __bitmap_empty(const unsigned long *bitmap, int bits);
extern int __bitmap_full(const unsigned long *bitmap, int bits);
extern int __bitmap_equal(const unsigned long *bitmap1,
                	const unsigned long *bitmap2, int bits);
extern void __bitmap_complement(unsigned long *dst, const unsigned long *src,
			int bits);
extern void __bitmap_shift_right(unsigned long *dst,
                        const unsigned long *src, int shift, int bits);
extern void __bitmap_shift_left(unsigned long *dst,
                        const unsigned long *src, int shift, int bits);
extern int __bitmap_and(unsigned long *dst, const unsigned long *bitmap1,
			const unsigned long *bitmap2, int bits);
extern void __bitmap_or(unsigned long *dst, const unsigned long *bitmap1,
			const unsigned long *bitmap2, int bits);
extern void __bitmap_xor(unsigned long *dst, const unsigned long *bitmap1,
			const unsigned long *bitmap2, int bits);
extern int __bitmap_andnot(unsigned long *dst, const unsigned long *bitmap1,
			const unsigned long *bitmap2, int bits);
extern int __bitmap_intersects(const unsigned long *bitmap1,
			const unsigned long *bitmap2, int bits);
extern int __bitmap_subset(const unsigned long *bitmap1,
			const unsigned long *bitmap2, int bits);
extern u32 __bitmap_sw_hweight32(u32 w);
extern int __bitmap_weight(const unsigned long *bitmap, int bits);
extern int bitmap_find_free_region(unsigned long *bitmap, int bits, int order);
extern void bitmap_release_region(unsigned long *bitmap, int pos, int order);
extern int bitmap_allocate_region(unsigned long *bitmap, int pos, int order);

/*
 * inline functions
 */
static inline bool bitmap_isset(unsigned long *bmap, int bit)
{
	return (bmap[BIT_WORD(bit)] & (0x1 << BIT_WORD_OFFSET(bit))) ? 
								TRUE : FALSE;
}

static inline void bitmap_setbit(unsigned long *bmap, unsigned long bit)
{
	bmap[BIT_WORD(bit)] |= (0x1 << BIT_WORD_OFFSET(bit));
}

static inline void bitmap_clearbit(unsigned long *bmap, unsigned long bit)
{
	bmap[BIT_WORD(bit)] &= ~(0x1 << BIT_WORD_OFFSET(bit));
}

static inline void bitmap_set(unsigned long *bmap, int start, int len)
{
	int bit;
	for (bit = start; bit < (start + len); bit++) {
		bmap[BIT_WORD(bit)] |= (0x1 << BIT_WORD_OFFSET(bit));
	}
}

static inline void bitmap_clear(unsigned long *bmap, int start, int len)
{
	int bit;
	for (bit = start; bit < (start + len); bit++) {
		bmap[BIT_WORD(bit)] &= ~(0x1 << BIT_WORD_OFFSET(bit));
	}
}

static inline void bitmap_zero(unsigned long *dst, int nbits)
{
	if (small_const_nbits(nbits))
		*dst = 0UL;
	else {
		int len = BIT_WORD(nbits) * sizeof(unsigned long);
		vmm_memset(dst, 0, len);
	}
}

static inline void bitmap_fill(unsigned long *dst, int nbits)
{
	size_t nlongs = BIT_WORD(nbits);
	if (!small_const_nbits(nbits)) {
		int len = (nlongs - 1) * sizeof(unsigned long);
		vmm_memset(dst, 0xff,  len);
	}
	dst[nlongs - 1] = BITMAP_LAST_WORD_MASK(nbits);
}

static inline void bitmap_copy(unsigned long *dst, const unsigned long *src, int nbits)
{
	if (small_const_nbits(nbits))
		*dst = *src;
	else {
		int len = BIT_WORD(nbits) * sizeof(unsigned long);
		vmm_memcpy(dst, src, len);
	}
}

static inline int bitmap_and(unsigned long *dst, const unsigned long *src1,
			const unsigned long *src2, int nbits)
{
	if (small_const_nbits(nbits))
		return (*dst = *src1 & *src2) != 0;
	return __bitmap_and(dst, src1, src2, nbits);
}

static inline void bitmap_or(unsigned long *dst, const unsigned long *src1,
			const unsigned long *src2, int nbits)
{
	if (small_const_nbits(nbits))
		*dst = *src1 | *src2;
	else
		__bitmap_or(dst, src1, src2, nbits);
}

static inline void bitmap_xor(unsigned long *dst, const unsigned long *src1,
			const unsigned long *src2, int nbits)
{
	if (small_const_nbits(nbits))
		*dst = *src1 ^ *src2;
	else
		__bitmap_xor(dst, src1, src2, nbits);
}

static inline int bitmap_andnot(unsigned long *dst, const unsigned long *src1,
			const unsigned long *src2, int nbits)
{
	if (small_const_nbits(nbits))
		return (*dst = *src1 & ~(*src2)) != 0;
	return __bitmap_andnot(dst, src1, src2, nbits);
}

static inline void bitmap_complement(unsigned long *dst, const unsigned long *src,
			int nbits)
{
	if (small_const_nbits(nbits))
		*dst = ~(*src) & BITMAP_LAST_WORD_MASK(nbits);
	else
		__bitmap_complement(dst, src, nbits);
}

static inline int bitmap_equal(const unsigned long *src1,
			const unsigned long *src2, int nbits)
{
	if (small_const_nbits(nbits))
		return ! ((*src1 ^ *src2) & BITMAP_LAST_WORD_MASK(nbits));
	else
		return __bitmap_equal(src1, src2, nbits);
}

static inline int bitmap_intersects(const unsigned long *src1,
			const unsigned long *src2, int nbits)
{
	if (small_const_nbits(nbits))
		return ((*src1 & *src2) & BITMAP_LAST_WORD_MASK(nbits)) != 0;
	else
		return __bitmap_intersects(src1, src2, nbits);
}

static inline int bitmap_subset(const unsigned long *src1,
			const unsigned long *src2, int nbits)
{
	if (small_const_nbits(nbits))
		return ! ((*src1 & ~(*src2)) & BITMAP_LAST_WORD_MASK(nbits));
	else
		return __bitmap_subset(src1, src2, nbits);
}

static inline int bitmap_empty(const unsigned long *src, int nbits)
{
	if (small_const_nbits(nbits))
		return ! (*src & BITMAP_LAST_WORD_MASK(nbits));
	else
		return __bitmap_empty(src, nbits);
}

static inline int bitmap_full(const unsigned long *src, int nbits)
{
	if (small_const_nbits(nbits))
		return ! (~(*src) & BITMAP_LAST_WORD_MASK(nbits));
	else
		return __bitmap_full(src, nbits);
}

static inline int bitmap_weight(const unsigned long *src, int nbits)
{
	if (small_const_nbits(nbits))
		return __bitmap_sw_hweight32(*src & BITMAP_LAST_WORD_MASK(nbits));
	return __bitmap_weight(src, nbits);
}

static inline void bitmap_shift_right(unsigned long *dst,
			const unsigned long *src, int n, int nbits)
{
	if (small_const_nbits(nbits))
		*dst = *src >> n;
	else
		__bitmap_shift_right(dst, src, n, nbits);
}

static inline void bitmap_shift_left(unsigned long *dst,
			const unsigned long *src, int n, int nbits)
{
	if (small_const_nbits(nbits))
		*dst = (*src << n) & BITMAP_LAST_WORD_MASK(nbits);
	else
		__bitmap_shift_left(dst, src, n, nbits);
}

#endif /* __BITMAP_H__ */
