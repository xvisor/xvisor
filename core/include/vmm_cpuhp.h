/**
 * Copyright (c) 2019 Anup Patel.
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
 * @file vmm_cpuhp.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Interface for CPU hotplug notifiers
 */

#ifndef __VMM_CPUHP_H__
#define __VMM_CPUHP_H__

#include <vmm_types.h>
#include <vmm_limits.h>
#include <libs/list.h>

enum vmm_cpuhp_states {
	VMM_CPUHP_STATE_OFFLINE = 0,
	VMM_CPUHP_STATE_HOST_IRQ,
	VMM_CPUHP_STATE_CLOCKSOURCE,
	VMM_CPUHP_STATE_CLOCKCHIP,
	VMM_CPUHP_STATE_TIMER,
	VMM_CPUHP_STATE_DELAY,
	VMM_CPUHP_STATE_SCHEDULER,
	VMM_CPUHP_STATE_SMP_IPI,
	VMM_CPUHP_STATE_WORKQUEUE
};

#define VMM_CPUHP_STATE_ONLINE U32_MAX

struct vmm_cpuhp_notify {
	/* Private */
	struct dlist head;
	/* Public */
	u32 state;
	char name[VMM_FIELD_NAME_SIZE];
	int (*startup)(struct vmm_cpuhp_notify *cpuhp, u32 cpu);
	int (*teardown)(struct vmm_cpuhp_notify *cpuhp, u32 cpu);
};

/** Get hotplug state of given CPU
 *  Note: This function can be called even before vmm_cpuhp_init()
 *  is called.
 */
u32 vmm_cpuhp_get_state(u32 cpu);

/* Set specified hotplug state for current CPU */
int vmm_cpuhp_set_state(u32 state);

/** Register CPU hotplug notifiers */
int vmm_cpuhp_register(struct vmm_cpuhp_notify *cpuhp, bool invoke_startup);

/** UnRegister CPU hotplug notifiers */
int vmm_cpuhp_unregister(struct vmm_cpuhp_notify *cpuhp);

/** Initialize CPU hotplug notifiers */
int vmm_cpuhp_init(void);

#endif /* __VMM_CPUHP_H__ */
