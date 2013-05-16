/**
 * Copyright (c) 2011 Anup Patel.
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
 * @file rtc-pl031.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Real Time Clock interface for ARM AMBA PrimeCell 031 RTC
 *
 * The source has been largely adapted from Linux 3.x or higher:
 * drivers/rtc/rtc-pl031.c
 *
 * Author: Deepak Saxena <dsaxena@plexity.net>
 *
 * Copyright 2006 (c) MontaVista Software, Inc.
 *
 * Author: Mian Yousaf Kaukab <mian.yousaf.kaukab@stericsson.com>
 * Copyright 2010 (c) ST-Ericsson AB
 *
 * The original code is licensed under the GPL.
 */

#include <linux/module.h>
#include <linux/rtc.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/amba/bus.h>
#include <linux/io.h>
#include <linux/bcd.h>
#include <linux/delay.h>
#include <linux/slab.h>

#define MODULE_DESC			"PL031 RTC Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_RTCDEV_CLASS_IPRIORITY+1)
#define	MODULE_INIT			pl031_driver_init
#define	MODULE_EXIT			pl031_driver_exit

/*
 * Register definitions
 */
#define	RTC_DR		0x00	/* Data read register */
#define	RTC_MR		0x04	/* Match register */
#define	RTC_LR		0x08	/* Data load register */
#define	RTC_CR		0x0c	/* Control register */
#define	RTC_IMSC	0x10	/* Interrupt mask and set register */
#define	RTC_RIS		0x14	/* Raw interrupt status register */
#define	RTC_MIS		0x18	/* Masked interrupt status register */
#define	RTC_ICR		0x1c	/* Interrupt clear register */
/* ST variants have additional timer functionality */
#define RTC_TDR		0x20	/* Timer data read register */
#define RTC_TLR		0x24	/* Timer data load register */
#define RTC_TCR		0x28	/* Timer control register */
#define RTC_YDR		0x30	/* Year data read register */
#define RTC_YMR		0x34	/* Year match register */
#define RTC_YLR		0x38	/* Year data load register */

#define RTC_CR_CWEN	(1 << 26)	/* Clockwatch enable bit */

#define RTC_TCR_EN	(1 << 1) /* Periodic timer enable bit */

/* Common bit definitions for Interrupt status and control registers */
#define RTC_BIT_AI	(1 << 0) /* Alarm interrupt bit */
#define RTC_BIT_PI	(1 << 1) /* Periodic interrupt bit. ST variants only. */

/* Common bit definations for ST v2 for reading/writing time */
#define RTC_SEC_SHIFT 0
#define RTC_SEC_MASK (0x3F << RTC_SEC_SHIFT) /* Second [0-59] */
#define RTC_MIN_SHIFT 6
#define RTC_MIN_MASK (0x3F << RTC_MIN_SHIFT) /* Minute [0-59] */
#define RTC_HOUR_SHIFT 12
#define RTC_HOUR_MASK (0x1F << RTC_HOUR_SHIFT) /* Hour [0-23] */
#define RTC_WDAY_SHIFT 17
#define RTC_WDAY_MASK (0x7 << RTC_WDAY_SHIFT) /* Day of Week [1-7] 1=Sunday */
#define RTC_MDAY_SHIFT 20
#define RTC_MDAY_MASK (0x1F << RTC_MDAY_SHIFT) /* Day of Month [1-31] */
#define RTC_MON_SHIFT 25
#define RTC_MON_MASK (0xF << RTC_MON_SHIFT) /* Month [1-12] 1=January */

#define RTC_TIMER_FREQ 32768

struct pl031_local {
	struct rtc_device rtc;
	void *base;
	u32 irq;
	u8 hw_designer;
	u8 hw_revision:4;
};

