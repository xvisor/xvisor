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
 * @file main.c
 * @author Philipp Ittershagen <pit@shgn.de>
 * @brief FreeRTOS sample application
 */

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include <arm_stdio.h>

/* priorities at which the tasks are created. */
#define mainQUEUE_RECEIVE_TASK_PRIORITY (tskIDLE_PRIORITY + 2)
#define mainQUEUE_SEND_TASK_PRIORITY (tskIDLE_PRIORITY + 1)

/* The rate at which data is sent to the queue.  The 200ms value is
converted to ticks using the portTICK_PERIOD_MS constant. */
#define mainQUEUE_SEND_FREQUENCY_MS (20 / portTICK_PERIOD_MS)

/* The number of items the queue can hold.  This is 1 as the receive
task will remove items as they are added, meaning the send task should
always find the queue empty. */
#define mainQUEUE_LENGTH (1)

static void recv_task(void *params);
static void send_task(void *params);

static QueueHandle_t queue_hndl = NULL;

void main_blinky(void)
{
        queue_hndl = xQueueCreate(mainQUEUE_LENGTH, sizeof(uint32_t));

        if (queue_hndl != NULL) {

                xTaskCreate(recv_task, "RX", configMINIMAL_STACK_SIZE, NULL,
                            mainQUEUE_RECEIVE_TASK_PRIORITY, NULL);

                xTaskCreate(send_task, "TX", configMINIMAL_STACK_SIZE, NULL,
                            mainQUEUE_SEND_TASK_PRIORITY, NULL);

                vTaskStartScheduler();
        }

        for (;;)
                ;
}

static void send_task(void *params)
{
        TickType_t next_wake_time = 0;
        const unsigned long send_val = 100UL;

        (void)params;

        for (;;) {
                vTaskDelayUntil(&next_wake_time, mainQUEUE_SEND_FREQUENCY_MS);

#if configUSE_TRACE_FACILITY == 1
                arm_printf("%s @%d\n", __func__, xTaskGetTickCount());
#endif

                xQueueSend(queue_hndl, &send_val, 0U);
        }
}

static void recv_task(void *params)
{
        unsigned long rxval;
        (void)params;

        for (;;) {
                xQueueReceive(queue_hndl, &rxval, portMAX_DELAY);

#if configUSE_TRACE_FACILITY == 1
                arm_printf("%s @%d\n", __func__, xTaskGetTickCount());
#endif
        }
}
