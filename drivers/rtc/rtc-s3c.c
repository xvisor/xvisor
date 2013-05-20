/**
 * Copyright (c) 2013 Jean-Christophe Dubois.
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
 * @file rtc-s3c.c
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief Exynos RTC support code
 *
 * Adapted from linux/drivers/rtc/rtc-s3c.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Copyright (c) 2004,2006 Simtec Electronics
 *	Ben Dooks, <ben@simtec.co.uk>
 *	http://armlinux.simtec.co.uk/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * S3C2410/S3C2440/S3C24XX Internal RTC Driver
*/

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/rtc.h>
#include <linux/bcd.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/spinlock.h>

#include <asm/irq.h>
#include <asm/io.h>
#include <exynos/regs-rtc.h>
#include <exynos/regs-clock.h>

#include <vmm_host_aspace.h>
#include <exynos/mach/map.h>

#define MODULE_DESC                     "S3C RTC Driver"
#define MODULE_AUTHOR                   "Jean-Christophe Dubois"
#define MODULE_LICENSE                  "GPL"
#define MODULE_IPRIORITY                (VMM_RTCDEV_CLASS_IPRIORITY+1)
#define MODULE_INIT                     s3c_rtc_driver_init
#define MODULE_EXIT                     s3c_rtc_driver_exit

enum s3c_cpu_type {
	TYPE_S3C2410,
	TYPE_S3C2416,
	TYPE_S3C2443,
	TYPE_S3C64XX,
};

/* I have yet to find an S3C implementation with more than one
 * of these rtc blocks in */

static struct clk *rtc_clk;
static void __iomem *s3c_rtc_base;
static int s3c_rtc_alarmno = NO_IRQ;
static int s3c_rtc_tickno = NO_IRQ;
static enum s3c_cpu_type s3c_rtc_cpu_type;

static DEFINE_SPINLOCK(s3c_rtc_pie_lock);

/**
 * This is a temporary solution until we have a clock management
 * API
 */
#undef clk_enable
static void clk_enable(struct clk *clk)
{
	u32 perir_reg = vmm_readl((void *)clk);

	if (!(perir_reg & (1 << 15))) {
		perir_reg |= (1 << 15);

		vmm_writel(perir_reg, (void *)clk);
	}
}

/**
 * This is a temporary solution until we have a clock management
 * API
 */
#undef clk_disable
static void clk_disable(struct clk *clk)
{
	u32 perir_reg = vmm_readl((void *)clk);

	if (perir_reg & (1 << 15)) {
		perir_reg &= ~(1 << 15);

		vmm_writel(perir_reg, (void *)clk);
	}
}

/**
 * This is a temporary solution until we have a clock management
 * API
 */
#undef clk_put
static void clk_put(struct clk *clk)
{
	vmm_host_iounmap((virtual_addr_t) clk, sizeof(u32));
}

/**
 * This is a temporary solution until we have a clock management
 * API
 */
#undef clk_get
static struct clk *clk_get(struct vmm_device *dev, const char *name)
{
	void *cmu_ptr = (void *)vmm_host_iomap(EXYNOS4_PA_CMU +
					       EXYNOS4_CLKGATE_IP_PERIR,
					       sizeof(u32));

	return cmu_ptr;
}

static bool is_power_of_2(unsigned long n)
{
	return (n != 0 && ((n & (n - 1)) == 0));
}

static void s3c_rtc_alarm_clk_enable(bool enable)
{
	static DEFINE_SPINLOCK(s3c_rtc_alarm_clk_lock);
	static bool alarm_clk_enabled;
	unsigned long irq_flags;

	spin_lock_irqsave(&s3c_rtc_alarm_clk_lock, irq_flags);
	if (enable) {
		if (!alarm_clk_enabled) {
			clk_enable(rtc_clk);
			alarm_clk_enabled = true;
		}
	} else {
		if (alarm_clk_enabled) {
			clk_disable(rtc_clk);
			alarm_clk_enabled = false;
		}
	}
	spin_unlock_irqrestore(&s3c_rtc_alarm_clk_lock, irq_flags);
}

/* IRQ Handlers */

