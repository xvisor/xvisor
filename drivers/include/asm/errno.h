#ifndef _ASM_ERRNO_H
#define _ASM_ERRNO_H

#include <vmm_error.h>

#define EFAIL			-(VMM_EFAIL)
#define EBUSY			-(VMM_EBUSY)
#define EUNKNOWN		-(VMM_EUNKNOWN)
#define EINVAL			-(VMM_EINVALID)
#define ENOTAVAIL		-(VMM_ENOTAVAIL)
#define EEXIST			-(VMM_EEXIST)
#define EALREADY		-(VMM_EALREADY)
#define EINVALID		-(VMM_EINVALID)
#define EOVERFLOW		-(VMM_EOVERFLOW)
#define ENOMEM			-(VMM_ENOMEM)
#define ENODEV			-(VMM_ENODEV)
#define ETIMEDOUT		-(VMM_ETIMEDOUT)
#define EIO			-(VMM_EIO)
#define ETIME			-(VMM_ETIME)
#define ENOENT			-(VMM_ENOENT)
#define ERANGE			-(VMM_ERANGE)
#define EILSEQ			-(VMM_EILSEQ)
#define EOPNOTSUPP		-(VMM_EOPNOTSUPP)
#define EFAULT			-(VMM_EFAULT)
#define ENOSYS			-(VMM_ENOSYS)
#define ENOSPC			-(VMM_ENOSPC)
#define ENODATA			-(VMM_ENODATA)
#define ENXIO			-(VMM_ENXIO)
#define EPROTONOSUPPORT		-(VMM_EPROTONOSUPPORT)

#endif /* defined(_ASM_ERRNO_H) */
