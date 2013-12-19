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
 * @file vmm_vdisplay.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for virtual display subsystem
 */

/* The virtual display subsystem has two important entites namely
 * vmm_vdisplay and vmm_surface.
 *
 * GUI rendering daemons (VNC daemon or FB daemon or ...) create 
 * vmm_surface instance and add/bind it to a vmm_vdisplay instance.
 * More than one GUI rendering daemons can add their vmm_surface
 * instances to a single vmm_vdisplay instance. The GUI rendering
 * daemons will also use vmm_vdisplay_one_update() API to periodically
 * update/sync vmm_surface instance with vmm_vdisplay instance.
 *
 * Display (or framebuffer) emulators create vmm_vdisplay instance
 * to emulate a virtual display. The display emulator will also use
 * vmm_vdisplay_surface_xxx() APIs to give hints to vmm_surface
 * instances about changes in virtual display.
 */

#ifndef __VMM_VDISPLAY_H_
#define __VMM_VDISPLAY_H_

#include <vmm_limits.h>
#include <vmm_types.h>
#include <vmm_notifier.h>
#include <libs/list.h>

#define VMM_VDISPLAY_IPRIORITY			0

/* Notifier event when virtual display is created */
#define VMM_VDISPLAY_EVENT_CREATE		0x01
/* Notifier event when virtual display is destroyed */
#define VMM_VDISPLAY_EVENT_DESTROY		0x02

/** Representation of virtual input notifier event */
struct vmm_vdisplay_event {
	void *data;
};

/** Register a notifier client to receive virtual display events */
int vmm_vdisplay_register_client(struct vmm_notifier_block *nb);

/** Unregister a notifier client to not receive virtual display events */
int vmm_vdisplay_unregister_client(struct vmm_notifier_block *nb);

/** Representation of a pixel format */
struct vmm_pixelformat {
	u8 bits_per_pixel;
	u8 bytes_per_pixel;
	u8 depth; /* color depth in bits */
	u32 rmask, gmask, bmask, amask;
	u8 rshift, gshift, bshift, ashift;
	u8 rmax, gmax, bmax, amax;
	u8 rbits, gbits, bbits, abits;
};

/** Default initialization for pixel format */
void vmm_pixelformat_init_default(struct vmm_pixelformat *pf, int bpp);

/** Default initialization with different endianness for pixel format */
void vmm_pixelformat_init_different_endian(struct vmm_pixelformat *pf, int bpp);

struct vmm_surface;

/** Representation of surface operations 
 *  Note: All surface operations are optional.
 *  Note: All surface operations are usually called with the
 *  'surface_list_lock' of the associated virtual display held
 *  hence, we cannot sleep in these operations.
 *  Note: Typically, all surface operations (except copyto_data
 *  and copyfrom_data) should be used to schedule a background or
 *  bottom-half work.
 */
struct vmm_surface_ops {
	void (*copyto_data)(struct vmm_surface *s,
			u32 dest_off, void *src, u32 len);
	void (*copyfrom_data)(struct vmm_surface *s,
			void *dest, u32 src_off, u32 len);

	void (*refresh)(struct vmm_surface *s);

	void (*gfx_update)(struct vmm_surface *s,
			   int x, int y, int w, int h);
	void (*gfx_resize)(struct vmm_surface *s, int w, int h);
	void (*gfx_copy)(struct vmm_surface *s,
			 int src_x, int src_y,
			 int dst_x, int dst_y,
			 int w, int h);

	void (*text_cursor)(struct vmm_surface *s,
			    int x, int y);
	void (*text_resize)(struct vmm_surface *s,
			    int w, int h);
	void (*text_update)(struct vmm_surface *s,
			    int x, int y, int w, int h);
};

#define VMM_SURFACE_BIG_ENDIAN_FLAG 		0x01
#define VMM_SURFACE_ALLOCED_FLAG		0x02

/** Representation of a surface */
struct vmm_surface {
	struct dlist head;
	char name[VMM_FIELD_NAME_SIZE];
	void *data;
	u32 data_size;
	int height;
	int width;
	u32 flags;
	struct vmm_pixelformat pf;
	const struct vmm_surface_ops *ops;
	void *priv;
};

/** Retrive private context of surface */
static inline void *vmm_surface_priv(struct vmm_surface *s)
{
	return (s) ? s->priv : NULL;
}

/** Initialize a surface */
int vmm_surface_init(struct vmm_surface *s,
		     const char *name,
		     void *data, u32 data_size,
		     int height, int width, u32 flags,
		     struct vmm_pixelformat *pf,
		     const struct vmm_surface_ops *ops,
		     void *priv);

/** Alloc a new surface */
struct vmm_surface *vmm_surface_alloc(const char *name,
				      void *data, u32 data_size,
				      int height, int width, u32 flags,
				      struct vmm_pixelformat *pf,
				      const struct vmm_surface_ops *ops,
				      void *priv);

