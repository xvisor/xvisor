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
 * @file pl110.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief PrimeCell PL110/PL111 (AMBA CLCD) emulator.
 *
 * The source has been largely adapted from QEMU sources:
 * hw/display/pl110.c
 * 
 * Arm PrimeCell PL110 Color LCD Controller
 *
 * Copyright (c) 2005-2009 CodeSourcery.
 * Written by Paul Brook
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>
#include <vmm_host_io.h>
#include <vmm_guest_aspace.h>
#include <vio/vmm_pixel_ops.h>
#include <vio/vmm_vdisplay.h>

#include "drawfn.h"

#define MODULE_DESC			"PL110 CLCD Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_VDISPLAY_IPRIORITY+1)
#define	MODULE_INIT			pl110_emulator_init
#define	MODULE_EXIT			pl110_emulator_exit

#define PL110_CR_EN   0x001
#define PL110_CR_BGR  0x100
#define PL110_CR_BEBO 0x200
#define PL110_CR_BEPO 0x400
#define PL110_CR_PWR  0x800

/* The Versatile/PB uses a slightly modified PL110 controller. */
enum pl110_version {
	PL110,
	PL110_VERSATILE,
	PL111
};

struct pl110_state {
	struct vmm_guest *guest;
	struct vmm_vdisplay *vdis;
	u8 id[8];
	u32 version;
	u32 irq;
	u32 mux_in;

	vmm_spinlock_t lock;
	u32 timing[4];
	u32 cr;
	u32 upbase;
	u32 lpbase;
	u32 int_status;
	u32 int_mask;
	u32 cols;
	u32 rows;
	enum drawfn_bppmode bpp;
	u32 mux_ctrl;
	u32 palette8[256];
	u32 palette15[256];
	u32 palette16[256];
	u32 palette32[256];
	u32 raw_palette[128];
};

/* Note: This function must be called with state lock held */
static int __pl110_enabled(struct pl110_state *s)
{
	return (s->cr & PL110_CR_EN) && (s->cr & PL110_CR_PWR);
}

static int pl110_enabled(struct pl110_state *s)
{
	int ret;

	vmm_spin_lock(&s->lock);
	ret = __pl110_enabled(s);
	vmm_spin_unlock(&s->lock);

	return ret;
}

static void pl110_display_invalidate(struct vmm_vdisplay *vdis)
{
	struct pl110_state *s = vmm_vdisplay_priv(vdis);

	if (pl110_enabled(s)) {
		vmm_vdisplay_surface_gfx_clear(vdis);
	}
}

static int pl110_display_pixeldata(struct vmm_vdisplay *vdis,
				   struct vmm_pixelformat *pf,
				   u32 *rows, u32 *cols,
				   physical_addr_t *pa)
{
	u32 flags;
	physical_addr_t gpa, hpa;
	physical_size_t gsz, hsz;
	int rc, bits_per_pixel, bytes_per_pixel;
	struct pl110_state *s = vmm_vdisplay_priv(vdis);

	if (!pl110_enabled(s)) {
		return VMM_ENOTAVAIL;
	}

	switch (s->bpp) {
	case DRAWFN_BPP_16:
	case DRAWFN_BPP_16_565:
		bits_per_pixel = 16;
		bytes_per_pixel = 2;
		break;
	case DRAWFN_BPP_32:
		bits_per_pixel = 32;
		bytes_per_pixel = 4;
		break;
	default:
		return VMM_EINVALID;
	};

	gpa = s->upbase;
	gsz = (s->cols * s->rows) * bytes_per_pixel;
	rc = vmm_guest_physical_map(s->guest, gpa, gsz, &hpa, &hsz, &flags);
	if (rc) {
		return rc;
	}

	if (!(flags & VMM_REGION_REAL) ||
	    !(flags & VMM_REGION_MEMORY) ||
	    !(flags & VMM_REGION_ISRAM)) {
		return VMM_EINVALID;
	}

	if (hsz < gsz) {
		return VMM_EINVALID;
	}

	vmm_pixelformat_init_default(pf, bits_per_pixel);
	*rows = s->rows;
	*cols = s->cols;
	*pa = hpa;

	return VMM_OK;
}

static void pl110_display_update(struct vmm_vdisplay *vdis,
				 struct vmm_surface *sf)
{
	u32 *palette;
	drawfn *fntable;
	physical_addr_t gphys;
	enum drawfn_format fmt;
	enum drawfn_order order;
	enum drawfn_bppmode bppmode;
	int cols, rows, first, last;
	int dest_width, src_width;
	struct pl110_state *s = vmm_vdisplay_priv(vdis);

