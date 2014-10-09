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
 * @file vmm_vdisplay.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for virtual display subsystem
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_heap.h>
#include <vmm_mutex.h>
#include <vmm_modules.h>
#include <vmm_guest_aspace.h>
#include <vio/vmm_vdisplay.h>
#include <libs/mathlib.h>
#include <libs/stringlib.h>

#define MODULE_DESC			"Virtual Display Framework"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_VDISPLAY_IPRIORITY)
#define	MODULE_INIT			vmm_vdisplay_init
#define	MODULE_EXIT			vmm_vdisplay_exit

struct vmm_vdisplay_ctrl {
	struct vmm_mutex vdis_list_lock;
	struct dlist vdis_list;
	struct vmm_blocking_notifier_chain notifier_chain;
};

static struct vmm_vdisplay_ctrl vdctrl;

int vmm_vdisplay_register_client(struct vmm_notifier_block *nb)
{
	return vmm_blocking_notifier_register(&vdctrl.notifier_chain, nb);
}
VMM_EXPORT_SYMBOL(vmm_vdisplay_register_client);

int vmm_vdisplay_unregister_client(struct vmm_notifier_block *nb)
{
	return vmm_blocking_notifier_unregister(&vdctrl.notifier_chain, nb);
}
VMM_EXPORT_SYMBOL(vmm_vdisplay_unregister_client);

void vmm_pixelformat_init_default(struct vmm_pixelformat *pf, int bpp)
{
	if (!pf) {
		return;
	}

	memset(pf, 0x00, sizeof(struct vmm_pixelformat));

	pf->bits_per_pixel = bpp;
	pf->bytes_per_pixel = DIV_ROUND_UP(bpp, 8);
	pf->depth = bpp == 32 ? 24 : bpp;

	switch (bpp) {
	case 15:
		pf->bits_per_pixel = 16;
		pf->rmask = 0x00007c00;
		pf->gmask = 0x000003E0;
		pf->bmask = 0x0000001F;
		pf->rmax = 31;
		pf->gmax = 31;
		pf->bmax = 31;
		pf->rshift = 10;
		pf->gshift = 5;
		pf->bshift = 0;
		pf->rbits = 5;
		pf->gbits = 5;
		pf->bbits = 5;
		break;
	case 16:
		pf->rmask = 0x0000F800;
		pf->gmask = 0x000007E0;
		pf->bmask = 0x0000001F;
		pf->rmax = 31;
		pf->gmax = 63;
		pf->bmax = 31;
		pf->rshift = 11;
		pf->gshift = 5;
		pf->bshift = 0;
		pf->rbits = 5;
		pf->gbits = 6;
		pf->bbits = 5;
		break;
	case 24:
		pf->rmask = 0x00FF0000;
		pf->gmask = 0x0000FF00;
		pf->bmask = 0x000000FF;
		pf->rmax = 255;
		pf->gmax = 255;
		pf->bmax = 255;
		pf->rshift = 16;
		pf->gshift = 8;
		pf->bshift = 0;
		pf->rbits = 8;
		pf->gbits = 8;
		pf->bbits = 8;
		break;
	case 32:
		pf->rmask = 0x00FF0000;
		pf->gmask = 0x0000FF00;
		pf->bmask = 0x000000FF;
		pf->rmax = 255;
		pf->gmax = 255;
		pf->bmax = 255;
		pf->rshift = 16;
		pf->gshift = 8;
		pf->bshift = 0;
		pf->rbits = 8;
		pf->gbits = 8;
		pf->bbits = 8;
		break;
	default:
		break;
	};
}
VMM_EXPORT_SYMBOL(vmm_pixelformat_init_default);