/** Free an alloced surface */
void vmm_surface_free(struct vmm_surface *s);

/** Retrive row stride of given surface */
static inline int vmm_surface_stride(struct vmm_surface *s)
{
	return (s) ? s->width * s->pf.bytes_per_pixel : 0;
}

/** Retrive data pointer of given surface */
static inline void *vmm_surface_data(struct vmm_surface *s)
{
	return (s) ? s->data : NULL;
}

/** Retrive width of given surface */
static inline int vmm_surface_width(struct vmm_surface *s)
{
	return (s) ? s->width : 0;
}

/** Retrive height of given surface */
static inline int vmm_surface_height(struct vmm_surface *s)
{
	return (s) ? s->height : 0;
}

/** Retrive bits-per-pixel of given surface */
static inline int vmm_surface_bits_per_pixel(struct vmm_surface *s)
{
	return (s) ? s->pf.bits_per_pixel : 0;
}

/** Retrive bytes-per-pixel of given surface */
static inline int vmm_surface_bytes_per_pixel(struct vmm_surface *s)
{
	return (((s) ? s->pf.bits_per_pixel : 0) + 7) / 8;
}

struct vmm_vdisplay;

/** Representation of a virtual display operations */
struct vmm_vdisplay_ops {
	void (*invalidate)(struct vmm_vdisplay *vdis);
	void (*gfx_update)(struct vmm_vdisplay *vdis, struct vmm_surface *s);
	void (*text_update)(struct vmm_vdisplay *vdis, unsigned long *text);
};

/** Representation of a virtual display */
struct vmm_vdisplay {
	struct dlist head;
	char name[VMM_FIELD_NAME_SIZE];
	vmm_spinlock_t surface_list_lock;
	struct dlist surface_list;
	const struct vmm_vdisplay_ops *ops;
	void *priv;
};

/** Update a particular surface for given virtual display */
void vmm_vdisplay_one_update(struct vmm_vdisplay *vdis,
			     struct vmm_surface *s);

/** Update all surfaces for given virtual display */
void vmm_vdisplay_update(struct vmm_vdisplay *vdis);

/** Invalidate a given virtual display */
void vmm_vdisplay_invalidate(struct vmm_vdisplay *vdis);

/** Text update a given virtual display */
void vmm_vdisplay_text_update(struct vmm_vdisplay *vdis,
			      unsigned long *chardata);

/** Refresh all surfaces for given virtual display */
void vmm_vdisplay_surface_refresh(struct vmm_vdisplay *vdis);

/** Update all surfaces for given virtual display */
void vmm_vdisplay_surface_gfx_update(struct vmm_vdisplay *vdis,
				     int x, int y, int w, int h);

/** Resize all surfaces for given virtual display */
void vmm_vdisplay_surface_gfx_resize(struct vmm_vdisplay *vdis,
				     int w, int h);

/** Copy data on all surfaces for given virtual display */
void vmm_vdisplay_surface_gfx_copy(struct vmm_vdisplay *vdis, 
				   int src_x, int src_y,
				   int dst_x, int dst_y,
				   int w, int h);

/** Set text cursor on all surfaces for given virtual display */
void vmm_vdisplay_surface_text_cursor(struct vmm_vdisplay *vdis,
				      int x, int y);

/** Update text on all surfaces for given virtual display */
void vmm_vdisplay_surface_text_update(struct vmm_vdisplay *vdis,
				      int x, int y, int w, int h);

/** Resize text on all surfaces for given virtual display */
void vmm_vdisplay_surface_text_resize(struct vmm_vdisplay *vdis,
				      int w, int h);

/** Add surface to a virtual display */
int vmm_vdisplay_add_surface(struct vmm_vdisplay *vdis,
			     struct vmm_surface *s);

/** Delete surface from a virtual display */
int vmm_vdisplay_del_surface(struct vmm_vdisplay *vdis,
			     struct vmm_surface *s);

/** Create a virtual display */
struct vmm_vdisplay *vmm_vdisplay_create(const char *name,
					 const struct vmm_vdisplay_ops *ops,
					 void *priv);

/** Destroy a virtual display */
int vmm_vdisplay_destroy(struct vmm_vdisplay *vdis);

/** Retrive private context of virtual display */
static inline void *vmm_vdisplay_priv(struct vmm_vdisplay *vdis)
{
	return (vdis) ? vdis->priv : NULL;
}

/** Find a virtual display with given name */
struct vmm_vdisplay *vmm_vdisplay_find(const char *name);

/** Get a virtual display with given index */
struct vmm_vdisplay *vmm_vdisplay_get(int index);

/** Count of available virtual displays */
u32 vmm_vdisplay_count(void);

#endif /* __VMM_VDISPLAY_H_ */

