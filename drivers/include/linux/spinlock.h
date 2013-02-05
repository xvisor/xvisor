#ifndef __LINUX_SPINLOCK_H
#define __LINUX_SPINLOCK_H

#include <vmm_spinlocks.h>

#define	spinlock_t		struct vmm_spinlock
#define	spin_lock		vmm_spin_lock
#define	spin_trylock		vmm_spin_trylock
#define	spin_unlock		vmm_spin_unlock
#define	spin_lock_irq		vmm_spin_lock_irq
#define	spin_unlock_irq		vmm_spin_unlock_irq
#define	spin_lock_irqsave	vmm_spin_lock_irqsave
#define	spin_unlock_irqrestore	vmm_spin_unlock_irqrestore

#define	spin_lock_init(__lock)	INIT_SPIN_LOCK(__lock)

#endif /* __LINUX_SPINLOCK_H */
