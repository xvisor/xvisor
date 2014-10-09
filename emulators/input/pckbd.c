/**
 * Copyright (c) 2014 Himanshu Chauhan
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
 * @file pckbd.c
 * @author Himanshu Chauhan <hschauhan@nulltrace.org>
 * @brief i8042 Keyboard Emulator.
 *
 * The source has been largely adapted from QEMU hw/input/pckbd.c
 *
 * QEMU PC keyboard emulation
 * Copyright (c) 2003 Fabrice Bellard
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>
#include <emu/input/ps2_emu.h>

#define MODULE_DESC			"i8042 Emulator"
#define MODULE_AUTHOR			"Himanshu Chauhan"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(PS2_EMU_IPRIORITY+1)
#define	MODULE_INIT			i8042_emulator_init
#define	MODULE_EXIT			i8042_emulator_exit

/* debug PC keyboard */
//#define DEBUG_KBD
#ifdef DEBUG_KBD
#define DPRINTF(fmt, ...)                                       \
	do { printf("KBD: " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...)
#endif

/*	Keyboard Controller Commands */
#define KBD_CCMD_READ_MODE	0x20	/* Read mode bits */
#define KBD_CCMD_WRITE_MODE	0x60	/* Write mode bits */
#define KBD_CCMD_GET_VERSION	0xA1	/* Get controller version */
#define KBD_CCMD_MOUSE_DISABLE	0xA7	/* Disable mouse interface */
#define KBD_CCMD_MOUSE_ENABLE	0xA8	/* Enable mouse interface */
#define KBD_CCMD_TEST_MOUSE	0xA9	/* Mouse interface test */
#define KBD_CCMD_SELF_TEST	0xAA	/* Controller self test */
#define KBD_CCMD_KBD_TEST	0xAB	/* Keyboard interface test */
#define KBD_CCMD_KBD_DISABLE	0xAD	/* Keyboard interface disable */
#define KBD_CCMD_KBD_ENABLE	0xAE	/* Keyboard interface enable */
#define KBD_CCMD_READ_INPORT    0xC0    /* read input port */
#define KBD_CCMD_READ_OUTPORT	0xD0    /* read output port */
#define KBD_CCMD_WRITE_OUTPORT	0xD1    /* write output port */
#define KBD_CCMD_WRITE_OBUF	0xD2
#define KBD_CCMD_WRITE_AUX_OBUF	0xD3    /* Write to output buffer as if
					   initiated by the auxiliary device */
#define KBD_CCMD_WRITE_MOUSE	0xD4	/* Write the following byte to the mouse */
#define KBD_CCMD_DISABLE_A20    0xDD    /* HP vectra only ? */
#define KBD_CCMD_ENABLE_A20     0xDF    /* HP vectra only ? */
#define KBD_CCMD_PULSE_BITS_3_0 0xF0    /* Pulse bits 3-0 of the output port P2. */
#define KBD_CCMD_RESET          0xFE    /* Pulse bit 0 of the output port P2 = CPU reset. */
#define KBD_CCMD_NO_OP          0xFF    /* Pulse no bits of the output port P2. */

/* Keyboard Commands */
#define KBD_CMD_SET_LEDS	0xED	/* Set keyboard leds */
#define KBD_CMD_ECHO		0xEE
#define KBD_CMD_GET_ID		0xF2	/* get keyboard ID */
#define KBD_CMD_SET_RATE	0xF3	/* Set typematic rate */
#define KBD_CMD_ENABLE		0xF4	/* Enable scanning */
#define KBD_CMD_RESET_DISABLE	0xF5	/* reset and disable scanning */
#define KBD_CMD_RESET_ENABLE	0xF6    /* reset and enable scanning */
#define KBD_CMD_RESET		0xFF	/* Reset */

/* Keyboard Replies */
#define KBD_REPLY_POR		0xAA	/* Power on reset */
#define KBD_REPLY_ACK		0xFA	/* Command ACK */
#define KBD_REPLY_RESEND	0xFE	/* Command NACK, send the cmd again */

/* Status Register Bits */
#define KBD_STAT_OBF		0x01	/* Keyboard output buffer full */
#define KBD_STAT_IBF		0x02	/* Keyboard input buffer full */
#define KBD_STAT_SELFTEST	0x04	/* Self test successful */
#define KBD_STAT_CMD		0x08	/* Last write was a command write (0=data) */
#define KBD_STAT_UNLOCKED	0x10	/* Zero if keyboard locked */
#define KBD_STAT_MOUSE_OBF	0x20	/* Mouse output buffer full */
#define KBD_STAT_GTO		0x40	/* General receive/xmit timeout */
#define KBD_STAT_PERR		0x80	/* Parity error */

/* Controller Mode Register Bits */
#define KBD_MODE_KBD_INT	0x01	/* Keyboard data generate IRQ1 */
#define KBD_MODE_MOUSE_INT	0x02	/* Mouse data generate IRQ12 */
#define KBD_MODE_SYS		0x04	/* The system flag (?) */
#define KBD_MODE_NO_KEYLOCK	0x08	/* The keylock doesn't affect the keyboard if set */
#define KBD_MODE_DISABLE_KBD	0x10	/* Disable keyboard interface */
#define KBD_MODE_DISABLE_MOUSE	0x20	/* Disable mouse interface */
#define KBD_MODE_KCC		0x40	/* Scan code conversion to PC format */
#define KBD_MODE_RFU		0x80

/* Output Port Bits */
#define KBD_OUT_RESET           0x01    /* 1=normal mode, 0=reset */
#define KBD_OUT_A20             0x02    /* x86 only */
#define KBD_OUT_OBF             0x10    /* Keyboard output buffer full */
#define KBD_OUT_MOUSE_OBF       0x20    /* Mouse output buffer full */

/* Mouse Commands */
#define AUX_SET_SCALE11		0xE6	/* Set 1:1 scaling */
#define AUX_SET_SCALE21		0xE7	/* Set 2:1 scaling */
#define AUX_SET_RES		0xE8	/* Set resolution */
#define AUX_GET_SCALE		0xE9	/* Get scaling factor */
#define AUX_SET_STREAM		0xEA	/* Set stream mode */
#define AUX_POLL		0xEB	/* Poll */
#define AUX_RESET_WRAP		0xEC	/* Reset wrap mode */
#define AUX_SET_WRAP		0xEE	/* Set wrap mode */
#define AUX_SET_REMOTE		0xF0	/* Set remote mode */
#define AUX_GET_TYPE		0xF2	/* Get type */
#define AUX_SET_SAMPLE		0xF3	/* Set sample rate */
#define AUX_ENABLE_DEV		0xF4	/* Enable aux device */
#define AUX_DISABLE_DEV		0xF5	/* Disable aux device */
#define AUX_SET_DEFAULT		0xF6
#define AUX_RESET		0xFF	/* Reset aux device */
#define AUX_ACK			0xFA	/* Command byte ACK. */

#define MOUSE_STATUS_REMOTE     0x40
#define MOUSE_STATUS_ENABLED    0x20
#define MOUSE_STATUS_SCALE21    0x10

#define KBD_PENDING_KBD         1
#define KBD_PENDING_AUX         2

typedef struct i8042_emu_state {
	struct vmm_guest *guest;
	struct ps2_emu_keyboard *kbd;
	struct ps2_emu_mouse *mouse;

	u8 is_mouse;
	u8 write_cmd; /* if non zero, write data to port 60 is expected */
	u8 status;
	u8 mode;
	u8 outport;
	/* Bitmask of devices with data available.  */
	u8 pending;

	u32 irq_kbd;
	u32 irq_mouse;
	u32 *a20_out;
	physical_addr_t mask;
	vmm_spinlock_t lock;
} i8042_emu_state_t;

/* update irq and KBD_STAT_[MOUSE_]OBF */
/* XXX: not generating the irqs if KBD_MODE_DISABLE_KBD is set may be
   incorrect, but it avoids having to simulate exact delays */
static void i8042_update_irq(i8042_emu_state_t *s)
{
	int irq_kbd_level, irq_mouse_level;

	vmm_spin_lock(&s->lock);
	irq_kbd_level = 0;
	irq_mouse_level = 0;
	s->status &= ~(KBD_STAT_OBF | KBD_STAT_MOUSE_OBF);
	s->outport &= ~(KBD_OUT_OBF | KBD_OUT_MOUSE_OBF);
	if (s->pending) {
		s->status |= KBD_STAT_OBF;
		s->outport |= KBD_OUT_OBF;
		/* kbd data takes priority over aux data.  */
		if (s->pending == KBD_PENDING_AUX) {
			s->status |= KBD_STAT_MOUSE_OBF;
			s->outport |= KBD_OUT_MOUSE_OBF;
			if (s->mode & KBD_MODE_MOUSE_INT)
				irq_mouse_level = 1;
		} else {
			if ((s->mode & KBD_MODE_KBD_INT) &&
			    !(s->mode & KBD_MODE_DISABLE_KBD))
				irq_kbd_level = 1;
		}
	}

	vmm_spin_unlock(&s->lock);
	vmm_devemu_emulate_irq(s->guest, s->irq_kbd, irq_kbd_level);
	vmm_devemu_emulate_irq(s->guest, s->irq_mouse, irq_mouse_level);
}

static void kbd_update_kbd_irq(void *opaque, int level)
{
	i8042_emu_state_t *s = (i8042_emu_state_t *)opaque;

	if (level)
		s->pending |= KBD_PENDING_KBD;
	else
		s->pending &= ~KBD_PENDING_KBD;

	i8042_update_irq(s);
}

static void kbd_update_aux_irq(void *opaque, int level)
{
	i8042_emu_state_t *s = (i8042_emu_state_t *)opaque;

	if (level)
		s->pending |= KBD_PENDING_AUX;
	else
		s->pending &= ~KBD_PENDING_AUX;

	i8042_update_irq(s);
}

static u64 kbd_read_status(void *opaque, physical_addr_t addr,
			   unsigned size)
{
	i8042_emu_state_t *s = opaque;
	int val;
	val = s->status;
	DPRINTF("kbd: read status=0x%02x\n", val);
	return val;
}

static void kbd_queue(i8042_emu_state_t *s, int b, int aux)
{
	if (aux)
		ps2_emu_queue(&s->mouse->state, b);
	else
		ps2_emu_queue(&s->kbd->state, b);
}

static void outport_write(i8042_emu_state_t *s, u32 val)
{
	DPRINTF("kbd: write outport=0x%02x\n", val);
	s->outport = val;
	if (s->a20_out) {
		vmm_printf("i8042 Emulator: A20 IRQ not supported!\n");
		//qemu_set_irq(*s->a20_out, (val >> 1) & 1);
	}
	if (!(val & 1)) {
		vmm_printf("i8042 Emulator: Reset the guest.\n");
		//qemu_system_reset_request();
	}
}

static void kbd_write_command(void *opaque, physical_addr_t addr,
			      u64 val, unsigned size)
{
	i8042_emu_state_t *s = opaque;

	DPRINTF("kbd: write cmd=0x%02x\n", val);

	/* Bits 3-0 of the output port P2 of the keyboard controller may be pulsed
	 * low for approximately 6 micro seconds. Bits 3-0 of the KBD_CCMD_PULSE
	 * command specify the output port bits to be pulsed.
	 * 0: Bit should be pulsed. 1: Bit should not be modified.
	 * The only useful version of this command is pulsing bit 0,
	 * which does a CPU reset.
	 */
	if((val & KBD_CCMD_PULSE_BITS_3_0) == KBD_CCMD_PULSE_BITS_3_0) {
		if(!(val & 1))
			val = KBD_CCMD_RESET;
		else
			val = KBD_CCMD_NO_OP;
	}

	switch(val) {
	case KBD_CCMD_READ_MODE:
		kbd_queue(s, s->mode, 0);
		break;
	case KBD_CCMD_WRITE_MODE:
	case KBD_CCMD_WRITE_OBUF:
	case KBD_CCMD_WRITE_AUX_OBUF:
	case KBD_CCMD_WRITE_MOUSE:
	case KBD_CCMD_WRITE_OUTPORT:
		s->write_cmd = val;
		break;
	case KBD_CCMD_MOUSE_DISABLE:
		s->mode |= KBD_MODE_DISABLE_MOUSE;
		break;
	case KBD_CCMD_MOUSE_ENABLE:
		s->mode &= ~KBD_MODE_DISABLE_MOUSE;
		break;
	case KBD_CCMD_TEST_MOUSE:
		kbd_queue(s, 0x00, 0);
		break;
	case KBD_CCMD_SELF_TEST:
		s->status |= KBD_STAT_SELFTEST;
		kbd_queue(s, 0x55, 0);
		break;
	case KBD_CCMD_KBD_TEST:
		kbd_queue(s, 0x00, 0);
		break;
	case KBD_CCMD_KBD_DISABLE:
		s->mode |= KBD_MODE_DISABLE_KBD;
		i8042_update_irq(s);
		break;
	case KBD_CCMD_KBD_ENABLE:
		s->mode &= ~KBD_MODE_DISABLE_KBD;
		i8042_update_irq(s);
		break;
	case KBD_CCMD_READ_INPORT:
		kbd_queue(s, 0x80, 0);
		break;
	case KBD_CCMD_READ_OUTPORT:
		kbd_queue(s, s->outport, 0);
		break;
	case KBD_CCMD_ENABLE_A20:
		if (s->a20_out) {
			vmm_printf("A20 Enable Interrupt not supported.\n");
			//qemu_irq_raise(*s->a20_out);
		}
		s->outport |= KBD_OUT_A20;
		break;
	case KBD_CCMD_DISABLE_A20:
		if (s->a20_out) {
			vmm_printf("A20 Disable Interrupt not supported.\n");
			//qemu_irq_lower(*s->a20_out);
		}
		s->outport &= ~KBD_OUT_A20;
		break;
	case KBD_CCMD_RESET:
		vmm_printf("i8042 Emulator: Guest wants to reset itself!\n");
		//qemu_system_reset_request();
		break;
	case KBD_CCMD_NO_OP:
		/* ignore that */
		break;
	default:
		vmm_printf("i8042 Emulator: Unsupported keyboard cmd=0x%02x\n", (int)val);
		break;
	}
}

static u64 kbd_read_data(void *opaque, physical_addr_t addr,
			 unsigned size)
{
	i8042_emu_state_t *s = opaque;
	u32 val;

	if (s->pending == KBD_PENDING_AUX)
		val = ps2_emu_read_data(&s->mouse->state);
	else
		val = ps2_emu_read_data(&s->kbd->state);

	DPRINTF("kbd: read data=0x%02x\n", val);
	return val;
}

static void kbd_write_data(void *opaque, physical_addr_t addr,
			   u64 val, unsigned size)
{
	i8042_emu_state_t *s = opaque;

	DPRINTF("kbd: write data=0x%02x\n", val);

	switch(s->write_cmd) {
	case 0:
		ps2_emu_write_keyboard(s->kbd, val);
		break;
	case KBD_CCMD_WRITE_MODE:
		s->mode = val;
		ps2_emu_keyboard_set_translation(s->kbd, (s->mode & KBD_MODE_KCC) != 0);
		/* ??? */
		i8042_update_irq(s);
		break;
	case KBD_CCMD_WRITE_OBUF:
		kbd_queue(s, val, 0);
		break;
	case KBD_CCMD_WRITE_AUX_OBUF:
		kbd_queue(s, val, 1);
		break;
	case KBD_CCMD_WRITE_OUTPORT:
		outport_write(s, val);
		break;
	case KBD_CCMD_WRITE_MOUSE:
		ps2_emu_write_mouse(s->mouse, val);
		break;
	default:
		break;
	}
	s->write_cmd = 0;
}

/* Memory mapped interface */
static u32 kbd_mm_readb (i8042_emu_state_t *s, physical_addr_t addr)
{
	if (addr & s->mask)
		return kbd_read_status(s, 0, 1) & 0xff;
	else
		return kbd_read_data(s, 0, 1) & 0xff;
}

static void kbd_mm_writeb (i8042_emu_state_t *s, physical_addr_t addr, u32 value)
{
	if (addr & s->mask)
		kbd_write_command(s, 0, value & 0xff, 1);
	else
		kbd_write_data(s, 0, value & 0xff, 1);
}

static int i8042_reg_read(i8042_emu_state_t *s, u32 offset, u32 *dst)
{
	*dst = kbd_mm_readb(s, offset);
	return VMM_OK;
}

static int i8042_reg_write(i8042_emu_state_t *s, u32 offset,
			   u32 src_mask, u32 val)
{
	kbd_mm_writeb(s, offset, val);
	return VMM_OK;
}

static int i8042_emulator_read8(struct vmm_emudev *edev,
				physical_addr_t offset,
				u8 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = i8042_reg_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFF;
	}

	return rc;
}

