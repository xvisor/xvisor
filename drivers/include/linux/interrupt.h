#ifndef _INTERRUPT_H
#define _INTERRUPT_H

#include <vmm_host_irq.h>

typedef vmm_irq_return_t irqreturn_t;

#define IRQ_NONE	VMM_IRQ_NONE
#define IRQ_HANDLED	VMM_IRQ_HANDLED

static inline int request_irq(unsigned int irq, 
			      vmm_host_irq_handler_t handler, 
			      unsigned long flags,
			      const char *name, void *dev)
{
	return vmm_host_irq_register(irq, name, handler, dev);
}

static inline void free_irq(unsigned int irq, void *dev)
{
	vmm_host_irq_unregister(irq, dev);
}

#define local_irq_save(flags) \
		flags = arch_cpu_irq_save()
#define local_irq_restore(flags) \
		arch_cpu_irq_restore(flags)

#endif /* _INTERRUPT_H */
