#ifndef _LINUX_I8042_H
#define _LINUX_I8042_H

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/errno.h>
#include <linux/types.h>

/*
 * Standard commands.
 */

#define I8042_CMD_CTL_RCTR	0x0120
#define I8042_CMD_CTL_WCTR	0x1060
#define I8042_CMD_CTL_TEST	0x01aa

#define I8042_CMD_KBD_DISABLE	0x00ad
#define I8042_CMD_KBD_ENABLE	0x00ae
#define I8042_CMD_KBD_TEST	0x01ab
#define I8042_CMD_KBD_LOOP	0x11d2

#define I8042_CMD_AUX_DISABLE	0x00a7
#define I8042_CMD_AUX_ENABLE	0x00a8
#define I8042_CMD_AUX_TEST	0x01a9
#define I8042_CMD_AUX_SEND	0x10d4
#define I8042_CMD_AUX_LOOP	0x11d3

#define I8042_CMD_MUX_PFX	0x0090
#define I8042_CMD_MUX_SEND	0x1090

struct serio;

/*
 * Arch-dependent inline functions and defines.
 */

#if defined(CONFIG_MACH_JAZZ)
#include "i8042-jazzio.h"
#elif defined(CONFIG_SGI_HAS_I8042)
#include "i8042-ip22io.h"
#elif defined(CONFIG_SNI_RM)
#include "i8042-snirm.h"
#elif defined(CONFIG_PPC)
#include "i8042-ppcio.h"
#elif defined(CONFIG_SPARC)
#include "i8042-sparcio.h"
#elif defined(CONFIG_X86) || defined(CONFIG_IA64)
#include "i8042-x86ia64io.h"
#elif defined(CONFIG_UNICORE32)
#include "i8042-unicore32io.h"
#else
#include "i8042-io.h"
#endif

/*
 * This is in 50us units, the time we wait for the i8042 to react. This
 * has to be long enough for the i8042 itself to timeout on sending a byte
 * to a non-existent mouse.
 */

#define I8042_CTL_TIMEOUT	10000

/*
 * Status register bits.
 */

#define I8042_STR_PARITY	0x80
#define I8042_STR_TIMEOUT	0x40
#define I8042_STR_AUXDATA	0x20
#define I8042_STR_KEYLOCK	0x10
#define I8042_STR_CMDDAT	0x08
#define I8042_STR_MUXERR	0x04
#define I8042_STR_IBF		0x02
#define	I8042_STR_OBF		0x01

/*
 * Control register bits.
 */

#define I8042_CTR_KBDINT	0x01
#define I8042_CTR_AUXINT	0x02
#define I8042_CTR_IGNKEYLOCK	0x08
#define I8042_CTR_KBDDIS	0x10
#define I8042_CTR_AUXDIS	0x20
#define I8042_CTR_XLATE		0x40

/*
 * Return codes.
 */

#define I8042_RET_CTL_TEST	0x55

/*
 * Expected maximum internal i8042 buffer size. This is used for flushing
 * the i8042 buffers.
 */

#define I8042_BUFFER_SIZE	16

/*
 * Number of AUX ports on controllers supporting active multiplexing
 * specification
 */

#define I8042_NUM_MUX_PORTS	4

/*
 * Debug.
 */

#ifdef DEBUG
static unsigned long i8042_start_time;
#define dbg_init() do { i8042_start_time = jiffies; } while (0)
#define dbg(format, arg...)							\
	do {									\
		if (i8042_debug)						\
			printk(KERN_DEBUG KBUILD_MODNAME ": [%d] " format,	\
			       (int) (jiffies - i8042_start_time), ##arg);	\
	} while (0)
#else
#define dbg_init() do { } while (0)
#define dbg(format, arg...)							\
	do {									\
		if (0)								\
			printk(KERN_DEBUG pr_fmt(format), ##arg);		\
	} while (0)
#endif

#if defined(CONFIG_SERIO_I8042) || defined(CONFIG_SERIO_I8042_MODULE)

void i8042_lock_chip(void);
void i8042_unlock_chip(void);
int i8042_command(unsigned char *param, int command);
bool i8042_check_port_owner(const struct serio *);
int i8042_install_filter(bool (*filter)(unsigned char data, unsigned char str,
					struct serio *serio));
int i8042_remove_filter(bool (*filter)(unsigned char data, unsigned char str,
				       struct serio *serio));

#else

static inline void i8042_lock_chip(void)
{
}

static inline void i8042_unlock_chip(void)
{
}

static inline int i8042_command(unsigned char *param, int command)
{
	return -ENODEV;
}

static inline bool i8042_check_port_owner(const struct serio *serio)
{
	return false;
}

static inline int i8042_install_filter(bool (*filter)(unsigned char data, unsigned char str,
					struct serio *serio))
{
	return -ENODEV;
}

static inline int i8042_remove_filter(bool (*filter)(unsigned char data, unsigned char str,
				       struct serio *serio))
{
	return -ENODEV;
}

#endif

#endif
