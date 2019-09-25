/**
 * Copyright (c) 2019 Anup Patel.
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
 * @file rtc-goldfish.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Real Time Clock interface for Goldfish RTC device
 *
 * The source has been largely adapted from Linux 5.x or higher:
 * drivers/rtc/rtc-goldfish.c
 *
 * Copyright (C) 2007 Google, Inc.
 * Copyright (C) 2017 Imagination Technologies Ltd.
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_limits.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <drv/rtc.h>

#define MODULE_DESC			"Goldfish RTC Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(RTC_DEVICE_CLASS_IPRIORITY+1)
#define	MODULE_INIT			goldfish_rtc_driver_init
#define	MODULE_EXIT			goldfish_rtc_driver_exit

#define TIMER_TIME_LOW		0x00	/* get low bits of current time  */
					/*   and update TIMER_TIME_HIGH  */
#define TIMER_TIME_HIGH		0x04	/* get high bits of time at last */
					/*   TIMER_TIME_LOW read         */
#define TIMER_ALARM_LOW		0x08	/* set low bits of alarm and     */
					/*   activate it                 */
#define TIMER_ALARM_HIGH	0x0c	/* set high bits of next alarm   */
#define TIMER_IRQ_ENABLED	0x10
#define TIMER_CLEAR_ALARM	0x14
#define TIMER_ALARM_STATUS	0x18
#define TIMER_CLEAR_INTERRUPT	0x1c

struct goldfish_rtc {
	void *base;
	u32 irq;
	struct rtc_device *rtc;
};

static int goldfish_rtc_read_alarm(struct rtc_device *dev,
				   struct rtc_wkalrm *alrm)
{
	u64 rtc_alarm;
	u64 rtc_alarm_low;
	u64 rtc_alarm_high;
	void *base;
	struct goldfish_rtc *rtcdrv;

	rtcdrv = dev->priv;
	base = rtcdrv->base;

	rtc_alarm_low = vmm_readl(base + TIMER_ALARM_LOW);
	rtc_alarm_high = vmm_readl(base + TIMER_ALARM_HIGH);
	rtc_alarm = (rtc_alarm_high << 32) | rtc_alarm_low;

	rtc_alarm = udiv64(rtc_alarm, NSEC_PER_SEC);
	memset(alrm, 0, sizeof(struct rtc_wkalrm));

	rtc_time64_to_tm(rtc_alarm, &alrm->time);

	if (vmm_readl(base + TIMER_ALARM_STATUS))
		alrm->enabled = 1;
	else
		alrm->enabled = 0;

	return 0;
}

static int goldfish_rtc_set_alarm(struct rtc_device *dev,
				  struct rtc_wkalrm *alrm)
{
	struct goldfish_rtc *rtcdrv;
	u64 rtc_alarm64;
	u64 rtc_status_reg;
	void *base;

	rtcdrv = dev->priv;
	base = rtcdrv->base;

	if (alrm->enabled) {
		rtc_alarm64 = rtc_tm_to_time64(&alrm->time) * NSEC_PER_SEC;
		vmm_writel((rtc_alarm64 >> 32), base + TIMER_ALARM_HIGH);
		vmm_writel(rtc_alarm64, base + TIMER_ALARM_LOW);
	} else {
		/*
		 * if this function was called with enabled=0
		 * then it could mean that the application is
		 * trying to cancel an ongoing alarm
		 */
		rtc_status_reg = vmm_readl(base + TIMER_ALARM_STATUS);
		if (rtc_status_reg)
			vmm_writel(1, base + TIMER_CLEAR_ALARM);
	}

	return 0;
}

static int goldfish_rtc_alarm_irq_enable(struct rtc_device *dev,
					 unsigned int enabled)
{
	void *base;
	struct goldfish_rtc *rtcdrv;

	rtcdrv = dev->priv;
	base = rtcdrv->base;

	if (enabled)
		vmm_writel(1, base + TIMER_IRQ_ENABLED);
	else
		vmm_writel(0, base + TIMER_IRQ_ENABLED);

	return 0;
}

static vmm_irq_return_t goldfish_rtc_interrupt(int irq, void *dev_id)
{
	struct goldfish_rtc *rtcdrv = dev_id;
	void *base = rtcdrv->base;

	vmm_writel(1, base + TIMER_CLEAR_INTERRUPT);

	rtc_update_irq(rtcdrv->rtc, 1, RTC_IRQF | RTC_AF);

	return VMM_IRQ_HANDLED;
}

