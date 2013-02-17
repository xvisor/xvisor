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
 * @file arm_sysregs.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM Realview / Versatile Express System Registers emulator.
 * @details This source file implements the ARM system-registers emulator.
 *
 * The source has been largely adapted from QEMU 0.14.xx hw/arm_sysctl.c 
 *
 * Status and system control registers for ARM RealView/Versatile boards.
 *
 * Copyright (c) 2006-2007 CodeSourcery.
 * Written by Paul Brook
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_spinlocks.h>
#include <vmm_devtree.h>
#include <vmm_timer.h>
#include <vmm_workqueue.h>
#include <vmm_manager.h>
#include <vmm_host_io.h>
#include <vmm_devemu.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>

#define MODULE_DESC			"Realview Sysctl Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			arm_sysregs_emulator_init
#define	MODULE_EXIT			arm_sysregs_emulator_exit

#define LOCK_VAL			0x0000a05f

#define REALVIEW_SYSID_PBA8		0x01780500
#define REALVIEW_PROCID_PBA8		0x00000000
#define VEXPRESS_SYSID_CA9		0x01900000
#define VEXPRESS_PROCID_CA9		0x0c000191
#define VERSATILEPB_SYSID_ARM926	0x41008004
#define VERSATILEPB_PROCID_ARM926	0x00000000

/* The PB926 actually uses a different format for
 * its SYS_ID register. Fortunately the bits which are
 * board type on later boards are distinct.
 */
#define BOARD_ID_PB926			0x0100
#define BOARD_ID_EB			0x0140
#define BOARD_ID_PBA8			0x0178
#define BOARD_ID_PBX			0x0182
#define BOARD_ID_VEXPRESS		0x0190

struct arm_sysregs {
	struct vmm_guest *guest;
	struct vmm_emupic *pic;
	vmm_spinlock_t lock;
	u64 ref_100hz;
	u64 ref_24mhz;
	u32 mux_in_irq[2];
	u32 mux_out_irq;
	struct vmm_work reboot;
	struct vmm_work shutdown;

	u32 sys_id;
	u32 leds;
	u32 lockval;
	u32 cfgdata1;
	u32 cfgdata2;
	u32 flags;
	u32 nvflags;
	u32 resetlevel;
	u32 proc_id;
	u32 sys_mci;
	u32 sys_cfgdata;
	u32 sys_cfgctrl;
	u32 sys_cfgstat;
	u32 sys_clcd;
};

static int board_id(struct arm_sysregs *s)
{
    /* Extract the board ID field from the SYS_ID register value */
    return (s->sys_id >> 16) & 0xfff;
}

