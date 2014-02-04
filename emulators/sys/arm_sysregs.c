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
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <vmm_spinlocks.h>
#include <vmm_devtree.h>
#include <vmm_timer.h>
#include <vmm_workqueue.h>
#include <vmm_manager.h>
#include <vmm_devemu.h>
#include <libs/mathlib.h>

#define MODULE_DESC			"Realview Sysctl Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			arm_sysregs_emulator_init
#define	MODULE_EXIT			arm_sysregs_emulator_exit

#define LOCK_VALUE			0x0000a05f

#define REALVIEW_SYSID_PBA8		0x01780500
#define REALVIEW_PROCID_PBA8		0x0e000000
#define REALVIEW_SYSID_EB11MP		0xc1400400
#define REALVIEW_PROCID_EB11MP		0x06000000
#define VEXPRESS_SYSID_CA9		0x1190f500
#define VEXPRESS_PROCID_CA9		0x0c000191
#define VEXPRESS_SYSID_CA15		0x1190f500
#define VEXPRESS_PROCID_CA15		0x14000237
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
	vmm_rwlock_t lock;
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
	u32 mb_clock[6];
	u32 *db_clock;
	u32 db_num_vsensors;
	u32 *db_voltage;
	u32 db_num_clocks;
	u32 *db_clock_reset;
};

static int board_id(struct arm_sysregs *s)
{
    /* Extract the board ID field from the SYS_ID register value */
    return (s->sys_id >> 16) & 0xfff;
}