static int goldfish_rtc_read_time(struct rtc_device *dev, struct rtc_time *tm)
{
	struct goldfish_rtc *rtcdrv;
	void *base;
	u64 time_high;
	u64 time_low;
	u64 time;

	rtcdrv = dev->priv;
	base = rtcdrv->base;

	time_low = vmm_readl(base + TIMER_TIME_LOW);
	time_high = vmm_readl(base + TIMER_TIME_HIGH);
	time = (time_high << 32) | time_low;

	time = udiv64(time, NSEC_PER_SEC);

	rtc_time64_to_tm(time, tm);

	return 0;
}

static int goldfish_rtc_set_time(struct rtc_device *dev, struct rtc_time *tm)
{
	struct goldfish_rtc *rtcdrv;
	void *base;
	u64 now64;

	rtcdrv = dev->priv;
	base = rtcdrv->base;

	now64 = rtc_tm_to_time64(tm) * NSEC_PER_SEC;
	vmm_writel((now64 >> 32), base + TIMER_TIME_HIGH);
	vmm_writel(now64, base + TIMER_TIME_LOW);

	return 0;
}

static const struct rtc_class_ops goldfish_rtc_ops = {
	.read_time	= goldfish_rtc_read_time,
	.set_time	= goldfish_rtc_set_time,
	.read_alarm	= goldfish_rtc_read_alarm,
	.set_alarm	= goldfish_rtc_set_alarm,
	.alarm_irq_enable = goldfish_rtc_alarm_irq_enable
};

static int goldfish_rtc_probe(struct vmm_device *dev)
{
	int rc;
	virtual_addr_t reg_base;
	struct goldfish_rtc *rtcdrv;

	rtcdrv = vmm_zalloc(sizeof(struct goldfish_rtc));
	if (!rtcdrv) {
		rc = VMM_ENOMEM;
		goto free_nothing;
	}

	rc = vmm_devtree_request_regmap(dev->of_node, &reg_base, 0,
					"Goldfish RTC");
	if (rc) {
		goto free_data;
	}
	rtcdrv->base = (void *)reg_base;

	rtcdrv->irq = vmm_devtree_irq_parse_map(dev->of_node, 0);
	if (!rtcdrv->irq) {
		rc = VMM_ENODEV;
		goto free_reg;
	}
	if ((rc = vmm_host_irq_register(rtcdrv->irq, dev->name,
					goldfish_rtc_interrupt, rtcdrv))) {
		goto free_reg;
	}

	rtcdrv->rtc = rtc_device_register(dev, dev->name,
					  &goldfish_rtc_ops, rtcdrv);
	if (VMM_IS_ERR(rtcdrv->rtc)) {
		rc = VMM_PTR_ERR(rtcdrv->rtc);
		goto free_irq;
	}

	vmm_devdrv_set_data(dev, rtcdrv);

	return VMM_OK;

free_irq:
	vmm_host_irq_unregister(rtcdrv->irq, rtcdrv);
free_reg:
	vmm_devtree_regunmap_release(dev->of_node,
				(virtual_addr_t)rtcdrv->base, 0);
free_data:
	vmm_free(rtcdrv);
free_nothing:
	return rc;
}

static int goldfish_rtc_remove(struct vmm_device *dev)
{
	struct goldfish_rtc *rtcdrv = vmm_devdrv_get_data(dev);

	if (!rtcdrv) {
		return VMM_EINVALID;
	}

	rtc_device_unregister(rtcdrv->rtc);
	vmm_host_irq_unregister(rtcdrv->irq, rtcdrv);
	vmm_devtree_regunmap_release(dev->of_node,
				(virtual_addr_t)rtcdrv->base, 0);
	vmm_free(rtcdrv);
	dev->priv = NULL;

	return VMM_OK;
}

static struct vmm_devtree_nodeid goldfish_rtc_of_match[] = {
	{ .compatible = "google,goldfish-rtc" },
	{ /* end of list */ },
};

static struct vmm_driver goldfish_rtc_driver = {
	.name = "goldfish_rtc",
	.match_table = goldfish_rtc_of_match,
	.probe = goldfish_rtc_probe,
	.remove = goldfish_rtc_remove,
};

static int __init goldfish_rtc_driver_init(void)
{
	return vmm_devdrv_register_driver(&goldfish_rtc_driver);
}

static void __exit goldfish_rtc_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&goldfish_rtc_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
