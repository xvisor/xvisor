#ifndef MC146818RTC_H
#define MC146818RTC_H

#include <vmm_timer.h>
#include "emu/rtc/mc146818rtc_regs.h"

struct cmos_rtc_state;
typedef struct cmos_rtc_state cmos_rtc_state_t;

struct cmos_rtc_state {
	u8 cmos_data[128];
	u8 cmos_index;
	s32 base_year;
	u64 base_rtc;
	u64 last_update;
	s64 offset;
	u32 irq;
	int it_shift;
	/* periodic timer */
	struct vmm_timer_event periodic_timer;
	s64 next_periodic_time;
	/* update-ended timer */
	struct vmm_timer_event update_timer;
	u64 next_alarm_time;
	u16 irq_reinject_on_ack_count;
	u32 period;
	struct vmm_guest *guest;
	struct vmm_spinlock lock;
	u8 (*rtc_cmos_read)(struct cmos_rtc_state *state, u32 offset);
	int (*rtc_cmos_write)(struct cmos_rtc_state *state, u32 offset, u8 value);
};

extern void __weak arch_guest_set_cmos(struct vmm_guest *guest,
				       struct cmos_rtc_state *s);

#endif /* !MC146818RTC_H */
