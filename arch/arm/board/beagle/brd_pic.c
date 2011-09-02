/**
 * Copyright (c) 2011 Pranav Sawargaonkar.
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
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief board specific progammable interrupt contoller
 */

#include <omap3/intc.h>
#include <vmm_board.h>
#include <vmm_error.h>

int vmm_pic_cpu_to_host_map(u32 cpu_irq_no)
{
	return omap3_intc_active_irq(cpu_irq_no);
}

int vmm_pic_pre_condition(u32 host_irq_no)
{
	return VMM_OK;
}

int vmm_pic_post_condition(u32 host_irq_no)
{
	return omap3_intc_ack_irq(host_irq_no);
}

int vmm_pic_irq_enable(u32 host_irq_no)
{
	return omap3_intc_unmask(host_irq_no);
}

int vmm_pic_irq_disable(u32 host_irq_no)
{
	return omap3_intc_mask(host_irq_no);
}

int vmm_pic_init(void)
{
	return omap3_intc_init();
}
