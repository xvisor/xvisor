/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file simplefb.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Simple Framebuffer emulator.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>
#include <vmm_host_io.h>
#include <vmm_guest_aspace.h>
#include <vio/vmm_pixel_ops.h>
#include <vio/vmm_vdisplay.h>
#include <libs/stringlib.h>

#include "drawfn.h"

#define MODULE_DESC			"Simple Framebuffer Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_VDISPLAY_IPRIORITY+1)
#define	MODULE_INIT			simplefb_emulator_init
#define	MODULE_EXIT			simplefb_emulator_exit

#define SIMPLEFB_MAGIC			0xa4a297a6 /* virt */
#define SIMPLEFB_VENDOR			0x52535658 /* XVSR */
#define SIMPLEFB_VERSION_0_1		0x00000001

struct simplefb_state {
	struct vmm_emudev *edev;
	struct vmm_guest *guest;
	struct vmm_vdisplay *vdis;
	struct vmm_notifier_block nb;
	vmm_spinlock_t lock;
	u32 magic;
	u32 vendor;
	u32 version;
	u32 reserved;
	char mode[16];
	u32 bits_per_pixel;
	u32 bytes_per_pixel;
	enum drawfn_bppmode bppmode;
	enum drawfn_format format;
	enum drawfn_order order;
	u32 width;
	u32 height;
	u32 stride;
	bool fb_base_avail;
	physical_addr_t fb_base;
	u32 fb_base_ms;
	u32 fb_base_ls;
};

static void simplefb_display_invalidate(struct vmm_vdisplay *vdis)
{
	vmm_vdisplay_surface_gfx_clear(vdis);
}

static int simplefb_display_pixeldata(struct vmm_vdisplay *vdis,
				      struct vmm_pixelformat *pf,
				      u32 *rows, u32 *cols,
				      physical_addr_t *pa)
{
	int rc;
	u32 flags;
	physical_addr_t gpa, hpa;
	physical_size_t gsz, hsz;
	struct simplefb_state *s = vmm_vdisplay_priv(vdis);

	if (!s->fb_base_avail) {
		return VMM_ENOTAVAIL;
	}

	gpa = s->fb_base;
	gsz = (s->width * s->height) * s->stride;
	rc = vmm_guest_physical_map(s->guest, gpa, gsz, &hpa, &hsz, &flags);
	if (rc) {
		return rc;
	}

	if (!(flags & VMM_REGION_REAL) ||
	    !(flags & VMM_REGION_MEMORY) ||
	    !(flags & VMM_REGION_ISRAM)) {
		return VMM_EINVALID;
	}

	vmm_pixelformat_init_default(pf, s->bits_per_pixel);
	*rows = s->height;
	*cols = s->width;
	*pa = hpa;

	return VMM_OK;
}

static void simplefb_display_update(struct vmm_vdisplay *vdis,
				    struct vmm_surface *sf)
{
	drawfn *fntable;
	physical_addr_t gphys;
	int width, height, first, last;
	int dest_width, src_width;
	enum drawfn_format fmt;
	enum drawfn_order order;
	enum drawfn_bppmode bppmode;
	struct simplefb_state *s = vmm_vdisplay_priv(vdis);

	switch (vmm_surface_bits_per_pixel(sf)) {
	case 16:
		fntable = drawfn_surface_fntable_16;
		dest_width = 2;
		break;
	case 24:
		fntable = drawfn_surface_fntable_24;
		dest_width = 3;
		break;
	case 32:
		fntable = drawfn_surface_fntable_32;
		dest_width = 4;
		break;
	default:
		vmm_printf("%s: Bad surface color depth\n", __func__);
		return;
	};

	vmm_spin_lock(&s->lock);

	src_width = s->stride;
	dest_width *= s->width;
	width = s->width;
	height = s->height;
	gphys = s->fb_base;
	bppmode = s->bppmode;
	fmt = s->format;
	order = s->order;

	vmm_spin_unlock(&s->lock);

	first = 0;
	vmm_surface_update(sf, s->guest, gphys, width, height,
			   src_width, dest_width, 0,
			   fntable[DRAWFN_FNTABLE_INDEX(fmt, order, bppmode)],
			   NULL, &first, &last);
	if (first >= 0) {
		vmm_vdisplay_surface_gfx_update(vdis, 0, first, width,
						last - first + 1);
	}
}

