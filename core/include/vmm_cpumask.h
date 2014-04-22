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
 * @file vmm_cpumask.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Interface for managing set of CPUs
 *
 * The header has been largely adapted from:
 * linux-xxx/include/linux/cpumask.h
 *
 * The original code is licensed under the GPL.
 */

#ifndef __VMM_CPUMASK_H__
#define __VMM_CPUMASK_H__

#include <vmm_types.h>
#include <libs/bitmap.h>

struct vmm_cpumask { 
	DECLARE_BITMAP(bits, CONFIG_CPU_COUNT); 
};

typedef struct vmm_cpumask vmm_cpumask_t;

/* Assuming CONFIG_CPU_COUNT is huge, a runtime limit is more efficient.  Also,
 * not all bits may be allocated. */
extern int vmm_cpu_count;

/**
 * vmm_cpumask_bits - get the bits in a cpumask
 * @maskp: the struct vmm_cpumask *
 *
 * You should only assume vmm_cpu_count bits of this mask are valid.  This is
 * a macro so it's const-correct.
 */
#define vmm_cpumask_bits(maskp) ((maskp)->bits)

/**
 * to_cpumask - convert an CONFIG_CPU_COUNT bitmap to a struct vmm_cpumask *
 * @bitmap: the bitmap
 *
 * This does the conversion, and can be used as a constant initializer.
 */
#define to_cpumask(bitmap)						\
	((struct vmm_cpumask *)(1 ? (bitmap)				\
			    : (void *)sizeof(__check_is_bitmap(bitmap))))

static inline int __check_is_bitmap(const unsigned long *bitmap)
{
	return 1;
}

/*
 * Special-case data structure for "single bit set only" constant CPU masks.
 *
 * We pre-generate all the 64 (or 32) possible bit positions, with enough
 * padding to the left and the right, and return the constant pointer
 * appropriately offset.
 */
extern const unsigned long
	cpu_bit_bitmap[BITS_PER_LONG+1][BITS_TO_LONGS(CONFIG_CPU_COUNT)];

static inline const struct vmm_cpumask *get_cpu_mask(unsigned int cpu)
{
	const unsigned long *p = cpu_bit_bitmap[1 + cpu % BITS_PER_LONG];
	p -= cpu / BITS_PER_LONG;
	return to_cpumask(p);
}

/**
 * vmm_cpumask_of - the cpumask containing just a given cpu
 * @cpu: the cpu (<= vmm_cpu_count)
 */
#define vmm_cpumask_of(cpu) (get_cpu_mask(cpu))

/**
 * vmm_cpumask_size - size to allocate for a 'struct vmm_cpumask' in bytes
 * This will eventually be a runtime variable, depending on vmm_cpu_count.
 */
static inline size_t vmm_cpumask_size(void)
{
	/* FIXME: Once all cpumask assignments are eliminated, this
	 * can be vmm_cpu_count */
	return BITS_TO_LONGS(CONFIG_CPU_COUNT) * sizeof(long);
}

#define VMM_CPU_MASK_LAST_WORD BITMAP_LAST_WORD_MASK(CONFIG_CPU_COUNT)

#if CONFIG_CPU_COUNT <= BITS_PER_LONG
#define VMM_CPU_BITS_ALL						\
{									\
	[BITS_TO_LONGS(CONFIG_CPU_COUNT)-1] = VMM_CPU_MASK_LAST_WORD	\
}

#else /* CONFIG_CPU_COUNT > BITS_PER_LONG */

#define VMM_CPU_BITS_ALL						\
{									\
	[0 ... BITS_TO_LONGS(CONFIG_CPU_COUNT)-2] = ~0UL,		\
	[BITS_TO_LONGS(CONFIG_CPU_COUNT)-1] = VMM_CPU_MASK_LAST_WORD	\
}
#endif /* CONFIG_CPU_COUNT > BITS_PER_LONG */

#define VMM_CPU_BITS_NONE						\
{									\
	[0 ... BITS_TO_LONGS(CONFIG_CPU_COUNT)-1] = 0UL			\
}

#define VMM_CPU_BITS_CPU0						\
{									\
	[0] =  1UL							\
}

#if CONFIG_CPU_COUNT <= BITS_PER_LONG

