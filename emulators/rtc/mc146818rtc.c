/*
 * MC146818 RTC emulation
 *
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
 * @author Himanshu Chauhan <hschauhan@nulltrace.org>
 * @brief MC146818 RTC Emulator
 * @details This source file implements the MC146818 rtc and bios
 * non-volatile memory.
 *
 * The source has been largely adapted from QEMU  hw/timer/mc146818rtc.c
 * 
 * Copyright (c) 2003-2004 Fabrice Bellard
 * The original code is licensed under the GPL.
 */

#include <vmm_stdio.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_timer.h>
#include <vmm_wallclock.h>
#include <vmm_devemu.h>
#include <vmm_spinlocks.h>
#include <libs/mathlib.h>
#include <emu/rtc/mc146818rtc.h>

#define MODULE_DESC			"MC146818 RTC Emulator"
#define MODULE_AUTHOR			"Himanshu Chauhan"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			mc146818_emulator_init
#define	MODULE_EXIT			mc146818_emulator_exit

enum {
	CMOS_LOG_LVL_ERR,
	CMOS_LOG_LVL_INFO,
	CMOS_LOG_LVL_DEBUG,
	CMOS_LOG_LVL_VERBOSE
};

static int cmos_default_log_lvl = CMOS_LOG_LVL_INFO;

