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
 * @file realview.c
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief Realview Sysctl emulator.
 * @details This source file implements the Realview Sysctl emulator.
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

#include <vmm_math.h>
#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_modules.h>
#include <vmm_spinlocks.h>
#include <vmm_devtree.h>
#include <vmm_timer.h>
#include <vmm_manager.h>
#include <vmm_devemu.h>

#define MODULE_VARID			realview_emulator_module
#define MODULE_NAME			"Realview Sysctl Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			realview_emulator_init
#define	MODULE_EXIT			realview_emulator_exit

#define REALVIEW_LOCK_VAL		0x0000a05f
#define REALVIEW_SYSID_PBA8		0x01780500
#define REALVIEW_PROCID_PBA8		0x00000000
#define REALVIEW_SYSID_VEXPRESS		0x01900000

struct realview_sysctl {
	vmm_guest_t *guest;
	vmm_spinlock_t lock;
	u64 ref_100hz;
	u64 ref_24mhz;

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
};

static int realview_emulator_read(vmm_emudev_t *edev,
			    	  physical_addr_t offset, 
				  void *dst, u32 dst_len)
{
	int rc = VMM_OK;
	u32 regval = 0x0;
	struct realview_sysctl * s = edev->priv;

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
		regval = vmm_udiv64((vmm_timer_timestamp() - s->ref_100hz), 
								10000000);
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
		if (s->sys_id == REALVIEW_SYSID_VEXPRESS) {
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
		regval = 0x1000;
		break;
	case 0x54: /* CLCDSER */
		regval = 0;
		break;
	case 0x58: /* BOOTCS */
		regval = 0;
		break;
	case 0x5c: /* 24MHz */
		/* Note: What we want is the below value 
		 * regval = vmm_udiv64((vmm_timer_timestamp() - s->ref_24mhz) * 24, 1000);
		 * In integer arithmetic division by constant can be simplified
		 * (a * 24) / 1000
		 * = a * (24 / 1000)
		 * = a * (3 / 125)
		 * ~ a * (3 / 128) [because (3 / 125) ~ (3 / 128)]
		 * ~ (a * 3) >> 7
		 */
		regval = ((vmm_timer_timestamp() - s->ref_24mhz) * 3) >> 7;
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
		if (s->sys_id != REALVIEW_SYSID_VEXPRESS) {
			rc = VMM_EFAIL;
		} else {
			regval = s->sys_cfgdata;
		}
		break;
	case 0xa4: /* SYS_CFGCTRL */
		if (s->sys_id != REALVIEW_SYSID_VEXPRESS) {
			rc = VMM_EFAIL;
		} else {
			regval = s->sys_cfgctrl;
		}
		break;
	case 0xa8: /* SYS_CFGSTAT */
		if (s->sys_id != REALVIEW_SYSID_VEXPRESS) {
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
			((u8 *)dst)[0] = regval & 0xFF;
			break;
		case 2:
			((u8 *)dst)[0] = regval & 0xFF;
			((u8 *)dst)[1] = (regval >> 8) & 0xFF;
			break;
		case 4:
			((u8 *)dst)[0] = regval & 0xFF;
			((u8 *)dst)[1] = (regval >> 8) & 0xFF;
			((u8 *)dst)[2] = (regval >> 16) & 0xFF;
			((u8 *)dst)[3] = (regval >> 24) & 0xFF;
			break;
		default:
			rc = VMM_EFAIL;
			break;
		};
	}

	return rc;
}

