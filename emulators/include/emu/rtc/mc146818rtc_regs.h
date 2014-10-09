/*
 * QEMU MC146818 RTC emulation
 *
 * Copyright (c) 2003-2004 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#ifndef RTC_REGS_H
#define RTC_REGS_H

#define RTC_ISA_IRQ 8

#define RTC_SECONDS             0
#define RTC_SECONDS_ALARM       1
#define RTC_MINUTES             2
#define RTC_MINUTES_ALARM       3
#define RTC_HOURS               4
#define RTC_HOURS_ALARM         5
#define RTC_ALARM_DONT_CARE    0xC0

#define RTC_DAY_OF_WEEK         6
#define RTC_DAY_OF_MONTH        7
#define RTC_MONTH               8
#define RTC_YEAR                9

#define RTC_REG_A               10
#define RTC_REG_B               11
#define RTC_REG_C               12
#define RTC_REG_D               13

/* PC cmos mappings */
#define RTC_CENTURY              0x32
#define RTC_IBM_PS2_CENTURY_BYTE 0x37

#define REG_A_UIP 0x80

#define REG_B_SET  0x80
#define REG_B_PIE  0x40
#define REG_B_AIE  0x20
#define REG_B_UIE  0x10
#define REG_B_SQWE 0x08
#define REG_B_DM   0x04
#define REG_B_24H  0x02

#define REG_C_UF   0x10
#define REG_C_IRQF 0x80
#define REG_C_PF   0x40
#define REG_C_AF   0x20
#define REG_C_MASK 0x70

/* Xvisor's CMOS Memory Map. Similar to Bochs and Qemu */
#define RTC_REG_SHUTDOWN_STATUS		0x0f
#define RTC_REG_FD_DRIVE_TYPE		0x10
/***********************************/
/* CMOS Memory Map. Fill as needed */
/***********************************/
#define RTC_REG_BASE_MEM_LO		0x15
#define RTC_REG_BASE_MEM_HI		0x16
#define RTC_REG_EXT_MEM_LO		0x17 /* in K */
#define RTC_REG_EXT_MEM_HI		0x18

#define RTC_REG_EXT_MEM_LO_COPY		0x30
#define RTC_REG_EXT_MEM_HI_COPY		0x31

#define RTC_REG_EXT_MEM_64K_LO		0x34
#define RTC_REG_EXT_MEM_64K_HI		0x35

#define RTC_REG_EXT_MEM_ABV_4GB_1	0x5b
#define RTC_REG_EXT_MEM_ABV_4GB_2	0x5c
#define RTC_REG_EXT_MEM_ABV_4GB_3	0x5d

#define RTC_REG_NR_PROCESSORS		0x5f

#endif
