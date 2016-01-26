/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file vsdaemon.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief vserial daemon library interface
 */

#ifndef __VSDAEMON_H__
#define __VSDAEMON_H__

#include <vmm_limits.h>
#include <vmm_types.h>
#include <vmm_threads.h>
#include <vio/vmm_vserial.h>
#include <libs/list.h>
#include <libs/netstack.h>

#define VSDAEMON_IPRIORITY			(VMM_VSERIAL_IPRIORITY + \
						 NETSTACK_IPRIORITY + 1)

struct vsdaemon;

struct vsdaemon_transport {
	/* list head */
	struct dlist head;

	/* transport name */
	char name[VMM_FIELD_NAME_SIZE];

	/* operations */
	int (*setup) (struct vsdaemon *vsd, int argc, char **argv);
	void (*cleanup) (struct vsdaemon *vsd);
	int (*main_loop) (struct vsdaemon *vsd);
	void (*receive_char) (struct vsdaemon *vsd, u8 ch);
};

struct vsdaemon {
	/* list head */
	struct dlist head;

	/* daemon name */
	char name[VMM_FIELD_NAME_SIZE];

	/* transport pointer */
	struct vsdaemon_transport *trans;

	/* vserial port */
	struct vmm_vserial *vser;

	/* underlying thread */
	struct vmm_thread *thread;

	/* transport specific data */
	void *trans_data;
};

/** Set vsdaemon transport data */
static inline void vsdaemon_transport_set_data(struct vsdaemon *vsd, void *data)
{
	if (vsd) {
		vsd->trans_data = data;
	}
}

/** Get vsdaemon transport data */
static inline void *vsdaemon_transport_get_data(struct vsdaemon *vsd)
{
	return (vsd) ? vsd->trans_data : NULL;
}

/** Get vsdaemon transport based on index */
struct vsdaemon_transport *vsdaemon_transport_get(int index);

/** Count vsdaemon transports */
u32 vsdaemon_transport_count(void);

/** Create vsdaemon instance */
int vsdaemon_create(const char *transport_name,
		    const char *vserial_name,
		    const char *daemon_name,
		    int argc, char **argv);

/** Destroy vsdaemon instance */
int vsdaemon_destroy(const char *daemon_name);

/** Get vsdaemon based on index */
struct vsdaemon *vsdaemon_get(int index);

/** Count vsdaemon instances */
u32 vsdaemon_count(void);

#endif /* __VSDAEMON_H__ */