static int arm_sysregs_reg_read(struct arm_sysregs *s,
				u32 offset, u32 *dst)
{
	int rc = VMM_OK;
	u64 tdiff;

	vmm_read_lock(&s->lock);

	switch (offset & ~0x3) {
	case 0x00: /* ID */
		*dst = s->sys_id;
		break;
	case 0x04: /* SW */
		/* General purpose hardware switches.
		We don't have a useful way of exposing these to the user.  */
		*dst = 0;
		break;
	case 0x08: /* LED */
		*dst = s->leds;
		break;
	case 0x20: /* LOCK */
		*dst = s->lockval;
		break;
	case 0x0c: /* OSC0 */
	case 0x10: /* OSC1 */
	case 0x14: /* OSC2 */
	case 0x18: /* OSC3 */
	case 0x1c: /* OSC4 */
		*dst = 0;
		break;
	case 0x24: /* 100HZ */
		tdiff = vmm_timer_timestamp() - s->ref_100hz;
		*dst = udiv64(tdiff, 10000000);
		break;
	case 0x28: /* CFGDATA1 */
		*dst = s->cfgdata1;
		break;
	case 0x2c: /* CFGDATA2 */
		*dst = s->cfgdata2;
		break;
	case 0x30: /* FLAGS */
		*dst = s->flags;
		break;
	case 0x38: /* NVFLAGS */
		*dst = s->nvflags;
		break;
	case 0x40: /* RESETCTL */
		if (board_id(s) == BOARD_ID_VEXPRESS) {
			/* reserved: RAZ/WI */
			*dst = 0;
		} else {
			*dst = s->resetlevel;
		}
		break;
	case 0x44: /* PCICTL */
		*dst = 1;
		break;
	case 0x48: /* MCI */
		*dst = s->sys_mci;
		break;
	case 0x4c: /* FLASH */
		*dst = 0;
		break;
	case 0x50: /* CLCD */
		*dst = s->sys_clcd;
		break;
	case 0x54: /* CLCDSER */
		*dst = 0;
		break;
	case 0x58: /* BOOTCS */
		*dst = 0;
		break;
	case 0x5c: /* 24MHz */
		tdiff = vmm_timer_timestamp() - s->ref_100hz;
		/* Note: What we want is the below value 
		 * *dst = udiv64(tdiff * 24, 1000);
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
		*dst = tdiff;
		break;
	case 0x60: /* MISC */
		*dst = 0;
		break;
	case 0x84: /* PROCID0 */
		*dst = s->proc_id;
		break;
	case 0x88: /* PROCID1 */
		*dst = 0xff000000;
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
		*dst = 0;
		break;
	case 0xa0: /* SYS_CFGDATA */
		if (board_id(s) != BOARD_ID_VEXPRESS) {
			rc = VMM_EFAIL;
		} else {
			*dst = s->sys_cfgdata;
		}
		break;
	case 0xa4: /* SYS_CFGCTRL */
		if (board_id(s) != BOARD_ID_VEXPRESS) {
			rc = VMM_EFAIL;
		} else {
			*dst = s->sys_cfgctrl;
		}
		break;
	case 0xa8: /* SYS_CFGSTAT */
		if (board_id(s) != BOARD_ID_VEXPRESS) {
			rc = VMM_EFAIL;
		} else {
			*dst = s->sys_cfgstat;
		}
		break;
	case 0xd8: /* PLDCTL1 */
		*dst = 0;
		break;
	default:
		rc = VMM_EFAIL;
		break;
	}

	vmm_read_unlock(&s->lock);

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

/* SYS_CFGCTRL functions */
#define SYS_CFG_OSC 1
#define SYS_CFG_VOLT 2
#define SYS_CFG_AMP 3
#define SYS_CFG_TEMP 4
#define SYS_CFG_RESET 5
#define SYS_CFG_SCC 6
#define SYS_CFG_MUXFPGA 7
#define SYS_CFG_SHUTDOWN 8
#define SYS_CFG_REBOOT 9
#define SYS_CFG_DVIMODE 11
#define SYS_CFG_POWER 12
#define SYS_CFG_ENERGY 13

/* SYS_CFGCTRL site field values */
#define SYS_CFG_SITE_MB 0
#define SYS_CFG_SITE_DB1 1
#define SYS_CFG_SITE_DB2 2

/**
 * Handle a VExpress SYS_CFGCTRL register read. On success, return true and
 * write the read value to *val. On failure, return false (and val may
 * or may not be written to).
 *
 * vexpress_cfgctrl_read:
 * @s: arm_sysctl_state pointer
 * @dcc, @function, @site, @position, @device: split out values from
 * SYS_CFGCTRL register
 * @val: pointer to where to put the read data on success
 */
static bool vexpress_cfgctrl_read(struct arm_sysregs *s, u32 dcc,
                                  u32 function, u32 site,
                                  u32 position, u32 device,
                                  u32 *val)
{
	/* We don't support anything other than DCC 0, board stack position 0
	 * or sites other than motherboard/daughterboard:
	 */
	if (dcc != 0 || position != 0 ||
	   (site != SYS_CFG_SITE_MB && site != SYS_CFG_SITE_DB1)) {
		goto cfgctrl_unimp;
	}

	switch (function) {
	case SYS_CFG_VOLT:
		if (site == SYS_CFG_SITE_DB1 && device < s->db_num_vsensors) {
			*val = s->db_voltage[device];
			return TRUE;
		}
		if (site == SYS_CFG_SITE_MB && device == 0) {
			/* There is only one motherboard voltage sensor:
			 * VIO : 3.3V : bus voltage between mother and
			 * daughterboard
			 */
			*val = 3300000;
			return TRUE;
		}
		break;
	case SYS_CFG_OSC:
		if (site == SYS_CFG_SITE_MB && device < sizeof(s->mb_clock)) {
			/* motherboard clock */
			*val = s->mb_clock[device];
			return TRUE;
		}
		if (site == SYS_CFG_SITE_DB1 && device < s->db_num_clocks) {
			/* daughterboard clock */
			*val = s->db_clock[device];
			return TRUE;
		}
		break;
	default:
		break;
	};

cfgctrl_unimp:
	vmm_printf("%s: Unimplemented SYS_CFGCTRL read of function "
		   "0x%x DCC 0x%x site 0x%x position 0x%x device 0x%x\n",
		   __func__, function, dcc, site, position, device);
	return FALSE;
}

/**
 * Handle a VExpress SYS_CFGCTRL register write. On success, return true.
 * On failure, return false.
 *
 * @s: arm_sysctl_state pointer
 * @dcc, @function, @site, @position, @device: split out values from
 * SYS_CFGCTRL register
 * @val: data to write
 */
static bool vexpress_cfgctrl_write(struct arm_sysregs *s, u32 dcc,
                                   u32 function, u32 site,
                                   u32 position, u32 device,
                                   u32 val)
{
	/* We don't support anything other than DCC 0, board stack position 0
	 * or sites other than motherboard/daughterboard:
	 */
	if (dcc != 0 || position != 0 ||
	    (site != SYS_CFG_SITE_MB && site != SYS_CFG_SITE_DB1)) {
		goto cfgctrl_unimp;
	}

	switch (function) {
	case SYS_CFG_OSC:
		if (site == SYS_CFG_SITE_MB && device < sizeof(s->mb_clock)) {
			/* motherboard clock */
			s->mb_clock[device] = val;
			return TRUE;
		}
		if (site == SYS_CFG_SITE_DB1 && device < s->db_num_clocks) {
			/* daughterboard clock */
			s->db_clock[device] = val;
			return TRUE;
		}
		break;
	case SYS_CFG_MUXFPGA:
		if (site == SYS_CFG_SITE_MB && device == 0) {
			/* Select whether video output comes from motherboard
			 * or daughterboard: ignore it as Xvisor doesn't
			 * support this.
			 */
			return TRUE;
		}
		break;
	case SYS_CFG_SHUTDOWN:
		if (site == SYS_CFG_SITE_MB && device == 0) {
			vmm_workqueue_schedule_work(NULL, &s->shutdown);
			return TRUE;
		}
		break;
	case SYS_CFG_REBOOT:
		if (site == SYS_CFG_SITE_MB && device == 0) {
			vmm_workqueue_schedule_work(NULL, &s->reboot);
			return TRUE;
		}
		break;
	case SYS_CFG_DVIMODE:
		if (site == SYS_CFG_SITE_MB && device == 0) {
			/* Selecting DVI mode is meaningless for Xvisor:
			 * we will always display the output correctly
			 * according to the pixel height/width programmed
			 * into the CLCD controller.
			 */
			return TRUE;
		}
		break;
	default:
		break;
	};

cfgctrl_unimp:
	vmm_printf("%s: Unimplemented SYS_CFGCTRL write of function "
		   "0x%x DCC 0x%x site 0x%x position 0x%x device 0x%x\n",
		   __func__, function, dcc, site, position, device);
	return FALSE;
}

static int arm_sysregs_reg_write(struct arm_sysregs *s,
				 u32 offset, u32 regmask, u32 regval)
{
	int rc = VMM_OK;

	vmm_write_lock(&s->lock);

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
		if (regval == LOCK_VALUE) {
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
			if (s->lockval == LOCK_VALUE) {
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
			if (s->lockval == LOCK_VALUE) {
				s->resetlevel &= regmask;
				s->resetlevel |= regval;
				if (s->resetlevel & 0x04) {
					vmm_workqueue_schedule_work(NULL, 
								&s->reboot);
				}
			}
			break;
		case BOARD_ID_EB:
			if (s->lockval == LOCK_VALUE) {
				s->resetlevel &= regmask;
				s->resetlevel |= regval;
				if (s->resetlevel & 0x08) {
					vmm_workqueue_schedule_work(NULL, 
								&s->reboot);
				}
			}
			break;
		case BOARD_ID_VEXPRESS:
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
		/* Undefined bits [19:18] are RAZ/WI, and writing to
		 * the start bit just triggers the action; it always
		 * reads as zero.
		 */
		s->sys_cfgctrl &= regmask;
		s->sys_cfgctrl |= regval & ~(3 << 18);
		if (s->sys_cfgctrl & (1 << 31)) {
			/* Start bit set -- actually do something */
			u32 dcc = (s->sys_cfgctrl >> 26) & 0xF;
			u32 function = (s->sys_cfgctrl >> 20) & 0x3F;
			u32 site = (s->sys_cfgctrl >> 16) & 0x3;
			u32 position = (s->sys_cfgctrl >> 12) & 0xF;
			u32 device = (s->sys_cfgctrl >> 0) & 0xFFF;
			s->sys_cfgstat = 1;			/* complete */
			if (s->sys_cfgctrl & (1 << 30)) {
				if (!vexpress_cfgctrl_write(s, dcc,
							    function, site,
							    position, device,
							    s->sys_cfgdata)) {
					s->sys_cfgstat |= 2;	/* error */
				}
			} else {
				u32 val;
				if (!vexpress_cfgctrl_read(s, dcc,
							   function, site,
							   position, device,
							   &val)) {
					s->sys_cfgstat |= 2;	/* error */
				} else {
					s->sys_cfgdata = val;
				}
			}
		}
		s->sys_cfgctrl &= ~(1 << 31);
		break;
	case 0xa8: /* SYS_CFGSTAT */
		if (board_id(s) != BOARD_ID_VEXPRESS) {
			rc =  VMM_EFAIL;
			break;
		}
		s->sys_cfgstat &= regmask;
		s->sys_cfgstat |= regval & 3;
		break;
	case 0xd8: /* PLDCTL1 */
		break;
	default:
		rc = VMM_EFAIL;
		break;
	}

	vmm_write_unlock(&s->lock);

	return rc;
}

static int arm_sysregs_emulator_read8(struct vmm_emudev *edev,
				      physical_addr_t offset, 
				      u8 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = arm_sysregs_reg_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFF;
	}

	return rc;
}

static int arm_sysregs_emulator_read16(struct vmm_emudev *edev,
				       physical_addr_t offset, 
				       u16 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = arm_sysregs_reg_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFFFF;
	}

	return rc;
}

