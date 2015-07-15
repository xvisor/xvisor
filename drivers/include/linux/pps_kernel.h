#ifndef LINUX_PPS_KERNEL_H
#define LINUX_PPS_KERNEL_H

#include <linux/time.h>

struct pps_event_time {
#ifdef CONFIG_NTP_PPS
	struct timespec ts_raw;
#endif /* CONFIG_NTP_PPS */
	struct timespec ts_real;
};

#endif /* !LINUX_PPS_KERNEL_H */
