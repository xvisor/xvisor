#ifndef _ASM_IRQ_H_
#define _ASM_IRQ_H_

#include <arch_cpu_irq.h>

/*
 * Use this value to indicate lack of interrupt
 * capability
 */
#ifndef NO_IRQ
#define NO_IRQ  ((unsigned int)(-1))
#endif


#endif /* _ASM_IRQ_H_ */
