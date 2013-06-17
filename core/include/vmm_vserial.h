/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_vserial.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for virtual serial port
 */
#ifndef _VMM_VSERIAL_H__
#define _VMM_VSERIAL_H__

#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <vmm_notifier.h>
#include <libs/list.h>
#include <libs/fifo.h>

struct vmm_vserial_receiver;
struct vmm_vserial;

/** Representation of a virtual serial port recevier 
 *  Note: receive callback can be called in any context hence
 *  hence we cannot sleep in receive callback.
 */
struct vmm_vserial_receiver {
	struct dlist head;
	void (*recv) (struct vmm_vserial *vser, void *priv, u8 data);
	void *priv;
};

/** Representation of a virtual serial port */
struct vmm_vserial {
	struct dlist head;
	char name[VMM_FIELD_NAME_SIZE];

	bool (*can_send) (struct vmm_vserial *vser);
	int (*send) (struct vmm_vserial *vser, u8 data);

	vmm_spinlock_t receiver_list_lock;
	struct dlist receiver_list;
	struct fifo *receive_fifo;
	void *priv;
};

/* Notifier event when virtual serial port is created */
#define VMM_VSERIAL_EVENT_CREATE		0x01
/* Notifier event when virtual serial port is destroyed */
#define VMM_VSERIAL_EVENT_DESTROY		0x02

/** Representation of virtual serial port notifier event */
struct vmm_vserial_event {
	struct vmm_vserial *vser;
	void *data;
};

/** Register a notifier client to receive virtual serial port events */
int vmm_vserial_register_client(struct vmm_notifier_block *nb);

/** Unregister a notifier client to not receive virtual serial port events */
int vmm_vserial_unregister_client(struct vmm_notifier_block *nb);

/** Send bytes to virtual serial port */
u32 vmm_vserial_send(struct vmm_vserial *vser, u8 *src, u32 len);

/** Receive bytes on virtual serial port */
u32 vmm_vserial_receive(struct vmm_vserial *vser, u8 *dst, u32 len);

/** Register receiver to a virtual serial port */
int vmm_vserial_register_receiver(struct vmm_vserial *vser, 
		void (*recv) (struct vmm_vserial *, void *, u8), void *priv);

/** Unregister a virtual serial port */
int vmm_vserial_unregister_receiver(struct vmm_vserial *vser,
		void (*recv) (struct vmm_vserial *, void *, u8), void *priv);

/** Create a virtual serial port */
struct vmm_vserial *vmm_vserial_create(const char *name,
				       bool (*can_send) (struct vmm_vserial *),
				       int (*send) (struct vmm_vserial *, u8),
				       u32 receive_fifo_size, void *priv);

/** Destroy a virtual serial port */
int vmm_vserial_destroy(struct vmm_vserial *vser);

/** Find a virtual serial port with given name */
struct vmm_vserial *vmm_vserial_find(const char *name);

/** Get a virtual serial port with given index */
struct vmm_vserial *vmm_vserial_get(int index);

/** Count of available virtual serial ports */
u32 vmm_vserial_count(void);

/** Initialize virtual serial port framework */
int vmm_vserial_init(void);

#endif