void vmm_pixelformat_init_different_endian(struct vmm_pixelformat *pf, int bpp)
{
	if (!pf) {
		return;
	}

	memset(pf, 0x00, sizeof(struct vmm_pixelformat));

	pf->bits_per_pixel = bpp;
	pf->bytes_per_pixel = DIV_ROUND_UP(bpp, 8);
	pf->depth = bpp == 32 ? 24 : bpp;

	switch (bpp) {
	case 24:
		pf->rmask = 0x000000FF;
		pf->gmask = 0x0000FF00;
		pf->bmask = 0x00FF0000;
		pf->rmax = 255;
		pf->gmax = 255;
		pf->bmax = 255;
		pf->rshift = 0;
		pf->gshift = 8;
		pf->bshift = 16;
		pf->rbits = 8;
		pf->gbits = 8;
		pf->bbits = 8;
		break;
	case 32:
		pf->rmask = 0x0000FF00;
		pf->gmask = 0x00FF0000;
		pf->bmask = 0xFF000000;
		pf->amask = 0x00000000;
		pf->amax = 255;
		pf->rmax = 255;
		pf->gmax = 255;
		pf->bmax = 255;
		pf->ashift = 0;
		pf->rshift = 8;
		pf->gshift = 16;
		pf->bshift = 24;
		pf->rbits = 8;
		pf->gbits = 8;
		pf->bbits = 8;
		pf->abits = 8;
		break;
	default:
		break;
	};
}
VMM_EXPORT_SYMBOL(vmm_pixelformat_init_different_endian);

void vmm_surface_update(struct vmm_surface *s,
			struct vmm_guest *guest,
			physical_addr_t src_gphys,
			int cols, int rows,
			int src_width,
			int dst_row_pitch,
			int dst_col_pitch,
			void (*fn)(struct vmm_surface *s,
				   void *priv, u8 *dst, const u8 *src,
				   int width, int dststep),
			void *fn_priv,
			int *first_row,
			int *last_row)
{
#define CHUNK_SIZE		256
	u32 len;
	int i, j;
	int chunk_len, chunk_cols, chunk_dst_row_pitch;
	u8 *dst, chunk[CHUNK_SIZE];

	/* Sanity check */
	if (!s || !guest || !first_row || !last_row) {
		return;
	}
	if ((rows <= 0) || (cols <= 0)) {
		return;
	}
	if ((src_width <= 0) || (dst_row_pitch == 0)) {
		return;
	}

	/* Clip rows and cols to fit the surface */
	rows = min(rows, vmm_surface_height(s));
	cols = min(cols, vmm_surface_width(s));

	/* Ensure that first_row is within limit */
	if ((*first_row < 0) || (rows <= *first_row)) {
		return;
	}

	/* Determine dst pointer */
	dst = vmm_surface_data(s);
	if (dst_col_pitch < 0) {
		dst -= dst_col_pitch * (cols - 1);
	}
	if (dst_row_pitch < 0) {
		dst -= dst_row_pitch * (rows - 1);
	}
	dst += (*first_row) * dst_row_pitch;

	/* Determine src guest physical address */
	src_gphys += (*first_row) * src_width;

	/* Update surface data in chunks */
	for (i = *first_row; i < rows; i++) {
		j = 0;
		while (j < src_width) {
			chunk_len = min(src_width - j, CHUNK_SIZE);
			chunk_cols = sdiv32((chunk_len * cols), src_width);
			chunk_len = sdiv32((chunk_cols * src_width), cols);
			chunk_dst_row_pitch =
				sdiv32((chunk_len * dst_row_pitch), src_width);
			
			len = vmm_guest_memory_read(guest, src_gphys,
						    chunk, chunk_len, FALSE);
			if (len != chunk_len) {
				goto next_chunk;
			}

			fn(s, fn_priv, dst, chunk, chunk_cols, dst_col_pitch);

next_chunk:
			j += chunk_len;
			src_gphys += chunk_len;
			dst += chunk_dst_row_pitch;
		}
	}

	*last_row = i;
}
VMM_EXPORT_SYMBOL(vmm_surface_update);