	if (!pl110_enabled(s)) {
		return;
	}

	switch (vmm_surface_bits_per_pixel(sf)) {
	case 0:
		return;
	case 8:
		fntable = drawfn_surface_fntable_8;
		dest_width = 1;
		palette = s->palette8;
		break;
	case 15:
		fntable = drawfn_surface_fntable_15;
		dest_width = 2;
		palette = s->palette15;
		break;
	case 16:
		fntable = drawfn_surface_fntable_16;
		dest_width = 2;
		palette = s->palette16;
		break;
	case 24:
		fntable = drawfn_surface_fntable_24;
		dest_width = 3;
		palette = s->palette32;
		break;
	case 32:
		fntable = drawfn_surface_fntable_32;
		dest_width = 4;
		palette = s->palette32;
		break;
	default:
		vmm_printf("%s: Bad color depth\n", __func__);
		return;
	};

	vmm_spin_lock(&s->lock);

	if (s->cr & PL110_CR_BGR) {
		fmt = DRAWFN_FORMAT_BGR;
	} else {
		fmt = DRAWFN_FORMAT_RGB;
	}

	if (s->cr & PL110_CR_BEBO) {
		order = DRAWFN_ORDER_BBBP;
	} else if (s->cr & PL110_CR_BEPO) {
		order = DRAWFN_ORDER_BBLP;
	} else {
		order = DRAWFN_ORDER_LBLP;
	}

	bppmode = s->bpp;
	if ((s->version != PL111) && (bppmode == DRAWFN_BPP_16)) {
		/* The PL110's native 16 bit mode is 5551; however
		 * most boards with a PL110 implement an external
		 * mux which allows bits to be reshuffled to give
		 * 565 format. The mux is typically controlled by
		 * an external system register.
		 * This is controlled by a GPIO input pin
		 * so boards can wire it up to their register.
		 *
		 * The PL111 straightforwardly implements both
		 * 5551 and 565 under control of the bpp field
		 * in the LCDControl register.
		 */
		switch (s->mux_ctrl) {
		case 3: /* 565 BGR */
			bppmode = DRAWFN_BPP_16_565;
			break;
		case 1: /* 5551 */
			break;
		case 0: /* 888; also if we have loaded vmstate from an old version */
		case 2: /* 565 RGB */
		default:
			/* treat as 565 but honour BGR bit */
			bppmode = DRAWFN_BPP_16_565;
			break;
		};
	}

	src_width = s->cols;
	switch (s->bpp) {
	case DRAWFN_BPP_1:
		src_width >>= 3;
		break;
	case DRAWFN_BPP_2:
		src_width >>= 2;
		break;
	case DRAWFN_BPP_4:
		src_width >>= 1;
		break;
	case DRAWFN_BPP_8:
		break;
	case DRAWFN_BPP_16:
	case DRAWFN_BPP_16_565:
	case DRAWFN_BPP_12:
		src_width <<= 1;
		break;
	case DRAWFN_BPP_32:
		src_width <<= 2;
		break;
	};

	dest_width *= s->cols;

	gphys = s->upbase;
	cols = s->cols;
	rows = s->rows;

	vmm_spin_unlock(&s->lock);

	first = 0;
	vmm_surface_update(sf, s->guest, gphys, cols, rows,
			   src_width, dest_width, 0,
			   fntable[DRAWFN_FNTABLE_INDEX(fmt, order, bppmode)],
			   palette, &first, &last);
	if (first >= 0) {
		vmm_vdisplay_surface_gfx_update(vdis, 0, first, cols,
						last - first + 1);
	}
}

/* Process IRQ asserted via device emulation framework */
static void pl110_mux_in_irq_handle(u32 irq, int cpu, int level, void *opaque)
{
	struct pl110_state *s = opaque;

	vmm_spin_lock(&s->lock);

	s->mux_ctrl = level;

	vmm_spin_unlock(&s->lock);
}

/* Note: This function must be called with state lock held */
static void __pl110_palette_update(struct pl110_state *s, int bpp, int n)
{
	int i;
	u32 raw;
	unsigned int r, g, b;

	raw = s->raw_palette[n];
	n <<= 1;
	for (i = 0; i < 2; i++) {
		r = (raw & 0x1f) << 3;
		raw >>= 5;
		g = (raw & 0x1f) << 3;
		raw >>= 5;
		b = (raw & 0x1f) << 3;
		/* The I bit is ignored.  */
		raw >>= 6;
		switch (bpp) {
		case 8:
			s->palette8[n] = rgb_to_pixel8(r, g, b);
			break;
		case 15:
			s->palette15[n] = rgb_to_pixel15(r, g, b);
			break;
		case 16:
			s->palette16[n] = rgb_to_pixel16(r, g, b);
			break;
		case 24:
		case 32:
			s->palette32[n] = rgb_to_pixel32(r, g, b);
			break;
		};
		n++;
	}
}

