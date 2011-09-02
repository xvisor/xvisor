/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file brd_pic.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief board specific progammable interrupt contoller
 */

#include <vmm_board.h>
#include <vmm_error.h>
#include <vmm_host_aspace.h>
#include <pba8_board.h>
#include <realview/gic.h>

int vmm_pic_cpu_to_host_map(u32 cpu_irq_no)
{
	return realview_gic_active_irq(0);
}

int vmm_pic_pre_condition(u32 host_irq_no)
{
	return VMM_OK;
}

int vmm_pic_post_condition(u32 host_irq_no)
{
	return realview_gic_ack_irq(0, host_irq_no);
}

int vmm_pic_irq_enable(u32 host_irq_no)
{
	return realview_gic_unmask(0, host_irq_no);
}

int vmm_pic_irq_disable(u32 host_irq_no)
{
	return realview_gic_mask(0, host_irq_no);
}

int vmm_pic_init(void)
{
	int ret;
	virtual_addr_t dist_base, cpu_base;

	dist_base = vmm_host_iomap(REALVIEW_PBA8_GIC_DIST_BASE, 0x1000);
	ret = realview_gic_dist_init(0, dist_base, IRQ_PBA8_GIC_START);
	if (ret) {
		return ret;
	}

	cpu_base = vmm_host_iomap(REALVIEW_PBA8_GIC_CPU_BASE, 0x1000);
	ret = realview_gic_cpu_init(0, cpu_base);
	if (ret) {
		return ret;
	}

	return VMM_OK;
}