static int pl031_alarm_irq_enable(struct rtc_device *rd,
				  unsigned int enabled)
{
	unsigned long imsc;
	struct pl031_local *ldata = rd->priv;

	/* Clear any pending alarm interrupts. */
	writel(RTC_BIT_AI, (void *)(ldata->base + RTC_ICR));

	imsc = readl((void *)(ldata->base + RTC_IMSC));

	if (enabled == 1) {
		writel(imsc | RTC_BIT_AI, (void *)(ldata->base + RTC_IMSC));
	} else {
		writel(imsc & ~RTC_BIT_AI, (void *)(ldata->base + RTC_IMSC));
	}

	return 0;
}

/*
 * Convert Gregorian date to ST v2 RTC format.
 */
static int pl031_stv2_tm_to_time(struct rtc_device *rd,
				 struct rtc_time *tm, unsigned long *st_time,
	unsigned long *bcd_year)
{
	int year = tm->tm_year + 1900;
	int wday = tm->tm_wday;

	/* wday masking is not working in hardware so wday must be valid */
	if (wday < -1 || wday > 6) {
		dev_err(rd->dev, "invalid wday value %d\n", tm->tm_wday);
		return -EINVAL;
	} else if (wday == -1) {
		/* wday is not provided, calculate it here */
		unsigned long time;
		struct rtc_time calc_tm;

		rtc_tm_to_time(tm, &time);
		rtc_time_to_tm(time, &calc_tm);
		wday = calc_tm.tm_wday;
	}

	*bcd_year = (bin2bcd(year % 100) | bin2bcd(year / 100) << 8);

	*st_time = ((tm->tm_mon + 1) << RTC_MON_SHIFT)
			|	(tm->tm_mday << RTC_MDAY_SHIFT)
			|	((wday + 1) << RTC_WDAY_SHIFT)
			|	(tm->tm_hour << RTC_HOUR_SHIFT)
			|	(tm->tm_min << RTC_MIN_SHIFT)
			|	(tm->tm_sec << RTC_SEC_SHIFT);

	return 0;
}

/*
 * Convert ST v2 RTC format to Gregorian date.
 */
static int pl031_stv2_time_to_tm(unsigned long st_time, unsigned long bcd_year,
	struct rtc_time *tm)
{
	tm->tm_year = bcd2bin(bcd_year) + (bcd2bin(bcd_year >> 8) * 100);
	tm->tm_mon  = ((st_time & RTC_MON_MASK) >> RTC_MON_SHIFT) - 1;
	tm->tm_mday = ((st_time & RTC_MDAY_MASK) >> RTC_MDAY_SHIFT);
	tm->tm_wday = ((st_time & RTC_WDAY_MASK) >> RTC_WDAY_SHIFT) - 1;
	tm->tm_hour = ((st_time & RTC_HOUR_MASK) >> RTC_HOUR_SHIFT);
	tm->tm_min  = ((st_time & RTC_MIN_MASK) >> RTC_MIN_SHIFT);
	tm->tm_sec  = ((st_time & RTC_SEC_MASK) >> RTC_SEC_SHIFT);

	tm->tm_yday = rtc_year_days(tm->tm_mday, tm->tm_mon, tm->tm_year);
	tm->tm_year -= 1900;

	return 0;
}

static int pl031_stv2_read_time(struct rtc_device *rd, struct rtc_time *tm)
{
	struct pl031_local *ldata = rd->priv;

	pl031_stv2_time_to_tm(readl(ldata->base + RTC_DR),
			readl(ldata->base + RTC_YDR), tm);

	return 0;
}

static int pl031_stv2_set_time(struct rtc_device *rd, struct rtc_time *tm)
{
	unsigned long time;
	unsigned long bcd_year;
	struct pl031_local *ldata = rd->priv;
	int ret;

	ret = pl031_stv2_tm_to_time(rd, tm, &time, &bcd_year);
	if (ret == 0) {
		writel(bcd_year, ldata->base + RTC_YLR);
		writel(time, ldata->base + RTC_LR);
	}

	return ret;
}