/* Resize virtual display. */
static void pl110_resize(struct pl110_state *s, int width, int height)
{
	bool do_gfx_resize = FALSE;

	vmm_spin_lock(&s->lock);

	if (width != s->cols || height != s->rows) {
		if (__pl110_enabled(s)) {
			do_gfx_resize = TRUE;
		}
	}
	s->cols = width;
	s->rows = height;

	vmm_spin_unlock(&s->lock);

	if (do_gfx_resize) {
		vmm_vdisplay_surface_gfx_resize(s->vdis, width, height);
	}
}

/* Update interrupts. */
static void pl110_update(struct pl110_state *s)
{
	/* TODO: Implement interrupts.  */
}

static int pl110_reg_read(struct pl110_state *s, u32 offset, u32 *dst)
{
	int rc = VMM_OK;

	if (offset >= 0xfe0 && offset < 0x1000) {
		*dst = s->id[(offset - 0xfe0) >> 2];
		return rc;
	}

	vmm_spin_lock(&s->lock);

	if (offset >= 0x200 && offset < 0x400) {
		*dst = s->raw_palette[(offset - 0x200) >> 2];
		vmm_spin_unlock(&s->lock);
		return rc;
	}

	switch (offset >> 2) {
	case 0: /* LCDTiming0 */
		*dst = s->timing[0];
		break;
	case 1: /* LCDTiming1 */
		*dst = s->timing[1];
		break;
	case 2: /* LCDTiming2 */
		*dst = s->timing[2];
		break;
	case 3: /* LCDTiming3 */
		*dst = s->timing[3];
		break;
	case 4: /* LCDUPBASE */
		*dst = s->upbase;
		break;
	case 5: /* LCDLPBASE */
		*dst = s->lpbase;
		break;
	case 6: /* LCDIMSC */
		if (s->version != PL110) {
			*dst = s->cr;
		} else {
			*dst = s->int_mask;
		}
		break;
	case 7: /* LCDControl */
		if (s->version != PL110) {
			*dst = s->int_mask;
		} else {
			*dst = s->cr;
		}
		break;
	case 8: /* LCDRIS */
		*dst = s->int_status;
		break;
	case 9: /* LCDMIS */
		*dst = s->int_status & s->int_mask;
		break;
	case 11: /* LCDUPCURR */
		/* TODO: Implement vertical refresh.  */
		*dst = s->upbase;
		break;
	case 12: /* LCDLPCURR */
		*dst = s->lpbase;
		break;
	default:
		rc = VMM_EFAIL;
		break;
	};

	vmm_spin_unlock(&s->lock);

	return rc;
}

static int pl110_reg_write(struct pl110_state *s, u32 offset,
			   u32 src_mask, u32 src)
{
	u32 val, n;
	bool resize = FALSE, update = FALSE;
	int resize_w, resize_h, rc = VMM_OK;

	vmm_spin_lock(&s->lock);

	if (offset >= 0x200 && offset < 0x400) {
		/* Palette.  */
		n = (offset - 0x200) >> 2;
		val = s->raw_palette[(offset - 0x200) >> 2];
		val = (val & src_mask) | (src & ~src_mask);
		s->raw_palette[(offset - 0x200) >> 2] = val;
		__pl110_palette_update(s, 8, n);
		__pl110_palette_update(s, 15, n);
		__pl110_palette_update(s, 16, n);
		__pl110_palette_update(s, 32, n);
		goto done;
	}

	switch (offset >> 2) {
	case 0: /* LCDTiming0 */
		s->timing[0] = (s->timing[0] & src_mask) | (src & ~src_mask);
		n = ((s->timing[0] & 0xfc) + 4) * 4;
		resize = TRUE;
		resize_w = n;
		resize_h = s->rows;
		break;
	case 1: /* LCDTiming1 */
		s->timing[1] = (s->timing[1] & src_mask) | (src & ~src_mask);
		n = (s->timing[1] & 0x3ff) + 1;
		resize = TRUE;
		resize_w = s->cols;
		resize_h = n;
		break;
	case 2: /* LCDTiming2 */
		s->timing[2] = (s->timing[2] & src_mask) | (src & ~src_mask);
		break;
	case 3: /* LCDTiming3 */
		s->timing[3] = (s->timing[3] & src_mask) | (src & ~src_mask);
		break;
	case 4: /* LCDUPBASE */
		s->upbase = (s->upbase & src_mask) | (src & ~src_mask);
		break;
	case 5: /* LCDLPBASE */
		s->lpbase = (s->lpbase & src_mask) | (src & ~src_mask);
		break;
	case 6: /* LCDIMSC */
		if (s->version != PL110) {
			goto control;
		}
	imsc:
		s->int_mask = (s->int_mask & src_mask) | (src & ~src_mask);
		update = TRUE;
		break;
	case 7: /* LCDControl */
		if (s->version != PL110) {
			goto imsc;
		}
	control:
		s->cr = (s->cr & src_mask) | (src & ~src_mask);
		s->bpp = (s->cr >> 1) & 7;
		resize = TRUE;
		resize_w = s->cols;
		resize_h = s->rows;
		break;
	case 10: /* LCDICR */
		s->int_status &= ~(src & ~src_mask);
		update = TRUE;
		break;
	default:
		rc = VMM_EFAIL;
		break;
	};

done:
	vmm_spin_unlock(&s->lock);

	if (update) {
		pl110_update(s);
	}

	if (resize) {
		pl110_resize(s, resize_w, resize_h);
	}

	/* For simplicity clear the surface whenever
	 * a control register is written to.
	 */
	pl110_display_invalidate(s->vdis);

	return rc;
}

