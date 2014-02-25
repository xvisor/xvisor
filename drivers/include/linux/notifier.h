/*
 *	Routines to manage notifier chains for passing status changes to any
 *	interested routines. We need this instead of hard coded call lists so
 *	that modules can poke their nose into the innards. The network devices
 *	needed them so here they are for the rest of you.
 *
 *				Alan Cox <Alan.Cox@linux.org>
 */
 
#ifndef _LINUX_NOTIFIER_H
#define _LINUX_NOTIFIER_H

#include <vmm_notifier.h>

#include <linux/errno.h>
#include <linux/mutex.h>

/* FIXME: This file just a place holder for most cases */

#define	notifier_block			vmm_notifier_block

#endif /* _LINUX_NOTIFIER_H */
