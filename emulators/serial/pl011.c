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
 * @file pl011.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief PrimeCell PL011 serial emulator.
 * @details This source file implements the PrimeCell PL011 serial emulator.
 *
 * The source has been largely adapted from QEMU 0.14.xx hw/pl011.c
 * 
 * Arm PrimeCell PL011 UART
 *
 * Copyright (c) 2006 CodeSourcery.
 * Written by Paul Brook
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_devemu.h>
#include <vio/vmm_vserial.h>
#include <libs/fifo.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"PL011 Serial Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_VSERIAL_IPRIORITY+1)
#define	MODULE_INIT			pl011_emulator_init
#define	MODULE_EXIT			pl011_emulator_exit

#define PL011_INT_TX			0x20
#define PL011_INT_RX			0x10

#define PL011_FLAG_TXFE			0x80
#define PL011_FLAG_RXFF			0x40
#define PL011_FLAG_TXFF			0x20
#define PL011_FLAG_RXFE			0x10

struct pl011_state {
	struct vmm_guest *guest;
	struct vmm_vserial *vser;
	vmm_spinlock_t lock;
	u8 id[8];
	u32 irq;
	u32 fifo_sz;	
	u32 flags;
	u32 lcr;
	u32 cr;
	u32 dmacr;
	u32 int_enabled;
	u32 int_level;
	u32 ilpr;
	u32 ibrd;
	u32 fbrd;
	u32 ifl;
	int rd_trig;
	struct fifo *rd_fifo;
};

static void pl011_set_irq(struct pl011_state *s, u32 level, u32 enabled)
{
	if (level & enabled) {
		vmm_devemu_emulate_irq(s->guest, s->irq, 1);
	} else {
		vmm_devemu_emulate_irq(s->guest, s->irq, 0);
	}
}

static void pl011_set_read_trigger(struct pl011_state *s)
{
#if 0
    /* The docs say the RX interrupt is triggered when the FIFO exceeds
       the threshold.  However linux only reads the FIFO in response to an
       interrupt.  Triggering the interrupt when the FIFO is non-empty seems
       to make things work.  */
    if (s->lcr & 0x10)
        s->read_trigger = (s->ifl >> 1) & 0x1c;
    else
#endif
        s->rd_trig = 1;
}

static int pl011_reg_read(struct pl011_state *s, u32 offset, u32 *dst)
{
	int rc = VMM_OK;
	u8 val = 0x0;
	bool set_irq = FALSE;
	u32 read_count = 0x0, level, enabled;

	vmm_spin_lock(&s->lock);

	switch (offset >> 2) {
	case 0: /* UARTDR */
		s->flags &= ~PL011_FLAG_RXFF;
		fifo_dequeue(s->rd_fifo, &val);
		*dst = val;
		read_count = fifo_avail(s->rd_fifo);
		if (read_count == 0) {
			s->flags |= PL011_FLAG_RXFE;
		}
		if (read_count == (s->rd_trig - 1)) {
			s->int_level &= ~PL011_INT_RX;
		}
		set_irq = TRUE;
		level = s->int_level;
		enabled = s->int_enabled;
		break;
	case 1: /* UARTCR */
		*dst = 0;
		break;
	case 6: /* UARTFR */
		*dst = s->flags;
		break;
	case 8: /* UARTILPR */
		*dst = s->ilpr;
		break;
	case 9: /* UARTIBRD */
        	*dst = s->ibrd;
		break;
	case 10: /* UARTFBRD */
		*dst = s->fbrd;
		break;
	case 11: /* UARTLCR_H */
		*dst = s->lcr;
		break;
	case 12: /* UARTCR */
		*dst = s->cr;
		break;
	case 13: /* UARTIFLS */
		*dst = s->ifl;
		break;
	case 14: /* UARTIMSC */
		*dst = s->int_enabled;
		break;
	case 15: /* UARTRIS */
		*dst = s->int_level;
		break;
	case 16: /* UARTMIS */
 		*dst = s->int_level & s->int_enabled;
		break;
	case 18: /* UARTDMACR */
		*dst = s->dmacr;
		break;
	default:
		if (offset >= 0xfe0 && offset < 0x1000) {
			*dst = s->id[(offset - 0xfe0) >> 2];
		} else {
			rc = VMM_EFAIL;
		}
		break;
	};

	vmm_spin_unlock(&s->lock);

	if (set_irq) {
		pl011_set_irq(s, level, enabled);
	}

	return rc;
}

