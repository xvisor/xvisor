/**
 * Copyright (C) 2015 Institut de Recherche Technologique SystemX and OpenWide.
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
 * @file sp805.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief SP805 Watchdog emulator.
 * @details This source file implements the SP805 watchdog emulator.
 *
 * The source has been largely adapted from sp805 emulator.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_manager.h>
#include <vmm_devemu.h>
#include <vmm_stdio.h>
#include <vmm_timer.h>
#include <vmm_spinlocks.h>
#include <libs/mathlib.h>

#define MODULE_DESC			"Sp805 Device Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			sp805_emulator_init
#define	MODULE_EXIT			sp805_emulator_exit

#define WDT_LOAD			0x000
#define WDT_VALUE			0x004
#define WDT_CTRL			0x008
#define WDT_CTRL_INTEN			(1 << 0)
#define WDT_CTRL_RESEN			(1 << 1)
#define WDT_CTRL_MASK			(WDT_CTRL_INTEN | WDT_CTRL_RESEN)

#define WDT_IT_CLR			0x00C
#define WDT_IT_RIS			0x010
#define WDT_IT_MIS			0x014
#define WDT_LOCK			0xC00
#define WDT_LOCK_ACCESS			0x1ACCE551

#define EMU_NAME(emudev)		((emudev)->node->name)

#undef DEBUG

#define sp805_msg(LEVEL, SP805, FORMAT...)				\
	do {								\
		vmm_lprintf(LEVEL, EMU_NAME(sp805->edev), FORMAT);	\
	} while (0)

#ifdef DEBUG

#define sp805_error(SP805, ...)					\
	sp805_msg(VMM_LOGLEVEL_ERROR, SP805, "Error - " __VA_ARGS__)

#define sp805_warning(SP805, ...)					\
	sp805_msg(VMM_LOGLEVEL_ERROR, SP805, "Warning - " __VA_ARGS__)

#define sp805_debug(SP805, ...)					\
	sp805_msg(VMM_LOGLEVEL_ERROR, SP805, __VA_ARGS__)

#else /* !DEBUG */

#define sp805_error(SP805, ...)					\
	sp805_msg(VMM_LOGLEVEL_ERROR, SP805, "Error - " __VA_ARGS__)

#define sp805_warning(SP805, ...)					\
	sp805_msg(VMM_LOGLEVEL_WARNING, SP805, "Warning - " __VA_ARGS__)

#define sp805_debug(SP805, ...)

#endif /* DEBUG */

struct sp805_state {
	struct vmm_emudev *edev;
	const char *id;
	u32 irq;
	int irq_level;
	u32 ris;
	u32 ctrl;
	u32 load;
	u32 freezed_value;
	u32 locked;
	u64 timestamp;
	struct vmm_guest *guest;
	struct vmm_spinlock lock;
	struct vmm_timer_event event;
};

/* Must be called with sp805->lock held */
static int _sp805_enabled(struct sp805_state *sp805)
{
	return sp805->ctrl & WDT_CTRL_INTEN;
}

/* Must be called with sp805->lock held */
static int _sp805_counter_reload(struct sp805_state *sp805)
{
	int rc = VMM_OK;
	u64 reload = (sp805->load + 1) * 1000;

	if (!_sp805_enabled(sp805)) {
		sp805_debug(sp805, "Disabled, event not started.\n");
		return VMM_OK;
	}

	sp805->timestamp = vmm_timer_timestamp();
	vmm_timer_event_stop(&sp805->event);
	rc = vmm_timer_event_start(&sp805->event, reload);
	sp805_debug(sp805, "Counter started: IRQ in %d ms (%d)\n",
		    udiv32(sp805->load + 1, 1000), rc);

	return rc;
}

/* Must be called with sp805->lock held */
static u32 _sp805_reg_value(struct sp805_state *sp805)
{
	u64 load = vmm_timer_timestamp();

	if (!_sp805_enabled(sp805)) {
		/* Interrupt disabled: counter is disabled */
		return sp805->freezed_value;
	}

	if (likely(load > sp805->timestamp)) {
		load = load - sp805->timestamp;
	} else {
		load = sp805->timestamp + ~load + 1;
	}

	return udiv64(load, 1000);
}

/* Must be called with sp805->lock held */
static int _sp805_counter_stop(struct sp805_state *sp805)
{
	int rc = VMM_OK;

	rc = vmm_timer_event_stop(&sp805->event);
	sp805->freezed_value = _sp805_reg_value(sp805);
	sp805_debug(sp805, "Counter stopped at 0x%08x(%d)\n",
		    sp805->freezed_value, rc);

	return rc;
}

