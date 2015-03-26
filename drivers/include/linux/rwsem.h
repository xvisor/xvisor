#ifndef _LINUX_RWSEM_H_
#define _LINUX_RWSEM_H_

#include <vmm_mutex.h>

#define DECLARE_RWSEM(mut)		DEFINE_MUTEX(mut)

#endif /* _LINUX_RWSEM_H_ */
