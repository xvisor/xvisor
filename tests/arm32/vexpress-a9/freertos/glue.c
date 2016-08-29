/**
 * Copyright (c) 2016 Philipp Ittershagen.
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
 * @file glue.c
 * @author Philipp Ittershagen <pit@shgn.de>
 * @brief Implementation of architecture-dependent FreeRTOS functions.
 */

#include "FreeRTOS.h"
#include "task.h"

#include <arm_io.h>
#include <arm_irq.h>
#include <arm_plat.h>
#include <arm_board.h>
#include <arm_timer.h>
#include <arm_stdio.h>

extern void main_blinky(void);

/* no MMU support, disabling firmware interrupt handlers */
void arm_mmu_syscall(struct pt_regs *regs) {}
void arm_mmu_prefetch_abort(struct pt_regs *regs) {}
void arm_mmu_data_abort(struct pt_regs *regs) {}

#define TIMER_INTCLR 0x0c

static int timer_tick_handler(u32 irq, struct pt_regs *regs)
{
        FreeRTOS_Tick_Handler();
        arm_writel(1, (void *)(V2M_TIMER0 + TIMER_INTCLR));
        return 0;
}

/* configure the timer for FreeRTOS */
void vConfigureTickInterrupt()
{
        arm_timer_init(configTICK_RATE_HZ);

        /* 'steal' interrupt handler */
        arm_irq_register(IRQ_V2M_TIMER0, &timer_tick_handler);

        arm_timer_enable();
}

#define MAX_IRQS 1024

void vApplicationIRQHandler(u32 irq)
{
        extern arm_irq_handler_t irq_hndls[MAX_IRQS];
        if (arm_board_pic_ack_irq(irq)) {
                while (1)
                        ;
        }
        if (irq_hndls[irq]) {
                if (irq_hndls[irq](irq, NULL))
                        while (1)
                                ;
        }
}

void vAssertCalled(const char *pcFile, unsigned long ulLine)
{
        volatile unsigned long ul = 0;

        (void)pcFile;
        (void)ulLine;

        taskENTER_CRITICAL();
        {
                arm_printf("%s: file=%s, line=%d!\n", __func__, pcFile, ulLine);
                /* Set ul to a non-zero value using the debugger to
                step out of this function. */
                while (ul == 0) {
                        portNOP();
                }
        }
        taskEXIT_CRITICAL();
}

void arm_init(void)
{
        arm_irq_disable();
        arm_irq_setup();
        arm_stdio_init();

        /* FreeRTOS will call vConfigureTickInterrupt and enable IRQs */
}

int arm_main(void)
{
        arm_puts("Welcome to FreeRTOS!\n");

        main_blinky();

        /* Don't expect to reach here. */
        return 0;
}

/* configUSE_STATIC_ALLOCATION is set to 1, so the application must
provide an implementation of vApplicationGetIdleTaskMemory() to
provide the memory that is used by the Idle task. */
void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
        /* If the buffers to be provided to the Idle task are declared
        inside this function then they must be declared static -
        otherwise they will be allocated on the stack and so not
        exists after this function exits. */
        static StaticTask_t xIdleTaskTCB;
        static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];

        /* Pass out a pointer to the StaticTask_t structure in which
        the Idle task's state will be stored. */
        *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;

        /* Pass out the array that will be used as the Idle task's stack. */
        *ppxIdleTaskStackBuffer = uxIdleTaskStack;

        /* Pass out the size of the array pointed to by
        *ppxIdleTaskStackBuffer.  Note that, as the array is
        necessarily of type StackType_t, configMINIMAL_STACK_SIZE is
        specified in words, not bytes. */
        *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}