static int pl011_reg_write(struct pl011_state *s, u32 offset, 
			   u32 src_mask, u32 src)
{
	u8 val;
	int rc = VMM_OK;
	bool set_irq = FALSE;
	bool recv_char = FALSE;
	u32 level, enabled;

	vmm_spin_lock(&s->lock);

	switch (offset >> 2) {
	case 0: /* UARTDR */
		/* ??? Check if transmitter is enabled.  */
		val = src;
		recv_char = TRUE;
		s->int_level |= PL011_INT_TX;
		set_irq = TRUE;
		level = s->int_level;
		enabled = s->int_enabled;
		break;
	case 1: /* UARTCR */
		s->cr = (s->cr & src_mask) | (src & ~src_mask);
		break;
	case 6: /* UARTFR */
		/* Writes to Flag register are ignored.  */
		break;
	case 8: /* UARTUARTILPR */
		s->ilpr = (s->ilpr & src_mask) | (src & ~src_mask);
		break;
	case 9: /* UARTIBRD */
		s->ibrd = (s->ibrd & src_mask) | (src & ~src_mask);
		break;
	case 10: /* UARTFBRD */
		s->fbrd = (s->fbrd & src_mask) | (src & ~src_mask);
		break;
	case 11: /* UARTLCR_H */
		s->lcr = src;
		pl011_set_read_trigger(s);
		break;
	case 12: /* UARTCR */
		/* ??? Need to implement the enable and loopback bits.  */
		s->cr = (s->cr & src_mask) | (src & ~src_mask);
		break;
	case 13: /* UARTIFS */
		s->ifl = (s->ifl & src_mask) | (src & ~src_mask);
		pl011_set_read_trigger(s);
		break;
	case 14: /* UARTIMSC */
		s->int_enabled = (s->int_enabled & src_mask) | 
				 (src & ~src_mask);
		set_irq = TRUE;
		level = s->int_level;
		enabled = s->int_enabled;
		break;
	case 17: /* UARTICR */
		s->int_level &= ~(src & ~src_mask);
		set_irq = TRUE;
		level = s->int_level;
		enabled = s->int_enabled;
		break;
	case 18: /* UARTDMACR */
		/* ??? DMA not implemented */
		s->dmacr = (s->dmacr & src_mask) | (src & ~src_mask);
		s->dmacr &= ~0x3;
		break;
	default:
		rc = VMM_EFAIL;
		break;
	};

	vmm_spin_unlock(&s->lock);

	if (recv_char) {
		vmm_vserial_receive(s->vser, &val, 1);
	}

	if (set_irq) {
		pl011_set_irq(s, level, enabled);
	}

	return rc;
}

static bool pl011_vserial_can_send(struct vmm_vserial *vser)
{
	struct pl011_state *s = vmm_vserial_priv(vser);
#if 0
	u32 rd_count;

	rd_count = fifo_avail(s->rd_fifo);
	if (s->lcr & 0x10) {
		return (rd_count < s->fifo_sz);
	} else {
		return (rd_count < 1);
	}
#endif
	return !fifo_isfull(s->rd_fifo);
}

static int pl011_vserial_send(struct vmm_vserial *vser, u8 data)
{
	bool set_irq = FALSE;
	u32 rd_count, level, enabled;
	struct pl011_state *s = vmm_vserial_priv(vser);

	fifo_enqueue(s->rd_fifo, &data, TRUE);
	rd_count = fifo_avail(s->rd_fifo);

	vmm_spin_lock(&s->lock);
	s->flags &= ~PL011_FLAG_RXFE;
	if (s->cr & 0x10 || rd_count == s->fifo_sz) {
		s->flags |= PL011_FLAG_RXFF;
	}
	if (rd_count >= s->rd_trig) {
		s->int_level |= PL011_INT_RX;
		set_irq = TRUE;
		level = s->int_level;
		enabled = s->int_enabled;
	}
	vmm_spin_unlock(&s->lock);

	if (set_irq) {
		pl011_set_irq(s, level, enabled);
	}

	return VMM_OK;
}

static int pl011_emulator_read8(struct vmm_emudev *edev,
				physical_addr_t offset, 
				u8 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = pl011_reg_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFF;
	}

	return rc;
}

static int pl011_emulator_read16(struct vmm_emudev *edev,
				 physical_addr_t offset, 
				 u16 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = pl011_reg_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFFFF;
	}

	return rc;
}

static int pl011_emulator_read32(struct vmm_emudev *edev,
				 physical_addr_t offset, 
				 u32 *dst)
{
	return pl011_reg_read(edev->priv, offset, dst);
}

static int pl011_emulator_write8(struct vmm_emudev *edev,
				 physical_addr_t offset, 
				 u8 src)
{
	return pl011_reg_write(edev->priv, offset, 0xFFFFFF00, src);
}

static int pl011_emulator_write16(struct vmm_emudev *edev,
				  physical_addr_t offset, 
				  u16 src)
{
	return pl011_reg_write(edev->priv, offset, 0xFFFF0000, src);
}

static int pl011_emulator_write32(struct vmm_emudev *edev,
				  physical_addr_t offset, 
				  u32 src)
{
	return pl011_reg_write(edev->priv, offset, 0x00000000, src);
}