#define VMM_CPU_MASK_ALL						\
(vmm_cpumask_t) { {							\
	[BITS_TO_LONGS(CONFIG_CPU_COUNT)-1] = VMM_CPU_MASK_LAST_WORD	\
} }

#else

#define VMM_CPU_MASK_ALL						\
(vmm_cpumask_t) { {							\
	[0 ... BITS_TO_LONGS(CONFIG_CPU_COUNT)-2] = ~0UL,		\
	[BITS_TO_LONGS(CONFIG_CPU_COUNT)-1] = VMM_CPU_MASK_LAST_WORD	\
} }

#endif

#define VMM_CPU_MASK_NONE						\
(vmm_cpumask_t) { {							\
	[0 ... BITS_TO_LONGS(CONFIG_CPU_COUNT)-1] =  0UL		\
} }

#define VMM_CPU_MASK_CPU0						\
(vmm_cpumask_t) { {							\
	[0] =  1UL							\
} }

/*
 * The following particular system cpumasks and operations manage
 * possible, present, active and online cpus.
 *
 *     cpu_possible_mask- has bit 'cpu' set iff cpu is populatable
 *     cpu_present_mask - has bit 'cpu' set iff cpu is populated
 *     cpu_online_mask  - has bit 'cpu' set iff cpu available to scheduler
 *     cpu_active_mask  - has bit 'cpu' set iff cpu available to migration
 *
 *  If !CONFIG_HOTPLUG_CPU, present == possible, and active == online.
 *
 *  The cpu_possible_mask is fixed at boot time, as the set of CPU id's
 *  that it is possible might ever be plugged in at anytime during the
 *  life of that system boot.  The cpu_present_mask is dynamic(*),
 *  representing which CPUs are currently plugged in.  And
 *  cpu_online_mask is the dynamic subset of cpu_present_mask,
 *  indicating those CPUs available for scheduling.
 *
 *  If HOTPLUG is enabled, then cpu_possible_mask is forced to have
 *  all CONFIG_CPU_COUNT bits set, otherwise it is just the set of CPUs that
 *  ACPI reports present at boot.
 *
 *  If HOTPLUG is enabled, then cpu_present_mask varies dynamically,
 *  depending on what ACPI reports as currently plugged in, otherwise
 *  cpu_present_mask is just a copy of cpu_possible_mask.
 *
 *  (*) Well, cpu_present_mask is dynamic in the hotplug case.  If not
 *      hotplug, it's a copy of cpu_possible_mask, hence fixed at boot.
 *
 * Subtleties:
 * 1) UP arch's (CONFIG_CPU_COUNT == 1, CONFIG_SMP not defined) hardcode
 *    assumption that their single CPU is online.  The UP
 *    cpu_{online,possible,present}_masks are placebos.  Changing them
 *    will have no useful affect on the following vmm_num_*_cpus()
 *    and vmm_cpu_*() macros in the UP case.  This ugliness is a UP
 *    optimization - don't waste any instructions or memory references
 *    asking if you're online or how many CPUs there are if there is
 *    only one CPU.
 */

extern const struct vmm_cpumask *const cpu_possible_mask;
extern const struct vmm_cpumask *const cpu_online_mask;
extern const struct vmm_cpumask *const cpu_present_mask;
extern const struct vmm_cpumask *const cpu_active_mask;

#if CONFIG_CPU_COUNT > 1
#define vmm_num_online_cpus()	vmm_cpumask_weight(cpu_online_mask)
#define vmm_num_possible_cpus()	vmm_cpumask_weight(cpu_possible_mask)
#define vmm_num_present_cpus()	vmm_cpumask_weight(cpu_present_mask)
#define vmm_num_active_cpus()	vmm_cpumask_weight(cpu_active_mask)
#define vmm_cpu_online(cpu)	vmm_cpumask_test_cpu((cpu), cpu_online_mask)
#define vmm_cpu_possible(cpu)	vmm_cpumask_test_cpu((cpu), cpu_possible_mask)
#define vmm_cpu_present(cpu)	vmm_cpumask_test_cpu((cpu), cpu_present_mask)
#define vmm_cpu_active(cpu)	vmm_cpumask_test_cpu((cpu), cpu_active_mask)
#else
#define vmm_num_online_cpus()	1U
#define vmm_num_possible_cpus()	1U
#define vmm_num_present_cpus()	1U
#define vmm_num_active_cpus()	1U
#define vmm_cpu_online(cpu)	((cpu) == 0)
#define vmm_cpu_possible(cpu)	((cpu) == 0)
#define vmm_cpu_present(cpu)	((cpu) == 0)
#define vmm_cpu_active(cpu)	((cpu) == 0)
#endif