static int simplefb_emulator_read(struct vmm_emudev *edev,
				  physical_addr_t offset,
				  u32 *dst,
				  u32 size)
{
	int rc = VMM_OK;
	struct simplefb_state *s = edev->priv;

	vmm_spin_lock(&s->lock);

	switch (offset) {
	case 0x00: /* MAGIC */
		*dst = s->magic;
		break;
	case 0x04: /* VENDOR */
		*dst = s->vendor;
		break;
	case 0x08: /* VERSION */
		*dst = s->version;
		break;
	case 0x0c: /* RESERVED */
		*dst = s->reserved;
		break;
	case 0x10: /* MODEx */
	case 0x14:
	case 0x18:
	case 0x1c:
	case 0x20:
	case 0x24:
	case 0x28:
	case 0x2c:
	case 0x30:
	case 0x34:
	case 0x38:
	case 0x3c:
	case 0x40:
	case 0x44:
	case 0x48:
	case 0x4c:
		*dst = (u32)s->mode[(offset - 0x10) >> 2];
		break;
	case 0x50: /* WIDTH */
		*dst = s->width;
		break;
	case 0x54: /* HEIGHT */
		*dst = s->height;
		break;
	case 0x58: /* STRIDE */
		*dst = s->stride;
		break;
	case 0x5c: /* FB_BASE_MS */
		*dst = s->fb_base_ms;
		break;
	case 0x60: /* FB_BASE_LS */
		*dst = s->fb_base_ls;
		break;
	default:
		rc = VMM_EFAIL;
		break;
	};

	vmm_spin_unlock(&s->lock);

	return rc;
}

static int simplefb_emulator_write(struct vmm_emudev *edev,
				   physical_addr_t offset,
				   u32 regmask,
				   u32 regval,
				   u32 size)
{
	/* We don't allow writes */
	return VMM_ENOTSUPP;
}

static int simplefb_emulator_reset(struct vmm_emudev *edev)
{
	/* Nothing to do here. */
	return VMM_OK;
}

static struct vmm_vdisplay_ops simplefb_ops = {
	.invalidate = simplefb_display_invalidate,
	.gfx_pixeldata = simplefb_display_pixeldata,
	.gfx_update = simplefb_display_update,
};

static int simplefb_guest_aspace_notification(struct vmm_notifier_block *nb,
					      unsigned long evt, void *data)
{
	int rc;
	physical_addr_t paddr;
	physical_size_t psize;
	struct vmm_region *reg;
	struct vmm_guest_aspace_event *edata = data;
	struct simplefb_state *s = container_of(nb, struct simplefb_state, nb);

	if (evt != VMM_GUEST_ASPACE_EVENT_INIT) {
		/* We are only interested in unregister events so,
		 * don't care about this event.
		 */
		return NOTIFY_DONE;
	}

	if (s->guest != edata->guest) {
		/* We are only interested in events for our guest */
		return NOTIFY_DONE;
	}

	rc = vmm_devtree_read_physaddr(s->edev->node, "base", &s->fb_base);
	if (rc) {
		vmm_printf("%s: guest=%s fb base not available\n",
			   __func__, s->guest->name);
		goto done;
	}

	reg = vmm_guest_find_region(s->guest, s->fb_base,
				    VMM_REGION_MEMORY, FALSE);
	if (!reg) {
		vmm_printf("%s: guest=%s region not found for "
			   "fb_base=0x%"PRIPADDR"\n",
			   __func__, s->guest->name, s->fb_base);
		goto done;
	}

	paddr = s->fb_base;
	psize = VMM_REGION_GPHYS_END(reg) - s->fb_base;
	if (psize < (s->height * s->stride)) {
		vmm_printf("%s: guest=%s invalid fb region size\n",
			   __func__, s->guest->name);
		goto done;
	}
	s->fb_base_ms = ((u64)paddr >> 32) & 0xffffffff;
	s->fb_base_ls = paddr & 0xffffffff;
	s->fb_base_avail = TRUE;

done:
	return NOTIFY_OK;
}

