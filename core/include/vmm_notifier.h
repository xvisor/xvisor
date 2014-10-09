/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file vmm_notifier.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Interface for notifier chain managment.
 * 
 * The notifer chain managment is highly inspired from Linux notifiers.
 *
 * The linux notifier interface can be found at:
 * <linux_source>/include/linux/notifier.h
 *
 * Linux notifier source is licensed under the GPL.
 */

#ifndef __VMM_NOTIFIER_H__
#define __VMM_NOTIFIER_H__

#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <vmm_semaphore.h>

/*
 * Notifier chains are of three types:
 *
 *	Atomic notifier chains: Chain callbacks run in interrupt/atomic
 *		context. Callouts are not allowed to block.
 *	Blocking notifier chains: Chain callbacks run in process context.
 *		Callouts are allowed to block.
 *	Raw notifier chains: There are no restrictions on callbacks,
 *		registration, or unregistration.  All locking and protection
 *		must be provided by the caller.
 *
 * vmm_atomic_notifier_register() may be called from an atomic context,
 * but vmm_blocking_notifier_register()  must be called from a process 
 * context. Ditto for the corresponding _unregister() routines.
 *
 * vmm_atomic_notifier_unregister() and vmm_blocking_notifier_unregister()
 * _must not_ be called from within the call chain.
 */

struct vmm_notifier_block {
	int (*notifier_call)(struct vmm_notifier_block *,
				 unsigned long, void *);
	struct vmm_notifier_block *next;
	int priority;
};

/** Notifier function callback return values */
#define NOTIFY_DONE		0x0000		/* Don't care */
#define NOTIFY_OK		0x0001		/* Suits me */
#define NOTIFY_STOP_MASK	0x8000		/* Don't call further */
#define NOTIFY_BAD		(NOTIFY_STOP_MASK|0x0002)
					/* Bad/Veto action */
/*
 * Clean way to return from the notifier and stop further calls.
 */
#define NOTIFY_STOP		(NOTIFY_OK|NOTIFY_STOP_MASK)

/* Encapsulate (negative) errno value (in particular, NOTIFY_BAD <=> EPERM). */
static inline int vmm_notifier_from_errno(int err)
{
	if (err)
		return NOTIFY_STOP_MASK | (NOTIFY_OK - err);

	return NOTIFY_OK;
}

/* Restore (negative) errno value from notify return value. */
static inline int vmm_notifier_to_errno(int ret)
{
	ret &= ~NOTIFY_STOP_MASK;
	return ret > NOTIFY_OK ? NOTIFY_OK - ret : 0;
}

/** Representation of an atoming notifier chain */
struct vmm_atomic_notifier_chain {
	vmm_spinlock_t lock;
	struct vmm_notifier_block *head;
};

#define ATOMIC_INIT_NOTIFIER_CHAIN(name) do {				\
			INIT_SPIN_LOCK(&(name)->lock);			\
			(name)->head = NULL;				\
		} while (0)
#define ATOMIC_NOTIFIER_INIT(name) {					\
		.lock =  __SPINLOCK_INITIALIZER(name.lock),		\
		.head = NULL }
#define ATOMIC_NOTIFIER_CHAIN(name)					\
	struct vmm_atomic_notifier_chain name =				\
		ATOMIC_NOTIFIER_INIT(name)

/**
 *  Add notifier to an atomic notifier chain
 *  @nc Pointer to head of the atomic notifier chain
 *  @nb New entry in notifier chain
 *
 *  @returns Currently always returns VMM_OK.
 */
int vmm_atomic_notifier_register(struct vmm_atomic_notifier_chain *nc,
				 struct vmm_notifier_block *nb);

/**
 *  Remove notifier from an atomic notifier chain
 *  @nc Pointer to head of the atomic notifier chain
 *  @nb Entry to remove from notifier chain
 *
 *  @returns VMM_OK on success or VMM_Exxxx on failure.
 */
int vmm_atomic_notifier_unregister(struct vmm_atomic_notifier_chain *nc,
				   struct vmm_notifier_block *nb);

/**
 *  Call functions in an atomic notifier chain
 *  @nc Pointer to the atomic notifier chain
 *  @val Value passed unmodified to notifier function
 *  @v Pointer passed unmodified to notifier function
 *  @nr_to_call	Number of notifier functions to be called. Don't care
 *		value of this parameter is -1.
 *  @nr_calls Records the number of notifications sent. Don't care
 *		value of this field is NULL.
 *  @returns value returned by the last notifier function called.
 */
int __vmm_atomic_notifier_call(struct vmm_atomic_notifier_chain *nc,
				unsigned long val, void *v, 
				int nr_to_call, int *nr_calls);

/**
 *  Call functions in an atomic notifier chain only once
 *  @nc Pointer to the atomic notifier chain
 *  @val Value passed unmodified to notifier function
 *  @v Pointer passed unmodified to notifier function
 *  @returns value returned by the last notifier function called.
 */
int vmm_atomic_notifier_call(struct vmm_atomic_notifier_chain *nc,
			     unsigned long val, void *v);



/** Representation of a blocking notifier chain */
struct vmm_blocking_notifier_chain {
	struct vmm_semaphore rwsem;
	struct vmm_notifier_block *head;
};

