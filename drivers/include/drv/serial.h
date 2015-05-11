/**
 * Copyright (c) 2015 Anup Patel.
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
 * @file serial.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Serial Port framework header.
 */

#ifndef __SERIAL_H_
#define __SERIAL_H_

#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <vmm_completion.h>
#include <vmm_chardev.h>
#include <libs/list.h>
#include <libs/fifo.h>

#define SERIAL_IPRIORITY		(1)

/* Serial Port */
struct serial {
	struct dlist head;
	struct vmm_chardev cdev;

	struct fifo *rx_fifo;
	struct vmm_completion rx_avail;

	vmm_spinlock_t tx_lock;
	u32 (*tx_func)(struct serial *p, u8 *src, size_t len);
	void *tx_priv;
};

/** Get private context for Serial Port Tx */
static inline void *serial_tx_priv(struct serial *p)
{
	return (p) ? p->tx_priv : NULL;
}

/** Receive data on Serial Port */
void serial_rx(struct serial *p, u8 *data, u32 len);

/** Create Serial Port */
struct serial *serial_create(struct vmm_device *dev,
			     u32 rx_fifo_size,
			     u32 (*tx_func)(struct serial *, u8 *, size_t),
			     void *tx_priv);

/** Destroy Serial Port */
void serial_destroy(struct serial *p);

/** Find a Serial Port with given name */
struct serial *serial_find(const char *name);

/** Count number of Serial Ports */
u32 serial_count(void);

#endif /* __SERIAL_H_ */
