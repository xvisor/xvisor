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
#include <vmm_string.h>
#include <vmm_modules.h>
#include <vmm_devtree.h>
#include <vmm_ringbuf.h>
#include <vmm_vserial.h>
#include <vmm_host_io.h>
#include <vmm_devemu.h>

#define MODULE_VARID			pl011_emulator_module
#define MODULE_NAME			"PL011 Serial Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		0
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
	struct vmm_ringbuf *rd_fifo;
};

static void pl011_set_irq(struct pl011_state * s)
{
	if (s->int_level & s->int_enabled) {
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

static int pl011_reg_read(struct pl011_state * s, u32 offset, u32 *dst)
{
	int rc = VMM_OK;
	u8 val = 0x0;
	u32 read_count = 0x0;

	vmm_spin_lock(&s->lock);

	switch (offset >> 2) {
	case 0: /* UARTDR */
		s->flags &= ~PL011_FLAG_RXFF;
		vmm_ringbuf_dequeue(s->rd_fifo, &val);
		*dst = val;
		read_count = vmm_ringbuf_avail(s->rd_fifo);
		if (read_count == 0) {
			s->flags |= PL011_FLAG_RXFE;
		}
		if (read_count == (s->rd_trig - 1)) {
			s->int_level &= ~PL011_INT_RX;
		}
		pl011_set_irq(s);
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

	return rc;
}

static int pl011_reg_write(struct pl011_state * s, u32 offset, 
			   u32 src_mask, u32 src)
{
	int rc = VMM_OK;
	u8 val;

	vmm_spin_lock(&s->lock);

	switch (offset >> 2) {
	case 0: /* UARTDR */
		/* ??? Check if transmitter is enabled.  */
		val = src;
		vmm_vserial_receive(s->vser, &val, 1);
		s->int_level |= PL011_INT_TX;
		pl011_set_irq(s);
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
		pl011_set_irq(s);
		break;
	case 17: /* UARTICR */
		s->int_level &= ~(src & ~src_mask);
		pl011_set_irq(s);
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

	return rc;
}

static bool pl011_vserial_can_send(struct vmm_vserial *vser)
{
	struct pl011_state * s = vser->priv;
#if 0
	u32 rd_count;

	rd_count = vmm_ringbuf_avail(s->rd_fifo);
	if (s->lcr & 0x10) {
		return (rd_count < s->fifo_sz);
	} else {
		return (rd_count < 1);
	}
#endif
	return !vmm_ringbuf_isfull(s->rd_fifo);
}

static int pl011_vserial_send(struct vmm_vserial *vser, u8 data)
{
	struct pl011_state * s = vser->priv;
	u32 rd_count;

	vmm_ringbuf_enqueue(s->rd_fifo, &data, TRUE);
	rd_count = vmm_ringbuf_avail(s->rd_fifo);
	vmm_spin_lock(&s->lock);
	s->flags &= ~PL011_FLAG_RXFE;
	if (s->cr & 0x10 || rd_count == s->fifo_sz) {
		s->flags |= PL011_FLAG_RXFF;
	}
	if (rd_count >= s->rd_trig) {
		s->int_level |= PL011_INT_RX;
		pl011_set_irq(s);
	}
	vmm_spin_unlock(&s->lock);

	return VMM_OK;
}

static int pl011_emulator_read(struct vmm_emudev *edev,
			       physical_addr_t offset, 
			       void *dst, u32 dst_len)
{
	int rc = VMM_OK;
	u32 regval = 0x0;
	struct pl011_state * s = edev->priv;

	rc = pl011_reg_read(s, offset & ~0x3, &regval);

	if (!rc) {
		regval = (regval >> ((offset & 0x3) * 8));
		switch (dst_len) {
		case 1:
			*(u8 *)dst = regval & 0xFF;
			break;
		case 2:
			*(u16 *)dst = arch_cpu_to_le16(regval & 0xFFFF);
			break;
		case 4:
			*(u32 *)dst = arch_cpu_to_le32(regval);
			break;
		default:
			rc = VMM_EFAIL;
			break;
		};
	}

	return rc;
}

static int pl011_emulator_write(struct vmm_emudev *edev,
				physical_addr_t offset, 
				void *src, u32 src_len)
{
	int i;
	u32 regmask = 0x0, regval = 0x0;
	struct pl011_state * s = edev->priv;

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

	return pl011_reg_write(s, offset & ~0x3, regmask, regval);
}

static int pl011_emulator_reset(struct vmm_emudev *edev)
{
	struct pl011_state * s = edev->priv;

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
				const struct vmm_emuid *eid)
{
	int rc = VMM_OK;
	char name[64];
	const char *attr;
	struct pl011_state * s;

	s = vmm_malloc(sizeof(struct pl011_state));
	if (!s) {
		rc = VMM_EFAIL;
		goto pl011_emulator_probe_done;
	}
	vmm_memset(s, 0x0, sizeof(struct pl011_state));

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

	attr = vmm_devtree_attrval(edev->node, "irq");
	if (attr) {
		s->irq = *((u32 *)attr);
	} else {
		rc = VMM_EFAIL;
		goto pl011_emulator_probe_freestate_fail;
	}

	attr = vmm_devtree_attrval(edev->node, "fifo_size");
	if (attr) {
		s->fifo_sz = *((u32 *)attr);
	} else {
		rc = VMM_EFAIL;
		goto pl011_emulator_probe_freestate_fail;
	}

	s->rd_fifo = vmm_ringbuf_alloc(1, s->fifo_sz);
	if (!s->rd_fifo) {
		rc = VMM_EFAIL;
		goto pl011_emulator_probe_freestate_fail;
	}

	vmm_strcpy(name, guest->node->name);
	vmm_strcat(name, "/");
	vmm_strcat(name, edev->node->name);
	s->vser = vmm_vserial_alloc(name, 
				    &pl011_vserial_can_send, 
				    &pl011_vserial_send, 
				    s->fifo_sz,
				    s);
	if (!(s->vser)) {
		goto pl011_emulator_probe_freerbuf_fail;
	}

	edev->priv = s;

	goto pl011_emulator_probe_done;

pl011_emulator_probe_freerbuf_fail:
	vmm_ringbuf_free(s->rd_fifo);
pl011_emulator_probe_freestate_fail:
	vmm_free(s);
pl011_emulator_probe_done:
	return rc;
}

static int pl011_emulator_remove(struct vmm_emudev *edev)
{
	struct pl011_state * s = edev->priv;

	if (s) {
		vmm_vserial_free(s->vser);
		vmm_ringbuf_free(s->rd_fifo);
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

static struct vmm_emuid pl011_emuid_table[] = {
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
	.probe = pl011_emulator_probe,
	.read = pl011_emulator_read,
	.write = pl011_emulator_write,
	.reset = pl011_emulator_reset,
	.remove = pl011_emulator_remove,
};

static int __init pl011_emulator_init(void)
{
	return vmm_devemu_register_emulator(&pl011_emulator);
}

static void pl011_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&pl011_emulator);
}

VMM_DECLARE_MODULE(MODULE_VARID, 
			MODULE_NAME, 
			MODULE_AUTHOR, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