static int sp805_reg_read(struct sp805_state *sp805,
			  physical_addr_t offset,
			  u32 *dst)
{
	int ret = VMM_OK;

	offset &= ~0x3;
	if (offset >= 0xfe0 && offset < 0x1000) {
		*dst = sp805->id[(offset - 0xfe0) >> 2];
		return ret;
	}

	vmm_spin_lock(&sp805->lock);

	switch (offset) {
	case WDT_LOAD:
		*dst = sp805->load;
		sp805_debug(sp805, "Read WDTLOAD: 0x%08x\n", *dst);
		break;
	case WDT_VALUE:
		*dst = _sp805_reg_value(sp805);
		sp805_debug(sp805, "Read WDTVALUE: 0x%08x\n", *dst);
		break;
	case WDT_CTRL:
		*dst = sp805->ctrl;
		sp805_debug(sp805, "Read WDTCTRL: 0x%08x\n", *dst);
		break;
	case WDT_IT_CLR: /* WDT_IT_CLR: Write only */
		*dst = 0x0;
		break;
	case WDT_IT_RIS:
		*dst = sp805->ris;
		sp805_debug(sp805, "Read WDTRIS: 0x%08x\n", *dst);
		break;
	case WDT_IT_MIS:
		*dst = sp805->ris & sp805->ctrl;
		sp805_debug(sp805, "Read WDTMIS: 0x%08x\n", *dst);
		break;
	case WDT_LOCK:
		*dst = sp805->locked;
		sp805_debug(sp805, "Read WDTLOCK: 0x%08x\n", *dst);
		break;
	default:
		ret = VMM_EINVALID;
		break;
	};

	vmm_spin_unlock(&sp805->lock);

	return ret;
}

static int sp805_emulator_read8(struct vmm_emudev *edev,
				physical_addr_t offset,
				u8 *dst)
{
	u32 val = 0;
	int ret = VMM_OK;
	int shift = (offset & 0x3) * 8;

	ret = sp805_reg_read(edev->priv, offset, &val);
	if (VMM_OK == ret) {
		*dst = (val >> shift) & 0xFF;
	}

	return ret;
}

static int sp805_emulator_read16(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u16 *dst)
{
	u32 val = 0;
	int ret = VMM_OK;
	int shift = (offset & 0x1) * 16;

	ret = sp805_reg_read(edev->priv, offset, &val);
	if (VMM_OK == ret) {
		*dst = (val >> shift) & 0xFFFF;
	}

	return ret;
}

static int sp805_emulator_read32(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u32 *dst)
{
	u32 val = 0;
	int ret = VMM_OK;

	ret = sp805_reg_read(edev->priv, offset, &val);
	if (VMM_OK == ret) {
		*dst = val;
	}

	return ret;
}

static int sp805_reg_write(struct sp805_state *sp805,
			   physical_addr_t offset,
			   u32 val)
{
	int ret = VMM_OK;

	offset &= ~0x3;

	if (sp805->locked) {
		sp805_warning(sp805, "Registers are locked\n");
		return VMM_OK;
	}

	vmm_spin_lock(&sp805->lock);

	switch (offset) {
	case WDT_LOAD:
		sp805->load = val;
		_sp805_counter_reload(sp805);
		break;
	case WDT_VALUE:
		/* WDT_VALUE: Read only */
		break;
	case WDT_CTRL:
		if (likely(val & WDT_CTRL_INTEN)) {
			/* Enabling IRQ */
			if (!(sp805->ctrl & WDT_CTRL_INTEN)) {
				/* The interrupt was disabled. Enabled now */
				_sp805_counter_reload(sp805);
			}
		} else {
			_sp805_counter_stop(sp805);
			/* Disabling IRQ */
			if (unlikely(sp805->irq_level)) {
				/* The interrupts is disabled */
				vmm_devemu_emulate_irq(sp805->guest,
						       sp805->irq, 0);
			}
		}
		sp805->ctrl = val & WDT_CTRL_MASK;
		break;
	case WDT_IT_CLR:
		if (likely(sp805->irq_level)) {
			vmm_devemu_emulate_irq(sp805->guest, sp805->irq, 0);
			sp805->irq_level = 0;
			_sp805_counter_reload(sp805);
		}
		break;
	case WDT_IT_RIS:
		/* WDT_IT_RIS: Read only */
		break;
	case WDT_IT_MIS:
		/* WDT_IT_MIS: Read only */
		break;
	case WDT_LOCK:
		/* WDT_LOCK: Special 32-bit write case */
		break;
	default:
		ret = VMM_EINVALID;
		break;
	};

	vmm_spin_unlock(&sp805->lock);

	return ret;
}

static int sp805_emulator_write8(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u8 src)
{
	u32 val = 0;
	u32 mask = 0xFF;
	int rc, shift = (offset & 3) * 8;
	struct sp805_state *sp805 = edev->priv;

	sp805_debug(sp805, "%s: Write 0x%02x at 0x%08x\n", src, offset);
	rc = sp805_reg_read(sp805, offset, &val);
	if (rc != VMM_OK) {
		return rc;
	}
	val &= mask;

	mask <<= shift;
	val = (src << shift) | (val & ~mask);

	return sp805_reg_write(sp805, offset, val);
}