static int pl110_emulator_read8(struct vmm_emudev *edev,
				physical_addr_t offset, 
				u8 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = pl110_reg_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFF;
	}

	return rc;
}

static int pl110_emulator_read16(struct vmm_emudev *edev,
				 physical_addr_t offset, 
				 u16 *dst)
{
	int rc;
	u32 regval = 0x0;

	rc = pl110_reg_read(edev->priv, offset, &regval);
	if (!rc) {
		*dst = regval & 0xFFFF;
	}

	return rc;
}

static int pl110_emulator_read32(struct vmm_emudev *edev,
				 physical_addr_t offset, 
				 u32 *dst)
{
	return pl110_reg_read(edev->priv, offset, dst);
}

static int pl110_emulator_write8(struct vmm_emudev *edev,
				 physical_addr_t offset, 
				 u8 src)
{
	return pl110_reg_write(edev->priv, offset, 0xFFFFFF00, src);
}

static int pl110_emulator_write16(struct vmm_emudev *edev,
				  physical_addr_t offset, 
				  u16 src)
{
	return pl110_reg_write(edev->priv, offset, 0xFFFF0000, src);
}

static int pl110_emulator_write32(struct vmm_emudev *edev,
				  physical_addr_t offset, 
				  u32 src)
{
	return pl110_reg_write(edev->priv, offset, 0x00000000, src);
}

static int pl110_emulator_reset(struct vmm_emudev *edev)
{
	struct pl110_state *s = edev->priv;

	vmm_spin_lock(&s->lock);

	s->cr = 0;
	s->mux_ctrl = 0;
	memset(s->raw_palette, 0, sizeof(s->raw_palette));
	memset(s->palette8, 0, sizeof(s->palette8));
	memset(s->palette15, 0, sizeof(s->palette15));
	memset(s->palette16, 0, sizeof(s->palette16));
	memset(s->palette32, 0, sizeof(s->palette32));

	vmm_spin_unlock(&s->lock);

	vmm_vdisplay_surface_gfx_clear(s->vdis);

	return VMM_OK;
}

static struct vmm_vdisplay_ops pl110_ops = {
	.invalidate = pl110_display_invalidate,
	.gfx_pixeldata = pl110_display_pixeldata,
	.gfx_update = pl110_display_update,
};

static struct vmm_devemu_irqchip pl110_mux_in_irqchip = {
	.name = "PL110_MUX_IN",
	.handle = pl110_mux_in_irq_handle,
};