static irqreturn_t s3c_rtc_alarmirq(int irq, void *id)
{
	//struct rtc_device *rdev = id;

	clk_enable(rtc_clk);
	rtc_update_irq(rdev, 1, RTC_AF | RTC_IRQF);

	if (s3c_rtc_cpu_type == TYPE_S3C64XX)
		writeb(S3C2410_INTP_ALM, s3c_rtc_base + S3C2410_INTP);

	clk_disable(rtc_clk);

	s3c_rtc_alarm_clk_enable(false);

	return IRQ_HANDLED;
}

static irqreturn_t s3c_rtc_tickirq(int irq, void *id)
{
	//struct rtc_device *rdev = id;

	clk_enable(rtc_clk);
	rtc_update_irq(rdev, 1, RTC_PF | RTC_IRQF);

	if (s3c_rtc_cpu_type == TYPE_S3C64XX)
		writeb(S3C2410_INTP_TIC, s3c_rtc_base + S3C2410_INTP);

	clk_disable(rtc_clk);
	return IRQ_HANDLED;
}

/* Update control registers */
static int s3c_rtc_setaie(struct rtc_device *dev, unsigned int enabled)
{
	unsigned int tmp;

	clk_enable(rtc_clk);
	tmp = readb(s3c_rtc_base + S3C2410_RTCALM) & ~S3C2410_RTCALM_ALMEN;

	if (enabled)
		tmp |= S3C2410_RTCALM_ALMEN;

	writeb(tmp, s3c_rtc_base + S3C2410_RTCALM);
	clk_disable(rtc_clk);

	s3c_rtc_alarm_clk_enable(enabled);

	return 0;
}

static int max_user_freq;

static int s3c_rtc_setfreq(struct rtc_device *rtc_dev, int freq)
{
	//struct platform_device *pdev = to_platform_device(dev);
	//struct rtc_device *rtc_dev = platform_get_drvdata(pdev);
	unsigned int tmp = 0;
	int val;

	if (!is_power_of_2(freq)) {
		return -EINVAL;
	}

	clk_enable(rtc_clk);
	spin_lock_irq(&s3c_rtc_pie_lock);

	if (s3c_rtc_cpu_type != TYPE_S3C64XX) {
		tmp = readb(s3c_rtc_base + S3C2410_TICNT);
		tmp &= S3C2410_TICNT_ENABLE;
	}
	//val = (rtc_dev->max_user_freq / freq) - 1;
	val = udiv32(max_user_freq, freq) - 1;

	if (s3c_rtc_cpu_type == TYPE_S3C2416
	    || s3c_rtc_cpu_type == TYPE_S3C2443) {
		tmp |= S3C2443_TICNT_PART(val);
		writel(S3C2443_TICNT1_PART(val), s3c_rtc_base + S3C2443_TICNT1);

		if (s3c_rtc_cpu_type == TYPE_S3C2416)
			writel(S3C2416_TICNT2_PART(val),
			       s3c_rtc_base + S3C2416_TICNT2);
	} else {
		tmp |= val;
	}

	writel(tmp, s3c_rtc_base + S3C2410_TICNT);
	spin_unlock_irq(&s3c_rtc_pie_lock);
	clk_disable(rtc_clk);

	return 0;
}

/* Time read/write */

static int s3c_rtc_gettime(struct rtc_device *dev, struct rtc_time *rtc_tm)
{
	unsigned int have_retried = 0;
	void __iomem *base = s3c_rtc_base;

	clk_enable(rtc_clk);
 retry_get_time:
	rtc_tm->tm_min = readb(base + S3C2410_RTCMIN);
	rtc_tm->tm_hour = readb(base + S3C2410_RTCHOUR);
	rtc_tm->tm_mday = readb(base + S3C2410_RTCDATE);
	rtc_tm->tm_mon = readb(base + S3C2410_RTCMON);
	rtc_tm->tm_year = readb(base + S3C2410_RTCYEAR);
	rtc_tm->tm_sec = readb(base + S3C2410_RTCSEC);

	/* the only way to work out wether the system was mid-update
	 * when we read it is to check the second counter, and if it
	 * is zero, then we re-try the entire read
	 */

	if (rtc_tm->tm_sec == 0 && !have_retried) {
		have_retried = 1;
		goto retry_get_time;
	}

	rtc_tm->tm_sec = bcd2bin(rtc_tm->tm_sec);
	rtc_tm->tm_min = bcd2bin(rtc_tm->tm_min);
	rtc_tm->tm_hour = bcd2bin(rtc_tm->tm_hour);
	rtc_tm->tm_mday = bcd2bin(rtc_tm->tm_mday);
	rtc_tm->tm_mon = bcd2bin(rtc_tm->tm_mon);
	rtc_tm->tm_year = bcd2bin(rtc_tm->tm_year);

	rtc_tm->tm_year += 100;

	rtc_tm->tm_mon -= 1;

	clk_disable(rtc_clk);

	return 0;
}

