/**
 * Copyright (c) 2017 Anup Patel.
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
 * @file gpio-forward.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief GPIO Forward Slave Emulator.
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>

#include <linux/gpio.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)			vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define MODULE_DESC			"GPIO Forward Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			gpio_forward_emulator_init
#define	MODULE_EXIT			gpio_forward_emulator_exit

struct gpio_forward_state {
	struct vmm_guest *guest;

	u32 in_count;
	u32 out_count;

	u32 *in_irq;
	u32 *out_gpio;

	u32 *out_irq;
	u32 *in_gpio;
};

static int gpio_forward_emulator_reset(struct vmm_emudev *edev)
{
	int i;
	struct gpio_forward_state *s = edev->priv;

	for (i = 0; i < s->in_count; i++) {
		gpio_direction_output(s->out_gpio[i], 0);
	}

	for (i = 0; i < s->out_count; i++) {
		gpio_direction_input(s->in_gpio[i]);
	}

	return VMM_OK;
}

static int gpio_forward_emulator_sync(struct vmm_emudev *edev,
				      unsigned long val, void *v)
{
	int i, level;
	struct gpio_forward_state *s = edev->priv;

	for (i = 0; i < s->out_count; i++) {
		level = __gpio_get_value(s->in_gpio[i]);
		vmm_devemu_emulate_irq(s->guest, s->out_irq[i], level);
	}

	return VMM_OK;
}

/* Process IRQ asserted in device emulation framework */
static void gpio_forward_irq_handle(u32 irq, int cpu, int level, void *opaque)
{
	int i, line;
	struct gpio_forward_state *s = opaque;

	DPRINTF("%s: irq=%d cpu=%d level=%d\n", __func__, irq, cpu, level);

	line = -1;
	for (i = 0; i < s->in_count; i++) {
		if (s->in_irq[i] == irq) {
			line = i;
			break;
		}
	}
	if (line == -1) {
		return;
	}

	__gpio_set_value(s->out_gpio[line], (level) ? 1 : 0);
}

static struct vmm_devemu_irqchip gpio_forward_irqchip = {
	.name = "GPIO_FORWARD",
	.handle = gpio_forward_irq_handle,
};

static int gpio_forward_emulator_probe(struct vmm_guest *guest,
				     struct vmm_emudev *edev,
				     const struct vmm_devtree_nodeid *eid)
{
	u32 i;
	int rc = VMM_OK;
	struct gpio_forward_state *s;

	s = vmm_zalloc(sizeof(struct gpio_forward_state));
	if (!s) {
		rc = VMM_ENOMEM;
		goto gpio_forward_emulator_probe_done;
	}

	s->guest = guest;

	s->in_count = vmm_devtree_attrlen(edev->node, "in_irq");
	s->in_count = s->in_count / sizeof(u32);

	s->out_count = vmm_devtree_attrlen(edev->node, "out_irq");
	s->out_count = s->out_count / sizeof(u32);

	if (s->in_count) {
		s->in_irq = vmm_zalloc(s->in_count * sizeof(*s->in_irq));
		if (!s->in_irq) {
			rc = VMM_ENOMEM;
			goto gpio_forward_emulator_probe_freestate_failed;
		}

		s->out_gpio = vmm_zalloc(s->in_count * sizeof(*s->out_gpio));
		if (!s->out_gpio) {
			rc = VMM_ENOMEM;
			goto gpio_forward_emulator_probe_freeinirq_failed;
		}

		rc = vmm_devtree_read_u32_array(edev->node, "in_irq",
						s->in_irq, s->in_count);
		if (rc) {
			goto gpio_forward_emulator_probe_freeoutgpio_failed;
		}

		rc = vmm_devtree_read_u32_array(edev->node, "out_gpio",
						s->out_gpio, s->in_count);
		if (rc) {
			goto gpio_forward_emulator_probe_freeoutgpio_failed;
		}
	}

	if (s->out_count) {
		s->out_irq = vmm_zalloc(s->out_count * sizeof(*s->out_irq));
		if (!s->out_irq) {
			rc = VMM_ENOMEM;
			goto gpio_forward_emulator_probe_freeoutgpio_failed;
		}

		s->in_gpio = vmm_zalloc(s->out_count * sizeof(*s->in_gpio));
		if (!s->in_gpio) {
			rc = VMM_ENOMEM;
			goto gpio_forward_emulator_probe_freeoutirq_failed;
		}

		rc = vmm_devtree_read_u32_array(edev->node, "out_irq",
						s->out_irq, s->out_count);
		if (rc) {
			goto gpio_forward_emulator_probe_freeingpio_failed;
		}

		rc = vmm_devtree_read_u32_array(edev->node, "in_gpio",
						s->in_gpio, s->out_count);
		if (rc) {
			goto gpio_forward_emulator_probe_freeingpio_failed;
		}
	}

	for (i = 0; i < s->in_count; i++) {
		vmm_devemu_register_irqchip(guest, s->in_irq[i],
					    &gpio_forward_irqchip, s);
	}

	edev->priv = s;

	goto gpio_forward_emulator_probe_done;

gpio_forward_emulator_probe_freeingpio_failed:
	if (s->in_gpio) {
		vmm_free(s->in_gpio);
	}
gpio_forward_emulator_probe_freeoutirq_failed:
	if (s->out_irq) {
		vmm_free(s->out_irq);
	}
gpio_forward_emulator_probe_freeoutgpio_failed:
	if (s->out_gpio) {
		vmm_free(s->out_gpio);
	}
gpio_forward_emulator_probe_freeinirq_failed:
	if (s->in_irq) {
		vmm_free(s->in_irq);
	}
gpio_forward_emulator_probe_freestate_failed:
	vmm_free(s);
gpio_forward_emulator_probe_done:
	return rc;
}

static int gpio_forward_emulator_remove(struct vmm_emudev *edev)
{
	u32 i;
	struct gpio_forward_state *s = edev->priv;

	if (!s) {
		return VMM_EFAIL;
	}

	for (i = 0; i < s->in_count; i++) {
		vmm_devemu_unregister_irqchip(s->guest, s->in_irq[i],
					      &gpio_forward_irqchip, s);
	}
	if (s->in_gpio) {
		vmm_free(s->in_gpio);
	}
	if (s->out_irq) {
		vmm_free(s->out_irq);
	}
	if (s->out_gpio) {
		vmm_free(s->out_gpio);
	}
	if (s->in_irq) {
		vmm_free(s->in_irq);
	}
	vmm_free(s);
	edev->priv = NULL;

	return VMM_OK;
}

static struct vmm_devtree_nodeid gpio_forward_emuid_table[] = {
	{ .type = "gpio-slave", .compatible = "gpio-forward", },
	{ /* end of list */ },
};

static struct vmm_emulator gpio_forward_emulator = {
	.name = "gpio-forward",
	.match_table = gpio_forward_emuid_table,
	.endian = VMM_DEVEMU_NATIVE_ENDIAN,
	.probe = gpio_forward_emulator_probe,
	.reset = gpio_forward_emulator_reset,
	.sync = gpio_forward_emulator_sync,
	.remove = gpio_forward_emulator_remove,
};

static int __init gpio_forward_emulator_init(void)
{
	return vmm_devemu_register_emulator(&gpio_forward_emulator);
}

static void __exit gpio_forward_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&gpio_forward_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