static int pl031_stv2_read_alarm(struct rtc_device *rd, struct rtc_wkalrm *alarm)
{
	struct pl031_local *ldata = rd->priv;
	int ret;

	ret = pl031_stv2_time_to_tm(readl(ldata->base + RTC_MR),
			readl(ldata->base + RTC_YMR), &alarm->time);

	alarm->pending = readl(ldata->base + RTC_RIS) & RTC_BIT_AI;
	alarm->enabled = readl(ldata->base + RTC_IMSC) & RTC_BIT_AI;

	return ret;
}

static int pl031_stv2_set_alarm(struct rtc_device *rd, struct rtc_wkalrm *alarm)
{
	struct pl031_local *ldata = rd->priv;
	unsigned long time;
	unsigned long bcd_year;
	int ret;

	/* At the moment, we can only deal with non-wildcarded alarm times. */
	ret = rtc_valid_tm(&alarm->time);
	if (ret == 0) {
		ret = pl031_stv2_tm_to_time(rd, &alarm->time,
					    &time, &bcd_year);
		if (ret == 0) {
			writel(bcd_year, ldata->base + RTC_YMR);
			writel(time, ldata->base + RTC_MR);

			pl031_alarm_irq_enable(rd, alarm->enabled);
		}
	}

	return ret;
}