/* verify cpu argument to vmm_cpumask_* operators */
static inline unsigned int vmm_cpumask_check(unsigned int cpu)
{
#ifdef CONFIG_DEBUG_PER_CPU_MAPS
	WARN_ON_ONCE(cpu >= vmm_cpu_count);
#endif /* CONFIG_DEBUG_PER_CPU_MAPS */
	return cpu;
}

#if CONFIG_CPU_COUNT == 1
/* Uniprocessor.  Assume all masks are "1". */
static inline unsigned int vmm_cpumask_first(const struct vmm_cpumask *srcp)
{
	return 0;
}

/* Valid inputs for n are -1 and 0. */
static inline unsigned int vmm_cpumask_next(int n, const struct vmm_cpumask *srcp)
{
	return n+1;
}

static inline unsigned int vmm_cpumask_next_zero(int n, const struct vmm_cpumask *srcp)
{
	return n+1;
}

static inline unsigned int vmm_cpumask_next_and(int n,
					    const struct vmm_cpumask *srcp,
					    const struct vmm_cpumask *andp)
{
	return n+1;
}

/* cpu must be a valid cpu, ie 0, so there's no other choice. */
static inline unsigned int vmm_cpumask_any_but(const struct vmm_cpumask *mask,
						unsigned int cpu)
{
	return 1;
}

#define for_each_cpu(cpu, mask)			\
	for ((cpu) = 0; (cpu) < 1; (cpu)++, (void)mask)
#define for_each_cpu_not(cpu, mask)		\
	for ((cpu) = 0; (cpu) < 1; (cpu)++, (void)mask)
#define for_each_cpu_and(cpu, mask, and)	\
	for ((cpu) = 0; (cpu) < 1; (cpu)++, (void)mask, (void)and)
#else
/**
 * vmm_cpumask_first - get the first cpu in a cpumask
 * @srcp: the cpumask pointer
 *
 * Returns >= vmm_cpu_count if no cpus set.
 */
static inline unsigned int vmm_cpumask_first(const struct vmm_cpumask *srcp)
{
	return find_first_bit(vmm_cpumask_bits(srcp), vmm_cpu_count);
}

/**
 * vmm_cpumask_next - get the next cpu in a cpumask
 * @n: the cpu prior to the place to search (ie. return will be > @n)
 * @srcp: the cpumask pointer
 *
 * Returns >= vmm_cpu_count if no further cpus set.
 */
static inline unsigned int vmm_cpumask_next(int n, const struct vmm_cpumask *srcp)
{
	/* -1 is a legal arg here. */
	if (n != -1)
		vmm_cpumask_check(n);
	return find_next_bit(vmm_cpumask_bits(srcp), vmm_cpu_count, n+1);
}

/**
 * vmm_cpumask_next_zero - get the next unset cpu in a cpumask
 * @n: the cpu prior to the place to search (ie. return will be > @n)
 * @srcp: the cpumask pointer
 *
 * Returns >= vmm_cpu_count if no further cpus unset.
 */
static inline unsigned int vmm_cpumask_next_zero(int n, const struct vmm_cpumask *srcp)
{
	/* -1 is a legal arg here. */
	if (n != -1)
		vmm_cpumask_check(n);
	return find_next_zero_bit(vmm_cpumask_bits(srcp), vmm_cpu_count, n+1);
}

int vmm_cpumask_next_and(int n, const struct vmm_cpumask *, const struct vmm_cpumask *);
int vmm_cpumask_any_but(const struct vmm_cpumask *mask, unsigned int cpu);

/**
 * for_each_cpu - iterate over every cpu in a mask
 * @cpu: the (optionally unsigned) integer iterator
 * @mask: the cpumask pointer
 *
 * After the loop, cpu is >= vmm_cpu_count.
 */
#define for_each_cpu(cpu, mask)					\
	for ((cpu) = -1;					\
		(cpu) = vmm_cpumask_next((cpu), (mask)),	\
		(cpu) < vmm_cpu_count;)

