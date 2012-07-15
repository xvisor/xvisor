#ifndef __LINUX_SPINLOCK_H
#define __LINUX_SPINLOCK_H

#include <vmm_spinlocks.h>

#define	spinlock_t		struct vmm_spinlock
#define	spin_lock_irqsave	vmm_spin_lock_irqsave
#define	spin_unlock_irqrestore	vmm_spin_unlock_irqrestore

#endif /* __LINUX_SPINLOCK_H */
