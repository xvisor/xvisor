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
 * @file arm_main.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM test code main file
 */

#include <arm_io.h>
#include <arm_irq.h>
#include <arm_timer.h>
#include <arm_string.h>
#include <arm_stdio.h>

/* Works in supervisor mode */
void arm_init(void)
{
	arm_irq_setup();

	arm_irq_enable();

	arm_stdio_init();

	arm_timer_init(1000, 1);

	arm_timer_enable();
}

void arm_reset(void)
{
	/* Unlock Lockable reigsters */
	arm_writel(REALVIEW_SYS_LOCKVAL, 
		   (void *)(REALVIEW_SYS_BASE + REALVIEW_SYS_LOCK_OFFSET));
#if 0
	arm_writel(REALVIEW_SYS_CTRL_RESET_CONFIGCLR, 
		   (void *)(pba8_csr_base + REALVIEW_SYS_RESETCTL_OFFSET));
#else
	arm_writel(0x100, 
		   (void *)(REALVIEW_SYS_BASE + REALVIEW_SYS_RESETCTL_OFFSET));
#endif
}

/* Works in user mode */
void arm_main(void)
{
	char line[256];

	arm_puts("ARM Realview PB-A8 Test Code\n\n");

	while(1) {
		arm_puts("arm-test# ");

		arm_gets(line, 256, '\n');

		if (arm_strcmp(line, "hi") == 0) {
			arm_puts("hello\n");
		} else if (arm_strcmp(line, "hello") == 0) {
			arm_puts("hi\n");
		} else if (arm_strcmp(line, "reset") == 0) {
			arm_puts("System reset ...\n\n");
			arm_reset();
			while (1);
		} else if (arm_strcmp(line, "help") == 0) {
			arm_puts("List of commands: \n");
			arm_puts("hi    - Say hi to ARM test code\n");
			arm_puts("hello - Say hello to ARM test code\n");
			arm_puts("reset - Reset the system\n");
			arm_puts("help  - List commands and their usage\n");
		}
	}
}