static int arm_sysregs_emulator_read32(struct vmm_emudev *edev,
				       physical_addr_t offset, 
				       u32 *dst)
{
	return arm_sysregs_reg_read(edev->priv, offset, dst);
}

static int arm_sysregs_emulator_write8(struct vmm_emudev *edev,
				       physical_addr_t offset, 
				       u8 src)
{
	return arm_sysregs_reg_write(edev->priv, offset, 0xFFFFFF00, src);
}

static int arm_sysregs_emulator_write16(struct vmm_emudev *edev,
					physical_addr_t offset, 
					u16 src)
{
	return arm_sysregs_reg_write(edev->priv, offset, 0xFFFF0000, src);
}

static int arm_sysregs_emulator_write32(struct vmm_emudev *edev,
					physical_addr_t offset, 
					u32 src)
{
	return arm_sysregs_reg_write(edev->priv, offset, 0x00000000, src);
}

static int arm_sysregs_emulator_reset(struct vmm_emudev *edev)
{
	int i;
	struct arm_sysregs *s = edev->priv;

	vmm_write_lock(&s->lock);

	s->ref_100hz = vmm_timer_timestamp();
	s->ref_24mhz = s->ref_100hz;

	s->leds = 0;
	s->lockval = 0;
	s->cfgdata1 = 0;
	s->cfgdata2 = 0;
	s->flags = 0;
	s->resetlevel = 0;
	/* Motherboard oscillators (in Hz) */
	s->mb_clock[0] = 50000000; /* Static memory clock: 50MHz */
	s->mb_clock[1] = 23750000; /* motherboard CLCD clock: 23.75MHz */
	s->mb_clock[2] = 24000000; /* IO FPGA peripheral clock: 24MHz */
	s->mb_clock[3] = 24000000; /* IO FPGA reserved clock: 24MHz */
	s->mb_clock[4] = 24000000; /* System bus global clock: 24MHz */
	s->mb_clock[5] = 24000000; /* IO FPGA reserved clock: 24MHz */
	/* Daughterboard oscillators: reset from property values */
	for (i = 0; i < s->db_num_clocks; i++) {
		s->db_clock[i] = s->db_clock_reset[i];
	}
	if (board_id(s) == BOARD_ID_VEXPRESS) {
		/* On VExpress this register will RAZ/WI */
		s->sys_clcd = 0;
	} else {
		/* All others: CLCDID 0x1f, indicating VGA */
		s->sys_clcd = 0x1f00;
	}

	vmm_write_unlock(&s->lock);

	return VMM_OK;
}