/**
 * for_each_cpu_not - iterate over every cpu in a complemented mask
 * @cpu: the (optionally unsigned) integer iterator
 * @mask: the cpumask pointer
 *
 * After the loop, cpu is >= vmm_cpu_count.
 */
#define for_each_cpu_not(cpu, mask)				\
	for ((cpu) = -1;					\
		(cpu) = vmm_cpumask_next_zero((cpu), (mask)),	\
		(cpu) < vmm_cpu_count;)

/**
 * for_each_cpu_and - iterate over every cpu in both masks
 * @cpu: the (optionally unsigned) integer iterator
 * @mask: the first cpumask pointer
 * @and: the second cpumask pointer
 *
 * This saves a temporary CPU mask in many places.  It is equivalent to:
 *	struct vmm_cpumask tmp;
 *	vmm_cpumask_and(&tmp, &mask, &and);
 *	for_each_cpu(cpu, &tmp)
 *		...
 *
 * After the loop, cpu is >= vmm_cpu_count.
 */
#define for_each_cpu_and(cpu, mask, and)				\
	for ((cpu) = -1;						\
		(cpu) = vmm_cpumask_next_and((cpu), (mask), (and)),	\
		(cpu) < vmm_cpu_count;)
#endif /* SMP */

/**
 * vmm_cpumask_set_cpu - set a cpu in a cpumask
 * @cpu: cpu number (< vmm_cpu_count)
 * @dstp: the cpumask pointer
 */
static inline void vmm_cpumask_set_cpu(unsigned int cpu, struct vmm_cpumask *dstp)
{
	set_bit(vmm_cpumask_check(cpu), vmm_cpumask_bits(dstp));
}

/**
 * vmm_cpumask_clear_cpu - clear a cpu in a cpumask
 * @cpu: cpu number (< vmm_cpu_count)
 * @dstp: the cpumask pointer
 */
static inline void vmm_cpumask_clear_cpu(int cpu, struct vmm_cpumask *dstp)
{
	clear_bit(vmm_cpumask_check(cpu), vmm_cpumask_bits(dstp));
}

/**
 * vmm_cpumask_test_cpu - test for a cpu in a cpumask
 * @cpu: cpu number (< vmm_cpu_count)
 * @cpumask: the cpumask pointer
 *
 * No static inline type checking - see Subtlety (1) above.
 */
#define vmm_cpumask_test_cpu(cpu, cpumask) \
	test_bit(vmm_cpumask_check(cpu), vmm_cpumask_bits((cpumask)))

/**
 * vmm_cpumask_test_and_set_cpu - atomically test and set a cpu in a cpumask
 * @cpu: cpu number (< vmm_cpu_count)
 * @cpumask: the cpumask pointer
 *
 * test_and_set_bit wrapper for cpumasks.
 */
static inline int vmm_cpumask_test_and_set_cpu(int cpu, struct vmm_cpumask *cpumask)
{
	return test_and_set_bit(vmm_cpumask_check(cpu), vmm_cpumask_bits(cpumask));
}

/**
 * vmm_cpumask_test_and_clear_cpu - atomically test and clear a cpu in a cpumask
 * @cpu: cpu number (< vmm_cpu_count)
 * @cpumask: the cpumask pointer
 *
 * test_and_clear_bit wrapper for cpumasks.
 */
static inline int vmm_cpumask_test_and_clear_cpu(int cpu, struct vmm_cpumask *cpumask)
{
	return test_and_clear_bit(vmm_cpumask_check(cpu), vmm_cpumask_bits(cpumask));
}

/**
 * vmm_cpumask_setall - set all cpus (< vmm_cpu_count) in a cpumask
 * @dstp: the cpumask pointer
 */
static inline void vmm_cpumask_setall(struct vmm_cpumask *dstp)
{
	bitmap_fill(vmm_cpumask_bits(dstp), vmm_cpu_count);
}

/**
 * vmm_cpumask_clear - clear all cpus (< vmm_cpu_count) in a cpumask
 * @dstp: the cpumask pointer
 */
static inline void vmm_cpumask_clear(struct vmm_cpumask *dstp)
{
	bitmap_zero(vmm_cpumask_bits(dstp), vmm_cpu_count);
}

/**
 * vmm_cpumask_and - *dstp = *src1p & *src2p
 * @dstp: the cpumask result
 * @src1p: the first input
 * @src2p: the second input
 */