#define CMOS_LOG(lvl, fmt, args...)					\
	do {								\
		if (CMOS_LOG_##lvl <= cmos_default_log_lvl) {		\
			vmm_printf("(%s:%d) " fmt, __func__,		\
				   __LINE__, ##args);			\
		}							\
	}while(0);

#define SEC_PER_MIN     60
#define MIN_PER_HOUR    60
#define SEC_PER_HOUR    3600
#define HOUR_PER_DAY    24
#define SEC_PER_DAY     86400

#define RTC_REINJECT_ON_ACK_COUNT 20
#define RTC_CLOCK_RATE            32768
#define UIP_HOLD_LENGTH           (8 * NSEC_PER_SEC / RTC_CLOCK_RATE)

typedef u64 time_t;

time_t mktimegm(struct vmm_timeinfo *tm)
{
	time_t t;
	int y = tm->tm_year + 1900, m = tm->tm_mon + 1, d = tm->tm_mday;
	if (m < 3) {
		m += 12;
		y--;
	}
	t = 86400ULL * (d + (153 * m - 457) / 5 + 365 * y + y / 4 - y / 100 +
			y / 400 - 719469);
	t += 3600 * tm->tm_hour + 60 * tm->tm_min + tm->tm_sec;
	return t;
}

static void rtc_set_time(cmos_rtc_state_t *s);
static void rtc_update_time(cmos_rtc_state_t *s);
static void rtc_set_cmos(cmos_rtc_state_t *s, const struct vmm_timeinfo *tm);
static inline int rtc_from_bcd(cmos_rtc_state_t *s, int a);
static u64 get_next_alarm(cmos_rtc_state_t *s);

static u8 rtc_cmos_read_memory(struct cmos_rtc_state *state, u32 offset)
{
	if (offset >=0 && offset <= 128)
		return state->cmos_data[offset];

	return 0;
}

static int rtc_cmos_write_memory(struct cmos_rtc_state *state, u32 offset, u8 value)
{
	if (offset >= 0 && offset <= 128) {
		state->cmos_data[offset] = value;
		return VMM_OK;
	}

	return VMM_EFAIL;
}

static void cmos_irq_raise(struct cmos_rtc_state *s)
{
	vmm_devemu_emulate_irq(s->guest, s->irq, 1);
}

static void cmos_irq_lower(struct cmos_rtc_state *s)
{
	vmm_devemu_emulate_irq(s->guest, s->irq, 0);
}

static inline bool rtc_running(cmos_rtc_state_t *s)
{
	return (!(s->cmos_data[RTC_REG_B] & REG_B_SET) &&
		(s->cmos_data[RTC_REG_A] & 0x70) <= 0x20);
}

static u64 get_guest_rtc_ns(cmos_rtc_state_t *s)
{
	u64 guest_rtc;
	u64 guest_clock = vmm_timer_timestamp();

	guest_rtc = s->base_rtc * NSEC_PER_SEC
		+ guest_clock - s->last_update + s->offset;
	return guest_rtc;
}

/* handle periodic timer */
static void periodic_timer_update(cmos_rtc_state_t *s, s64 current_time)
{
	int period_code, period;
	s64 cur_clock, next_irq_clock;

	period_code = s->cmos_data[RTC_REG_A] & 0x0f;
	if (period_code != 0
	    && (s->cmos_data[RTC_REG_B] & REG_B_PIE)) {
		if (period_code <= 2)
			period_code += 7;
		/* period in 32 Khz cycles */
		period = 1 << (period_code - 1);
		/* compute 32 khz clock */
		cur_clock = muldiv64(current_time, RTC_CLOCK_RATE, 1000000000LL);
		next_irq_clock = (cur_clock & ~(period - 1)) + period;
		s->next_periodic_time =
			muldiv64(next_irq_clock, 1000000000LL, RTC_CLOCK_RATE) + 1;
		vmm_timer_event_stop(&s->periodic_timer);
		vmm_timer_event_start(&s->periodic_timer, s->next_periodic_time);
	} else {
		vmm_timer_event_stop(&s->periodic_timer);
	}
}

static void rtc_periodic_timer(struct vmm_timer_event *event)
{
	cmos_rtc_state_t *s = (cmos_rtc_state_t *)event->priv;

	periodic_timer_update(s, s->next_periodic_time);
	s->cmos_data[RTC_REG_C] |= REG_C_PF;
	if (s->cmos_data[RTC_REG_B] & REG_B_PIE) {
		s->cmos_data[RTC_REG_C] |= REG_C_IRQF;
			cmos_irq_raise(s);
	}
}

/* handle update-ended timer */
static void check_update_timer(cmos_rtc_state_t *s)
{
	u64 next_update_time;
	u64 guest_nsec;
	int next_alarm_sec;

	/* From the data sheet: "Holding the dividers in reset prevents
	 * interrupts from operating, while setting the SET bit allows"
	 * them to occur.  However, it will prevent an alarm interrupt
	 * from occurring, because the time of day is not updated.
	 */
	if ((s->cmos_data[RTC_REG_A] & 0x60) == 0x60) {
		vmm_timer_event_stop(&s->update_timer);
		return;
	}
	if ((s->cmos_data[RTC_REG_C] & REG_C_UF) &&
	    (s->cmos_data[RTC_REG_B] & REG_B_SET)) {
		vmm_timer_event_stop(&s->update_timer);
		return;
	}
	if ((s->cmos_data[RTC_REG_C] & REG_C_UF) &&
	    (s->cmos_data[RTC_REG_C] & REG_C_AF)) {
		vmm_timer_event_stop(&s->update_timer);
		return;
	}

	guest_nsec = get_guest_rtc_ns(s) % NSEC_PER_SEC;
	/* if UF is clear, reprogram to next second */
	next_update_time = vmm_timer_timestamp()
		+ NSEC_PER_SEC - guest_nsec;

	/* Compute time of next alarm.  One second is already accounted
	 * for in next_update_time.
	 */
	next_alarm_sec = get_next_alarm(s);
	s->next_alarm_time = next_update_time + (next_alarm_sec - 1) * NSEC_PER_SEC;

	if (s->cmos_data[RTC_REG_C] & REG_C_UF) {
		/* UF is set, but AF is clear.  Program the timer to target
		 * the alarm time.  */
		next_update_time = s->next_alarm_time;
	}

	vmm_timer_event_stop(&s->update_timer);
	vmm_timer_event_start(&s->update_timer, next_update_time);
}

static inline u8 convert_hour(cmos_rtc_state_t *s, u8 hour)
{
	if (!(s->cmos_data[RTC_REG_B] & REG_B_24H)) {
		hour %= 12;
		if (s->cmos_data[RTC_HOURS] & 0x80) {
			hour += 12;
		}
	}
	return hour;
}

static u64 get_next_alarm(cmos_rtc_state_t *s)
{
	s32 alarm_sec, alarm_min, alarm_hour, cur_hour, cur_min, cur_sec;
	s32 hour, min, sec;

	rtc_update_time(s);

	alarm_sec = rtc_from_bcd(s, s->cmos_data[RTC_SECONDS_ALARM]);
	alarm_min = rtc_from_bcd(s, s->cmos_data[RTC_MINUTES_ALARM]);
	alarm_hour = rtc_from_bcd(s, s->cmos_data[RTC_HOURS_ALARM]);
	alarm_hour = alarm_hour == -1 ? -1 : convert_hour(s, alarm_hour);

	cur_sec = rtc_from_bcd(s, s->cmos_data[RTC_SECONDS]);
	cur_min = rtc_from_bcd(s, s->cmos_data[RTC_MINUTES]);
	cur_hour = rtc_from_bcd(s, s->cmos_data[RTC_HOURS]);
	cur_hour = convert_hour(s, cur_hour);

	if (alarm_hour == -1) {
		alarm_hour = cur_hour;
		if (alarm_min == -1) {
			alarm_min = cur_min;
			if (alarm_sec == -1) {
				alarm_sec = cur_sec + 1;
			} else if (cur_sec > alarm_sec) {
				alarm_min++;
			}
		} else if (cur_min == alarm_min) {
			if (alarm_sec == -1) {
				alarm_sec = cur_sec + 1;
			} else {
				if (cur_sec > alarm_sec) {
					alarm_hour++;
				}
			}
			if (alarm_sec == SEC_PER_MIN) {
				/* wrap to next hour, minutes is not in don't care mode */
				alarm_sec = 0;
				alarm_hour++;
			}
		} else if (cur_min > alarm_min) {
			alarm_hour++;
		}
	} else if (cur_hour == alarm_hour) {
		if (alarm_min == -1) {
			alarm_min = cur_min;
			if (alarm_sec == -1) {
				alarm_sec = cur_sec + 1;
			} else if (cur_sec > alarm_sec) {
				alarm_min++;
			}
			
			if (alarm_sec == SEC_PER_MIN) {
				alarm_sec = 0;
				alarm_min++;
			}
			/* wrap to next day, hour is not in don't care mode */
			alarm_min %= MIN_PER_HOUR;
		} else if (cur_min == alarm_min) {
			if (alarm_sec == -1) {
				alarm_sec = cur_sec + 1;
			}
			/* wrap to next day, hours+minutes not in don't care mode */
			alarm_sec %= SEC_PER_MIN;
		}
	}
	
	/* values that are still don't care fire at the next min/sec */
	if (alarm_min == -1) {
		alarm_min = 0;
	}
	if (alarm_sec == -1) {
		alarm_sec = 0;
	}
	
	/* keep values in range */
	if (alarm_sec == SEC_PER_MIN) {
		alarm_sec = 0;
		alarm_min++;
	}
	if (alarm_min == MIN_PER_HOUR) {
		alarm_min = 0;
		alarm_hour++;
	}
	alarm_hour %= HOUR_PER_DAY;
	
	hour = alarm_hour - cur_hour;
	min = hour * MIN_PER_HOUR + alarm_min - cur_min;
	sec = min * SEC_PER_MIN + alarm_sec - cur_sec;
	return sec <= 0 ? sec + SEC_PER_DAY : sec;
}

static void rtc_update_timer(struct vmm_timer_event *ev)
{
	cmos_rtc_state_t *s = (cmos_rtc_state_t *)ev->priv;
	s32 irqs = REG_C_UF;
	s32 new_irqs;


	CMOS_LOG(LVL_ERR, "%s: enter\n", __func__);

	if ((s->cmos_data[RTC_REG_A] & 0x60) == 0x60)
		vmm_panic("%s: Invalid DIV state in register A\n", __func__);
	
	/* UIP might have been latched, update time and clear it.  */
	rtc_update_time(s);
	s->cmos_data[RTC_REG_A] &= ~REG_A_UIP;
	
	if (vmm_timer_timestamp() >= s->next_alarm_time) {
		irqs |= REG_C_AF;
		if (s->cmos_data[RTC_REG_B] & REG_B_AIE) {
			/* FIXME: Do system wakeup */
		}
	}
	
	new_irqs = irqs & ~s->cmos_data[RTC_REG_C];
	s->cmos_data[RTC_REG_C] |= irqs;
	if ((new_irqs & s->cmos_data[RTC_REG_B]) != 0) {
		s->cmos_data[RTC_REG_C] |= REG_C_IRQF;
		cmos_irq_raise(s);
	}
	check_update_timer(s);
}

static int cmos_ioport_write(cmos_rtc_state_t *s, u32 addr,
                              u32 src_mask, u32 data)
{
	CMOS_LOG(LVL_INFO, "CMOS: write: Addr: 0x%x mask: 0x%x data: 0x%x\n",
		 addr, src_mask, data);

	if ((addr & 1) == 0) {
		s->cmos_index = data & 0x7f;
		CMOS_LOG(LVL_INFO, "CMOS: Index: %d\n", s->cmos_index);
	} else {
		CMOS_LOG(LVL_INFO,"cmos: write index=0x%02x val=0x%02x\n",
			 s->cmos_index, data);
		switch(s->cmos_index) {
		case RTC_SECONDS_ALARM:
		case RTC_MINUTES_ALARM:
		case RTC_HOURS_ALARM:
			s->cmos_data[s->cmos_index] = data;
			check_update_timer(s);
			break;
		case RTC_IBM_PS2_CENTURY_BYTE:
			s->cmos_index = RTC_CENTURY;
			/* fall through */
		case RTC_CENTURY:
		case RTC_SECONDS:
		case RTC_MINUTES:
		case RTC_HOURS:
		case RTC_DAY_OF_WEEK:
		case RTC_DAY_OF_MONTH:
		case RTC_MONTH:
		case RTC_YEAR:
			s->cmos_data[s->cmos_index] = data;
			/* if in set mode, do not update the time */
			if (rtc_running(s)) {
				rtc_set_time(s);
				check_update_timer(s);
			}
			break;
		case RTC_REG_A:
			if ((data & 0x60) == 0x60) {
				if (rtc_running(s)) {
					rtc_update_time(s);
				}
				/* What happens to UIP when divider reset is enabled is
				 * unclear from the datasheet.  Shouldn't matter much
				 * though.
				 */
				s->cmos_data[RTC_REG_A] &= ~REG_A_UIP;
			} else if (((s->cmos_data[RTC_REG_A] & 0x60) == 0x60) &&
				   (data & 0x70)  <= 0x20) {
				/* when the divider reset is removed, the first update cycle
				 * begins one-half second later*/
				if (!(s->cmos_data[RTC_REG_B] & REG_B_SET)) {
					s->offset = 500000000;
					rtc_set_time(s);
				}
				s->cmos_data[RTC_REG_A] &= ~REG_A_UIP;
			}
			/* UIP bit is read only */
			s->cmos_data[RTC_REG_A] = (data & ~REG_A_UIP) |
				(s->cmos_data[RTC_REG_A] & REG_A_UIP);
			periodic_timer_update(s, vmm_timer_timestamp());
			check_update_timer(s);
			break;
		case RTC_REG_B:
			if (data & REG_B_SET) {
				/* update cmos to when the rtc was stopping */
				if (rtc_running(s)) {
					rtc_update_time(s);
				}
				/* set mode: reset UIP mode */
				s->cmos_data[RTC_REG_A] &= ~REG_A_UIP;
				data &= ~REG_B_UIE;
			} else {
				/* if disabling set mode, update the time */
				if ((s->cmos_data[RTC_REG_B] & REG_B_SET) &&
				    (s->cmos_data[RTC_REG_A] & 0x70) <= 0x20) {
					s->offset = get_guest_rtc_ns(s) % NSEC_PER_SEC;
					rtc_set_time(s);
				}
			}
			/* if an interrupt flag is already set when the interrupt
			 * becomes enabled, raise an interrupt immediately.  */
			if (data & s->cmos_data[RTC_REG_C] & REG_C_MASK) {
				s->cmos_data[RTC_REG_C] |= REG_C_IRQF;
				cmos_irq_raise(s);
			} else {
				s->cmos_data[RTC_REG_C] &= ~REG_C_IRQF;
				cmos_irq_lower(s);
			}
			s->cmos_data[RTC_REG_B] = data;
			periodic_timer_update(s, vmm_timer_timestamp());
			check_update_timer(s);
			break;
		case RTC_REG_C:
		case RTC_REG_D:
			/* cannot write to them */
			break;
		default:
			s->cmos_data[s->cmos_index] = data;
			break;
		}
	}

	return VMM_OK;
}

static inline int rtc_to_bcd(cmos_rtc_state_t *s, int a)
{
	if (s->cmos_data[RTC_REG_B] & REG_B_DM) {
		return a;
	} else {
		return ((a / 10) << 4) | (a % 10);
	}
}

static inline int rtc_from_bcd(cmos_rtc_state_t *s, int a)
{
	if ((a & 0xc0) == 0xc0) {
		return -1;
	}
	if (s->cmos_data[RTC_REG_B] & REG_B_DM) {
		return a;
	} else {
		return ((a >> 4) * 10) + (a & 0x0f);
	}
}

static void rtc_get_time(cmos_rtc_state_t *s, struct vmm_timeinfo *tm)
{
	tm->tm_sec = rtc_from_bcd(s, s->cmos_data[RTC_SECONDS]);
	tm->tm_min = rtc_from_bcd(s, s->cmos_data[RTC_MINUTES]);
	tm->tm_hour = rtc_from_bcd(s, s->cmos_data[RTC_HOURS] & 0x7f);
	if (!(s->cmos_data[RTC_REG_B] & REG_B_24H)) {
		tm->tm_hour %= 12;
		if (s->cmos_data[RTC_HOURS] & 0x80) {
			tm->tm_hour += 12;
		}
	}
	tm->tm_wday = rtc_from_bcd(s, s->cmos_data[RTC_DAY_OF_WEEK]) - 1;
	tm->tm_mday = rtc_from_bcd(s, s->cmos_data[RTC_DAY_OF_MONTH]);
	tm->tm_mon = rtc_from_bcd(s, s->cmos_data[RTC_MONTH]) - 1;
	tm->tm_year =
		rtc_from_bcd(s, s->cmos_data[RTC_YEAR]) + s->base_year +
		rtc_from_bcd(s, s->cmos_data[RTC_CENTURY]) * 100 - 1900;
}

static void rtc_set_time(cmos_rtc_state_t *s)
{
	struct vmm_timeinfo tm;

	rtc_get_time(s, &tm);
	s->base_rtc = mktimegm(&tm);
	s->last_update = vmm_timer_timestamp();
}

static void rtc_set_cmos(cmos_rtc_state_t *s, const struct vmm_timeinfo *tm)
{
	int year;
	
	s->cmos_data[RTC_SECONDS] = rtc_to_bcd(s, tm->tm_sec);
	s->cmos_data[RTC_MINUTES] = rtc_to_bcd(s, tm->tm_min);
	if (s->cmos_data[RTC_REG_B] & REG_B_24H) {
		/* 24 hour format */
		s->cmos_data[RTC_HOURS] = rtc_to_bcd(s, tm->tm_hour);
	} else {
        /* 12 hour format */
		int h = (tm->tm_hour % 12) ? tm->tm_hour % 12 : 12;
		s->cmos_data[RTC_HOURS] = rtc_to_bcd(s, h);
		if (tm->tm_hour >= 12)
			s->cmos_data[RTC_HOURS] |= 0x80;
	}
	s->cmos_data[RTC_DAY_OF_WEEK] = rtc_to_bcd(s, tm->tm_wday + 1);
	s->cmos_data[RTC_DAY_OF_MONTH] = rtc_to_bcd(s, tm->tm_mday);
	s->cmos_data[RTC_MONTH] = rtc_to_bcd(s, tm->tm_mon + 1);
	year = tm->tm_year + 1900 - s->base_year;
	s->cmos_data[RTC_YEAR] = rtc_to_bcd(s, year % 100);
	s->cmos_data[RTC_CENTURY] = rtc_to_bcd(s, year / 100);
}

static void rtc_update_time(cmos_rtc_state_t *s)
{
	struct vmm_timeinfo tm;
	time_t guest_sec;
	s64 guest_nsec;

	guest_nsec = get_guest_rtc_ns(s);
	guest_sec = guest_nsec / NSEC_PER_SEC;

	vmm_wallclock_mkinfo(guest_sec, 0, &tm);
	
	/* Is SET flag of Register B disabled? */
	if ((s->cmos_data[RTC_REG_B] & REG_B_SET) == 0) {
		rtc_set_cmos(s, &tm);
	}
}

static int update_in_progress(cmos_rtc_state_t *s)
{
	s64 guest_nsec;

	if (!rtc_running(s)) {
		return 0;
	}
	if (vmm_timer_event_pending(&s->update_timer)) {
		s64 next_update_time = vmm_timer_event_expiry_time(&s->update_timer);
		/* Latch UIP until the timer expires.  */
		if (vmm_timer_timestamp() >=
		    (next_update_time - UIP_HOLD_LENGTH)) {
			s->cmos_data[RTC_REG_A] |= REG_A_UIP;
			return 1;
		}
	}

	guest_nsec = get_guest_rtc_ns(s);
	/* UIP bit will be set at last 244us of every second. */
	if ((guest_nsec % NSEC_PER_SEC) >= (NSEC_PER_SEC - UIP_HOLD_LENGTH)) {
		return 1;
	}
	return 0;
}

static u64 cmos_ioport_read(cmos_rtc_state_t *s, u32 addr,
			    u32 *dst)
{
	int ret = VMM_OK;
	CMOS_LOG(LVL_INFO, "CMOS Read: addr: 0x%x\n", addr);

	if ((addr & 1) == 0) {
		*dst = 0xff;
		CMOS_LOG(LVL_INFO, "Returning FF\n");
	} else {
		CMOS_LOG(LVL_INFO, "CMOS INDEX: %d\n", s->cmos_index);
		switch(s->cmos_index) {
		case RTC_IBM_PS2_CENTURY_BYTE:
			s->cmos_index = RTC_CENTURY;
			/* fall through */
		case RTC_CENTURY:
		case RTC_SECONDS:
		case RTC_MINUTES:
		case RTC_HOURS:
		case RTC_DAY_OF_WEEK:
		case RTC_DAY_OF_MONTH:
		case RTC_MONTH:
		case RTC_YEAR:
			/* if not in set mode, calibrate cmos before
			 * reading*/
			if (rtc_running(s)) {
				rtc_update_time(s);
			}
			*dst = s->cmos_data[s->cmos_index];
			break;
		case RTC_REG_A:
			if (update_in_progress(s)) {
				s->cmos_data[s->cmos_index] |= REG_A_UIP;
			} else {
				s->cmos_data[s->cmos_index] &= ~REG_A_UIP;
			}
			*dst = s->cmos_data[s->cmos_index];
			break;
		case RTC_REG_C:
			*dst = s->cmos_data[s->cmos_index];
			cmos_irq_lower(s);
			s->cmos_data[RTC_REG_C] = 0x00;
			if (*dst & (REG_C_UF | REG_C_AF)) {
				check_update_timer(s);
			}
			break;
		default:
			*dst = s->cmos_data[s->cmos_index];
			break;
		}
		CMOS_LOG(LVL_INFO,"cmos: read index=0x%02x val=0x%02x\n",
			 s->cmos_index, *dst);
	}

	return ret;
}

void rtc_set_memory(cmos_rtc_state_t *s, int addr, int val)
{
	if (addr >= 0 && addr <= 127)
		s->cmos_data[addr] = val;
}

int rtc_get_memory(cmos_rtc_state_t *s, int addr)
{
	return s->cmos_data[addr];
}

static int rtc_set_date_from_host(cmos_rtc_state_t *s)
{
	int rc;
	struct vmm_timezone tz;
	struct vmm_timeval tv;
	struct vmm_timeinfo tm;

	if ((rc = vmm_wallclock_get_timeofday(&tv, &tz))) {
		return rc;
	}

	tv.tv_sec -= tz.tz_minuteswest * 60;

	s->base_rtc = tv.tv_sec;//mktimegm(&tm);
	s->last_update = vmm_timer_timestamp();
	s->offset = 0;

	vmm_wallclock_mkinfo(tv.tv_sec, 0, &tm);

	rtc_set_cmos(s, &tm);

	return VMM_OK;
}

static int mc146818_emulator_reset(struct vmm_emudev *edev)
{
	cmos_rtc_state_t *s = (cmos_rtc_state_t *)edev->priv;
	
	s->cmos_data[RTC_REG_B] &= ~(REG_B_PIE | REG_B_AIE | REG_B_SQWE);
	s->cmos_data[RTC_REG_C] &= ~(REG_C_UF | REG_C_IRQF | REG_C_PF | REG_C_AF);
	check_update_timer(s);

	cmos_irq_lower(s);

	return VMM_OK;
}

static int mc146818_state_init(cmos_rtc_state_t *s)
{
	s->cmos_data[RTC_REG_A] = 0x26;
	s->cmos_data[RTC_REG_B] = 0x02;
	s->cmos_data[RTC_REG_C] = 0x00;
	s->cmos_data[RTC_REG_D] = 0x80;

	/* This is for historical reasons.  The default base year qdev property
	 * was set to 2000 for most machine types before the century byte was
	 * implemented.
	 *
	 * This if statement means that the century byte will be always 0
	 * (at least until 2079...) for base_year = 1980, but will be set
	 * correctly for base_year = 2000.
	 */
	if (s->base_year == 2000) {
		s->base_year = 0;
	}

	rtc_set_date_from_host(s);

	check_update_timer(s);

	return VMM_OK;
}

static int mc146818_emulator_read8(struct vmm_emudev *edev,
				   physical_addr_t offset, 
				   u8 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = cmos_ioport_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFF;
	}

	return rc;
}