int vmm_surface_init(struct vmm_surface *s,
		     const char *name,
		     void *data, u32 data_size,
		     int height, int width, u32 flags,
		     struct vmm_pixelformat *pf,
		     const struct vmm_surface_ops *ops,
		     void *priv)
{
	if (!s || !name || !data || !pf || !ops) {
		return VMM_EFAIL;
	}
	if (height<=0 || width<=0) {
		return VMM_EINVALID;
	}
	if (data_size < (width * height * pf->bytes_per_pixel)) {
		return VMM_EINVALID;
	}

	INIT_LIST_HEAD(&s->head);
	if (strlcpy(s->name, name, sizeof(s->name)) >= sizeof(s->name)) {
		return VMM_EINVALID;
	}
	s->data = data;
	s->data_size = data_size;
	s->height = height;
	s->width = width;
	s->flags = flags;
#ifdef CONFIG_CPU_BE
	s->flags |= VMM_SURFACE_BIG_ENDIAN_FLAG;
#endif
	memcpy(&s->pf, pf, sizeof(struct vmm_pixelformat));
	s->ops = ops;
	s->priv = NULL;

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_surface_init);

struct vmm_surface *vmm_surface_alloc(const char *name,
				      void *data, u32 data_size,
				      int height, int width, u32 flags,
				      struct vmm_pixelformat *pf,
				      const struct vmm_surface_ops *ops,
				      void *priv)
{
	struct vmm_surface *s;

	s = vmm_zalloc(sizeof(struct vmm_surface));
	if (!s) {
		return NULL;
	}

	if (vmm_surface_init(s, name, data, data_size, height, width,
			     flags | VMM_SURFACE_ALLOCED_FLAG,
			     pf, ops, priv)) {
		vmm_free(s);
		return NULL;
	}

	return s;
}
VMM_EXPORT_SYMBOL(vmm_surface_alloc);

void vmm_surface_free(struct vmm_surface *s)
{
	if (!s) {
		return;
	}
	if (!(s->flags & VMM_SURFACE_ALLOCED_FLAG)) {
		return;
	}

	vmm_free(s);
}
VMM_EXPORT_SYMBOL(vmm_surface_free);

int vmm_vdisplay_get_pixeldata(struct vmm_vdisplay *vdis,
			       struct vmm_pixelformat *pf,
			       u32 *rows, u32 *cols,
			       physical_addr_t *pa)
{
	if (!vdis || !pf || !rows || !cols || !pa) {
		return VMM_EFAIL;
	}

	if (vdis->ops && vdis->ops->gfx_pixeldata) {
		return vdis->ops->gfx_pixeldata(vdis, pf, rows, cols, pa);
	}

	return VMM_EOPNOTSUPP;
}
VMM_EXPORT_SYMBOL(vmm_vdisplay_get_pixeldata);

void vmm_vdisplay_one_update(struct vmm_vdisplay *vdis,
			     struct vmm_surface *s)
{
	if (!vdis || !s) {
		return;
	}

	if (vdis->ops && vdis->ops->gfx_update) {
		vdis->ops->gfx_update(vdis, s);
	}
}
VMM_EXPORT_SYMBOL(vmm_vdisplay_one_update);

void vmm_vdisplay_update(struct vmm_vdisplay *vdis)
{
	irq_flags_t flags;
	struct vmm_surface *s;

	if (!vdis) {
		return;
	}

	vmm_spin_lock_irqsave(&vdis->surface_list_lock, flags);

	list_for_each_entry(s, &vdis->surface_list, head) {
		vmm_vdisplay_one_update(vdis, s);
	}

	vmm_spin_unlock_irqrestore(&vdis->surface_list_lock, flags);
}
VMM_EXPORT_SYMBOL(vmm_vdisplay_update);

void vmm_vdisplay_invalidate(struct vmm_vdisplay *vdis)
{
	if (!vdis) {
		return;
	}

	if (vdis->ops && vdis->ops->invalidate) {
		vdis->ops->invalidate(vdis);
	}
}
VMM_EXPORT_SYMBOL(vmm_vdisplay_invalidate);

void vmm_vdisplay_text_update(struct vmm_vdisplay *vdis,
			      unsigned long *chardata)
{
	if (!vdis || !chardata) {
		return;
	}

	if (vdis->ops && vdis->ops->text_update) {
		vdis->ops->text_update(vdis, chardata);
	}
}
VMM_EXPORT_SYMBOL(vmm_vdisplay_text_update);