static irqreturn_t pl031_irq_handler(int irq_no, void *dev)
{
	struct pl031_local *ldata = (struct pl031_local *)dev;
	unsigned long rtcmis;
	unsigned long events = 0;

	rtcmis = readl((void *)(ldata->base + RTC_MIS));
	if (rtcmis) {
		writel(rtcmis, (void *)(ldata->base + RTC_ICR));

		if (rtcmis & RTC_BIT_AI)
			events |= (RTC_AF | RTC_IRQF);

		/* Timer interrupt is only available in ST variants */
		if ((rtcmis & RTC_BIT_PI) &&
			(ldata->hw_designer == AMBA_VENDOR_ST))
			events |= (RTC_PF | RTC_IRQF);

		rtc_update_irq(ldata->rtc, 1, events);

		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static int pl031_read_time(struct rtc_device *rd, struct rtc_time *tm)
{
	struct pl031_local *ldata = rd->priv;

	rtc_time_to_tm(readl(ldata->base + RTC_DR), tm);

	return 0;
}

static int pl031_set_time(struct rtc_device *rd, struct rtc_time *tm)
{
	unsigned long time;
	struct pl031_local *ldata = rd->priv;
	int ret;

	ret = rtc_tm_to_time(tm, &time);

	if (ret == 0)
		writel(time, ldata->base + RTC_LR);

	return ret;
}

static int pl031_read_alarm(struct rtc_device *rd, struct rtc_wkalrm *alarm)
{
	struct pl031_local *ldata = rd->priv;

	rtc_time_to_tm(readl(ldata->base + RTC_MR), &alarm->time);

	alarm->pending = readl(ldata->base + RTC_RIS) & RTC_BIT_AI;
	alarm->enabled = readl(ldata->base + RTC_IMSC) & RTC_BIT_AI;

	return 0;
}

static int pl031_set_alarm(struct rtc_device *rd, struct rtc_wkalrm *alarm)
{
	struct pl031_local *ldata = rd->priv;
	unsigned long time;
	int ret;

	/* At the moment, we can only deal with non-wildcarded alarm times. */
	ret = rtc_valid_tm(&alarm->time);
	if (ret == 0) {
		ret = rtc_tm_to_time(&alarm->time, &time);
		if (ret == 0) {
			writel(time, ldata->base + RTC_MR);
			pl031_alarm_irq_enable(rd, alarm->enabled);
		}
	}

	return ret;
}

static int pl031_driver_probe(struct vmm_device *dev,
			      const struct vmm_devtree_nodeid *devid)
{
	int rc;
	u32 periphid;
	virtual_addr_t reg_base;
	struct pl031_local *ldata;

	ldata = vmm_zalloc(sizeof(struct pl031_local));
	if (!ldata) {
		rc = VMM_ENOMEM;
		goto free_nothing;
	}

	rc = vmm_devtree_regmap(dev->node, &reg_base, 0);
	if (rc) {
		goto free_ldata;
	}
	ldata->base = (void *)reg_base;

	ldata->hw_designer = amba_manf(dev);
	ldata->hw_revision = amba_rev(dev);

	rc = vmm_devtree_irq_get(dev->node, &ldata->irq, 0);
	if (rc) {
		rc = VMM_EFAIL;
		goto free_reg;
	}
	if ((rc = vmm_host_irq_register(ldata->irq, dev->node->name,
					pl031_irq_handler, ldata))) {
		goto free_reg;
	}

	strcpy(ldata->rtc.name, dev->node->name);
	ldata->rtc.dev = dev;
	periphid = amba_periphid(dev);
	if ((periphid & 0x000fffff) == 0x00041031) {
		/* ARM variant */
		ldata->rtc.get_time = pl031_read_time;
		ldata->rtc.set_time = pl031_set_time;
		ldata->rtc.get_alarm = pl031_read_alarm;
		ldata->rtc.set_alarm = pl031_set_alarm;
		ldata->rtc.alarm_irq_enable = pl031_alarm_irq_enable;
	} else if ((periphid & 0x00ffffff) == 0x00180031) {
		/* ST Micro variant - stv1 */
		ldata->rtc.get_time = pl031_read_time;
		ldata->rtc.set_time = pl031_set_time;
		ldata->rtc.get_alarm = pl031_read_alarm;
		ldata->rtc.set_alarm = pl031_set_alarm;
		ldata->rtc.alarm_irq_enable = pl031_alarm_irq_enable;
	} else if ((periphid & 0x00ffffff) == 0x00280031) {
		/* ST Micro variant - stv2 */
		ldata->rtc.get_time = pl031_stv2_read_time;
		ldata->rtc.set_time = pl031_stv2_set_time;
		ldata->rtc.get_alarm = pl031_stv2_read_alarm;
		ldata->rtc.set_alarm = pl031_stv2_set_alarm;
		ldata->rtc.alarm_irq_enable = pl031_alarm_irq_enable;
	} else {
		rc = VMM_EFAIL;
		goto free_irq;
	}
	ldata->rtc.priv = ldata;

	rc = vmm_rtcdev_register(&ldata->rtc);
	if (rc) {
		goto free_irq;
	}

	dev->priv = ldata;

	return VMM_OK;

free_irq:
	vmm_host_irq_unregister(ldata->irq, ldata);
free_reg:
	vmm_devtree_regunmap(dev->node, (virtual_addr_t)ldata->base, 0);
free_ldata:
	vmm_free(ldata);
free_nothing:
	return rc;
}

static int pl031_driver_remove(struct vmm_device *dev)
{
	struct pl031_local *ldata = dev->priv;

	if (ldata) {
		vmm_rtcdev_unregister(&ldata->rtc);
		vmm_host_irq_unregister(ldata->irq, ldata);
		vmm_devtree_regunmap(dev->node, (virtual_addr_t)ldata->base, 0);
		vmm_free(ldata);
		dev->priv = NULL;
	}

	return VMM_OK;
}

static struct vmm_devtree_nodeid pl031_devid_table[] = {
	{.type = "rtc",.compatible = "arm,pl031"},
	{ /* end of list */ },
};

static struct vmm_driver pl031_driver = {
	.name = "pl031_rtc",
	.match_table = pl031_devid_table,
	.probe = pl031_driver_probe,
	.remove = pl031_driver_remove,
};

static int __init pl031_driver_init(void)
{
	return vmm_devdrv_register_driver(&pl031_driver);
}

static void __exit pl031_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&pl031_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
