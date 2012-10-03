#ifndef _LINUX_ERRNO_H
#define _LINUX_ERRNO_H

#include <vmm_error.h>

#define EFAIL			-(VMM_EFAIL)
#define EUNKNOWN		-(VMM_EUNKOWN)
#define EINVAL			-(VMM_EINVALID)
#define ENOTAVAIL		-(VMM_ENOTAVAIL)
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

#endif /* defined(_LINUX_ERRNO_H) */