static int mc146818_emulator_read16(struct vmm_emudev *edev,
				   physical_addr_t offset, 
				   u16 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = cmos_ioport_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFFFF;
	}

	return rc;
}

static int mc146818_emulator_read32(struct vmm_emudev *edev,
				    physical_addr_t offset, 
				    u32 *dst)
{
	return cmos_ioport_read(edev->priv, offset, dst);
}

static int mc146818_emulator_write8(struct vmm_emudev *edev,
				    physical_addr_t offset, 
				    u8 src)
{
	CMOS_LOG(LVL_INFO, "offset: 0x%lx src: 0x%x\n", offset, src);
	return cmos_ioport_write(edev->priv, offset, 0xFFFFFF00, src);
}

static int mc146818_emulator_write16(struct vmm_emudev *edev,
				    physical_addr_t offset, 
				    u16 src)
{
	CMOS_LOG(LVL_INFO, "offset: 0x%lx src: 0x%x\n", offset, src);
	return cmos_ioport_write(edev->priv, offset, 0xFFFF0000, src);
}

static int mc146818_emulator_write32(struct vmm_emudev *edev,
				    physical_addr_t offset, 
				    u32 src)
{
	CMOS_LOG(LVL_INFO, "offset: 0x%lx src: 0x%x\n", offset, src);
	return cmos_ioport_write(edev->priv, offset, 0x00000000, src);
}

