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
 * @file pl031.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for PrimeCell PL031 RTC driver.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_scheduler.h>
#include <vmm_completion.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <rtc/vmm_rtcdev.h>

#define MODULE_VARID			pl031_driver_module
#define MODULE_NAME			"PL031 RTC Driver"
#define MODULE_AUTHOR			"Anup Patel"
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

/* Common bit definitions for Interrupt status and control registers */
#define RTC_BIT_AI	(1 << 0) /* Alarm interrupt bit */

struct pl031_local {
	struct vmm_rtcdev * rtc;
	virtual_addr_t base;
	u32 irq;
};

#if 0
static int pl031_alarm_irq_enable(struct pl031_local *ldata,
				  unsigned int enabled)
{
	unsigned long imsc;

	/* Clear any pending alarm interrupts. */
	vmm_writel(RTC_BIT_AI, (void *)(ldata->base + RTC_ICR));

	imsc = vmm_readl((void *)(ldata->base + RTC_IMSC));

	if (enabled == 1) {
		vmm_writel(imsc | RTC_BIT_AI, (void *)(ldata->base + RTC_IMSC));
	} else {
		vmm_writel(imsc & ~RTC_BIT_AI, (void *)(ldata->base + RTC_IMSC));
	}

	return 0;
}
#endif

static vmm_irq_return_t pl031_irq_handler(u32 irq_no, 
					  arch_regs_t * regs, 
					  void *dev)
{
	unsigned long rtcmis;
	struct pl031_local *ldata = (struct pl031_local *)dev;

	rtcmis = vmm_readl((void *)(ldata->base + RTC_MIS));
	if (rtcmis) {
		vmm_writel(rtcmis, (void *)(ldata->base + RTC_ICR));
	}

	return VMM_IRQ_HANDLED;
}

static int pl031_set_time(struct vmm_rtcdev *rdev, struct vmm_rtc_time * tm)
{
	unsigned long time;
	struct pl031_local *ldata = rdev->priv;

	vmm_rtc_tm_to_time(tm, &time);

	vmm_writel(time, (void *)(ldata->base + RTC_LR));

	return VMM_OK;
}

static int pl031_get_time(struct vmm_rtcdev *rdev, struct vmm_rtc_time * tm)
{
	struct pl031_local *ldata = rdev->priv;

	vmm_rtc_time_to_tm(vmm_readl((void *)(ldata->base + RTC_DR)), tm);

	return VMM_OK;
}

static int pl031_driver_probe(struct vmm_device *dev,
			      const struct vmm_devid *devid)
{
	int rc;
	const char *attr;
	struct vmm_rtcdev *rd;
	struct pl031_local *ldata;

	rd = vmm_malloc(sizeof(struct vmm_rtcdev));
	if (!rd) {
		rc = VMM_EFAIL;
		goto free_nothing;
	}
	vmm_memset(rd, 0, sizeof(struct vmm_rtcdev));

	ldata = vmm_malloc(sizeof(struct pl031_local));
	if (!ldata) {
		rc = VMM_EFAIL;
		goto free_rtcdev;
	}
	vmm_memset(ldata, 0, sizeof(struct pl031_local));

	vmm_strcpy(rd->name, dev->node->name);
	rd->dev = dev;
	rd->get_time = pl031_get_time;
	rd->set_time = pl031_set_time;
	rd->priv = ldata;

	rc = vmm_devdrv_regmap(dev, &ldata->base, 0);
	if (rc) {
		goto free_ldata;
	}

	attr = vmm_devtree_attrval(dev->node, "irq");
	if (!attr) {
		rc = VMM_EFAIL;
		goto free_ldata;
	}
	ldata->irq = *((u32 *) attr);
	if ((rc = vmm_host_irq_register(ldata->irq, dev->node->name,
					pl031_irq_handler, ldata))) {
		goto free_ldata;
	}

	rc = vmm_rtcdev_register(rd);
	if (rc) {
		goto free_ldata;
	}

	return VMM_OK;

 free_ldata:
	vmm_free(ldata);
 free_rtcdev:
	vmm_free(rd);
 free_nothing:
	return rc;
}

static int pl031_driver_remove(struct vmm_device *dev)
{
	int rc = VMM_OK;
	struct vmm_rtcdev *rd = (struct vmm_rtcdev *)dev->priv;

	if (rd) {
		rc = vmm_rtcdev_unregister(rd);
		vmm_free(rd->priv);
		vmm_free(rd);
		dev->priv = NULL;
	}

	return rc;
}

static struct vmm_devid pl031_devid_table[] = {
	{.type = "rtc",.compatible = "pl031"},
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

static void pl031_driver_exit(void)
{
	vmm_devdrv_unregister_driver(&pl031_driver);
}

VMM_DECLARE_MODULE(MODULE_VARID,
		   MODULE_NAME,
		   MODULE_AUTHOR,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