static int s3c_rtc_settime(struct rtc_device *dev, struct rtc_time *tm)
{
	void __iomem *base = s3c_rtc_base;
	int year = tm->tm_year - 100;

	/* we get around y2k by simply not supporting it */

	if (year < 0 || year >= 100) {
		dev_err(dev->dev, "rtc only supports 100 years\n");
		return -EINVAL;
	}

	clk_enable(rtc_clk);
	writeb(bin2bcd(tm->tm_sec), base + S3C2410_RTCSEC);
	writeb(bin2bcd(tm->tm_min), base + S3C2410_RTCMIN);
	writeb(bin2bcd(tm->tm_hour), base + S3C2410_RTCHOUR);
	writeb(bin2bcd(tm->tm_mday), base + S3C2410_RTCDATE);
	writeb(bin2bcd(tm->tm_mon + 1), base + S3C2410_RTCMON);
	writeb(bin2bcd(year), base + S3C2410_RTCYEAR);
	clk_disable(rtc_clk);

	return 0;
}

static int s3c_rtc_getalarm(struct rtc_device *dev, struct rtc_wkalrm *alrm)
{
	struct rtc_time *alm_tm = &alrm->time;
	void __iomem *base = s3c_rtc_base;
	unsigned int alm_en;

	clk_enable(rtc_clk);
	alm_tm->tm_sec = readb(base + S3C2410_ALMSEC);
	alm_tm->tm_min = readb(base + S3C2410_ALMMIN);
	alm_tm->tm_hour = readb(base + S3C2410_ALMHOUR);
	alm_tm->tm_mon = readb(base + S3C2410_ALMMON);
	alm_tm->tm_mday = readb(base + S3C2410_ALMDATE);
	alm_tm->tm_year = readb(base + S3C2410_ALMYEAR);

	alm_en = readb(base + S3C2410_RTCALM);

	alrm->enabled = (alm_en & S3C2410_RTCALM_ALMEN) ? 1 : 0;

	/* decode the alarm enable field */

	if (alm_en & S3C2410_RTCALM_SECEN)
		alm_tm->tm_sec = bcd2bin(alm_tm->tm_sec);
	else
		alm_tm->tm_sec = -1;

	if (alm_en & S3C2410_RTCALM_MINEN)
		alm_tm->tm_min = bcd2bin(alm_tm->tm_min);
	else
		alm_tm->tm_min = -1;

	if (alm_en & S3C2410_RTCALM_HOUREN)
		alm_tm->tm_hour = bcd2bin(alm_tm->tm_hour);
	else
		alm_tm->tm_hour = -1;

	if (alm_en & S3C2410_RTCALM_DAYEN)
		alm_tm->tm_mday = bcd2bin(alm_tm->tm_mday);
	else
		alm_tm->tm_mday = -1;

	if (alm_en & S3C2410_RTCALM_MONEN) {
		alm_tm->tm_mon = bcd2bin(alm_tm->tm_mon);
		alm_tm->tm_mon -= 1;
	} else {
		alm_tm->tm_mon = -1;
	}

	if (alm_en & S3C2410_RTCALM_YEAREN)
		alm_tm->tm_year = bcd2bin(alm_tm->tm_year);
	else
		alm_tm->tm_year = -1;

	clk_disable(rtc_clk);
	return 0;
}

static int s3c_rtc_setalarm(struct rtc_device *dev, struct rtc_wkalrm *alrm)
{
	struct rtc_time *tm = &alrm->time;
	void __iomem *base = s3c_rtc_base;
	unsigned int alrm_en;

	clk_enable(rtc_clk);

	alrm_en = readb(base + S3C2410_RTCALM) & S3C2410_RTCALM_ALMEN;
	writeb(0x00, base + S3C2410_RTCALM);

	if (tm->tm_sec < 60 && tm->tm_sec >= 0) {
		alrm_en |= S3C2410_RTCALM_SECEN;
		writeb(bin2bcd(tm->tm_sec), base + S3C2410_ALMSEC);
	}

	if (tm->tm_min < 60 && tm->tm_min >= 0) {
		alrm_en |= S3C2410_RTCALM_MINEN;
		writeb(bin2bcd(tm->tm_min), base + S3C2410_ALMMIN);
	}

	if (tm->tm_hour < 24 && tm->tm_hour >= 0) {
		alrm_en |= S3C2410_RTCALM_HOUREN;
		writeb(bin2bcd(tm->tm_hour), base + S3C2410_ALMHOUR);
	}

	writeb(alrm_en, base + S3C2410_RTCALM);

	s3c_rtc_setaie(dev, alrm->enabled);

	clk_disable(rtc_clk);
	return 0;
}

