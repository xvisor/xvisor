/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file vexpres-sysreg.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM Versatile Express sysreg driver
 *
 * Adapted from linux/drivers/mfd/vexpres-sysreg.c
 *
 * Copyright (C) 2012 ARM Limited
 *
 * The original source is licensed under GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_timer.h>
#include <vmm_host_io.h>
#include <vmm_devdrv.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <drv/vexpress.h>

#define MODULE_DESC			"VExpress Sysreg Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			vexpress_sysreg_init
#define	MODULE_EXIT			vexpress_sysreg_exit

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)	vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

/* We really don't require fancy timer based 
 * polling for vexpress_sysreg_config_func_exec()
 * (like Linux) instead we use normal polling loop.
 *
 * Enable the below option for explicity using
 * timer based polling loop.
 */
#undef USE_TIMER_BASED_CONFIG_EXEC

#define SYS_ID			0x000
#define SYS_SW			0x004
#define SYS_LED			0x008
#define SYS_100HZ		0x024
#define SYS_FLAGS		0x030
#define SYS_FLAGSSET		0x030
#define SYS_FLAGSCLR		0x034
#define SYS_NVFLAGS		0x038
#define SYS_NVFLAGSSET		0x038
#define SYS_NVFLAGSCLR		0x03c
#define SYS_MCI			0x048
#define SYS_FLASH		0x04c
#define SYS_CFGSW		0x058
#define SYS_24MHZ		0x05c
#define SYS_MISC		0x060
#define SYS_DMA			0x064
#define SYS_PROCID0		0x084
#define SYS_PROCID1		0x088
#define SYS_CFGDATA		0x0a0
#define SYS_CFGCTRL		0x0a4
#define SYS_CFGSTAT		0x0a8

#define SYS_HBI_MASK		0xfff
#define SYS_ID_HBI_SHIFT	16
#define SYS_PROCIDx_HBI_SHIFT	0

#define SYS_LED_LED(n)		(1 << (n))

#define SYS_MCI_CARDIN		(1 << 0)
#define SYS_MCI_WPROT		(1 << 1)

#define SYS_FLASH_WPn		(1 << 0)

#define SYS_MISC_MASTERSITE	(1 << 14)

#define SYS_CFGCTRL_START	(1 << 31)
#define SYS_CFGCTRL_WRITE	(1 << 30)
#define SYS_CFGCTRL_DCC(n)	(((n) & 0xf) << 26)
#define SYS_CFGCTRL_FUNC(n)	(((n) & 0x3f) << 20)
#define SYS_CFGCTRL_SITE(n)	(((n) & 0x3) << 16)
#define SYS_CFGCTRL_POSITION(n)	(((n) & 0xf) << 12)
#define SYS_CFGCTRL_DEVICE(n)	(((n) & 0xfff) << 0)

#define SYS_CFGSTAT_ERR		(1 << 1)
#define SYS_CFGSTAT_COMPLETE	(1 << 0)

static void *vexpress_sysreg_base;
static struct vmm_device *vexpress_sysreg_dev;
static int vexpress_master_site;

void vexpress_flags_set(u32 data)
{
	vmm_writel(~0, vexpress_sysreg_base + SYS_FLAGSCLR);
	vmm_writel(data, vexpress_sysreg_base + SYS_FLAGSSET);
}

u32 vexpress_get_procid(int site)
{
	if (site == VEXPRESS_SITE_MASTER)
		site = vexpress_master_site;

	return vmm_readl(vexpress_sysreg_base + (site == VEXPRESS_SITE_DB1 ?
			SYS_PROCID0 : SYS_PROCID1));
}

u32 vexpress_get_hbi(int site)
{
	u32 id;

	switch (site) {
	case VEXPRESS_SITE_MB:
		id = vmm_readl(vexpress_sysreg_base + SYS_ID);
		return (id >> SYS_ID_HBI_SHIFT) & SYS_HBI_MASK;
	case VEXPRESS_SITE_MASTER:
	case VEXPRESS_SITE_DB1:
	case VEXPRESS_SITE_DB2:
		id = vexpress_get_procid(site);
		return (id >> SYS_PROCIDx_HBI_SHIFT) & SYS_HBI_MASK;
	}

	return ~0;
}