static int simplefb_emulator_probe(struct vmm_guest *guest,
				   struct vmm_emudev *edev,
				   const struct vmm_devtree_nodeid *eid)
{
	int rc = VMM_OK;
	char name[64];
	const char *str;
	struct simplefb_state *s;

	s = vmm_zalloc(sizeof(struct simplefb_state));
	if (!s) {
		rc = VMM_ENOMEM;
		goto simplefb_emulator_probe_fail;
	}

	s->edev = edev;
	s->guest = guest;
	INIT_SPIN_LOCK(&s->lock);

	s->magic = SIMPLEFB_MAGIC;
	s->vendor = SIMPLEFB_VENDOR;
	s->version = (u32)((unsigned long)eid->data);

	rc = vmm_devtree_read_u32(edev->node, "width", &s->width);
	if (rc) {
		goto simplefb_emulator_probe_freestate_fail;
	}

	rc = vmm_devtree_read_u32(edev->node, "height", &s->height);
	if (rc) {
		goto simplefb_emulator_probe_freestate_fail;
	}

	rc = vmm_devtree_read_string(edev->node, "mode", &str);
	if (rc) {
		goto simplefb_emulator_probe_freestate_fail;
	}

	if (strcmp(str, "r5g6b5") == 0) {
		strncpy(s->mode, str, sizeof(s->mode));
		s->bits_per_pixel = 16;
		s->bytes_per_pixel = 2;
		s->bppmode = DRAWFN_BPP_16;
		s->format = DRAWFN_FORMAT_RGB;
	} else if (strcmp(str, "x1r5g5b5") == 0) {
		strncpy(s->mode, str, sizeof(s->mode));
		s->bits_per_pixel = 16;
		s->bytes_per_pixel = 2;
		s->bppmode = DRAWFN_BPP_16_565;
		s->format = DRAWFN_FORMAT_RGB;
	} else if (strcmp(str, "a1r5g5b5") == 0) {
		strncpy(s->mode, str, sizeof(s->mode));
		s->bits_per_pixel = 16;
		s->bytes_per_pixel = 2;
		s->bppmode = DRAWFN_BPP_16_565;
		s->format = DRAWFN_FORMAT_RGB;
	} else if (strcmp(str, "r8g8b8") == 0) {
		strncpy(s->mode, str, sizeof(s->mode));
		s->bits_per_pixel = 24;
		s->bytes_per_pixel = 4;
		s->bppmode = DRAWFN_BPP_32;
		s->format = DRAWFN_FORMAT_RGB;
	} else if (strcmp(str, "x8r8g8b8") == 0) {
		strncpy(s->mode, str, sizeof(s->mode));
		s->bits_per_pixel = 32;
		s->bytes_per_pixel = 4;
		s->bppmode = DRAWFN_BPP_32;
		s->format = DRAWFN_FORMAT_RGB;
	} else if (strcmp(str, "a8r8g8b8") == 0) {
		strncpy(s->mode, str, sizeof(s->mode));
		s->bits_per_pixel = 32;
		s->bytes_per_pixel = 4;
		s->bppmode = DRAWFN_BPP_32;
		s->format = DRAWFN_FORMAT_RGB;
	} else if (strcmp(str, "a8b8g8r8") == 0) {
		strncpy(s->mode, str, sizeof(s->mode));
		s->bits_per_pixel = 32;
		s->bytes_per_pixel = 4;
		s->bppmode = DRAWFN_BPP_32;
		s->format = DRAWFN_FORMAT_BGR;
	} else {
		rc = VMM_EINVALID;
		goto simplefb_emulator_probe_freestate_fail;
	}

	rc = vmm_devtree_read_string(edev->node, "order", &str);
	if (rc) {
		goto simplefb_emulator_probe_freestate_fail;
	}

	if (strcmp(str, "lblp") == 0) {
		s->order = DRAWFN_ORDER_LBLP;
	} else if (strcmp(str, "bbbp") == 0) {
		s->order = DRAWFN_ORDER_BBBP;
	} else if (strcmp(str, "bblp") == 0) {
		s->order = DRAWFN_ORDER_BBLP;
	} else {
		rc = VMM_EINVALID;
		goto simplefb_emulator_probe_freestate_fail;
	}

	if (vmm_devtree_read_u32(edev->node, "stride", &s->height)) {
		s->stride = (s->width * s->bits_per_pixel) / 8;
	} else {
		if (s->stride < ((s->width * s->bits_per_pixel) / 8)) {
			rc = VMM_EINVALID;
			goto simplefb_emulator_probe_freestate_fail;
		}
	}

	strlcpy(name, guest->name, sizeof(name));
	strlcat(name, "/", sizeof(name));
	if (strlcat(name, edev->node->name, sizeof(name)) >= sizeof(name)) {
		rc = VMM_EOVERFLOW;
		goto simplefb_emulator_probe_freestate_fail;
	}

	s->nb.notifier_call = &simplefb_guest_aspace_notification;
	s->nb.priority = 0;
	rc = vmm_guest_aspace_register_client(&s->nb);
	if (rc) {
		goto simplefb_emulator_probe_freestate_fail;
	}

	s->vdis = vmm_vdisplay_create(name, &simplefb_ops, s);
	if (!s->vdis) {
		rc = VMM_ENOMEM;
		goto simplefb_emulator_probe_unreg_client_fail;
	}

	edev->priv = s;

	return VMM_OK;

simplefb_emulator_probe_unreg_client_fail:
	vmm_guest_aspace_unregister_client(&s->nb);
simplefb_emulator_probe_freestate_fail:
	vmm_free(s);
simplefb_emulator_probe_fail:
	return rc;
}

static int simplefb_emulator_remove(struct vmm_emudev *edev)
{
	int rc = VMM_OK;
	struct simplefb_state *s = edev->priv;

	if (!s) {
		return VMM_EFAIL;
	}

	vmm_vdisplay_destroy(s->vdis);
	vmm_guest_aspace_unregister_client(&s->nb);
	vmm_free(s);

	return rc;
}

static struct vmm_devtree_nodeid simplefb_emuid_table[] = {
	{ .type = "display",
	  .compatible = "simplefb-0.1",
	  .data = (void *)SIMPLEFB_VERSION_0_1,
	},
	{ /* end of list */ },
};

VMM_DECLARE_EMULATOR_SIMPLE(simplefb_emulator,
			    "simplefb",
			    simplefb_emuid_table,
			    VMM_DEVEMU_LITTLE_ENDIAN,
			    simplefb_emulator_probe,
			    simplefb_emulator_remove,
			    simplefb_emulator_reset,
			    simplefb_emulator_read,
			    simplefb_emulator_write);

static int __init simplefb_emulator_init(void)
{
	return vmm_devemu_register_emulator(&simplefb_emulator);
}

static void __exit simplefb_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&simplefb_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