static void __surface_refresh(struct vmm_surface *sf)
{
	if (sf->ops && sf->ops->refresh) {
		sf->ops->refresh(sf);
	}
}

void vmm_vdisplay_surface_refresh(struct vmm_vdisplay *vdis)
{
	irq_flags_t flags;
	struct vmm_surface *sf;

	if (!vdis) {
		return;
	}

	vmm_spin_lock_irqsave(&vdis->surface_list_lock, flags);

	list_for_each_entry(sf, &vdis->surface_list, head) {
		__surface_refresh(sf);
	}

	vmm_spin_unlock_irqrestore(&vdis->surface_list_lock, flags);
}
VMM_EXPORT_SYMBOL(vmm_vdisplay_surface_refresh);

static void __surface_gfx_clear(struct vmm_surface *sf)
{
	if (sf->ops && sf->ops->gfx_clear) {
		sf->ops->gfx_clear(sf);
	}
}

void vmm_vdisplay_surface_gfx_clear(struct vmm_vdisplay *vdis)
{
	irq_flags_t flags;
	struct vmm_surface *sf;

	if (!vdis) {
		return;
	}

	vmm_spin_lock_irqsave(&vdis->surface_list_lock, flags);

	list_for_each_entry(sf, &vdis->surface_list, head) {
		__surface_gfx_clear(sf);
	}

	vmm_spin_unlock_irqrestore(&vdis->surface_list_lock, flags);
}
VMM_EXPORT_SYMBOL(vmm_vdisplay_surface_gfx_clear);

static void __surface_gfx_update(struct vmm_surface *sf,
				 int x, int y, int w, int h)
{
	int width = vmm_surface_width(sf);
	int height = vmm_surface_height(sf);

	x = max(x, 0);
	y = max(y, 0);
	x = min(x, width);
	y = min(y, height);
	w = min(w, width - x);
	h = min(h, height - y);

	if (sf->ops && sf->ops->gfx_update) {
		sf->ops->gfx_update(sf, x, y, w, h);
	}
}

void vmm_vdisplay_surface_gfx_update(struct vmm_vdisplay *vdis,
				     int x, int y, int w, int h)
{
	irq_flags_t flags;
	struct vmm_surface *sf;

	if (!vdis) {
		return;
	}

	vmm_spin_lock_irqsave(&vdis->surface_list_lock, flags);

	list_for_each_entry(sf, &vdis->surface_list, head) {
		__surface_gfx_update(sf, x, y, w, h);
	}

	vmm_spin_unlock_irqrestore(&vdis->surface_list_lock, flags);
}
VMM_EXPORT_SYMBOL(vmm_vdisplay_surface_gfx_update);

static void __surface_gfx_resize(struct vmm_surface *s, int w, int h)
{
	w = max(w, 0);
	h = max(h, 0);

	if (s->ops && s->ops->gfx_resize) {
		s->ops->gfx_resize(s, w, h);
	}
}

void vmm_vdisplay_surface_gfx_resize(struct vmm_vdisplay *vdis,
				     int w, int h)
{
	irq_flags_t flags;
	struct vmm_surface *sf;

	if (!vdis) {
		return;
	}

	vmm_spin_lock_irqsave(&vdis->surface_list_lock, flags);

	list_for_each_entry(sf, &vdis->surface_list, head) {
		__surface_gfx_resize(sf, w, h);
	}

	vmm_spin_unlock_irqrestore(&vdis->surface_list_lock, flags);
}
VMM_EXPORT_SYMBOL(vmm_vdisplay_surface_gfx_resize);

