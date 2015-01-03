#ifndef _LINUX_IRQ_H
#define _LINUX_IRQ_H

#include <vmm_host_extirq.h>

#include <linux/bug.h>
#include <linux/interrupt.h>

static inline unsigned int irq_find_mapping(struct irq_domain *host,
					    irq_hw_number_t hwirq)
{
	return vmm_host_extirq_find_mapping(host, hwirq);
}

/* FIXME: Need to fix this */
static inline int can_request_irq(unsigned int irq, unsigned long irqflags)
{
	WARN_ONCE("FIXME!! FIXME!!");
	return 1;
}
#endif