static int pl011_emulator_reset(struct vmm_emudev *edev)
{
	struct pl011_state *s = edev->priv;

	vmm_spin_lock(&s->lock);

	s->rd_trig = 1;
	s->ifl = 0x12;
	s->cr = 0x300;
	s->flags = 0x90;

	vmm_spin_unlock(&s->lock);

	return VMM_OK;
}

static int pl011_emulator_probe(struct vmm_guest *guest,
				struct vmm_emudev *edev,
				const struct vmm_devtree_nodeid *eid)
{
	int rc = VMM_OK;
	char name[64];
	struct pl011_state *s;

	s = vmm_zalloc(sizeof(struct pl011_state));
	if (!s) {
		rc = VMM_EFAIL;
		goto pl011_emulator_probe_done;
	}

	s->guest = guest;
	INIT_SPIN_LOCK(&s->lock);

	if (eid->data) {
		s->id[0] = ((u32 *)eid->data)[0];
		s->id[1] = ((u32 *)eid->data)[1];
		s->id[2] = ((u32 *)eid->data)[2];
		s->id[3] = ((u32 *)eid->data)[3];
		s->id[4] = ((u32 *)eid->data)[4];
		s->id[5] = ((u32 *)eid->data)[5];
		s->id[6] = ((u32 *)eid->data)[6];
		s->id[7] = ((u32 *)eid->data)[7];
	}

	rc = vmm_devtree_irq_get(edev->node, &s->irq, 0);
	if (rc) {
		goto pl011_emulator_probe_freestate_fail;
	}

	rc = vmm_devtree_read_u32(edev->node, "fifo_size", &s->fifo_sz);
	if (rc) {
		goto pl011_emulator_probe_freestate_fail;
	}

	s->rd_fifo = fifo_alloc(1, s->fifo_sz);
	if (!s->rd_fifo) {
		rc = VMM_EFAIL;
		goto pl011_emulator_probe_freestate_fail;
	}

	strlcpy(name, guest->name, sizeof(name));
	strlcat(name, "/", sizeof(name));
	if (strlcat(name, edev->node->name, sizeof(name)) >= sizeof(name)) {
		rc = VMM_EOVERFLOW;
		goto pl011_emulator_probe_freerbuf_fail;
	}
	s->vser = vmm_vserial_create(name, 
				     &pl011_vserial_can_send, 
				     &pl011_vserial_send, 
				     s->fifo_sz, s);
	if (!(s->vser)) {
		goto pl011_emulator_probe_freerbuf_fail;
	}

	edev->priv = s;

	goto pl011_emulator_probe_done;

pl011_emulator_probe_freerbuf_fail:
	fifo_free(s->rd_fifo);
pl011_emulator_probe_freestate_fail:
	vmm_free(s);
pl011_emulator_probe_done:
	return rc;
}

static int pl011_emulator_remove(struct vmm_emudev *edev)
{
	struct pl011_state *s = edev->priv;

	if (s) {
		vmm_vserial_destroy(s->vser);
		fifo_free(s->rd_fifo);
		vmm_free(s);
		edev->priv = NULL;
	}

	return VMM_OK;
}

static u32 pl011_configs[] = {
	/* === arm === */
	/* id0 */ 0x11,  
	/* id1 */ 0x10, 
	/* id2 */ 0x14, 
	/* id3 */ 0x00, 
	/* id4 */ 0x0d, 
	/* id5 */ 0xf0, 
	/* id6 */ 0x05, 
	/* id7 */ 0xb1,
	/* === luminary === */
	/* id0 */ 0x11, 
	/* id1 */ 0x00, 
	/* id2 */ 0x18, 
	/* id3 */ 0x01, 
	/* id4 */ 0x0d, 
	/* id5 */ 0xf0, 
	/* id6 */ 0x05, 
	/* id7 */ 0xb1,
};

static struct vmm_devtree_nodeid pl011_emuid_table[] = {
	{ .type = "serial", 
	  .compatible = "primecell,arm,pl011", 
	  .data = &pl011_configs[0],
	},
	{ .type = "serial", 
	  .compatible = "primecell,luminary,pl011", 
	  .data = &pl011_configs[8],
	},
	{ /* end of list */ },
};

static struct vmm_emulator pl011_emulator = {
	.name = "pl011",
	.match_table = pl011_emuid_table,
	.endian = VMM_DEVEMU_LITTLE_ENDIAN,
	.probe = pl011_emulator_probe,
	.read8 = pl011_emulator_read8,
	.write8 = pl011_emulator_write8,
	.read16 = pl011_emulator_read16,
	.write16 = pl011_emulator_write16,
	.read32 = pl011_emulator_read32,
	.write32 = pl011_emulator_write32,
	.reset = pl011_emulator_reset,
	.remove = pl011_emulator_remove,
};

static int __init pl011_emulator_init(void)
{
	return vmm_devemu_register_emulator(&pl011_emulator);
}

static void __exit pl011_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&pl011_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
