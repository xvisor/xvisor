#ifndef LINUX_CLOCKSOURCE_H
#define LINUX_CLOCKSOURCE_H

#include <vmm_clockchip.h>
#include <vmm_clocksource.h>

# if 1
#  define timecounter		vmm_timecounter
#  define cyclecounter		vmm_clocksource
# endif /* 1 */

#endif /* !LINUX_CLOCKSOURCE_H */