static int i8042_emulator_read16(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u16 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = i8042_reg_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFFFF;
	}

	return rc;
}

static int i8042_emulator_read32(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u32 *dst)
{
	return i8042_reg_read(edev->priv, offset, dst);
}

static int i8042_emulator_write8(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u8 src)
{
	return i8042_reg_write(edev->priv, offset, 0xFFFFFF00, src);
}

static int i8042_emulator_write16(struct vmm_emudev *edev,
				  physical_addr_t offset,
				  u16 src)
{
	return i8042_reg_write(edev->priv, offset, 0xFFFF0000, src);
}

static int i8042_emulator_write32(struct vmm_emudev *edev,
				  physical_addr_t offset,
				  u32 src)
{
	return i8042_reg_write(edev->priv, offset, 0x00000000, src);
}

static int i8042_emulator_reset(struct vmm_emudev *edev)
{
	int rc = VMM_OK;
	i8042_emu_state_t *s = edev->priv;

	vmm_spin_lock(&s->lock);

	s->mode = KBD_MODE_KBD_INT | KBD_MODE_MOUSE_INT;
	s->status = KBD_STAT_CMD | KBD_STAT_UNLOCKED;
	s->outport = KBD_OUT_RESET | KBD_OUT_A20;
	s->pending = 0;

	vmm_spin_unlock(&s->lock);

	if ((rc = ps2_emu_reset_mouse(s->mouse)) != VMM_OK) {
		vmm_printf("Failed to reset mouse!\n");
		return rc;
	}

	if ((rc = ps2_emu_reset_keyboard(s->kbd)) != VMM_OK)
		vmm_printf("Failed to reset keyboard!\n");

	return rc;
}