static int mc146818_emulator_probe(struct vmm_guest *guest, struct vmm_emudev *edev,
				   const struct vmm_devtree_nodeid *eid)
{
	int rc;
	cmos_rtc_state_t *s = vmm_zalloc(sizeof(cmos_rtc_state_t));

	CMOS_LOG(LVL_ERR, "Probing MC146818 RTC Emulator.\n");

	if (!s)
		goto _error;

	s->guest = guest;

	INIT_SPIN_LOCK(&s->lock);

	rc = vmm_devtree_irq_get(edev->node, &s->irq, 0);
	if (rc) {
		CMOS_LOG(LVL_ERR, "Failed to get IRQ entry in guest DTS.\n");
		goto _error;
	}

	INIT_TIMER_EVENT(&s->periodic_timer, &rtc_periodic_timer, s);
	INIT_TIMER_EVENT(&s->update_timer, &rtc_update_timer, s);

	if (mc146818_state_init(s) != VMM_OK) {
		CMOS_LOG(LVL_ERR, "Failed to initialize default state of CMOS/RTC\n");
		goto _error;
	}

	edev->priv = (void *)s;

	s->rtc_cmos_read = &rtc_cmos_read_memory;
	s->rtc_cmos_write = &rtc_cmos_write_memory;

	arch_guest_set_cmos(guest, s);

	return VMM_OK;

 _error:
	if (s) vmm_free(s);
	s = NULL;
	return VMM_EFAIL;
}

