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

#define	notifier_block				vmm_notifier_block

#define notifier_from_errno(err)		vmm_notifier_from_errno(err)
#define notifier_to_errno(ret)			vmm_notifier_to_errno(ret)

#define atomic_notifier_head			vmm_atomic_notifier_chain
#define blocking_notifier_head			vmm_blocking_notifier_chain
#define raw_notifier_head			vmm_raw_notifier_chain
#define srcu_notifier_head			vmm_atomic_notifier_chain

#define ATOMIC_INIT_NOTIFIER_HEAD(nc)		ATOMIC_INIT_NOTIFIER_CHAIN(nc)
#define BLOCKING_INIT_NOTIFIER_HEAD(nc)		BLOCKING_INIT_NOTIFIER_CHAIN(nc)
#define RAW_INIT_NOTIFIER_HEAD(nc)		RAW_INIT_NOTIFIER_CHAIN(nc)
#define srcu_init_notifier_head(nc)		ATOMIC_INIT_NOTIFIER_CHAIN(nc)

#define srcu_cleanup_notifier_head(nc)		do { (void)nc; } while (0)

#define ATOMIC_NOTIFIER_HEAD(name)		ATOMIC_NOTIFIER_CHAIN(name)
#define BLOCKING_NOTIFIER_HEAD(name)		BLOCKING_NOTIFIER_CHAIN(name)
#define RAW_NOTIFIER_HEAD(name)			RAW_NOTIFIER_CHAIN(name)

#define atomic_notifier_chain_register(nc, nb)	vmm_atomic_notifier_register(nc, nb)
#define blocking_notifier_chain_register(nc, nb)	\
						vmm_blocking_notifier_register(nc, nb)
#define raw_notifier_chain_register(nc, nb)	vmm_raw_notifier_register(nc, nb)
#define srcu_notifier_chain_register(nc, nb)	vmm_atomic_notifier_register(nc, nb)

#define atomic_notifier_chain_unregister(nc, nb) vmm_atomic_notifier_unregister(nc, nb)
#define blocking_notifier_chain_unregister(nc, nb)	\
						vmm_blocking_notifier_unregister(nc, nb)
#define raw_notifier_chain_unregister(nc, nb)	vmm_raw_notifier_unregister(nc, nb)
#define srcu_notifier_chain_unregister(nc, nb)	vmm_atomic_notifier_unregister(nc, nb)

#define atomic_notifier_call_chain(nc, val, v)	vmm_atomic_notifier_call(nc, val, v)
#define blocking_notifier_call_chain(nc, val, v)	\
						vmm_blocking_notifier_call(nc, val, v)
#define raw_notifier_call_chain(nc, val, v)	vmm_blocking_notifier_call(nc, val, v)
#define srcu_notifier_call_chain(nc, val, v)	vmm_atomic_notifier_call(nc, val, v)

#endif /* _LINUX_NOTIFIER_H */