static int arm_sysregs_emulator_read(struct vmm_emudev *edev,
			    	     physical_addr_t offset, 
				     void *dst, u32 dst_len)
{
	int rc = VMM_OK;
	u32 regval = 0x0;
	u64 tdiff;
	struct arm_sysregs *s = edev->priv;

	vmm_spin_lock(&s->lock);

	switch (offset & ~0x3) {
	case 0x00: /* ID */
		regval = s->sys_id;
		break;
	case 0x04: /* SW */
		/* General purpose hardware switches.
		We don't have a useful way of exposing these to the user.  */
		regval = 0;
		break;
	case 0x08: /* LED */
		regval = s->leds;
		break;
	case 0x20: /* LOCK */
		regval = s->lockval;
		break;
	case 0x0c: /* OSC0 */
	case 0x10: /* OSC1 */
	case 0x14: /* OSC2 */
	case 0x18: /* OSC3 */
	case 0x1c: /* OSC4 */
		regval = 0;
		break;
	case 0x24: /* 100HZ */
		tdiff = vmm_timer_timestamp() - s->ref_100hz;
		regval = udiv64(tdiff, 10000000);
		break;
	case 0x28: /* CFGDATA1 */
		regval = s->cfgdata1;
		break;
	case 0x2c: /* CFGDATA2 */
		regval = s->cfgdata2;
		break;
	case 0x30: /* FLAGS */
		regval = s->flags;
		break;
	case 0x38: /* NVFLAGS */
		regval = s->nvflags;
		break;
	case 0x40: /* RESETCTL */
		if (board_id(s) == BOARD_ID_VEXPRESS) {
			/* reserved: RAZ/WI */
			regval = 0;
		} else {
			regval = s->resetlevel;
		}
		break;
	case 0x44: /* PCICTL */
		regval = 1;
		break;
	case 0x48: /* MCI */
		regval = s->sys_mci;
		break;
	case 0x4c: /* FLASH */
		regval = 0;
		break;
	case 0x50: /* CLCD */
		regval = s->sys_clcd;
		break;
	case 0x54: /* CLCDSER */
		regval = 0;
		break;
	case 0x58: /* BOOTCS */
		regval = 0;
		break;
	case 0x5c: /* 24MHz */
		tdiff = vmm_timer_timestamp() - s->ref_100hz;
		/* Note: What we want is the below value 
		 * regval = udiv64(tdiff * 24, 1000);
		 * In integer arithmetic division by constant can be simplified
		 * (a * 24) / 1000
		 * = a * (24 / 1000)
		 * = a * (3 / 125)
		 * = a * (3 / 128) * (128 / 125)
		 * = a * (3 / 128) + a * (3 / 128) * (3 / 125)
		 * ~ a * (3 / 128) + a * (3 / 128) * (3 / 128) 
		 * ~ (a * 3) >> 7 + (a * 9) >> 14 
		 */
		tdiff = ((tdiff * 3) >> 7) + ((tdiff * 9) >> 14);
		regval = tdiff;
		break;
	case 0x60: /* MISC */
		regval = 0;
		break;
	case 0x84: /* PROCID0 */
		regval = s->proc_id;
		break;
	case 0x88: /* PROCID1 */
		regval = 0xff000000;
		break;
	case 0x64: /* DMAPSR0 */
	case 0x68: /* DMAPSR1 */
	case 0x6c: /* DMAPSR2 */
	case 0x70: /* IOSEL */
	case 0x74: /* PLDCTL */
	case 0x80: /* BUSID */
	case 0x8c: /* OSCRESET0 */
	case 0x90: /* OSCRESET1 */
	case 0x94: /* OSCRESET2 */
	case 0x98: /* OSCRESET3 */
	case 0x9c: /* OSCRESET4 */
	case 0xc0: /* SYS_TEST_OSC0 */
	case 0xc4: /* SYS_TEST_OSC1 */
	case 0xc8: /* SYS_TEST_OSC2 */
	case 0xcc: /* SYS_TEST_OSC3 */
	case 0xd0: /* SYS_TEST_OSC4 */
		regval = 0;
		break;
	case 0xa0: /* SYS_CFGDATA */
		if (board_id(s) != BOARD_ID_VEXPRESS) {
			rc = VMM_EFAIL;
		} else {
			regval = s->sys_cfgdata;
		}
		break;
	case 0xa4: /* SYS_CFGCTRL */
		if (board_id(s) != BOARD_ID_VEXPRESS) {
			rc = VMM_EFAIL;
		} else {
			regval = s->sys_cfgctrl;
		}
		break;
	case 0xa8: /* SYS_CFGSTAT */
		if (board_id(s) != BOARD_ID_VEXPRESS) {
			rc = VMM_EFAIL;
		} else {
			regval = s->sys_cfgstat;
		}
		break;
	default:
		rc = VMM_EFAIL;
		break;
	}

	vmm_spin_unlock(&s->lock);

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

static void arm_sysregs_shutdown(struct vmm_work *w)
{
	struct arm_sysregs *s = container_of(w, struct arm_sysregs, shutdown);

	vmm_manager_guest_reset(s->guest);
}

static void arm_sysregs_reboot(struct vmm_work *w)
{
	struct arm_sysregs *s = container_of(w, struct arm_sysregs, reboot);

	vmm_manager_guest_reset(s->guest);
	vmm_manager_guest_kick(s->guest);
}

static int arm_sysregs_emulator_write(struct vmm_emudev *edev,
				      physical_addr_t offset, 
				      void *src, u32 src_len)
{
	int rc = VMM_OK, i;
	u32 regmask = 0x0, regval = 0x0;
	struct arm_sysregs *s = edev->priv;

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

	vmm_spin_lock(&s->lock);

	switch (offset & ~0x3) {
	case 0x08: /* LED */
		s->leds &= regmask;
		s->leds |= regval;
		break;
	case 0x0c: /* OSC0 */
	case 0x10: /* OSC1 */
	case 0x14: /* OSC2 */
	case 0x18: /* OSC3 */
	case 0x1c: /* OSC4 */
		/* ??? */
		break;
	case 0x20: /* LOCK */
		s->lockval &= regmask;
		if (regval == LOCK_VAL) {
			s->lockval = regval;
		} else {
			s->lockval = regval & 0x7fff;
		}
		break;
	case 0x28: /* CFGDATA1 */
		/* ??? Need to implement this.  */
		s->cfgdata1 &= regmask;
		s->cfgdata1 |= regval;
		break;
	case 0x2c: /* CFGDATA2 */
		/* ??? Need to implement this.  */
		s->cfgdata2 &= regmask;
		s->cfgdata2 |= regval;
		break;
	case 0x30: /* FLAGSSET */
		s->flags |= regval;
		break;
	case 0x34: /* FLAGSCLR */
		s->flags &= ~regval;
		break;
	case 0x38: /* NVFLAGSSET */
		s->nvflags |= regval;
		break;
	case 0x3c: /* NVFLAGSCLR */
		s->nvflags &= ~regval;
		break;
	case 0x40: /* RESETCTL */
		switch (board_id(s)) {
		case BOARD_ID_PB926:
			if (s->lockval == LOCK_VAL) {
				s->resetlevel &= regmask;
				s->resetlevel |= regval;
				if (s->resetlevel & 0x100) {
					vmm_workqueue_schedule_work(NULL, 
								&s->reboot);
				}
			}
			break;
		case BOARD_ID_PBX:
		case BOARD_ID_PBA8:
			if (s->lockval == LOCK_VAL) {
				s->resetlevel &= regmask;
				s->resetlevel |= regval;
				if (s->resetlevel & 0x04) {
					vmm_workqueue_schedule_work(NULL, 
								&s->reboot);
				}
			}
			break;
		case BOARD_ID_VEXPRESS:
		case BOARD_ID_EB:
		default:
			/* reserved: RAZ/WI */
			break;
		};
		break;
	case 0x44: /* PCICTL */
		/* nothing to do.  */
		break;
	case 0x4c: /* FLASH */
	case 0x50: /* CLCD */
		switch (board_id(s)) {
		case BOARD_ID_PB926:
			/* On 926 bits 13:8 are R/O, bits 1:0 control
			 * the mux that defines how to interpret the PL110
			 * graphics format, and other bits are r/w but we
			 * don't implement them to do anything.
			 */
			s->sys_clcd &= 0x3f00;
			s->sys_clcd |= regval & ~0x3f00;
			vmm_devemu_emulate_irq(s->guest, 
					       s->mux_out_irq, regval & 0x3);
			break;
		case BOARD_ID_EB:
			/* The EB is the same except that there is no mux since
			 * the EB has a PL111.
			 */
			s->sys_clcd &= 0x3f00;
			s->sys_clcd |= regval & ~0x3f00;
			break;
		case BOARD_ID_PBA8:
		case BOARD_ID_PBX:
			/* On PBA8 and PBX bit 7 is r/w and all other bits
			 * are either r/o or RAZ/WI.
			 */
			s->sys_clcd &= (1 << 7);
			s->sys_clcd |= regval & ~(1 << 7);
			break;
		case BOARD_ID_VEXPRESS:
		default:
			/* On VExpress this register is unimplemented 
			 * and will RAZ/WI */
			break;
		};
		break;
	case 0x54: /* CLCDSER */
	case 0x64: /* DMAPSR0 */
	case 0x68: /* DMAPSR1 */
	case 0x6c: /* DMAPSR2 */
	case 0x70: /* IOSEL */
	case 0x74: /* PLDCTL */
	case 0x80: /* BUSID */
	case 0x84: /* PROCID0 */
	case 0x88: /* PROCID1 */
	case 0x8c: /* OSCRESET0 */
	case 0x90: /* OSCRESET1 */
	case 0x94: /* OSCRESET2 */
	case 0x98: /* OSCRESET3 */
	case 0x9c: /* OSCRESET4 */
		break;
	case 0xa0: /* SYS_CFGDATA */
		if (board_id(s) != BOARD_ID_VEXPRESS) {
			rc =  VMM_EFAIL;
			break;
		}
		s->sys_cfgdata &= regmask;
		s->sys_cfgdata |= regval;
		break;
	case 0xa4: /* SYS_CFGCTRL */
		if (board_id(s) != BOARD_ID_VEXPRESS) {
			rc =  VMM_EFAIL;
			break;
		}
		s->sys_cfgctrl &= regmask;
		s->sys_cfgctrl |= regval & ~(3 << 18);
		s->sys_cfgstat = 1;            /* complete */
		switch (s->sys_cfgctrl) {
		case 0xc0800000: /* SYS_CFG_SHUTDOWN to motherboard */
			vmm_workqueue_schedule_work(NULL, &s->shutdown);
			break;
		case 0xc0900000: /* SYS_CFG_REBOOT to motherboard */
			vmm_workqueue_schedule_work(NULL, &s->reboot);
			break;
		default:
			s->sys_cfgstat |= 2;        /* error */
		}
		break;
	case 0xa8: /* SYS_CFGSTAT */
		if (board_id(s) != BOARD_ID_VEXPRESS) {
			rc =  VMM_EFAIL;
			break;
		}
		s->sys_cfgstat &= regmask;
		s->sys_cfgstat |= regval & 3;
		break;
	default:
		rc = VMM_EFAIL;
		break;
	}

	vmm_spin_unlock(&s->lock);

	return rc;
}

static int arm_sysregs_emulator_reset(struct vmm_emudev *edev)
{
	struct arm_sysregs *s = edev->priv;

	vmm_spin_lock(&s->lock);

	s->ref_100hz = vmm_timer_timestamp();
	s->ref_24mhz = s->ref_100hz;

	s->leds = 0;
	s->lockval = 0x10000;
	s->cfgdata1 = 0;
	s->cfgdata2 = 0;
	s->flags = 0;
	s->resetlevel = 0;
	if (board_id(s) == BOARD_ID_VEXPRESS) {
		/* On VExpress this register will RAZ/WI */
		s->sys_clcd = 0;
	} else {
		/* All others: CLCDID 0x1f, indicating VGA */
		s->sys_clcd = 0x1f00;
	}

	vmm_spin_unlock(&s->lock);

	return VMM_OK;
}

/* Process IRQ asserted in device emulation framework */
static int arm_sysregs_irq_handle(struct vmm_emupic *epic, 
				  u32 irq, int cpu, int level)
{
	int bit;
	struct arm_sysregs * s = epic->priv;

	if (s->mux_in_irq[0] == irq) {
		/* For PB926 and EB write-protect is bit 2 of SYS_MCI;
		 * for all later boards it is bit 1.
		 */
		vmm_spin_lock(&s->lock);
		bit = 2;
		if ((board_id(s) == BOARD_ID_PB926) || 
		    (board_id(s) == BOARD_ID_EB)) {
			bit = 4;
		}
		s->sys_mci &= ~bit;
		if (level) {
			s->sys_mci |= bit;
		}
		vmm_spin_unlock(&s->lock);
	} else if (s->mux_in_irq[1] == irq) {
		vmm_spin_lock(&s->lock);
		s->sys_mci &= ~1;
		if (level) {
			s->sys_mci |= 1;
		}
		vmm_spin_unlock(&s->lock);
	} else {
		return VMM_EMUPIC_GPIO_UNHANDLED;
	}

	return VMM_EMUPIC_GPIO_HANDLED;
}

static int arm_sysregs_emulator_probe(struct vmm_guest *guest,
				   struct vmm_emudev *edev,
				   const struct vmm_emuid *eid)
{
	int rc = VMM_OK;
	const char * attr;
	struct arm_sysregs *s;

	s = vmm_zalloc(sizeof(struct arm_sysregs));
	if (!s) {
		rc = VMM_EFAIL;
		goto arm_sysregs_emulator_probe_done;
	}

	s->guest = guest;
	INIT_SPIN_LOCK(&s->lock);
	s->ref_100hz = vmm_timer_timestamp();
	s->ref_24mhz = s->ref_100hz;

	if (eid->data) {
		s->sys_id = ((u32 *)eid->data)[0];
		s->proc_id = ((u32 *)eid->data)[1];
	}

	s->pic = vmm_zalloc(sizeof(struct vmm_emupic));
	if (!s->pic) {
		goto arm_sysregs_emulator_probe_freestate_fail;
	}

	strcpy(s->pic->name, edev->node->name);
	s->pic->type = VMM_EMUPIC_GPIO;
	s->pic->handle = &arm_sysregs_irq_handle;
	s->pic->priv = s;
	if ((rc = vmm_devemu_register_pic(guest, s->pic))) {
		goto arm_sysregs_emulator_probe_freepic_fail;
	}

	attr = vmm_devtree_attrval(edev->node, "mux_in_irq");
	if (attr) {
		s->mux_in_irq[0] = ((u32 *)attr)[0];
		s->mux_in_irq[1] = ((u32 *)attr)[1];
	} else {
		rc = VMM_EFAIL;
		goto arm_sysregs_emulator_probe_unregpic_fail;
	}

	attr = vmm_devtree_attrval(edev->node, "mux_out_irq");
	if (attr) {
		s->mux_out_irq = ((u32 *)attr)[0];
	} else {
		rc = VMM_EFAIL;
		goto arm_sysregs_emulator_probe_unregpic_fail;
	}

	INIT_WORK(&s->shutdown, arm_sysregs_shutdown);
	INIT_WORK(&s->reboot, arm_sysregs_reboot);

	edev->priv = s;

	goto arm_sysregs_emulator_probe_done;

arm_sysregs_emulator_probe_unregpic_fail:
	vmm_devemu_unregister_pic(s->guest, s->pic);
arm_sysregs_emulator_probe_freepic_fail:
	vmm_free(s->pic);
arm_sysregs_emulator_probe_freestate_fail:
	vmm_free(s);
arm_sysregs_emulator_probe_done:
	return rc;
}

static int arm_sysregs_emulator_remove(struct vmm_emudev *edev)
{
	int rc;
	struct arm_sysregs *s = edev->priv;

	if (s) {
		if (s->pic) {
			rc = vmm_devemu_unregister_pic(s->guest, s->pic);
			if (rc) {
				return rc;
			}
			vmm_free(s->pic);
		}
		vmm_free(s);
		edev->priv = NULL;
	}

	return VMM_OK;
}

static u32 versatile_sysids[] = {
	/* === VERSATILE PB === */
	/* sys_id */ VERSATILEPB_SYSID_ARM926, 
	/* proc_id */ VERSATILEPB_PROCID_ARM926, 
};

static u32 realview_sysids[] = {
	/* === PBA8 === */
	/* sys_id */ REALVIEW_SYSID_PBA8, 
	/* proc_id */ REALVIEW_PROCID_PBA8, 
};

static u32 vexpress_sysids[] = {
	/* === PBA8 === */
	/* sys_id */ VEXPRESS_SYSID_CA9, 
	/* proc_id */ VEXPRESS_PROCID_CA9, 
};

static struct vmm_emuid arm_sysregs_emuid_table[] = {
	{ .type = "sys", 
	  .compatible = "versatilepb,arm926", 
	  .data = &versatile_sysids[0] 
	},
	{ .type = "sys", 
	  .compatible = "realview,pb-a8", 
	  .data = &realview_sysids[0] 
	},
	{ .type = "sys", 
	  .compatible = "vexpress,a9", 
	  .data = &vexpress_sysids[0] 
	},
	{ /* end of list */ },
};

static struct vmm_emulator arm_sysregs_emulator = {
	.name = "arm_sysregs",
	.match_table = arm_sysregs_emuid_table,
	.probe = arm_sysregs_emulator_probe,
	.read = arm_sysregs_emulator_read,
	.write = arm_sysregs_emulator_write,
	.reset = arm_sysregs_emulator_reset,
	.remove = arm_sysregs_emulator_remove,
};

static int __init arm_sysregs_emulator_init(void)
{
	return vmm_devemu_register_emulator(&arm_sysregs_emulator);
}

static void __exit arm_sysregs_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&arm_sysregs_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