static int mc146818_emulator_remove(struct vmm_emudev *edev)
{
	struct cmos_rtc_state *s = edev->priv;

	vmm_free(s);

	return VMM_OK;
}

static struct vmm_devtree_nodeid mc146818_emuid_table[] = {
	{
		.type = "rtc", 
		.compatible = "motorola,mc146818", 
		.data = NULL,
	},
	{ /* end of list */ },
};

static struct vmm_emulator mc146818rtc_emulator = {
	.name = "mc146818",
	.match_table = mc146818_emuid_table,
	.endian = VMM_DEVEMU_LITTLE_ENDIAN,
	.probe = mc146818_emulator_probe,
	.read8 = mc146818_emulator_read8,
	.write8 = mc146818_emulator_write8,
	.read16 = mc146818_emulator_read16,
	.write16 = mc146818_emulator_write16,
	.read32 = mc146818_emulator_read32,
	.write32 = mc146818_emulator_write32,
	.reset = mc146818_emulator_reset,
	.remove = mc146818_emulator_remove
};

static int __init mc146818_emulator_init(void)
{
	return vmm_devemu_register_emulator(&mc146818rtc_emulator);
}

static void __exit mc146818_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&mc146818rtc_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
		   MODULE_AUTHOR, 
		   MODULE_LICENSE, 
		   MODULE_IPRIORITY, 
		   MODULE_INIT, 
		   MODULE_EXIT);