/* Process IRQ asserted via device emulation framework */
static void arm_sysregs_irq_handle(u32 irq, int cpu, int level, void *opaque)
{
	int bit;
	struct arm_sysregs *s = opaque;

	vmm_write_lock(&s->lock);

	if (s->mux_in_irq[0] == irq) {
		/* For PB926 and EB write-protect is bit 2 of SYS_MCI;
		 * for all later boards it is bit 1.
		 */
		bit = 2;
		if ((board_id(s) == BOARD_ID_PB926) || 
		    (board_id(s) == BOARD_ID_EB)) {
			bit = 4;
		}
		s->sys_mci &= ~bit;
		if (level) {
			s->sys_mci |= bit;
		}
	} else if (s->mux_in_irq[1] == irq) {
		s->sys_mci &= ~1;
		if (level) {
			s->sys_mci |= 1;
		}
	}

	vmm_write_unlock(&s->lock);
}

static int arm_sysregs_emulator_probe(struct vmm_guest *guest,
				   struct vmm_emudev *edev,
				   const struct vmm_devtree_nodeid *eid)
{
	int i, j, rc = VMM_OK;
	const char *attr;
	struct arm_sysregs *s;

	s = vmm_zalloc(sizeof(struct arm_sysregs));
	if (!s) {
		rc = VMM_EFAIL;
		goto arm_sysregs_emulator_probe_done;
	}

	s->guest = guest;
	INIT_RW_LOCK(&s->lock);
	s->ref_100hz = vmm_timer_timestamp();
	s->ref_24mhz = s->ref_100hz;

	if (eid->data) {
		i = 0;
		s->sys_id = ((u32 *)eid->data)[i++];
		s->proc_id = ((u32 *)eid->data)[i++];
		s->db_num_vsensors = ((u32 *)eid->data)[i++];
		if (s->db_num_vsensors) {
			s->db_voltage =
				vmm_zalloc(sizeof(u32)*s->db_num_vsensors);
			if (!s->db_voltage) {
				rc = VMM_ENOMEM;
				goto arm_sysregs_emulator_probe_freestate_fail;
			}
			for (j = 0; j < s->db_num_vsensors; j++) {
				s->db_voltage[j] = ((u32 *)eid->data)[i++];
			}
		}
		s->db_num_clocks = ((u32 *)eid->data)[i++];
		if (s->db_num_clocks) {
			s->db_clock =
				vmm_zalloc(sizeof(u32)*s->db_num_clocks);
			if (!s->db_clock) {
				rc = VMM_ENOMEM;
				goto arm_sysregs_emulator_probe_freevolt_fail;
			}
			s->db_clock_reset =
				vmm_zalloc(sizeof(u32)*s->db_num_clocks);
			if (!s->db_clock_reset) {
				vmm_free(s->db_clock);
				s->db_clock = NULL;
				rc = VMM_ENOMEM;
				goto arm_sysregs_emulator_probe_freevolt_fail;
			}
			for (j = 0; j < s->db_num_clocks; j++) {
				s->db_clock_reset[j] = ((u32 *)eid->data)[i++];
			}
		}
	}

	attr = vmm_devtree_attrval(edev->node, "mux_in_irq");
	if (attr) {
		s->mux_in_irq[0] = ((u32 *)attr)[0];
		s->mux_in_irq[1] = ((u32 *)attr)[1];
	} else {
		rc = VMM_EFAIL;
		goto arm_sysregs_emulator_probe_freeclock_fail;
	}

	attr = vmm_devtree_attrval(edev->node, "mux_out_irq");
	if (attr) {
		s->mux_out_irq = ((u32 *)attr)[0];
	} else {
		rc = VMM_EFAIL;
		goto arm_sysregs_emulator_probe_freeclock_fail;
	}

	INIT_WORK(&s->shutdown, arm_sysregs_shutdown);
	INIT_WORK(&s->reboot, arm_sysregs_reboot);

	vmm_devemu_register_irq_handler(guest, s->mux_in_irq[0],
					edev->node->name, 
					arm_sysregs_irq_handle, s);
	vmm_devemu_register_irq_handler(guest, s->mux_in_irq[1], 
					edev->node->name,
					arm_sysregs_irq_handle, s);

	edev->priv = s;

	goto arm_sysregs_emulator_probe_done;

arm_sysregs_emulator_probe_freeclock_fail:
	if (s->db_clock) {
		vmm_free(s->db_clock);
	}
	if (s->db_clock_reset) {
		vmm_free(s->db_clock_reset);
	}
arm_sysregs_emulator_probe_freevolt_fail:
	if (s->db_voltage) {
		vmm_free(s->db_voltage);
	}
arm_sysregs_emulator_probe_freestate_fail:
	vmm_free(s);
arm_sysregs_emulator_probe_done:
	return rc;
}