static inline int vmm_cpumask_and(struct vmm_cpumask *dstp,
			       const struct vmm_cpumask *src1p,
			       const struct vmm_cpumask *src2p)
{
	return bitmap_and(vmm_cpumask_bits(dstp), vmm_cpumask_bits(src1p),
				       vmm_cpumask_bits(src2p), vmm_cpu_count);
}

/**
 * cpumask_or - *dstp = *src1p | *src2p
 * @dstp: the cpumask result
 * @src1p: the first input
 * @src2p: the second input
 */
static inline void vmm_cpumask_or(struct vmm_cpumask *dstp, const struct vmm_cpumask *src1p,
			      const struct vmm_cpumask *src2p)
{
	bitmap_or(vmm_cpumask_bits(dstp), vmm_cpumask_bits(src1p),
				      vmm_cpumask_bits(src2p), vmm_cpu_count);
}

/**
 * vmm_cpumask_xor - *dstp = *src1p ^ *src2p
 * @dstp: the cpumask result
 * @src1p: the first input
 * @src2p: the second input
 */
static inline void vmm_cpumask_xor(struct vmm_cpumask *dstp,
			       const struct vmm_cpumask *src1p,
			       const struct vmm_cpumask *src2p)
{
	bitmap_xor(vmm_cpumask_bits(dstp), vmm_cpumask_bits(src1p),
				       vmm_cpumask_bits(src2p), vmm_cpu_count);
}

/**
 * vmm_cpumask_andnot - *dstp = *src1p & ~*src2p
 * @dstp: the cpumask result
 * @src1p: the first input
 * @src2p: the second input
 */
static inline int vmm_cpumask_andnot(struct vmm_cpumask *dstp,
				  const struct vmm_cpumask *src1p,
				  const struct vmm_cpumask *src2p)
{
	return bitmap_andnot(vmm_cpumask_bits(dstp), vmm_cpumask_bits(src1p),
					  vmm_cpumask_bits(src2p), vmm_cpu_count);
}

/**
 * vmm_cpumask_complement - *dstp = ~*srcp
 * @dstp: the cpumask result
 * @srcp: the input to invert
 */
static inline void vmm_cpumask_complement(struct vmm_cpumask *dstp,
				      const struct vmm_cpumask *srcp)
{
	bitmap_complement(vmm_cpumask_bits(dstp), vmm_cpumask_bits(srcp),
					      vmm_cpu_count);
}

/**
 * vmm_cpumask_equal - *src1p == *src2p
 * @src1p: the first input
 * @src2p: the second input
 */
static inline bool vmm_cpumask_equal(const struct vmm_cpumask *src1p,
				const struct vmm_cpumask *src2p)
{
	return bitmap_equal(vmm_cpumask_bits(src1p), vmm_cpumask_bits(src2p),
						 vmm_cpu_count);
}

/**
 * vmm_cpumask_intersects - (*src1p & *src2p) != 0
 * @src1p: the first input
 * @src2p: the second input
 */
static inline bool vmm_cpumask_intersects(const struct vmm_cpumask *src1p,
				     const struct vmm_cpumask *src2p)
{
	return bitmap_intersects(vmm_cpumask_bits(src1p), vmm_cpumask_bits(src2p),
						      vmm_cpu_count);
}

/**
 * vmm_cpumask_subset - (*src1p & ~*src2p) == 0
 * @src1p: the first input
 * @src2p: the second input
 */
static inline int vmm_cpumask_subset(const struct vmm_cpumask *src1p,
				 const struct vmm_cpumask *src2p)
{
	return bitmap_subset(vmm_cpumask_bits(src1p), vmm_cpumask_bits(src2p),
						  vmm_cpu_count);
}

/**
 * vmm_cpumask_empty - *srcp == 0
 * @srcp: the cpumask to that all cpus < vmm_cpu_count are clear.
 */
static inline bool vmm_cpumask_empty(const struct vmm_cpumask *srcp)
{
	return bitmap_empty(vmm_cpumask_bits(srcp), vmm_cpu_count);
}

/**
 * vmm_cpumask_full - *srcp == 0xFFFFFFFF...
 * @srcp: the cpumask to that all cpus < vmm_cpu_count are set.
 */