void *vexpress_get_24mhz_clock_base(void)
{
	return vexpress_sysreg_base + SYS_24MHZ;
}

static void vexpress_sysreg_find_prop(struct vmm_devtree_node *node,
					const char *name, u32 *val)
{
	void *aval;

	while (node) {
		aval = vmm_devtree_attrval(node, name);
		if (aval) {
			*val = *((u32 *)aval);
			return;
		}
		node = node->parent;
	}
}

unsigned __vexpress_get_site(struct vmm_device *dev,
			     struct vmm_devtree_node *node)
{
	u32 site = 0;

	WARN_ON(dev && node && dev->node != node);
	if (dev && !node)
		node = dev->node;

	if (node) {
		vexpress_sysreg_find_prop(node, "arm,vexpress,site", &site);
	} else if (dev && strncmp(dev->name, "ct:", 3) == 0) {
		site = VEXPRESS_SITE_MASTER;
	}

	if (site == VEXPRESS_SITE_MASTER)
		site = vexpress_master_site;

	return site;
}

struct vexpress_sysreg_config_func {
	u32 template;
	u32 device;
};

static struct vexpress_config_bridge *vexpress_sysreg_config_bridge;
static struct vmm_timer_event vexpress_sysreg_config_timer;
static u32 *vexpress_sysreg_config_data;
static int vexpress_sysreg_config_tries;

static void *vexpress_sysreg_config_func_get(struct vmm_device *dev,
					     struct vmm_devtree_node *node)
{
	struct vexpress_sysreg_config_func *config_func;
	u32 site;
	u32 position = 0;
	u32 dcc = 0;
	u32 func_device[2];
	void *aval;
	int err = VMM_EFAULT;

	if (dev && !node)
		node = dev->node;

	if (node) {
		vexpress_sysreg_find_prop(node, "arm,vexpress,site", &site);
		vexpress_sysreg_find_prop(node, "arm,vexpress,position",
								&position);
		vexpress_sysreg_find_prop(node, "arm,vexpress,dcc", &dcc);
		aval = vmm_devtree_attrval(node, "arm,vexpress-sysreg,func");
		if (aval) {
			func_device[0] = ((u32 *)aval)[0];
			func_device[1] = ((u32 *)aval)[1];
			err = VMM_OK;
		} else {
			err = VMM_ENOENT;
		}
	}
	if (err)
		return NULL;

	config_func = vmm_zalloc(sizeof(*config_func));
	if (!config_func)
		return NULL;

	config_func->template = SYS_CFGCTRL_DCC(dcc);
	config_func->template |= SYS_CFGCTRL_FUNC(func_device[0]);
	config_func->template |= SYS_CFGCTRL_SITE(site == VEXPRESS_SITE_MASTER ?
						  vexpress_master_site : site);
	config_func->template |= SYS_CFGCTRL_POSITION(position);
	config_func->device |= func_device[1];

	DPRINTF("%s: func 0x%p = 0x%x, %d\n",
		vexpress_sysreg_dev->name, config_func,
		config_func->template, config_func->device);

	return config_func;
}

static void vexpress_sysreg_config_func_put(void *func)
{
	vmm_free(func);
}