static int arm_sysregs_emulator_remove(struct vmm_emudev *edev)
{
	struct arm_sysregs *s = edev->priv;

	if (!s) {
		return VMM_EFAIL;
	}

	vmm_devemu_unregister_irq_handler(s->guest, s->mux_in_irq[0], 
					  arm_sysregs_irq_handle, s);
	vmm_devemu_unregister_irq_handler(s->guest, s->mux_in_irq[1], 
					  arm_sysregs_irq_handle, s);
	if (s->db_clock) {
		vmm_free(s->db_clock);
	}
	if (s->db_clock_reset) {
		vmm_free(s->db_clock_reset);
	}
	if (s->db_voltage) {
		vmm_free(s->db_voltage);
	}
	vmm_free(s);
	edev->priv = NULL;

	return VMM_OK;
}

static u32 versatilepb_sysids[] = {
	/* === VersatilePB === */
	/* sys_id */ VERSATILEPB_SYSID_ARM926,
	/* proc_id */ VERSATILEPB_PROCID_ARM926,
	/* len-db-voltage */ 0,
	/* len-db-clock */ 0,
};

static u32 realview_ebmpcore_sysids[] = {
	/* === Realview-EB-MPCore === */
	/* sys_id */ REALVIEW_SYSID_EB11MP,
	/* proc_id */ REALVIEW_PROCID_EB11MP,
	/* len-db-voltage */ 0,
	/* len-db-clock */ 0,
};

