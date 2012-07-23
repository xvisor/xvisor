#ifndef _INTERRUPT_H
#define _INTERRUPT_H

#include <vmm_host_irq.h>

typedef vmm_irq_return_t irqreturn_t;

#define IRQ_NONE	VMM_IRQ_NONE
#define IRQ_HANDLED	VMM_IRQ_HANDLED

#endif /* _INTERRUPT_H */