static inline bool vmm_cpumask_full(const struct vmm_cpumask *srcp)
{
	return bitmap_full(vmm_cpumask_bits(srcp), vmm_cpu_count);
}

/**
 * vmm_cpumask_weight - Count of bits in *srcp
 * @srcp: the cpumask to count bits (< vmm_cpu_count) in.
 */
static inline unsigned int vmm_cpumask_weight(const struct vmm_cpumask *srcp)
{
	return bitmap_weight(vmm_cpumask_bits(srcp), vmm_cpu_count);
}

/**
 * vmm_cpumask_shift_right - *dstp = *srcp >> n
 * @dstp: the cpumask result
 * @srcp: the input to shift
 * @n: the number of bits to shift by
 */
static inline void vmm_cpumask_shift_right(struct vmm_cpumask *dstp,
				       const struct vmm_cpumask *srcp, int n)
{
	bitmap_shift_right(vmm_cpumask_bits(dstp), vmm_cpumask_bits(srcp), n,
					       vmm_cpu_count);
}

/**
 * vmm_cpumask_shift_left - *dstp = *srcp << n
 * @dstp: the cpumask result
 * @srcp: the input to shift
 * @n: the number of bits to shift by
 */
static inline void vmm_cpumask_shift_left(struct vmm_cpumask *dstp,
				      const struct vmm_cpumask *srcp, int n)
{
	bitmap_shift_left(vmm_cpumask_bits(dstp), vmm_cpumask_bits(srcp), n,
					      vmm_cpu_count);
}

/**
 * vmm_cpumask_copy - *dstp = *srcp
 * @dstp: the result
 * @srcp: the input cpumask
 */
static inline void vmm_cpumask_copy(struct vmm_cpumask *dstp,
				const struct vmm_cpumask *srcp)
{
	bitmap_copy(vmm_cpumask_bits(dstp), vmm_cpumask_bits(srcp), vmm_cpu_count);
}

/**
 * vmm_cpumask_any - pick a "random" cpu from *srcp
 * @srcp: the input cpumask
 *
 * Returns >= vmm_cpu_count if no cpus set.
 */
#define vmm_cpumask_any(srcp) vmm_cpumask_first(srcp)

/**
 * vmm_cpumask_first_and - return the first cpu from *srcp1 & *srcp2
 * @src1p: the first input
 * @src2p: the second input
 *
 * Returns >= vmm_cpu_count if no cpus set in both.  See also vmm_cpumask_next_and().
 */
#define vmm_cpumask_first_and(src1p, src2p) vmm_cpumask_next_and(-1, (src1p), (src2p))

/**
 * vmm_cpumask_any_and - pick a "random" cpu from *mask1 & *mask2
 * @mask1: the first input cpumask
 * @mask2: the second input cpumask
 *
 * Returns >= vmm_cpu_count if no cpus set.
 */
#define vmm_cpumask_any_and(mask1, mask2) vmm_cpumask_first_and((mask1), (mask2))

/* It's common to want to use cpu_all_mask in struct member initializers,
 * so it has to refer to an address rather than a pointer. */
extern const DECLARE_BITMAP(cpu_all_bits, CONFIG_CPU_COUNT);
#define cpu_all_mask to_cpumask(cpu_all_bits)

/* First bits of cpu_bit_bitmap are in fact unset. */
#define cpu_none_mask to_cpumask(cpu_bit_bitmap[0])

#define for_each_possible_cpu(cpu) for_each_cpu((cpu), cpu_possible_mask)
#define for_each_online_cpu(cpu)   for_each_cpu((cpu), cpu_online_mask)
#define for_each_present_cpu(cpu)  for_each_cpu((cpu), cpu_present_mask)

/* Wrappers for arch boot code to manipulate normally-constant masks */
void vmm_set_cpu_possible(unsigned int cpu, bool possible);
void vmm_set_cpu_present(unsigned int cpu, bool present);
void vmm_set_cpu_online(unsigned int cpu, bool online);
void vmm_set_cpu_active(unsigned int cpu, bool active);
void vmm_init_cpu_present(const struct vmm_cpumask *src);
void vmm_init_cpu_possible(const struct vmm_cpumask *src);
void vmm_init_cpu_online(const struct vmm_cpumask *src);

#endif /* __VMM_CPUMASK_H__ */