static u32 realview_pba8_sysids[] = {
	/* === Realview-PB-aA8 === */
	/* sys_id */ REALVIEW_SYSID_PBA8,
	/* proc_id */ REALVIEW_PROCID_PBA8,
	/* len-db-voltage */ 0,
	/* len-db-clock */ 0,
};

static u32 vexpress_a9_sysids[] = {
	/* === VExpress-A9 === */
	/* sys_id */ VEXPRESS_SYSID_CA9,
	/* proc_id */ VEXPRESS_PROCID_CA9,
	/* len-db-voltage */ 6,
	/* db-voltage */ 1000000, /* VD10 : 1.0V : SoC internal logic voltage */
	/* db-voltage */ 1000000, /* VD10_S2 : 1.0V : PL310, L2 cache, RAM, non-PL310 logic */
	/* db-voltage */ 1000000, /* VD10_S3 : 1.0V : Cortex-A9, cores, MPEs, SCU, PL310 logic */
	/* db-voltage */ 1800000, /* VCC1V8 : 1.8V : DDR2 SDRAM, test chip DDR2 I/O supply */
	/* db-voltage */ 900000, /* DDR2VTT : 0.9V : DDR2 SDRAM VTT termination voltage */
	/* db-voltage */ 3300000, /* VCC3V3 : 3.3V : local board supply for misc external logic */
	/* len-db-clock */ 3,
	/* db-clock */ 45000000, /* AMBA AXI ACLK: 45MHz */
	/* db-clock */ 23750000, /* daughterboard CLCD clock: 23.75MHz */
	/* db-clock */ 66670000, /* Test chip reference clock: 66.67MHz */
};

static u32 vexpress_a15_sysids[] = {
	/* === VExpress-A15 === */
	/* sys_id */ VEXPRESS_SYSID_CA15,
	/* proc_id */ VEXPRESS_PROCID_CA15,
	/* len-db-voltage */ 1,
	/* db-voltage */ 900000, /* Vcore: 0.9V : CPU core voltage */
	/* len-db-clock */ 9,
	/* db-clock */ 60000000, /* OSCCLK0: 60MHz : CPU_CLK reference */
	/* db-clock */ 0, /* OSCCLK1: reserved */
	/* db-clock */ 0, /* OSCCLK2: reserved */
	/* db-clock */ 0, /* OSCCLK3: reserved */
	/* db-clock */ 40000000, /* OSCCLK4: 40MHz : external AXI master clock */
	/* db-clock */ 23750000, /* OSCCLK5: 23.75MHz : HDLCD PLL reference */
	/* db-clock */ 50000000, /* OSCCLK6: 50MHz : static memory controller clock */
	/* db-clock */ 60000000, /* OSCCLK7: 60MHz : SYSCLK reference */
	/* db-clock */ 40000000, /* OSCCLK8: 40MHz : DDR2 PLL reference */
};

static struct vmm_devtree_nodeid arm_sysregs_emuid_table[] = {
	{ .type = "sys", 
	  .compatible = "versatilepb,arm926", 
	  .data = &versatilepb_sysids[0] 
	},
	{ .type = "sys", 
	  .compatible = "realview,eb-mpcore", 
	  .data = &realview_ebmpcore_sysids[0] 
	},
	{ .type = "sys", 
	  .compatible = "realview,pb-a8", 
	  .data = &realview_pba8_sysids[0] 
	},
	{ .type = "sys", 
	  .compatible = "vexpress,a9", 
	  .data = &vexpress_a9_sysids[0] 
	},
	{ .type = "sys", 
	  .compatible = "vexpress,a15", 
	  .data = &vexpress_a15_sysids[0] 
	},
	{ /* end of list */ },
};

static struct vmm_emulator arm_sysregs_emulator = {
	.name = "arm_sysregs",
	.match_table = arm_sysregs_emuid_table,
	.endian = VMM_DEVEMU_LITTLE_ENDIAN,
	.probe = arm_sysregs_emulator_probe,
	.read8 = arm_sysregs_emulator_read8,
	.write8 = arm_sysregs_emulator_write8,
	.read16 = arm_sysregs_emulator_read16,
	.write16 = arm_sysregs_emulator_write16,
	.read32 = arm_sysregs_emulator_read32,
	.write32 = arm_sysregs_emulator_write32,
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