static int i8042_emulator_probe(struct vmm_guest *guest,
				struct vmm_emudev *edev,
				const struct vmm_devtree_nodeid *eid)
{
	int rc = VMM_OK;
	char name[64];
	i8042_emu_state_t *s;

	s = vmm_zalloc(sizeof(i8042_emu_state_t));
	if (!s) {
		rc = VMM_ENOMEM;
		goto i8042_emulator_probe_fail;
	}

	s->guest = guest;
	rc = vmm_devtree_irq_get(edev->node, &s->irq_kbd, 0);
	if (rc) {
		goto i8042_emulator_probe_freestate_fail;
	}

	rc = vmm_devtree_irq_get(edev->node, &s->irq_mouse, 1);
	if (rc) {
		goto i8042_emulator_probe_freestate_fail;
	}

	INIT_SPIN_LOCK(&s->lock);

	strlcpy(name, guest->name, sizeof(name));
	strlcat(name, "/", sizeof(name));
	if (strlcat(name, edev->node->name, sizeof(name)) >= sizeof(name)) {
		rc = VMM_EOVERFLOW;
		goto i8042_emulator_probe_freestate_fail;
	}

	s->mouse = ps2_emu_alloc_mouse(name, kbd_update_aux_irq, s);
	if (!s->mouse) {
		rc = VMM_ENOMEM;
		goto i8042_emulator_probe_freestate_fail;
	}

	s->kbd = ps2_emu_alloc_keyboard(name, kbd_update_kbd_irq, s);
	if (!s->kbd) {
		rc = VMM_ENOMEM;
		goto i8042_emulator_probe_freestate_fail;
	}

	edev->priv = s;

	return VMM_OK;

i8042_emulator_probe_freestate_fail:
	vmm_free(s);
i8042_emulator_probe_fail:
	return rc;
}