static int vexpress_sysreg_config_func_exec(void *func, int offset,
					    bool write, u32 *data)
{
	int status;
	struct vexpress_sysreg_config_func *config_func = func;
	u32 command;

	if (WARN_ON(!vexpress_sysreg_base))
		return VMM_ENOENT;

	command = vmm_readl(vexpress_sysreg_base + SYS_CFGCTRL);
	if (WARN_ON(command & SYS_CFGCTRL_START))
		return VMM_EBUSY;

	command = SYS_CFGCTRL_START;
	command |= write ? SYS_CFGCTRL_WRITE : 0;
	command |= config_func->template;
	command |= SYS_CFGCTRL_DEVICE(config_func->device + offset);

	/* Use a canary for reads */
	if (!write)
		*data = 0xdeadbeef;

	DPRINTF("%s: command %x, data %x\n",
		vexpress_sysreg_dev->name, command, *data);
	vmm_writel(*data, vexpress_sysreg_base + SYS_CFGDATA);
	vmm_writel(0, vexpress_sysreg_base + SYS_CFGSTAT);
	vmm_writel(command, vexpress_sysreg_base + SYS_CFGCTRL);
	arch_smp_mb();

#ifdef USE_TIMER_BASED_CONFIG_EXEC
	if (vexpress_sysreg_dev) {
#else
	if (0) {
#endif
		/* Schedule completion check */
		if (!write)
			vexpress_sysreg_config_data = data;
		vexpress_sysreg_config_tries = 100;
		vmm_timer_event_start(&vexpress_sysreg_config_timer, 100000);
		status = VEXPRESS_CONFIG_STATUS_WAIT;
	} else {
		/* Early execution, no timer available, have to spin */
		u32 cfgstat;

		do {
			arch_smp_mb(); /* FIXME: cpu_relax(); */
			cfgstat = vmm_readl(vexpress_sysreg_base + SYS_CFGSTAT);
		} while (!cfgstat);

		if (!write && (cfgstat & SYS_CFGSTAT_COMPLETE))
			*data = vmm_readl(vexpress_sysreg_base + SYS_CFGDATA);
		status = VEXPRESS_CONFIG_STATUS_DONE;

		if (cfgstat & SYS_CFGSTAT_ERR)
			status = VMM_EINVALID;
	}

	return status;
}

struct vexpress_config_bridge_info vexpress_sysreg_config_bridge_info = {
	.name = "vexpress-sysreg",
	.func_get = vexpress_sysreg_config_func_get,
	.func_put = vexpress_sysreg_config_func_put,
	.func_exec = vexpress_sysreg_config_func_exec,
};

static void vexpress_sysreg_config_complete(struct vmm_timer_event *ev)
{
	int status = VEXPRESS_CONFIG_STATUS_DONE;
	u32 cfgstat = vmm_readl(vexpress_sysreg_base + SYS_CFGSTAT);

	if (cfgstat & SYS_CFGSTAT_ERR)
		status = VMM_EINVALID;
	if (!vexpress_sysreg_config_tries--)
		status = VMM_ETIMEDOUT;

	if (status < 0) {
		vmm_printf("%s: error %d\n", 
			   vexpress_sysreg_dev->name, status);
	} else if (!(cfgstat & SYS_CFGSTAT_COMPLETE)) {
		vmm_timer_event_start(&vexpress_sysreg_config_timer, 50000);
		return;
	}

	if (vexpress_sysreg_config_data) {
		*vexpress_sysreg_config_data = 
				vmm_readl(vexpress_sysreg_base + SYS_CFGDATA);
		DPRINTF("%s: read data %x\n",
			vexpress_sysreg_dev->name,
			*vexpress_sysreg_config_data);
		vexpress_sysreg_config_data = NULL;
	}

	vexpress_config_complete(vexpress_sysreg_config_bridge, status);
}

void vexpress_sysreg_setup(struct vmm_devtree_node *node)
{
	if (WARN_ON(!vexpress_sysreg_base))
		return;

	if (vmm_readl(vexpress_sysreg_base + SYS_MISC) & SYS_MISC_MASTERSITE)
		vexpress_master_site = VEXPRESS_SITE_DB2;
	else
		vexpress_master_site = VEXPRESS_SITE_DB1;

	vexpress_sysreg_config_bridge = vexpress_config_bridge_register(
			node, &vexpress_sysreg_config_bridge_info);
	WARN_ON(!vexpress_sysreg_config_bridge);
}

void __init vexpress_sysreg_early_init(void *base)
{
	vexpress_sysreg_base = base;
	vexpress_sysreg_setup(NULL);
}

void __init vexpress_sysreg_of_early_init(void)
{
	int err;
	virtual_addr_t base_va;
	struct vmm_devtree_node *node;

	if (vexpress_sysreg_base)
		return;

	node = vmm_devtree_find_compatible(NULL, NULL, "arm,vexpress-sysreg");
	if (node) {
		err = vmm_devtree_regmap(node, &base_va, 0);
		if (err) {
			vmm_printf("%s: Faild to map registers (err %d)\n",
				   __func__, err);
			return;
		}		
		vexpress_sysreg_base = (void *)base_va;
		vexpress_sysreg_setup(node);
	}
}


#if 0 /* FIXME: #ifdef CONFIG_GPIOLIB */

#define VEXPRESS_SYSREG_GPIO(_name, _reg, _value) \
	[VEXPRESS_GPIO_##_name] = { \
		.reg = _reg, \
		.value = _reg##_##_value, \
	}

static struct vexpress_sysreg_gpio {
	unsigned long reg;
	u32 value;
} vexpress_sysreg_gpios[] = {
	VEXPRESS_SYSREG_GPIO(MMC_CARDIN,	SYS_MCI,	CARDIN),
	VEXPRESS_SYSREG_GPIO(MMC_WPROT,		SYS_MCI,	WPROT),
	VEXPRESS_SYSREG_GPIO(FLASH_WPn,		SYS_FLASH,	WPn),
	VEXPRESS_SYSREG_GPIO(LED0,		SYS_LED,	LED(0)),
	VEXPRESS_SYSREG_GPIO(LED1,		SYS_LED,	LED(1)),
	VEXPRESS_SYSREG_GPIO(LED2,		SYS_LED,	LED(2)),
	VEXPRESS_SYSREG_GPIO(LED3,		SYS_LED,	LED(3)),
	VEXPRESS_SYSREG_GPIO(LED4,		SYS_LED,	LED(4)),
	VEXPRESS_SYSREG_GPIO(LED5,		SYS_LED,	LED(5)),
	VEXPRESS_SYSREG_GPIO(LED6,		SYS_LED,	LED(6)),
	VEXPRESS_SYSREG_GPIO(LED7,		SYS_LED,	LED(7)),
};

static int vexpress_sysreg_gpio_direction_input(struct gpio_chip *chip,
				       unsigned offset)
{
	return 0;
}

static int vexpress_sysreg_gpio_get(struct gpio_chip *chip,
				       unsigned offset)
{
	struct vexpress_sysreg_gpio *gpio = &vexpress_sysreg_gpios[offset];
	u32 reg_value = readl(vexpress_sysreg_base + gpio->reg);

	return !!(reg_value & gpio->value);
}

static void vexpress_sysreg_gpio_set(struct gpio_chip *chip,
				       unsigned offset, int value)
{
	struct vexpress_sysreg_gpio *gpio = &vexpress_sysreg_gpios[offset];
	u32 reg_value = readl(vexpress_sysreg_base + gpio->reg);

	if (value)
		reg_value |= gpio->value;
	else
		reg_value &= ~gpio->value;

	writel(reg_value, vexpress_sysreg_base + gpio->reg);
}

static int vexpress_sysreg_gpio_direction_output(struct gpio_chip *chip,
						unsigned offset, int value)
{
	vexpress_sysreg_gpio_set(chip, offset, value);

	return 0;
}

static struct gpio_chip vexpress_sysreg_gpio_chip = {
	.label = "vexpress-sysreg",
	.direction_input = vexpress_sysreg_gpio_direction_input,
	.direction_output = vexpress_sysreg_gpio_direction_output,
	.get = vexpress_sysreg_gpio_get,
	.set = vexpress_sysreg_gpio_set,
	.ngpio = ARRAY_SIZE(vexpress_sysreg_gpios),
	.base = 0,
};


#define VEXPRESS_SYSREG_GREEN_LED(_name, _default_trigger, _gpio) \
	{ \
		.name = "v2m:green:"_name, \
		.default_trigger = _default_trigger, \
		.gpio = VEXPRESS_GPIO_##_gpio, \
	}

struct gpio_led vexpress_sysreg_leds[] = {
	VEXPRESS_SYSREG_GREEN_LED("user1",	"heartbeat",	LED0),
	VEXPRESS_SYSREG_GREEN_LED("user2",	"mmc0",		LED1),
	VEXPRESS_SYSREG_GREEN_LED("user3",	"cpu0",		LED2),
	VEXPRESS_SYSREG_GREEN_LED("user4",	"cpu1",		LED3),
	VEXPRESS_SYSREG_GREEN_LED("user5",	"cpu2",		LED4),
	VEXPRESS_SYSREG_GREEN_LED("user6",	"cpu3",		LED5),
	VEXPRESS_SYSREG_GREEN_LED("user7",	"cpu4",		LED6),
	VEXPRESS_SYSREG_GREEN_LED("user8",	"cpu5",		LED7),
};

struct gpio_led_platform_data vexpress_sysreg_leds_pdata = {
	.num_leds = ARRAY_SIZE(vexpress_sysreg_leds),
	.leds = vexpress_sysreg_leds,
};

#endif

static int vexpress_sysreg_probe(struct vmm_device *dev, 
				 const struct vmm_devtree_nodeid *devid)
{
	int err;
	virtual_addr_t base_va;

	if (!vexpress_sysreg_base) {
		err = vmm_devtree_regmap(dev->node, &base_va, 0);
		if (err) {
			return err;
		}		
		vexpress_sysreg_base = (void *)base_va;
		vexpress_sysreg_setup(dev->node);
	}

	if (!vexpress_sysreg_base) {
		vmm_printf("%s: Failed to obtain base address!\n", __func__);
		return VMM_EFAULT;
	}

	INIT_TIMER_EVENT(&vexpress_sysreg_config_timer, 
			 vexpress_sysreg_config_complete, NULL);

	vexpress_sysreg_dev = dev;

#if 0 /* FIXME: #ifdef CONFIG_GPIOLIB */
	vexpress_sysreg_gpio_chip.dev = &pdev->dev;
	err = gpiochip_add(&vexpress_sysreg_gpio_chip);
	if (err) {
		vexpress_config_bridge_unregister(
				vexpress_sysreg_config_bridge);
		dev_err(&pdev->dev, "Failed to register GPIO chip! (%d)\n",
				err);
		return err;
	}

	platform_device_register_data(vexpress_sysreg_dev, "leds-gpio",
			PLATFORM_DEVID_AUTO, &vexpress_sysreg_leds_pdata,
			sizeof(vexpress_sysreg_leds_pdata));
#endif

	return 0;
}

static int vexpress_sysreg_remove(struct vmm_device *dev)
{
	/* Nothing to do here for now. */
	return VMM_OK;
}

static const struct vmm_devtree_nodeid vexpress_sysreg_match[] = {
	{ .compatible = "arm,vexpress-sysreg", },
	{},
};

static struct vmm_driver vexpress_sysreg_driver = {
	.name = "vexpress_sysreg",
	.match_table = vexpress_sysreg_match,
	.probe = vexpress_sysreg_probe,
	.remove = vexpress_sysreg_remove,
};

static int __init vexpress_sysreg_init(void)
{
	/* Note: vexpress_sysreg_of_early_init() must be called
	 * from arch_board_early_init() or arch_cpu_early_init()
	 * before we reach here.
	 */
	return vmm_devdrv_register_driver(&vexpress_sysreg_driver);
}

static void __exit vexpress_sysreg_exit(void)
{
	vmm_devdrv_unregister_driver(&vexpress_sysreg_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
