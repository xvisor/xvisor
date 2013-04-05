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
 * @file arm11mpcore.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM11 MPCore Private Memory Emulator.
 * @details This source file implements the private memory region present
 * in ARM11 multiprocessor system 
 *
 * The source has been adapted from QEMU hw/arm11mpcore.c
 * 
 * ARM11MPCore internal peripheral emulation.
 *
 * Copyright (c) 2006-2007 CodeSourcery.
 * Written by Paul Brook
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_manager.h>
#include <vmm_scheduler.h>
#include <vmm_host_io.h>
#include <vmm_devemu.h>
#include <libs/stringlib.h>
#include <emu/arm_mptimer_emulator.h>
#include <emu/gic_emulator.h>

#define MODULE_DESC			"ARM11MPCore Private Region Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			arm11mpcore_emulator_init
#define	MODULE_EXIT			arm11mpcore_emulator_exit

/* Memory map (addresses are offsets from PERIPHBASE):
 *  0x0000-0x00ff -- Snoop Control Unit
 *  0x0100-0x01ff -- GIC CPU interface
 *  0x0200-0x02ff -- Global Timer
 *  0x0300-0x05ff -- nothing
 *  0x0600-0x06ff -- private timers and watchdogs
 *  0x0700-0x0fff -- nothing
 *  0x1000-0x1fff -- GIC Distributor
 *
 * We currently implement only the SCU and GIC portions.
 */

struct arm11mpcore_priv_state {
	struct vmm_guest *guest;
	struct vmm_emupic *pic;
	vmm_spinlock_t lock;

	/* Configuration */
	u32 num_cpu;

	/* Snoop Control Unit */
	u32 scu_control;

	/* Private & Watchdog Timer Block */
	struct mptimer_state *mpt;

	/* GIC-state */
	struct gic_state *gic;
};

static int arm11mpcore_scu_read(struct arm11mpcore_priv_state *s, 
				u32 offset, u32 *dst)
{
	int rc = VMM_OK;

	if (!s || !dst) {
		return VMM_EFAIL;
	}

	vmm_spin_lock(&s->lock);

	switch (offset) {
	case 0x00: /* Control */
		*dst = s->scu_control;
		break;
	case 0x04: /* Configuration */
		*dst = (((1 << s->num_cpu) - 1) << 4) | (s->num_cpu - 1);
		break;
	case 0x08: /* CPU Status */
		*dst = 0;
		break;
	case 0x0c: /* Invalidate all. */
		*dst = 0;
		break;
	default:
		rc = VMM_EFAIL;
		break;
	}

	vmm_spin_unlock(&s->lock);

	return rc;
}

static int arm11mpcore_scu_write(struct arm11mpcore_priv_state *s, 
				 u32 offset, u32 src_mask, u32 src)
{
	int rc = VMM_OK;

	if (!s) {
		return VMM_EFAIL;
	}

	src = src & ~src_mask;

	vmm_spin_lock(&s->lock);

	switch (offset) {
	case 0x00: /* Control */
		s->scu_control = src & 1;
		break;
	case 0x0c: /* Invalidate All. */
		break;
	default:
		rc = VMM_EFAIL;
		break;
	}

	vmm_spin_unlock(&s->lock);

	return rc;
}

static int arm11mpcore_emulator_read(struct vmm_emudev *edev,
				     physical_addr_t offset, 
				     void *dst, u32 dst_len)
{
	struct arm11mpcore_priv_state *s = edev->priv;
	int rc = VMM_OK;
	u32 regval = 0x0;

	if (offset < 0x100) {
		/* Read SCU block */
		rc = arm11mpcore_scu_read(s, offset & 0xFC, &regval);
	} else if (0x600 <= offset && offset < 0x700) {
		/* Read Private & Watchdog Timer blocks */
		rc = mptimer_reg_read(s->mpt, offset & 0xFC, &regval);
	} else {
		/* Read GIC */
		rc = gic_reg_read(s->gic, offset, &regval);
	}

	if (!rc) {
		regval = (regval >> ((offset & 0x3) * 8));
		switch (dst_len) {
		case 1:
			*(u8 *)dst = regval & 0xFF;
			break;
		case 2:
			*(u16 *)dst = vmm_cpu_to_le16(regval & 0xFFFF);
			break;
		case 4:
			*(u32 *)dst = vmm_cpu_to_le32(regval);
			break;
		default:
			rc = VMM_EFAIL;
			break;
		};
	}

	return rc;
}