static struct rtc_device s3c_rtcops = {
	.name = "s3c-rtc",
	.get_time = s3c_rtc_gettime,
	.set_time = s3c_rtc_settime,
	.get_alarm = s3c_rtc_getalarm,
	.set_alarm = s3c_rtc_setalarm,
	.alarm_irq_enable = s3c_rtc_setaie,
};

static void s3c_rtc_enable(struct vmm_device *pdev, int en)
{
	void __iomem *base = s3c_rtc_base;
	unsigned int tmp;

	if (s3c_rtc_base == NULL)
		return;

	clk_enable(rtc_clk);
	if (!en) {
		tmp = readw(base + S3C2410_RTCCON);
		if (s3c_rtc_cpu_type == TYPE_S3C64XX)
			tmp &= ~S3C64XX_RTCCON_TICEN;
		tmp &= ~S3C2410_RTCCON_RTCEN;
		writew(tmp, base + S3C2410_RTCCON);

		if (s3c_rtc_cpu_type != TYPE_S3C64XX) {
			tmp = readb(base + S3C2410_TICNT);
			tmp &= ~S3C2410_TICNT_ENABLE;
			writeb(tmp, base + S3C2410_TICNT);
		}
	} else {
		/* re-enable the device, and check it is ok */

		if ((readw(base + S3C2410_RTCCON) & S3C2410_RTCCON_RTCEN) == 0) {
			tmp = readw(base + S3C2410_RTCCON);
			writew(tmp | S3C2410_RTCCON_RTCEN,
			       base + S3C2410_RTCCON);
		}

		if ((readw(base + S3C2410_RTCCON) & S3C2410_RTCCON_CNTSEL)) {
			tmp = readw(base + S3C2410_RTCCON);
			writew(tmp & ~S3C2410_RTCCON_CNTSEL,
			       base + S3C2410_RTCCON);
		}

		if ((readw(base + S3C2410_RTCCON) & S3C2410_RTCCON_CLKRST)) {
			tmp = readw(base + S3C2410_RTCCON);
			writew(tmp & ~S3C2410_RTCCON_CLKRST,
			       base + S3C2410_RTCCON);
		}
	}
	clk_disable(rtc_clk);
}

static int s3c_rtc_driver_remove(struct vmm_device *dev)
{
	struct rtc_device *rtc = dev->priv;

	vmm_host_irq_unregister(s3c_rtc_alarmno, rtc);
	vmm_host_irq_unregister(s3c_rtc_tickno, rtc);

	dev->priv = NULL;
	vmm_rtcdev_unregister(rtc);

	s3c_rtc_setaie(rtc, 0);

	clk_put(rtc_clk);
	rtc_clk = NULL;

	vmm_devtree_regunmap(dev->node, (virtual_addr_t) s3c_rtc_base, 0);

	return 0;
}