static int i8042_emulator_remove(struct vmm_emudev *edev)
{
	int rc = VMM_OK;
	i8042_emu_state_t *s = edev->priv;

	if (s) {
		if (s->is_mouse) {
			rc = ps2_emu_free_mouse(s->mouse);
			s->mouse = NULL;
		} else {
			rc = ps2_emu_free_keyboard(s->kbd);
			s->kbd = NULL;
		}
		vmm_free(s);
	}

	return rc;
}

static struct vmm_devtree_nodeid i8042_emuid_table[] = {
	{ .type = "input",
	  .compatible = "i8042,keyboard,mouse",
	  .data = (const void *)NULL,
	},
	{ /* end of list */ },
};

static struct vmm_emulator i8042_emulator = {
	.name = "i8042",
	.match_table = i8042_emuid_table,
	.endian = VMM_DEVEMU_LITTLE_ENDIAN,
	.probe = i8042_emulator_probe,
	.read8 = i8042_emulator_read8,
	.write8 = i8042_emulator_write8,
	.read16 = i8042_emulator_read16,
	.write16 = i8042_emulator_write16,
	.read32 = i8042_emulator_read32,
	.write32 = i8042_emulator_write32,
	.reset = i8042_emulator_reset,
	.remove = i8042_emulator_remove,
};

static int __init i8042_emulator_init(void)
{
	return vmm_devemu_register_emulator(&i8042_emulator);
}

static void __exit i8042_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&i8042_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
