#ifndef _LINUX_IRQ_H
#define _LINUX_IRQ_H

#include <linux/interrupt.h>
#include <vmm_host_extirq.h>

static inline unsigned int irq_find_mapping(struct irq_domain *host,
					    irq_hw_number_t hwirq)
{
	return vmm_host_extirq_find_mapping(host, hwirq);
}

#endif