static int s3c_rtc_driver_probe(struct vmm_device *pdev,
				const struct vmm_devtree_nodeid *devid)
{
	u32 alarmno, tickno;
	struct rtc_time rtc_tm;
	int ret, tmp, rc;

	/* find the IRQs */

	rc = vmm_devtree_irq_get(pdev->node, &alarmno, 0);
	if (rc) {
		rc = VMM_EFAIL;
		return rc;
	}
	s3c_rtc_alarmno = alarmno;
	rc = vmm_devtree_irq_get(pdev->node, &tickno, 1);
	if (rc) {
		rc = VMM_EFAIL;
		return rc;
	}
	s3c_rtc_tickno = tickno;

	/* get the memory region */

	rc = vmm_devtree_regmap(pdev->node, (virtual_addr_t *)&s3c_rtc_base,
				0);
	if (rc) {
		dev_err(pdev, "failed ioremap()\n");
		ret = rc;
		goto err_nomap;
	}

	rtc_clk = clk_get(pdev, "rtc");
	if (rtc_clk == NULL) {
		dev_err(pdev, "failed to find rtc clock source\n");
		ret = -ENODEV;
		goto err_clk;
	}

	clk_enable(rtc_clk);

	/* check to see if everything is setup correctly */

	s3c_rtc_enable(pdev, 1);

	//device_init_wakeup(pdev, 1);

	/* register RTC and exit */

	s3c_rtcops.dev = pdev;

	rc = vmm_rtcdev_register(&s3c_rtcops);

	if (rc) {
		dev_err(pdev, "cannot attach rtc\n");
		ret = rc;
		goto err_nortc;
	}

	s3c_rtc_cpu_type = (enum s3c_cpu_type)devid->data;

	/* Check RTC Time */

	s3c_rtc_gettime(NULL, &rtc_tm);

	if (!rtc_valid_tm(&rtc_tm)) {
		dev_warn(pdev,
			 "warning: invalid RTC value so initializing it\n");

		rtc_tm.tm_year = 100;
		rtc_tm.tm_mon = 0;
		rtc_tm.tm_mday = 1;
		rtc_tm.tm_hour = 0;
		rtc_tm.tm_min = 0;
		rtc_tm.tm_sec = 0;

		s3c_rtc_settime(NULL, &rtc_tm);
	}

	if (s3c_rtc_cpu_type != TYPE_S3C2410)
		max_user_freq = 32768;
	else
		max_user_freq = 128;

	if (s3c_rtc_cpu_type == TYPE_S3C2416
	    || s3c_rtc_cpu_type == TYPE_S3C2443) {
		tmp = readw(s3c_rtc_base + S3C2410_RTCCON);
		tmp |= S3C2443_RTCCON_TICSEL;
		writew(tmp, s3c_rtc_base + S3C2410_RTCCON);
	}

	pdev->priv = &s3c_rtcops;

	s3c_rtc_setfreq(&s3c_rtcops, 1);

	if ((rc =
	     vmm_host_irq_register(s3c_rtc_alarmno, "s3c_rtc_alarm",
				   s3c_rtc_alarmirq, &s3c_rtcops))) {
		dev_err(pdev, "IRQ%d error %d\n", s3c_rtc_alarmno, rc);
		goto err_alarm_irq;
	}

	if ((rc =
	     vmm_host_irq_register(s3c_rtc_tickno, "s3c_rtc_tick",
				   s3c_rtc_tickirq, &s3c_rtcops))) {
		dev_err(pdev, "IRQ%d error %d\n", s3c_rtc_tickno, rc);
		goto err_tick_irq;
	}

	clk_disable(rtc_clk);

	return 0;

 err_tick_irq:
	vmm_host_irq_unregister(s3c_rtc_alarmno, &s3c_rtcops);

 err_alarm_irq:
	pdev->priv = NULL;
	vmm_rtcdev_unregister(&s3c_rtcops);

 err_nortc:
	s3c_rtc_enable(pdev, 0);
	clk_disable(rtc_clk);
	clk_put(rtc_clk);

 err_clk:
	vmm_devtree_regunmap(pdev->node, (virtual_addr_t) s3c_rtc_base, 0);

 err_nomap:
	return ret;
}

static struct vmm_devtree_nodeid s3c_devid_table[] = {
	{
	 .type = "rtc",
	 .compatible = "samsung,s3c2410-rtc",
	 .data = (void *)TYPE_S3C2410},
	{
	 .type = "rtc",
	 .compatible = "samsung,s3c2416-rtc",
	 .data = (void *)TYPE_S3C2416},
	{
	 .type = "rtc",
	 .compatible = "samsung,s3c2443-rtc",
	 .data = (void *)TYPE_S3C2443},
	{
	 .type = "rtc",
	 .compatible = "samsung,s3c6410-rtc",
	 .data = (void *)TYPE_S3C64XX},
	{ /* end of list */ },
};

static struct vmm_driver s3c_rtc_driver = {
	.name = "s3c_rtc",
	.match_table = s3c_devid_table,
	.probe = s3c_rtc_driver_probe,
	.remove = s3c_rtc_driver_remove,
};

static int __init s3c_rtc_driver_init(void)
{
	return vmm_devdrv_register_driver(&s3c_rtc_driver);
}

static void __exit s3c_rtc_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&s3c_rtc_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
