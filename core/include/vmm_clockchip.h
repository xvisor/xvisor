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
 * @file vmm_clockchip.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Interface for clockchip managment
 */
#ifndef _VMM_CLOCKCHIP_H__
#define _VMM_CLOCKCHIP_H__

#include <vmm_types.h>
#include <vmm_cpumask.h>
#include <vmm_devtree.h>
#include <libs/mathlib.h>
#include <libs/list.h>

/* Clockchip mode commands */
enum vmm_clockchip_mode {
	VMM_CLOCKCHIP_MODE_UNUSED = 0,
	VMM_CLOCKCHIP_MODE_SHUTDOWN,
	VMM_CLOCKCHIP_MODE_PERIODIC,
	VMM_CLOCKCHIP_MODE_ONESHOT,
	VMM_CLOCKCHIP_MODE_RESUME,
};

/* Clockchip features */
#define VMM_CLOCKCHIP_FEAT_PERIODIC	0x000001
#define VMM_CLOCKCHIP_FEAT_ONESHOT	0x000002

struct vmm_clockchip;

/**
 * Hardware abstraction a clockchip device
 *
 * @head:		List head for registration
 * @name:		ptr to clockchip name
 * @hirq:		host irq number
 * @rating:		variable to rate clock event devices
 * @cpumask:		cpumask to indicate for which CPUs this device works
 * @features:		features
 * @mult:		nanosecond to cycles multiplier
 * @shift:		nanoseconds to cycles divisor (power of two)
 * @max_delta_ns:	maximum delta value in ns
 * @min_delta_ns:	minimum delta value in ns
 * @event_handler:	Assigned by the framework to be called by the low
 *			level handler of the event source
 * @set_mode:		set mode function
 * @set_next_event:	set next event function
 * @mode:		operating mode assigned by the management code
 * @bound_on:		Bound on host CPU
 * @next_event:		local storage for the next event in oneshot mode
 */
struct vmm_clockchip {
	struct dlist head;
	const char *name;
	u32 hirq;
	int rating;
	const struct vmm_cpumask *cpumask;
	unsigned int features;
	u32 mult;
	u32 shift;
	u64 max_delta_ns;
	u64 min_delta_ns;
	void (*event_handler) (struct vmm_clockchip *cc);
	void (*set_mode) (enum vmm_clockchip_mode mode, struct vmm_clockchip *cc);
	int (*set_next_event) (unsigned long evt, struct vmm_clockchip *cc);
	enum vmm_clockchip_mode mode;
	u32 bound_on;
	u64 next_event;
	void *priv;
};

#define VMM_NSEC_PER_SEC	1000000000UL

/* nodeid table based clockchip initialization callback */
typedef int (*vmm_clockchip_init_t)(struct vmm_devtree_node *);

/* declare nodeid table based initialization for clocksource */
#define VMM_CLOCKCHIP_INIT_DECLARE(name, compat, fn)	\
VMM_DEVTREE_NIDTBL_ENTRY(name, "clockchip", "", "", compat, fn)

/**
 * clocks_calc_mult_shift - calculate mult/shift factors for scaled math of clocks
 * @mult:       pointer to mult variable
 * @shift:      pointer to shift variable
 * @from:       frequency to convert from
 * @to:         frequency to convert to
 * @maxsec:     guaranteed runtime conversion range in seconds
 *
 * The function evaluates the shift/mult pair for the scaled math
 * operations of clocksources and clockevents.
 *
 * @to and @from are frequency values in HZ. For clock sources @to is
 * VMM_NSEC_PER_SEC == 1GHz and @from is the counter frequency. For clock
 * event @to is the counter frequency and @from is VMM_NSEC_PER_SEC.
 *
 * The @maxsec conversion range argument controls the time frame in
 * seconds which must be covered by the runtime conversion with the
 * calculated mult and shift factors. This guarantees that no 64bit
 * overflow happens when the input value of the conversion is
 * multiplied with the calculated mult factor. Larger ranges may
 * reduce the conversion accuracy by chosing smaller mult and shift
 * factors.
 */
static inline void vmm_clocks_calc_mult_shift(u32 *mult, u32 *shift,
					      u32 from, u32 to, u32 maxsec)
{
        u64 tmp;
        u32 sft, sftacc= 32;

        /* 
	 * Calculate the shift factor which is limiting the conversion
         * range:
         */
        tmp = ((u64)maxsec * from) >> 32;
        while (tmp) {
                tmp >>=1;
                sftacc--;
        }

        /*
         * Find the conversion shift/mult pair which has the best
         * accuracy and fits the maxsec conversion range:
         */
        for (sft = 32; sft > 0; sft--) {
                tmp = (u64) to << sft;
                tmp += from / 2;
                tmp = udiv64(tmp, from);
                if ((tmp >> sftacc) == 0)
                        break;
        }
        *mult = tmp;
        *shift = sft;
}

/** Convert kHz clockchip to clockchip mult */
static inline u32 vmm_clockchip_khz2mult(u32 khz, u32 shift)
{
	u64 tmp = ((u64)khz) << shift;
	tmp = udiv64(tmp, (u64)1000000);
	return (u32)tmp;
}

/** Convert Hz clockchip to clockchip mult */
static inline u32 vmm_clockchip_hz2mult(u32 hz, u32 shift)
{
	u64 tmp = ((u64)hz) << shift;
	tmp = udiv64(tmp, (u64)1000000000);
	return (u32)tmp;
}

/** Convert tick delta to nanoseconds */
static inline u64 vmm_clockchip_delta2ns(u64 delta, struct vmm_clockchip *cc)
{
	u64 tmp = (u64)delta << cc->shift;
	return udiv64(tmp, cc->mult);
}

/** Set event handler for clockchip */
void vmm_clockchip_set_event_handler(struct vmm_clockchip *cc, 
		void (*event_handler) (struct vmm_clockchip *));

/** Program clockchip for next event after delta nanoseconds */
int vmm_clockchip_program_event(struct vmm_clockchip *cc,
				u64 now_ns, u64 expires_ns);

/** Change mode of clockchip */
void vmm_clockchip_set_mode(struct vmm_clockchip *cc, 
			    enum vmm_clockchip_mode mode);

/** Register clockchip */
int vmm_clockchip_register(struct vmm_clockchip *cc);

/** Register clockchip */
int vmm_clockchip_unregister(struct vmm_clockchip *cc);

/** Find best rated clockchip for given host CPU and bind it */
struct vmm_clockchip *vmm_clockchip_bind_best(u32 hcpu);

/** Unbind clockchip from host CPU */
int vmm_clockchip_unbind(struct vmm_clockchip *cc);

/** Retrive clockchip with given index */
struct vmm_clockchip *vmm_clockchip_get(int index);

/** Count number of clockchips */
u32 vmm_clockchip_count(void);

/** Initialize clockchip managment subsystem */
int vmm_clockchip_init(void);

#endif
