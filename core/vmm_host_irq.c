/**
 * Copyright (c) 2010 Anup Patel.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file vmm_host_irq.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief source code for host interrupts
 */

#include <vmm_cpu.h>
#include <vmm_board.h>
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_spinlocks.h>
#include <vmm_heap.h>
#include <vmm_host_irq.h>

struct vmm_host_irqs_ctrl {
	vmm_spinlock_t lock;
	u32 irq_count;
	bool *enabled;
	vmm_host_irq_handler_t *handler;
	void **dev;
};

typedef struct vmm_host_irqs_ctrl vmm_host_irqs_ctrl_t;

vmm_host_irqs_ctrl_t hirqctrl;

int vmm_host_irq_exec(u32 cpu_irq_no, vmm_user_regs_t * regs)
{
	int cond;
	s32 host_irq_no = vmm_pic_cpu_to_host_map(cpu_irq_no);
	if (-1 < host_irq_no && host_irq_no < hirqctrl.irq_count) {
		if (hirqctrl.handler[host_irq_no] != NULL &&
		    hirqctrl.enabled[host_irq_no]) {
			cond = vmm_pic_pre_condition(host_irq_no);
			if (!cond) {
				hirqctrl.handler[host_irq_no] (host_irq_no, 
					   regs, hirqctrl.dev[host_irq_no]);
				cond = vmm_pic_post_condition(host_irq_no);
			}
			return cond;
		}
	}
	return VMM_ENOTAVAIL;
}

bool vmm_host_irq_isenabled(u32 host_irq_no)
{
	if (host_irq_no < hirqctrl.irq_count) {
		return hirqctrl.enabled[host_irq_no];
	}
	return FALSE;
}

int vmm_host_irq_enable(u32 host_irq_no)
{
	if (host_irq_no < hirqctrl.irq_count) {
		if (!hirqctrl.enabled[host_irq_no]) {
			hirqctrl.enabled[host_irq_no] = TRUE;
			return vmm_pic_irq_enable(host_irq_no);
		}
	}
	return VMM_EFAIL;
}

int vmm_host_irq_disable(u32 host_irq_no)
{
	if (host_irq_no < hirqctrl.irq_count) {
		if (hirqctrl.enabled[host_irq_no]) {
			hirqctrl.enabled[host_irq_no] = FALSE;
			return vmm_pic_irq_disable(host_irq_no);
		}
	}
	return VMM_EFAIL;
}

int vmm_host_irq_register(u32 host_irq_no, 
			  vmm_host_irq_handler_t handler,
			  void *dev)
{
	if (host_irq_no < hirqctrl.irq_count) {
		if (hirqctrl.handler[host_irq_no] == NULL) {
			vmm_spin_lock(&hirqctrl.lock);
			hirqctrl.handler[host_irq_no] = handler;
			hirqctrl.dev[host_irq_no] = dev;
			vmm_spin_unlock(&hirqctrl.lock);
			return vmm_host_irq_enable(host_irq_no);
		}
	}
	return VMM_EFAIL;
}

int vmm_host_irq_init(void)
{
	int ret;
	u32 ite;

	/* Clear the memory of control structure */
	vmm_memset(&hirqctrl, 0, sizeof(hirqctrl));

	/* Initialize spin lock */
	INIT_SPIN_LOCK(&hirqctrl.lock);

	/* Get host irq count */
	hirqctrl.irq_count = vmm_pic_irq_count();

	/* Allocate memory for enabled array */
	hirqctrl.enabled = (bool *)vmm_malloc(sizeof(bool) * hirqctrl.irq_count);

	/* Set default values to enabled array */
	for (ite = 0; ite < hirqctrl.irq_count; ite++) {
		hirqctrl.enabled[ite] = FALSE;
	}

	/* Allocate memory for handler array */
	hirqctrl.handler = vmm_malloc(sizeof(vmm_host_irq_handler_t) * 
					hirqctrl.irq_count);

	/* Allocate memory for dev array */
	hirqctrl.dev = vmm_malloc(sizeof(void *) * hirqctrl.irq_count);

	/* Reset the handler array */
	for (ite = 0; ite < hirqctrl.irq_count; ite++) {
		hirqctrl.handler[ite] = NULL;
		hirqctrl.dev[ite] = NULL;
	}

	/* Initialize board specific PIC */
	ret = vmm_pic_init();
	if (ret) {
		return ret;
	}

	/** Setup interrupts in CPU */
	ret = vmm_cpu_irq_setup();
	if (ret) {
		return ret;
	}

	/** Enable interrupts in CPU */
	vmm_cpu_irq_enable();

	return VMM_OK;
}