static void __surface_gfx_copy(struct vmm_surface *s,
				int src_x, int src_y,
				int dst_x, int dst_y,
				int w, int h)
{
	int src_w, src_h, dst_w, dst_h;
	int width = vmm_surface_width(s);
	int height = vmm_surface_height(s);

	src_x = max(src_x, 0);
	src_y = max(src_y, 0);
	src_x = min(src_x, width);
	src_y = min(src_y, height);
	src_w = min(w, width - src_x);
	src_h = min(h, height - src_y);

	dst_x = max(dst_x, 0);
	dst_y = max(dst_y, 0);
	dst_x = min(dst_x, width);
	dst_y = min(dst_y, height);
	dst_w = min(w, width - dst_x);
	dst_h = min(h, height - dst_y);

	w = min(src_w, dst_w);
	h = min(src_h, dst_h);

	if (s->ops && s->ops->gfx_copy) {
		s->ops->gfx_copy(s, src_x, src_y, dst_x, dst_y, w, h);
	} else if (s->ops && s->ops->gfx_update) {
		 /* FIXME: */
		s->ops->gfx_update(s, dst_x, dst_y, w, h);
	}
}

void vmm_vdisplay_surface_gfx_copy(struct vmm_vdisplay *vdis, 
				   int src_x, int src_y,
				   int dst_x, int dst_y,
				   int w, int h)
{
	irq_flags_t flags;
	struct vmm_surface *sf;

	if (!vdis) {
		return;
	}

	vmm_spin_lock_irqsave(&vdis->surface_list_lock, flags);

	list_for_each_entry(sf, &vdis->surface_list, head) {
		__surface_gfx_copy(sf, src_x, src_y, dst_x, dst_y, w, h);
	}

	vmm_spin_unlock_irqrestore(&vdis->surface_list_lock, flags);
}
VMM_EXPORT_SYMBOL(vmm_vdisplay_surface_gfx_copy);

static void __surface_text_clear(struct vmm_surface *s)
{
	if (s->ops && s->ops->text_clear) {
		s->ops->text_clear(s);
	}
}

void vmm_vdisplay_surface_text_clear(struct vmm_vdisplay *vdis)
{
	irq_flags_t flags;
	struct vmm_surface *sf;

	if (!vdis) {
		return;
	}

	vmm_spin_lock_irqsave(&vdis->surface_list_lock, flags);

	list_for_each_entry(sf, &vdis->surface_list, head) {
		__surface_text_clear(sf);
	}

	vmm_spin_unlock_irqrestore(&vdis->surface_list_lock, flags);
}
VMM_EXPORT_SYMBOL(vmm_vdisplay_surface_text_clear);

static void __surface_text_cursor(struct vmm_surface *s, int x, int y)
{
	if (s->ops && s->ops->text_cursor) {
		s->ops->text_cursor(s, x, y);
	}
}

void vmm_vdisplay_surface_text_cursor(struct vmm_vdisplay *vdis,
				      int x, int y)
{
	irq_flags_t flags;
	struct vmm_surface *sf;

	if (!vdis) {
		return;
	}

	vmm_spin_lock_irqsave(&vdis->surface_list_lock, flags);

	list_for_each_entry(sf, &vdis->surface_list, head) {
		__surface_text_cursor(sf, x, y);
	}

	vmm_spin_unlock_irqrestore(&vdis->surface_list_lock, flags);
}
VMM_EXPORT_SYMBOL(vmm_vdisplay_surface_text_cursor);

static void __surface_text_update(struct vmm_surface *s,
				  int x, int y, int w, int h)
{
	if (s->ops && s->ops->text_update) {
		s->ops->text_update(s, x, y, w, h);
	}
}

void vmm_vdisplay_surface_text_update(struct vmm_vdisplay *vdis,
				      int x, int y, int w, int h)
{
	irq_flags_t flags;
	struct vmm_surface *sf;

	if (!vdis) {
		return;
	}

	vmm_spin_lock_irqsave(&vdis->surface_list_lock, flags);

	list_for_each_entry(sf, &vdis->surface_list, head) {
		__surface_text_update(sf, x, y, w, h);
	}

	vmm_spin_unlock_irqrestore(&vdis->surface_list_lock, flags);
}
VMM_EXPORT_SYMBOL(vmm_vdisplay_surface_text_update);

static void __surface_text_resize(struct vmm_surface *s, int w, int h)
{
	if (s->ops && s->ops->text_resize) {
		s->ops->text_resize(s, w, h);
	}
}