#define BLOCKING_INIT_NOTIFIER_CHAIN(name) do {				\
			INIT_SEMAPHORE(&(name)->rwsem, 1, 1);		\
			(name)->head = NULL;				\
		} while (0)
#define BLOCKING_NOTIFIER_INIT(name) {					\
		.rwsem = __SEMAPHORE_INITIALIZER((name).rwsem, 1, 1),	\
		.head = NULL }
#define BLOCKING_NOTIFIER_CHAIN(name)					\
	struct vmm_blocking_notifier_chain name =			\
		BLOCKING_NOTIFIER_INIT(name)

/**
 *  Add notifier to a blocking notifier chain
 *  @nc Pointer to head of the blocking notifier chain
 *  @nb New entry in notifier chain
 *
 *  @returns Currently always returns VMM_OK.
 */
int vmm_blocking_notifier_register(struct vmm_blocking_notifier_chain *nc,
				   struct vmm_notifier_block *nb);

/**
 *  Add notifier to a blocking notifier chain only if it is not already
 *  registered with the notifier chain
 *  @nc Pointer to head of the blocking notifier chain
 *  @nb New entry in notifier chain
 *
 *  @returns Currently always returns VMM_OK.
 */
int vmm_blocking_notifier_cond_register(struct vmm_blocking_notifier_chain *nc,
					struct vmm_notifier_block *nb);

/**
 *  Remove notifier from a blocking notifier chain
 *  @nc Pointer to head of the blocking notifier chain
 *  @nb Entry to remove from notifier chain
 *
 *  @returns VMM_OK on success or VMM_Exxxx on failure.
 */
int vmm_blocking_notifier_unregister(struct vmm_blocking_notifier_chain *nc,
				     struct vmm_notifier_block *nb);

/**
 *  Call functions in a blocking notifier chain
 *  @nc Pointer to the blocking notifier chain
 *  @val Value passed unmodified to notifier function
 *  @v Pointer passed unmodified to notifier function
 *  @nr_to_call	Number of notifier functions to be called. Don't care
 *		value of this parameter is -1.
 *  @nr_calls Records the number of notifications sent. Don't care
 *		value of this field is NULL.
 *  @returns value returned by the last notifier function called.
 */
int __vmm_blocking_notifier_call(struct vmm_blocking_notifier_chain *nc,
				 unsigned long val, void *v, 
				 int nr_to_call, int *nr_calls);

/**
 *  Call functions in a blocking notifier chain only once
 *  @nc Pointer to the blocking notifier chain
 *  @val Value passed unmodified to notifier function
 *  @v Pointer passed unmodified to notifier function
 *  @returns value returned by the last notifier function called.
 */
int vmm_blocking_notifier_call(struct vmm_blocking_notifier_chain *nh,
				unsigned long val, void *v);


/** Representation of a raw notifier chain */
struct vmm_raw_notifier_chain {
	struct vmm_notifier_block *head;
};

#define RAW_INIT_NOTIFIER_CHAIN(name) do {				\
			(name)->head = NULL;				\
		} while (0)
#define RAW_NOTIFIER_INIT(name)	{					\
		.head = NULL }
#define RAW_NOTIFIER_CHAIN(name)					\
	struct vmm_raw_notifier_chain name =				\
		RAW_NOTIFIER_INIT(name)

/**
 *  Add notifier to a raw notifier chain
 *  @nc Pointer to head of the raw notifier chain
 *  @nb New entry in notifier chain
 *
 *  @returns Currently always returns VMM_OK.
 */
int vmm_raw_notifier_register(struct vmm_raw_notifier_chain *nc,
			      struct vmm_notifier_block *nb);

/**
 *  Remove notifier from a raw notifier chain
 *  @nc Pointer to head of the raw notifier chain
 *  @nb Entry to remove from notifier chain
 *
 *  @returns VMM_OK on success or VMM_Exxxx on failure.
 */
int vmm_raw_notifier_unregister(struct vmm_raw_notifier_chain *nc,
				struct vmm_notifier_block *nb);

/**
 *  Call functions in a raw notifier chain
 *  @nc Pointer to the raw notifier chain
 *  @val Value passed unmodified to notifier function
 *  @v Pointer passed unmodified to notifier function
 *  @nr_to_call	Number of notifier functions to be called. Don't care
 *		value of this parameter is -1.
 *  @nr_calls Records the number of notifications sent. Don't care
 *		value of this field is NULL.
 *  @returns value returned by the last notifier function called.
 */
int __vmm_raw_notifier_call(struct vmm_raw_notifier_chain *nc,
			    unsigned long val, void *v, 
			    int nr_to_call, int *nr_calls);

/**
 *  Call functions in a raw notifier chain only once
 *  @nc Pointer to the raw notifier chain
 *  @val Value passed unmodified to notifier function
 *  @v Pointer passed unmodified to notifier function
 *  @returns value returned by the last notifier function called.
 */
int vmm_raw_notifier_call(struct vmm_raw_notifier_chain *nc,
			  unsigned long val, void *v);

#endif /* __VMM_NOTIFIER_H__ */
