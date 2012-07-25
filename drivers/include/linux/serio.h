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
 * @file serio.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief The Serio abstraction interface
 *
 * The source has been largely adapted from Linux 3.x or higher:
 * include/linux/serio.h
 *
 * Copyright (C) 1999-2002 Vojtech Pavlik
 *
 * The original code is licensed under the GPL.
 */

#ifndef _SERIO_H
#define _SERIO_H

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/mod_devicetable.h>

/** SERIO module intialization priority */
#define SERIO_IPRIORITY				1

struct serio {
	void *port_data;

	char name[32];
	char phys[32];

	bool manual_bind;	/* For xvisor, we ignore this field */

	struct serio_device_id id;

	spinlock_t lock;		/* protects critical sections from port's interrupt handler */

	int (*write)(struct serio *, unsigned char);
	int (*open)(struct serio *);
	void (*close)(struct serio *);
	int (*start)(struct serio *);
	void (*stop)(struct serio *);

	struct serio *parent;
	struct list_head child_node;	/* Entry in parent->children list */
	struct list_head children;
	unsigned int depth;		/* level of nesting in serio hierarchy */

	struct serio_driver *drv;	/* accessed from interrupt, must be protected by serio->lock and serio->sem */
	spinlock_t drv_lock;		/* protects serio->drv so attributes can pin driver */

	void *priv;

	struct list_head node;
};
#define to_serio_port(d)	container_of(d, struct serio, dev)

struct serio_driver {
	const char *name;	/* Only for xvisor */
	const char *description;

	const struct serio_device_id *id_table;
	bool manual_bind;	/* For xvisor, we ignore this field */

	void (*write_wakeup)(struct serio *);
	irqreturn_t (*interrupt)(struct serio *, unsigned char, unsigned int);
	int  (*connect)(struct serio *, struct serio_driver *drv);
	int  (*reconnect)(struct serio *);
	void (*disconnect)(struct serio *);
	void (*cleanup)(struct serio *);

	struct list_head node;
};
#define to_serio_driver(d)	container_of(d, struct serio_driver, driver)

int serio_open(struct serio *serio, struct serio_driver *drv);
void serio_close(struct serio *serio);
void serio_rescan(struct serio *serio);
void serio_reconnect(struct serio *serio);
irqreturn_t serio_interrupt(struct serio *serio, unsigned char data, unsigned int flags);

void __serio_register_port(struct serio *serio);
static inline void serio_register_port(struct serio *serio)
{
	__serio_register_port(serio);
}

void serio_unregister_port(struct serio *serio);
void serio_unregister_child_port(struct serio *serio);

int __serio_register_driver(struct serio_driver *drv, const char *name);
static inline int serio_register_driver(struct serio_driver *drv, const char *name)
{
	return __serio_register_driver(drv, name);
}
void serio_unregister_driver(struct serio_driver *drv);

static inline int serio_write(struct serio *serio, unsigned char data)
{
	if (serio->write)
		return serio->write(serio, data);
	else
		return -1;
}

static inline void serio_drv_write_wakeup(struct serio *serio)
{
	if (serio->drv && serio->drv->write_wakeup)
		serio->drv->write_wakeup(serio);
}

/*
 * Use the following functions to manipulate serio's per-port
 * driver-specific data.
 */
static inline void *serio_get_drvdata(struct serio *serio)
{
	return serio->priv;
}

static inline void serio_set_drvdata(struct serio *serio, void *data)
{
	serio->priv = data;
}

/*
 * Use the following functions to protect critical sections in
 * driver code from port's interrupt handler
 */
static inline void serio_pause_rx(struct serio *serio)
{
	spin_lock_irq(&serio->lock);
}

static inline void serio_continue_rx(struct serio *serio)
{
	spin_unlock_irq(&serio->lock);
}

/*
 * bit masks for use in "interrupt" flags (3rd argument)
 */
#define SERIO_TIMEOUT	1
#define SERIO_PARITY	2
#define SERIO_FRAME	4

/*
 * Serio types
 */
#define SERIO_XT	0x00
#define SERIO_8042	0x01
#define SERIO_RS232	0x02
#define SERIO_HIL_MLC	0x03
#define SERIO_PS_PSTHRU	0x05
#define SERIO_8042_XL	0x06

/*
 * Serio protocols
 */
#define SERIO_UNKNOWN	0x00
#define SERIO_MSC	0x01
#define SERIO_SUN	0x02
#define SERIO_MS	0x03
#define SERIO_MP	0x04
#define SERIO_MZ	0x05
#define SERIO_MZP	0x06
#define SERIO_MZPP	0x07
#define SERIO_VSXXXAA	0x08
#define SERIO_SUNKBD	0x10
#define SERIO_WARRIOR	0x18
#define SERIO_SPACEORB	0x19
#define SERIO_MAGELLAN	0x1a
#define SERIO_SPACEBALL	0x1b
#define SERIO_GUNZE	0x1c
#define SERIO_IFORCE	0x1d
#define SERIO_STINGER	0x1e
#define SERIO_NEWTON	0x1f
#define SERIO_STOWAWAY	0x20
#define SERIO_H3600	0x21
#define SERIO_PS2SER	0x22
#define SERIO_TWIDKBD	0x23
#define SERIO_TWIDJOY	0x24
#define SERIO_HIL	0x25
#define SERIO_SNES232	0x26
#define SERIO_SEMTECH	0x27
#define SERIO_LKKBD	0x28
#define SERIO_ELO	0x29
#define SERIO_MICROTOUCH	0x30
#define SERIO_PENMOUNT	0x31
#define SERIO_TOUCHRIGHT	0x32
#define SERIO_TOUCHWIN	0x33
#define SERIO_TAOSEVM	0x34
#define SERIO_FUJITSU	0x35
#define SERIO_ZHENHUA	0x36
#define SERIO_INEXIO	0x37
#define SERIO_TOUCHIT213	0x38
#define SERIO_W8001	0x39
#define SERIO_DYNAPRO	0x3a
#define SERIO_HAMPSHIRE	0x3b
#define SERIO_PS2MULT	0x3c

#endif /* _SERIO_H */