void vmm_vdisplay_surface_text_resize(struct vmm_vdisplay *vdis,
				      int w, int h)
{
	irq_flags_t flags;
	struct vmm_surface *sf;

	if (!vdis) {
		return;
	}

	vmm_spin_lock_irqsave(&vdis->surface_list_lock, flags);

	list_for_each_entry(sf, &vdis->surface_list, head) {
		__surface_text_resize(sf, w, h);
	}

	vmm_spin_unlock_irqrestore(&vdis->surface_list_lock, flags);
}
VMM_EXPORT_SYMBOL(vmm_vdisplay_surface_text_resize);

int vmm_vdisplay_add_surface(struct vmm_vdisplay *vdis,
			     struct vmm_surface *s)
{
	bool found;
	irq_flags_t flags;
	struct vmm_surface *sf;

	if (!vdis || !s) {
		return VMM_EINVALID;
	}

	vmm_spin_lock_irqsave(&vdis->surface_list_lock, flags);

	sf = NULL;
	found = FALSE;
	list_for_each_entry(sf, &vdis->surface_list, head) {
		if (strncmp(s->name, sf->name, sizeof(s->name)) == 0) {
			found = TRUE;
			break;
		}
	}
	if (found) {
		vmm_spin_unlock_irqrestore(&vdis->surface_list_lock, flags);
		return VMM_EEXIST;
	}

	INIT_LIST_HEAD(&s->head);
	list_add_tail(&s->head, &vdis->surface_list);

	vmm_spin_unlock_irqrestore(&vdis->surface_list_lock, flags);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_vdisplay_add_surface);

int vmm_vdisplay_del_surface(struct vmm_vdisplay *vdis,
			     struct vmm_surface *s)
{
	bool found;
	irq_flags_t flags;
	struct vmm_surface *sf;

	if (!vdis || !s) {
		return VMM_EINVALID;
	}

	vmm_spin_lock_irqsave(&vdis->surface_list_lock, flags);

	sf = NULL;
	found = FALSE;
	list_for_each_entry(sf, &vdis->surface_list, head) {
		if (strncmp(s->name, sf->name, sizeof(s->name)) == 0) {
			found = TRUE;
			break;
		}
	}
	if (!found) {
		vmm_spin_unlock_irqrestore(&vdis->surface_list_lock, flags);
		return VMM_ENOTAVAIL;
	}

	list_del(&sf->head);

	vmm_spin_unlock_irqrestore(&vdis->surface_list_lock, flags);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_vdisplay_del_surface);