static int arm11mpcore_emulator_write(struct vmm_emudev *edev,
				      physical_addr_t offset, 
				      void *src, u32 src_len)
{
	struct arm11mpcore_priv_state *s = edev->priv;
	int rc = VMM_OK, i;
	u32 regmask = 0x0, regval = 0x0;

	switch (src_len) {
	case 1:
		regmask = 0xFFFFFF00;
		regval = *(u8 *)src;
		break;
	case 2:
		regmask = 0xFFFF0000;
		regval = vmm_le16_to_cpu(*(u16 *)src);
		break;
	case 4:
		regmask = 0x00000000;
		regval = vmm_le32_to_cpu(*(u32 *)src);
		break;
	default:
		return VMM_EFAIL;
		break;
	};

	for (i = 0; i < (offset & 0x3); i++) {
		regmask = (regmask << 8) | ((regmask >> 24) & 0xFF);
	}
	regval = (regval << ((offset & 0x3) * 8));

	if (offset < 0x100) {
		/* Write SCU */
		rc = arm11mpcore_scu_write(s, offset & 0xFC, regmask, regval);
	} else if (0x600 <= offset && offset < 0x700) {
		/* Write Private & Watchdog Timer blocks */
		rc = mptimer_reg_write(s->mpt, offset & 0xFC, regmask, regval);
	} else {
		/* Write GIC */
		rc = gic_reg_write(s->gic, offset, regmask, regval);
	}

	return rc;
}

static int arm11mpcore_emulator_reset(struct vmm_emudev *edev)
{
	struct arm11mpcore_priv_state *s = edev->priv;

	/* Reset SCU state */
	s->scu_control = 0;

	/* Reset GIC state */
	gic_state_reset(s->gic);

	/* Reset Private & Watchdog Timer state */
	mptimer_state_reset(s->mpt);

	return VMM_OK;
}

static int arm11mpcore_emulator_probe(struct vmm_guest *guest,
				      struct vmm_emudev *edev,
				      const struct vmm_devtree_nodeid *eid)
{
	int rc = VMM_OK;
	struct arm11mpcore_priv_state *s;
	const char *attr;
	u32 parent_irq, timer_irq, wdt_irq;

	s = vmm_zalloc(sizeof(struct arm11mpcore_priv_state));
	if (!s) {
		rc = VMM_EFAIL;
		goto arm11mp_probe_done;
	}

	s->num_cpu = guest->vcpu_count;

	attr = vmm_devtree_attrval(edev->node, "parent_irq");
	if (!attr) {
		goto arm11mp_probe_failed;
	}
	parent_irq = *((u32 *)attr);

	attr = vmm_devtree_attrval(edev->node, "timer_irq");
	if (!attr) {
		goto arm11mp_probe_failed;
	}
	timer_irq = ((u32 *)attr)[0];
	wdt_irq = ((u32 *)attr)[1];

	/* Allocate and init MPT state */
	if (!(s->mpt = mptimer_state_alloc(guest, edev, s->num_cpu, 1000000,
				 	  timer_irq, wdt_irq))) {
		goto arm11mp_probe_failed;
	}

	/* Allocate and init GIC state */
	if (!(s->gic = gic_state_alloc(edev->node->name, guest, 
				       GIC_TYPE_ARM11MPCORE, s->num_cpu,
				       FALSE, 0, 96, parent_irq))) {
		goto arm11mp_gic_alloc_failed;
	}

	s->guest = guest;
	INIT_SPIN_LOCK(&s->lock);

	edev->priv = s;

	goto arm11mp_probe_done;

arm11mp_gic_alloc_failed:
	mptimer_state_free(s->mpt);

arm11mp_probe_failed:
	vmm_free(s);
	rc = VMM_EFAIL;

arm11mp_probe_done:
	return rc;
}


static int arm11mpcore_emulator_remove(struct vmm_emudev *edev)
{
	struct arm11mpcore_priv_state *s = edev->priv;

	if (s) {
		/* Remove GIC state */
		gic_state_free(s->gic);

		/* Remove MPtimer state */
		mptimer_state_free(s->mpt);

		vmm_free(s);
	}

	return VMM_OK;
}

static struct vmm_devtree_nodeid arm11mpcore_emuid_table[] = {
	{ .type = "misc", 
	  .compatible = "arm,arm11mpcore", 
	  .data = NULL,
	},
	{ /* end of list */ },
};

static struct vmm_emulator arm11mpcore_emulator = {
	.name = "arm11mpcore",
	.match_table = arm11mpcore_emuid_table,
	.probe = arm11mpcore_emulator_probe,
	.read = arm11mpcore_emulator_read,
	.write = arm11mpcore_emulator_write,
	.reset = arm11mpcore_emulator_reset,
	.remove = arm11mpcore_emulator_remove,
};

static int __init arm11mpcore_emulator_init(void)
{
	return vmm_devemu_register_emulator(&arm11mpcore_emulator);
}

static void __exit arm11mpcore_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&arm11mpcore_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
