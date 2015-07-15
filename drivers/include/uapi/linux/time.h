#ifndef _UAPI_LINUX_TIME_H
#define _UAPI_LINUX_TIME_H

#include <linux/types.h>
#if 1
# include <uapi/asm-generic/posix_types.h>
#endif /* 1 */

#ifndef _STRUCT_TIMESPEC
#define _STRUCT_TIMESPEC
struct timespec {
	__kernel_time_t	tv_sec;			/* seconds */
	long		tv_nsec;		/* nanoseconds */
};
#endif

#endif /* _UAPI_LINUX_TIME_H */
