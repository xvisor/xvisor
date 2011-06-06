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
 * @brief source file for Realview System emulator.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
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

	if ((dst_len != 1) &&
	    (dst_len != 2) &&
	    (dst_len != 4)) {
		return VMM_EFAIL;
	}

	switch (offset) {
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
	case 0x24: /* 100HZ */
		/* ??? Implement these.  */
		regval = 0;
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
		/* FIXME */
		/* regval = muldiv64(qemu_get_clock_ns(vm_clock), 
				24000000, get_ticks_per_sec()); */
		regval = 0;
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

	return rc;
}

static int realview_emulator_write(vmm_emudev_t *edev,
				   physical_addr_t offset, 
				   void *src, u32 src_len)
{
//	struct realview_sysctl * s = edev->priv;

	return VMM_OK;
}

static int realview_emulator_reset(vmm_emudev_t *edev)
{
	struct realview_sysctl * s = edev->priv;

	s->leds = 0;
	s->lockval = 0;
	s->cfgdata1 = 0;
	s->cfgdata2 = 0;
	s->flags = 0;
	s->resetlevel = 0;

	return VMM_OK;
}

static int realview_emulator_probe(vmm_guest_t *guest,
				   vmm_emudev_t *edev,
				   const vmm_emuid_t *eid)
{
	struct realview_sysctl * s;

	s = vmm_malloc(sizeof(struct realview_sysctl));

	vmm_memset(s, 0x0, sizeof(struct realview_sysctl));

	edev->priv = s;

	s->guest = guest;
	if (eid->data) {
		s->sys_id = ((u32 *)eid->data)[0];
		s->proc_id = ((u32 *)eid->data)[1];
	}

	return VMM_OK;
}

static int realview_emulator_remove(vmm_emudev_t *edev)
{
	vmm_free(edev->priv);

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

static int realview_emulator_init(void)
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