static int sp805_emulator_write16(struct vmm_emudev *edev,
				  physical_addr_t offset,
				  u16 src)
{
	u32 val = 0;
	u32 mask = 0xFFFF;
	int rc, shift = (offset & 1) * 16;
	struct sp805_state *sp805 = edev->priv;

	sp805_debug(sp805, "%s: Write 0x%04x at 0x%08x\n", src, offset);
	rc = sp805_reg_read(sp805, offset, &val);
	if (rc != VMM_OK) {
		return rc;
	}
	val &= mask;

	mask <<= shift;
	val = (src << shift) | (val & ~mask);

	return sp805_reg_write(sp805, offset, val);
}

static int sp805_emulator_write32(struct vmm_emudev *edev,
				  physical_addr_t offset,
				  u32 src)
{
	int ret = VMM_OK;
	struct sp805_state *sp805 = edev->priv;

	sp805_debug(sp805, "Write 0x%08x at 0x%08x\n", src, offset);

	if (WDT_LOCK == offset) {
		vmm_spin_lock(&sp805->lock);
		if (WDT_LOCK_ACCESS == src) {
			sp805->locked = 0;
			sp805_debug(sp805, "Unlocked\n");
		} else {
			sp805->locked = 1;
			sp805_debug(sp805, "Locked\n");
		}
		vmm_spin_unlock(&sp805->lock);
	} else {
		ret = sp805_reg_write(sp805, offset, src);
	}

	return ret;
}

static void sp805_reg_reset(struct sp805_state *sp805)
{
	vmm_spin_lock(&sp805->lock);

	sp805->load = 0xFFFFFFFF;
	sp805->freezed_value = 0xFFFFFFFF;
	sp805->locked = 0;
	sp805->timestamp = vmm_timer_timestamp();

	vmm_spin_unlock(&sp805->lock);
}

static int sp805_emulator_reset(struct vmm_emudev *edev)
{
	struct sp805_state *sp805 = edev->priv;

	sp805_debug(sp805, "Reset\n");
	sp805_reg_reset(sp805);

	return VMM_OK;
}

static void sp805_emulator_event(struct vmm_timer_event *evt)
{
	struct sp805_state *sp805 = evt->priv;

	sp805_debug(sp805, "Event\n");
	if (sp805->ctrl & WDT_CTRL_INTEN) {
		if (unlikely(sp805->irq_level)) {
			sp805_debug(sp805, "Request guest reboot\n");
			vmm_manager_guest_reboot_request(sp805->guest);
			return;
		} else {
			vmm_spin_lock(&sp805->lock);
			sp805->irq_level = 1;
			vmm_devemu_emulate_irq(sp805->guest, sp805->irq, 1);
			vmm_spin_unlock(&sp805->lock);
			sp805_debug(sp805, "IRQ triggered\n");
		}
	}

	vmm_spin_lock(&sp805->lock);
	_sp805_counter_reload(sp805);
	vmm_spin_unlock(&sp805->lock);
}

static int sp805_emulator_probe(struct vmm_guest *guest,
				struct vmm_emudev *edev,
				const struct vmm_devtree_nodeid *eid)
{
	int rc;
	struct sp805_state *sp805 = NULL;

	sp805 = vmm_zalloc(sizeof(struct sp805_state));
	if (!sp805) {
		return VMM_EFAIL;
	}

	sp805->edev = edev;
	sp805->id = eid->data;
	sp805->guest = guest;
	INIT_SPIN_LOCK(&sp805->lock);
	INIT_TIMER_EVENT(&sp805->event, sp805_emulator_event, sp805);

	rc = vmm_devtree_irq_get(edev->node, &sp805->irq, 0);
	if (rc) {
		vmm_free(sp805);
		return rc;
	}

	edev->priv = sp805;

	sp805_debug(sp805, "Probed\n");

	return VMM_OK;
}

static int sp805_emulator_remove(struct vmm_emudev *edev)
{
	struct sp805_state *sp805 = edev->priv;

	sp805_debug(sp805, "Removed\n");
	vmm_timer_event_stop(&sp805->event);
	vmm_free(sp805);
	edev->priv = NULL;

	return VMM_OK;
}

static const u8 sp805_ids[] = {
	/* Watchdog ID */
	0x05, 0x18, 0x14, 0x00,
	/* PrimeCell ID */
	0xd, 0xf0, 0x05, 0xb1
};

static struct vmm_devtree_nodeid sp805_emuid_table[] = {
	{
		.type = "watchdog",
		.compatible = "primecell,sp805",
		.data = &sp805_ids,
	},
	{ /* end of list */ },
};

static struct vmm_emulator sp805_emulator = {
	.name = "sp805",
	.match_table = sp805_emuid_table,
	.endian = VMM_DEVEMU_NATIVE_ENDIAN,
	.probe = sp805_emulator_probe,
	.read8 = sp805_emulator_read8,
	.write8 = sp805_emulator_write8,
	.read16 = sp805_emulator_read16,
	.write16 = sp805_emulator_write16,
	.read32 = sp805_emulator_read32,
	.write32 = sp805_emulator_write32,
	.reset = sp805_emulator_reset,
	.remove = sp805_emulator_remove,
};

static int __init sp805_emulator_init(void)
{
	return vmm_devemu_register_emulator(&sp805_emulator);
}

static void __exit sp805_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&sp805_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