static int pl110_emulator_probe(struct vmm_guest *guest,
				struct vmm_emudev *edev,
				const struct vmm_devtree_nodeid *eid)
{
	int rc = VMM_OK;
	char name[64];
	struct pl110_state *s;

	s = vmm_zalloc(sizeof(struct pl110_state));
	if (!s) {
		rc = VMM_ENOMEM;
		goto pl110_emulator_probe_fail;
	}

	s->guest = guest;
	s->id[0] = ((u32 *)eid->data)[0];
	s->id[1] = ((u32 *)eid->data)[1];
	s->id[2] = ((u32 *)eid->data)[2];
	s->id[3] = ((u32 *)eid->data)[3];
	s->id[4] = ((u32 *)eid->data)[4];
	s->id[5] = ((u32 *)eid->data)[5];
	s->id[6] = ((u32 *)eid->data)[6];
	s->id[7] = ((u32 *)eid->data)[7];
	s->version = ((u32 *)eid->data)[8];

	rc = vmm_devtree_read_u32_atindex(edev->node,
					  VMM_DEVTREE_INTERRUPTS_ATTR_NAME,
					  &s->irq, 0);
	if (rc) {
		goto pl110_emulator_probe_freestate_fail;
	}
	if (vmm_devtree_read_u32(edev->node, "mux_in", &s->mux_in)) {
		s->mux_in = UINT_MAX;
	}
	INIT_SPIN_LOCK(&s->lock);

	strlcpy(name, guest->name, sizeof(name));
	strlcat(name, "/", sizeof(name));
	if (strlcat(name, edev->node->name, sizeof(name)) >= sizeof(name)) {
		rc = VMM_EOVERFLOW;
		goto pl110_emulator_probe_freestate_fail;
	}

	s->vdis = vmm_vdisplay_create(name, &pl110_ops, s);
	if (!s->vdis) {
		rc = VMM_ENOMEM;
		goto pl110_emulator_probe_freestate_fail;
	}

	if (s->mux_in != UINT_MAX) {
		vmm_devemu_register_irqchip(guest, s->mux_in,
					    &pl110_mux_in_irqchip, s);
	}

	edev->priv = s;

	return VMM_OK;

pl110_emulator_probe_freestate_fail:
	vmm_free(s);
pl110_emulator_probe_fail:
	return rc;
}

static int pl110_emulator_remove(struct vmm_emudev *edev)
{
	int rc = VMM_OK;
	struct pl110_state *s = edev->priv;

	if (!s) {
		return VMM_EFAIL;
	}

	if (s->mux_in != UINT_MAX) {
		vmm_devemu_unregister_irqchip(s->guest, s->mux_in,
					      &pl110_mux_in_irqchip, s);
	}
	vmm_vdisplay_destroy(s->vdis);
	vmm_free(s);

	return rc;
}

static u32 pl110_config[] = {
	/* id0 */ 0x10,
	/* id1 */ 0x11,
	/* id2 */ 0x04,
	/* id3 */ 0x00,
	/* id4 */ 0x0d,
	/* id5 */ 0xf0,
	/* id6 */ 0x05,
	/* id7 */ 0xb1,
	/* ver */ PL110,
};

/* The ARM documentation (DDI0224C) says the CLCDC on the Versatile board
 * has a different ID (0x93, 0x10, 0x04, 0x00, ...). However the hardware
 * itself has the same ID values as a stock PL110, and guests (in
 * particular Linux) rely on this. We emulate what the hardware does,
 * rather than what the docs claim it ought to do.
 */
static u32 pl110_versatile_config[] = {
	/* id0 */ 0x10,
	/* id1 */ 0x11,
	/* id2 */ 0x04,
	/* id3 */ 0x00,
	/* id4 */ 0x0d,
	/* id5 */ 0xf0,
	/* id6 */ 0x05,
	/* id7 */ 0xb1,
	/* ver */ PL110_VERSATILE,
};

static u32 pl111_config[] = {
	/* id0 */ 0x11,
	/* id1 */ 0x11,
	/* id2 */ 0x24,
	/* id3 */ 0x00,
	/* id4 */ 0x0d,
	/* id5 */ 0xf0,
	/* id6 */ 0x05,
	/* id7 */ 0xb1,
	/* ver */ PL111,
};

static struct vmm_devtree_nodeid pl110_emuid_table[] = {
	{ .type = "display", 
	  .compatible = "primecell,pl110",
	  .data = &pl110_config,
	},
	{ .type = "display", 
	  .compatible = "primecell,pl110,versatile",
	  .data = &pl110_versatile_config,
	},
	{ .type = "display", 
	  .compatible = "primecell,pl111",
	  .data = &pl111_config,
	},
	{ /* end of list */ },
};

static struct vmm_emulator pl110_emulator = {
	.name = "pl110",
	.match_table = pl110_emuid_table,
	.endian = VMM_DEVEMU_LITTLE_ENDIAN,
	.probe = pl110_emulator_probe,
	.read8 = pl110_emulator_read8,
	.write8 = pl110_emulator_write8,
	.read16 = pl110_emulator_read16,
	.write16 = pl110_emulator_write16,
	.read32 = pl110_emulator_read32,
	.write32 = pl110_emulator_write32,
	.reset = pl110_emulator_reset,
	.remove = pl110_emulator_remove,
};

static int __init pl110_emulator_init(void)
{
	return vmm_devemu_register_emulator(&pl110_emulator);
}

static void __exit pl110_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&pl110_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
