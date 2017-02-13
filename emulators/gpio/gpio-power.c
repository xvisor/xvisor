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
 * @file gpio-power.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief GPIO Power Slave Emulator.
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)			vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define MODULE_DESC			"GPIO Power Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			gpio_power_emulator_init
#define	MODULE_EXIT			gpio_power_emulator_exit

enum gpio_power_sample_type {
	GPIO_POWER_SAMPLE_EDGE_FALLING = 0,
	GPIO_POWER_SAMPLE_EDGE_RISING = 1,
};

struct gpio_power_state {
	struct vmm_guest *guest;
	vmm_spinlock_t lock;
	enum gpio_power_sample_type sample_type;

	u32 in_data;
	u32 in_irq[2];
};

static int gpio_power_emulator_reset(struct vmm_emudev *edev)
{
	struct gpio_power_state *s = edev->priv;

	vmm_spin_lock(&s->lock);

	s->in_data = 0;

	vmm_spin_unlock(&s->lock);

	return VMM_OK;
}

/* Process IRQ asserted in device emulation framework */
static void gpio_power_irq_handle(u32 irq, int cpu, int level, void *opaque)
{
	u32 mask;
	int i, line;
	bool trigger = FALSE;
	struct gpio_power_state *s = opaque;

	DPRINTF("%s: irq=%d cpu=%d level=%d\n", __func__, irq, cpu, level);

	line = -1;
	for (i = 0; i < 2; i++) {
		if (s->in_irq[i] == irq) {
			line = i;
			break;
		}
	}
	if (line == -1) {
		return;
	}

	mask = 1 << line;

	vmm_spin_lock(&s->lock);

	switch (s->sample_type) {
	case GPIO_POWER_SAMPLE_EDGE_FALLING:
		if ((s->in_data & mask) && !level)
			trigger = TRUE;
		break;
	case GPIO_POWER_SAMPLE_EDGE_RISING:
		if (!(s->in_data & mask) && level)
			trigger = TRUE;
		break;
	default:
		break;
	};

	s->in_data &= ~mask;
	s->in_data |= (level) ? mask : 0x0;

	vmm_spin_unlock(&s->lock);

	if (trigger) {
		switch (line) {
		case 0:
			vmm_manager_guest_reboot_request(s->guest);
			break;
		case 1:
			vmm_manager_guest_shutdown_request(s->guest);
			break;
		default:
			break;
		};
	}
}

static struct vmm_devemu_irqchip gpio_power_irqchip = {
	.name = "GPIO_POWER",
	.handle = gpio_power_irq_handle,
};

static int gpio_power_emulator_probe(struct vmm_guest *guest,
				     struct vmm_emudev *edev,
				     const struct vmm_devtree_nodeid *eid)
{
	int rc = VMM_OK;
	const char *str;
	struct gpio_power_state *s;

	s = vmm_zalloc(sizeof(struct gpio_power_state));
	if (!s) {
		rc = VMM_EFAIL;
		goto gpio_power_emulator_probe_done;
	}

	rc = vmm_devtree_read_u32_array(edev->node, "in_irq",
					s->in_irq, array_size(s->in_irq));
	if (rc) {
		goto gpio_power_emulator_probe_freestate_failed;
	}

	s->guest = guest;
	INIT_SPIN_LOCK(&s->lock);

	rc = vmm_devtree_read_string(edev->node, "sample_type", &str);
	if (rc) {
		goto gpio_power_emulator_probe_freestate_failed;
	}

	if (!strcmp(str, "edge-falling")) {
		s->sample_type = GPIO_POWER_SAMPLE_EDGE_FALLING;
	} else if (!strcmp(str, "edge-rising")) {
		s->sample_type = GPIO_POWER_SAMPLE_EDGE_RISING;
	} else {
		rc = VMM_EINVALID;
		goto gpio_power_emulator_probe_freestate_failed;
	}

	vmm_devemu_register_irqchip(guest, s->in_irq[0],
				    &gpio_power_irqchip, s);
	vmm_devemu_register_irqchip(guest, s->in_irq[1],
				    &gpio_power_irqchip, s);

	edev->priv = s;

	goto gpio_power_emulator_probe_done;

gpio_power_emulator_probe_freestate_failed:
	vmm_free(s);
gpio_power_emulator_probe_done:
	return rc;
}

static int gpio_power_emulator_remove(struct vmm_emudev *edev)
{
	struct gpio_power_state *s = edev->priv;

	if (!s) {
		return VMM_EFAIL;
	}

	vmm_devemu_unregister_irqchip(s->guest, s->in_irq[0],
				      &gpio_power_irqchip, s);
	vmm_devemu_unregister_irqchip(s->guest, s->in_irq[1],
				      &gpio_power_irqchip, s);
	vmm_free(s);
	edev->priv = NULL;

	return VMM_OK;
}

static struct vmm_devtree_nodeid gpio_power_emuid_table[] = {
	{ .type = "gpio-slave", .compatible = "gpio-power", },
	{ /* end of list */ },
};

static struct vmm_emulator gpio_power_emulator = {
	.name = "gpio-power",
	.match_table = gpio_power_emuid_table,
	.endian = VMM_DEVEMU_NATIVE_ENDIAN,
	.probe = gpio_power_emulator_probe,
	.reset = gpio_power_emulator_reset,
	.remove = gpio_power_emulator_remove,
};

static int __init gpio_power_emulator_init(void)
{
	return vmm_devemu_register_emulator(&gpio_power_emulator);
}

static void __exit gpio_power_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&gpio_power_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
