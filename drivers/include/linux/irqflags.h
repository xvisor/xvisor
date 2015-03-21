#ifndef __LINUX_IRQFLAGS_H
#define __LINUX_IRQFLAGS_H

#define local_irq_disable	vmm_scheduler_preempt_disable
#define local_irq_enable	vmm_scheduler_preempt_enable
#define irqs_disabled()		arch_cpu_irq_disabled()

#endif /* __LINUX_IRQFLAGS_H */
