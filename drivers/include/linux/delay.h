#ifndef _LINUX_DELAY_H
#define _LINUX_DELAY_H

#include <arch_delay.h>
#include <vmm_delay.h>

#define udelay(x)	vmm_udelay(x)
#define msleep(x)	vmm_mdelay(x)
#define mdelay(x)	vmm_mdelay(x)
#define ssleep(x)	vmm_sdelay(x)
#define sdelay(x)	vmm_sdelay(x)

#define __delay(x)	arch_delay_loop(x)

#endif /* defined(_LINUX_DELAY_H) */
