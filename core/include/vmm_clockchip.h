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
 * @brief header file for clock chips
 */
#ifndef _VMM_CLOCKCHIP_H__
#define _VMM_CLOCKCHIP_H__

#include <vmm_types.h>
#include <vmm_list.h>
#include <vmm_math.h>
#include <arch_regs.h>

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

typedef void (*vmm_clockchip_event_handler_t) (struct vmm_clockchip * cc,
						arch_regs_t * regs);
typedef void (*vmm_clockchip_set_mode_t) (enum vmm_clockchip_mode mode, 
					  struct vmm_clockchip * cc);
typedef int (*vmm_clockchip_set_next_event_t) (unsigned long evt, 
						struct vmm_clockchip * cc);
typedef int (*vmm_clockchip_expire_t) (struct vmm_clockchip * cc);

/**
 * Hardware abstraction a clock chip device
 *
 * @head:		List head for registration
 * @name:		ptr to clockchip name
 * @hirq:		host irq number
 * @rating:		variable to rate clock event devices
 * @event_handler:	Assigned by the framework to be called by the low
 *			level handler of the event source
 * @set_next_event:	set next event function
 * @next_event:		local storage for the next event in oneshot mode
 * @max_delta_ns:	maximum delta value in ns
 * @min_delta_ns:	minimum delta value in ns
 * @mult:		nanosecond to cycles multiplier
 * @shift:		nanoseconds to cycles divisor (power of two)
 * @mode:		operating mode assigned by the management code
 * @features:		features
 * @set_mode:		set mode function
 */
struct vmm_clockchip {
	struct dlist head;
	const char *name;
	u32 hirq;
	int rating;
	unsigned int features;
	u32 mult;
	u32 shift;
	u64 max_delta_ns;
	u64 min_delta_ns;
	vmm_clockchip_set_mode_t set_mode;
	vmm_clockchip_set_next_event_t set_next_event;
	vmm_clockchip_expire_t expire;
	vmm_clockchip_event_handler_t event_handler;

	enum vmm_clockchip_mode mode;
	u64 next_event;
	void *priv;
};

/** Convert kHz clockchip to clockchip mult */
static inline u32 vmm_clockchip_khz2mult(u32 khz, u32 shift)
{
	u64 tmp = ((u64)khz) << shift;
	tmp = vmm_udiv64(tmp, (u64)1000000);
	return (u32)tmp;
}

/** Convert Hz clockchip to clockchip mult */
static inline u32 vmm_clockchip_hz2mult(u32 hz, u32 shift)
{
	u64 tmp = ((u64)hz) << shift;
	tmp = vmm_udiv64(tmp, (u64)1000000000);
	return (u32)tmp;
}

/** Convert tick delta to nanoseconds */
static inline u64 vmm_clockchip_delta2ns(u32 delta, struct vmm_clockchip *cc)
{
	u64 tmp = (u64)delta << cc->shift;
	return vmm_udiv64(tmp, cc->mult);
}

/** Set event handler for clockchip */
void vmm_clockchip_set_event_handler(struct vmm_clockchip *cc, 
				  vmm_clockchip_event_handler_t event_handler);

/** Program clockchip for next event after delta nanoseconds */
int vmm_clockchip_program_event(struct vmm_clockchip *cc,
				u64 now_ns, u64 expires_ns);

/** Force clockchip to expire and cause next event immediatly */
int vmm_clockchip_force_expiry(struct vmm_clockchip *cc, u64 now_ns);

/** Change mode of clockchip */
void vmm_clockchip_set_mode(struct vmm_clockchip *cc, 
			    enum vmm_clockchip_mode mode);

/** Register clockchip */
int vmm_clockchip_register(struct vmm_clockchip *cc);

/** Register clockchip */
int vmm_clockchip_unregister(struct vmm_clockchip *cc);

/** Get best rated clockchip */
struct vmm_clockchip *vmm_clockchip_best(void);

/** Find a clockchip */
struct vmm_clockchip *vmm_clockchip_find(const char *name);

/** Retrive clockchip with given index */
struct vmm_clockchip *vmm_clockchip_get(int index);

/** Count number of clockchips */
u32 vmm_clockchip_count(void);

/** Initialize clockchip managment subsystem */
int vmm_clockchip_init(void);

#endif
