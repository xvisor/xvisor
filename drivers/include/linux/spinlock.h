#ifndef __LINUX_SPINLOCK_H
#define __LINUX_SPINLOCK_H

#include <vmm_spinlocks.h>

#include <asm/processor.h>

#define raw_spinlock_t			struct vmm_spinlock
#define raw_spin_lock			vmm_spin_lock
#define raw_spin_unlock			vmm_spin_unlock
#define raw_spin_lock_irq		vmm_spin_lock_irq
#define raw_spin_unlock_irq		vmm_spin_unlock_irq
#define raw_spin_lock_irqsave		vmm_spin_lock_irqsave
#define raw_spin_unlock_irqrestore	vmm_spin_unlock_irqrestore
#define DEFINE_RAW_SPINLOCK		DEFINE_SPINLOCK

#define	spinlock_t		struct vmm_spinlock
#define	spin_lock		vmm_spin_lock
#define	spin_trylock		vmm_spin_trylock
#define	spin_unlock		vmm_spin_unlock
#define	spin_lock_irq		vmm_spin_lock_irq
#define	spin_unlock_irq		vmm_spin_unlock_irq
#define	spin_lock_irqsave	vmm_spin_lock_irqsave
#define spin_trylock_irqsave	vmm_spin_trylock_irqsave
#define	spin_unlock_irqrestore	vmm_spin_unlock_irqrestore

#define	spin_lock_init(__lock)	INIT_SPIN_LOCK(__lock)

/* FIXME: We don't have read/write semantics */
#define rw_semaphore		vmm_mutex
#define DEFINE_RWSEM		DEFINE_MUTEX
#define down_read		vmm_mutex_lock
#define up_read			vmm_mutex_unlock
#define down_write		vmm_mutex_lock
#define up_write		vmm_mutex_unlock

#define read_lock		vmm_read_lock
#define read_unlock		vmm_read_unlock
#define write_lock		vmm_write_lock
#define write_unlock		vmm_write_unlock

#define atomic_read		arch_atomic_read
#define atomic_write		arch_atomic_write

#endif /* __LINUX_SPINLOCK_H */