static int realview_emulator_write(vmm_emudev_t *edev,
				   physical_addr_t offset, 
				   void *src, u32 src_len)
{
	int rc = VMM_OK, i;
	u32 regmask = 0x0, regval = 0x0;
	struct realview_sysctl * s = edev->priv;

	switch (src_len) {
	case 1:
		regmask = 0xFFFFFF00;
		regval = ((u8 *)src)[0];
		break;
	case 2:
		regmask = 0xFFFF0000;
		regval = ((u8 *)src)[0];
		regval |= (((u8 *)src)[1] << 8);
		break;
	case 4:
		regmask = 0x00000000;
		regval = ((u8 *)src)[0];
		regval |= (((u8 *)src)[1] << 8);
		regval |= (((u8 *)src)[2] << 16);
		regval |= (((u8 *)src)[3] << 24);
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
		if (regval == REALVIEW_LOCK_VAL) {
			s->lockval |= (regval & 0xffff);
			s->lockval &= ~(0x10000);
		} else {
			s->lockval |= (regval & 0xffff);
			s->lockval |= (0x10000);
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
		if (s->sys_id == REALVIEW_SYSID_VEXPRESS) {
			/* reserved: RAZ/WI */
			break;
		}
		if (!(s->lockval & 0x10000)) {
			s->resetlevel &= regmask;
			s->resetlevel |= regval;
		}
		break;
	case 0x44: /* PCICTL */
		/* nothing to do.  */
		break;
	case 0x4c: /* FLASH */
	case 0x50: /* CLCD */
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
		if (s->sys_id != REALVIEW_SYSID_VEXPRESS) {
			rc =  VMM_EFAIL;
			break;
		}
		s->sys_cfgdata &= regmask;
		s->sys_cfgdata |= regval;
		break;
	case 0xa4: /* SYS_CFGCTRL */
		if (s->sys_id != REALVIEW_SYSID_VEXPRESS) {
			rc =  VMM_EFAIL;
			break;
		}
		s->sys_cfgctrl &= regmask;
		s->sys_cfgctrl |= regval & ~(3 << 18);
		s->sys_cfgstat = 1;            /* complete */
		switch (s->sys_cfgctrl) {
		case 0xc0800000: /* SYS_CFG_SHUTDOWN to motherboard */
			/* FIXME: system_shutdown_request(); */
			break;
		case 0xc0900000: /* SYS_CFG_REBOOT to motherboard */
			/* FIXME: system_reset_request(); */
			break;
		default:
			s->sys_cfgstat |= 2;        /* error */
		}
		break;
	case 0xa8: /* SYS_CFGSTAT */
		if (s->sys_id != REALVIEW_SYSID_VEXPRESS) {
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

	/* FIXME: Comparision does not work with linux */
#if 0 /* QEMU checks bit 8 which is wrong */
	if (s->resetlevel & 0x100) {
#else
	if (s->resetlevel & 0x04) {
#endif
		vmm_manager_guest_reset(s->guest);
		vmm_manager_guest_kick(s->guest);
	}

	return rc;
}

static int realview_emulator_reset(vmm_emudev_t *edev)
{
	struct realview_sysctl * s = edev->priv;

	vmm_spin_lock(&s->lock);

	s->ref_100hz = vmm_timer_timestamp();
	s->ref_24mhz = s->ref_100hz;

	s->leds = 0;
	s->lockval = 0x10000;
	s->cfgdata1 = 0;
	s->cfgdata2 = 0;
	s->flags = 0;
	s->resetlevel = 0;

	vmm_spin_unlock(&s->lock);

	return VMM_OK;
}

static int realview_emulator_probe(vmm_guest_t *guest,
				   vmm_emudev_t *edev,
				   const vmm_emuid_t *eid)
{
	int rc = VMM_OK;
	struct realview_sysctl * s;

	s = vmm_malloc(sizeof(struct realview_sysctl));
	if (!s) {
		rc = VMM_EFAIL;
		goto realview_emulator_probe_done;
	}
	vmm_memset(s, 0x0, sizeof(struct realview_sysctl));

	edev->priv = s;

	s->guest = guest;
	INIT_SPIN_LOCK(&s->lock);
	s->ref_100hz = vmm_timer_timestamp();
	s->ref_24mhz = s->ref_100hz;
	if (eid->data) {
		s->sys_id = ((u32 *)eid->data)[0];
		s->proc_id = ((u32 *)eid->data)[1];
	}

realview_emulator_probe_done:
	return rc;
}

static int realview_emulator_remove(vmm_emudev_t *edev)
{
	struct realview_sysctl * s = edev->priv;

	edev->priv = NULL;

	vmm_free(s);

	return VMM_OK;
}

static u32 realview_sysids[] = {
	/* === PBA8 === */
	/* sys_id */ REALVIEW_SYSID_PBA8, 
	/* proc_id */ REALVIEW_PROCID_PBA8, 
};

static vmm_emuid_t realview_emuid_table[] = {
	{ .type = "sys", 
	  .compatible = "realview,pb-a8", 
	  .data = &realview_sysids[0] 
	},
	{ /* end of list */ },
};

static vmm_emulator_t realview_emulator = {
	.name = "realview",
	.match_table = realview_emuid_table,
	.probe = realview_emulator_probe,
	.read = realview_emulator_read,
	.write = realview_emulator_write,
	.reset = realview_emulator_reset,
	.remove = realview_emulator_remove,
};

static int __init realview_emulator_init(void)
{
	return vmm_devemu_register_emulator(&realview_emulator);
}

static void realview_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&realview_emulator);
}

VMM_DECLARE_MODULE(MODULE_VARID, 
			MODULE_NAME, 
			MODULE_AUTHOR, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
