/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file cpu_interrupts.c
 * @version 1.0
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief source code for handling cpu interrupts
 */

#include <vmm_error.h>
#include <vmm_types.h>
#include <vmm_cpu.h>
#include <vmm_scheduler.h>
#include <vmm_host_irq.h>
#include <vmm_stdio.h>
#include <cpu_emulate.h>
#include <cpu_interrupts.h>
#include <cpu_timer.h>
#include <cpu_mmu.h>
#include <cpu_asm_macros.h>

#ifdef CONFIG_I8259
#include <pics/i8259.h>
#endif

extern virtual_addr_t isa_vbase;

void setup_interrupts()
{
        u32 ebase = read_c0_ebase();
        ebase &= ~0x3FFF000UL;
        write_c0_ebase(ebase);

        u32 sr = read_c0_status();
        sr &= ~(0x01UL << 22);
        sr &= ~(0x3UL << 1);
        write_c0_status(sr);

        u32 cause = read_c0_status();
        cause |= 0x01UL << 23;
        write_c0_cause(cause);
#if CONFIG_I8259
        i8259_init((void *)(isa_vbase + 0x300), 0);
#endif
}

int vmm_cpu_irq_setup(void)
{
	setup_interrupts();
	
	return VMM_OK;
}

void vmm_cpu_irq_enable(void)
{
	enable_interrupts();
#ifdef CONFIG_I8259
        i8259_enable_int(-1); /* enable all interrupts. */
#endif
}

void vmm_cpu_irq_disable(void)
{
	disable_interrupts();
#ifdef CONFIG_I8259
        i8259_disable_int(-1); /* disable all interrupts */
#endif
}

irq_flags_t vmm_cpu_irq_save(void)
{
	irq_flags_t flags;
	__asm__ __volatile__("di %0\n\t"
			     :"=r"(flags));
	flags &= (0x0000FF00UL);
        return flags;
}

void vmm_cpu_irq_restore(irq_flags_t flags)
{
	irq_flags_t temp;
	write_c0_status(read_c0_status() | flags);
	__asm__ __volatile__("ei %0\n\t"
			     :"=r"(temp));
}

s32 generic_int_handler(vmm_user_regs_t *uregs)
{
        u32 cause = read_c0_cause();
        u32 oints = 0;

        if (cause & SYS_TIMER_INT_STATUS_MASK) {
                return handle_internal_timer_interrupt(uregs);
        }

        /* Higher the interrupt number, higher its priority */
        for (oints = NR_SYS_INT - 1; oints >= 0; oints++) {
                switch (cause & (0x01UL << (oints + NR_SYS_INT))) {
                case SYS_INT0_MASK:
                case SYS_INT1_MASK:
                case SYS_INT2_MASK:
                case SYS_INT3_MASK:
                case SYS_INT4_MASK:
                case SYS_INT5_MASK:
                case SYS_INT6_MASK:
                case SYS_INT7_MASK:
                default:
                        /* FIXME: Do something here */
                        return 0;
                        break;
                }
        }

        return 0;
}