struct vmm_vdisplay *vmm_vdisplay_create(const char *name,
					 const struct vmm_vdisplay_ops *ops,
					 void *priv)
{
	bool found;
	struct vmm_vdisplay *vdis;
	struct vmm_vdisplay_event event;

	if (!name || !ops) {
		return NULL;
	}

	vdis = NULL;
	found = FALSE;

	vmm_mutex_lock(&vdctrl.vdis_list_lock);

	list_for_each_entry(vdis, &vdctrl.vdis_list, head) {
		if (strcmp(name, vdis->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		vmm_mutex_unlock(&vdctrl.vdis_list_lock);
		return NULL;
	}

	vdis = vmm_malloc(sizeof(struct vmm_vdisplay));
	if (!vdis) {
		vmm_mutex_unlock(&vdctrl.vdis_list_lock);
		return NULL;
	}

	INIT_LIST_HEAD(&vdis->head);
	if (strlcpy(vdis->name, name, sizeof(vdis->name)) >=
	    sizeof(vdis->name)) {
		vmm_free(vdis);
		vmm_mutex_unlock(&vdctrl.vdis_list_lock);
		return NULL;
	}
	INIT_SPIN_LOCK(&vdis->surface_list_lock);
	INIT_LIST_HEAD(&vdis->surface_list);
	vdis->ops = ops;
	vdis->priv = priv;

	list_add_tail(&vdis->head, &vdctrl.vdis_list);

	vmm_mutex_unlock(&vdctrl.vdis_list_lock);

	/* Broadcast create event */
	event.data = vdis;
	vmm_blocking_notifier_call(&vdctrl.notifier_chain, 
				   VMM_VDISPLAY_EVENT_CREATE, 
				   &event);

	return vdis;
}
VMM_EXPORT_SYMBOL(vmm_vdisplay_create);

int vmm_vdisplay_destroy(struct vmm_vdisplay *vdis)
{
	bool found;
	irq_flags_t flags;
	struct vmm_surface *sf;
	struct vmm_vdisplay *vd;
	struct vmm_vdisplay_event event;

	if (!vdis) {
		return VMM_EFAIL;
	}

	/* Broadcast destroy event */
	event.data = vdis;
	vmm_blocking_notifier_call(&vdctrl.notifier_chain, 
				   VMM_VDISPLAY_EVENT_DESTROY, 
				   &event);

	vmm_spin_lock_irqsave(&vdis->surface_list_lock, flags);
	while (!list_empty(&vdis->surface_list)) {
		sf = list_first_entry(&vdis->surface_list,
					struct vmm_surface, head);
		list_del(&sf->head);
	}
	vmm_spin_unlock_irqrestore(&vdis->surface_list_lock, flags);

	vmm_mutex_lock(&vdctrl.vdis_list_lock);

	if (list_empty(&vdctrl.vdis_list)) {
		vmm_mutex_unlock(&vdctrl.vdis_list_lock);
		return VMM_EFAIL;
	}

	vd = NULL;
	found = FALSE;
	list_for_each_entry(vd, &vdctrl.vdis_list, head) {
		if (strcmp(vd->name, vdis->name) == 0) {
			found = TRUE;
			break;
		}
	}
	if (!found) {
		vmm_mutex_unlock(&vdctrl.vdis_list_lock);
		return VMM_ENOTAVAIL;
	}

	list_del(&vd->head);
	vmm_free(vd);

	vmm_mutex_unlock(&vdctrl.vdis_list_lock);

	return VMM_OK;
}
VMM_EXPORT_SYMBOL(vmm_vdisplay_destroy);

struct vmm_vdisplay *vmm_vdisplay_find(const char *name)
{
	bool found;
	struct vmm_vdisplay *vd;

	if (!name) {
		return NULL;
	}

	found = FALSE;
	vd = NULL;

	vmm_mutex_lock(&vdctrl.vdis_list_lock);

	list_for_each_entry(vd, &vdctrl.vdis_list, head) {
		if (strcmp(vd->name, name) == 0) {
			found = TRUE;
			break;
		}
	}

	vmm_mutex_unlock(&vdctrl.vdis_list_lock);

	if (!found) {
		return NULL;
	}

	return vd;
}
VMM_EXPORT_SYMBOL(vmm_vdisplay_find);

struct vmm_vdisplay *vmm_vdisplay_get(int index)
{
	bool found;
	struct vmm_vdisplay *vd;

	if (index < 0) {
		return NULL;
	}

	vd = NULL;
	found = FALSE;

	vmm_mutex_lock(&vdctrl.vdis_list_lock);

	list_for_each_entry(vd, &vdctrl.vdis_list, head) {
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	vmm_mutex_unlock(&vdctrl.vdis_list_lock);

	if (!found) {
		return NULL;
	}

	return vd;
}
VMM_EXPORT_SYMBOL(vmm_vdisplay_get);

u32 vmm_vdisplay_count(void)
{
	u32 retval = 0;
	struct vmm_vdisplay *vd;

	vmm_mutex_lock(&vdctrl.vdis_list_lock);

	list_for_each_entry(vd, &vdctrl.vdis_list, head) {
		retval++;
	}

	vmm_mutex_unlock(&vdctrl.vdis_list_lock);

	return retval;
}
VMM_EXPORT_SYMBOL(vmm_vdisplay_count);

static int __init vmm_vdisplay_init(void)
{
	memset(&vdctrl, 0, sizeof(vdctrl));

	INIT_MUTEX(&vdctrl.vdis_list_lock);
	INIT_LIST_HEAD(&vdctrl.vdis_list);
	BLOCKING_INIT_NOTIFIER_CHAIN(&vdctrl.notifier_chain);

	return VMM_OK;
}

static void __exit vmm_vdisplay_exit(void)
{
	/* Nothing to do here. */
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);

