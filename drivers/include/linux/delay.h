#ifndef _LINUX_DELAY_H
#define _LINUX_DELAY_H

#include <vmm_delay.h>

#define udelay vmm_udelay
#define msleep vmm_mdelay
#define mdelay vmm_mdelay
#define ssleep vmm_sdelay
#define sdelay vmm_sdelay

#endif /* defined(_LINUX_DELAY_H) */
