#ifndef _LINUX_MUTEX_H_
#define _LINUX_MUTEX_H_

#include <vmm_mutex.h>

#define mutex			vmm_mutex

#define mutex_init(x)		INIT_MUTEX(x)
#define mutex_lock(x)		vmm_mutex_lock(x)
#define mutex_trylock(x)	vmm_mutex_trylock(x)
#define mutex_unlock(x)		vmm_mutex_unlock(x)
#define mutex_destroy(x)	do {} while (0);

#endif /* _LINUX_MUTEX_H_ */
