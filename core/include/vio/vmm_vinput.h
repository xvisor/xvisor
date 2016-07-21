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
 * @file vmm_vinput.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for virtual input subsystem
 */

#ifndef __VMM_VINPUT_H_
#define __VMM_VINPUT_H_

#include <vmm_limits.h>
#include <vmm_types.h>
#include <vmm_spinlocks.h>
#include <vmm_notifier.h>
#include <libs/list.h>

#define VMM_VINPUT_IPRIORITY			0

/* Notifier event when virtual keyboard is created */
#define VMM_VINPUT_EVENT_CREATE_KEYBOARD	0x01
/* Notifier event when virtual keyboard is destroyed */
#define VMM_VINPUT_EVENT_DESTROY_KEYBOARD	0x02
/* Notifier event when virtual mouse is created */
#define VMM_VINPUT_EVENT_CREATE_MOUSE		0x03
/* Notifier event when virtual mouse is destroyed */
#define VMM_VINPUT_EVENT_DESTROY_MOUSE		0x04

/** Representation of virtual input notifier event */
struct vmm_vinput_event {
	void *data;
};

/** Register a notifier client to receive virtual input events */
int vmm_vinput_register_client(struct vmm_notifier_block *nb);

/** Unregister a notifier client to not receive virtual input events */
int vmm_vinput_unregister_client(struct vmm_notifier_block *nb);

/* Keyboard LED bits */
#define VMM_SCROLL_LOCK_LED (1 << 0)
#define VMM_NUM_LOCK_LED    (1 << 1)
#define VMM_CAPS_LOCK_LED   (1 << 2)

struct vmm_vkeyboard;

/** Representation of a virtual keyboard */
struct vmm_vkeyboard_led_handler {
	struct dlist head;
	void (*led_change) (struct vmm_vkeyboard *vkbd, 
			    int ledstate, void *priv);
	void *priv;
};

/** Representation of a virtual keyboard */
struct vmm_vkeyboard {
	struct dlist head;
	char name[VMM_FIELD_NAME_SIZE];
	vmm_spinlock_t ledstate_lock;
	int ledstate;
	struct dlist led_handler_list;
	void (*kbd_event) (struct vmm_vkeyboard *vkbd, int keycode);
	void *priv;
};

/** Create a virtual keyboard */
struct vmm_vkeyboard *vmm_vkeyboard_create(const char *name,
			void (*kbd_event) (struct vmm_vkeyboard *, int),
			void *priv);

/** Destroy a virtual keyboard */
int vmm_vkeyboard_destroy(struct vmm_vkeyboard *vkbd);

/** Retrive private context of virtual keyboard */
static inline void *vmm_vkeyboard_priv(struct vmm_vkeyboard *vkbd)
{
	return (vkbd) ? vkbd->priv : NULL;
}

/** Trigger virtual keyboard event */
int vmm_vkeyboard_event(struct vmm_vkeyboard *vkbd, int keycode);

/** Add led handler to a virtual keyboard */
int vmm_vkeyboard_add_led_handler(struct vmm_vkeyboard *vkbd,
		void (*led_change) (struct vmm_vkeyboard *, int, void *),
		void *priv);

/** Delete led handler from a virtual keyboard */
int vmm_vkeyboard_del_led_handler(struct vmm_vkeyboard *vkbd,
		void (*led_change) (struct vmm_vkeyboard *, int, void *),
		void *priv);

/** Set ledstate of virtual keyboard */
void vmm_vkeyboard_set_ledstate(struct vmm_vkeyboard *vkbd, int ledstate);

/** Get ledstate of virtual keyboard */
int vmm_vkeyboard_get_ledstate(struct vmm_vkeyboard *vkbd);

/** Find a virtual keyboard with given name */
struct vmm_vkeyboard *vmm_vkeyboard_find(const char *name);

/** Iterate over each virtual keyboard */
int vmm_vkeyboard_iterate(struct vmm_vkeyboard *start, void *data,
			  int (*fn)(struct vmm_vkeyboard *vkbd, void *data));

/** Count of available virtual keyboards */
u32 vmm_vkeyboard_count(void);

/* Mouse buttons */
#define VMM_MOUSE_LBUTTON	0x01
#define VMM_MOUSE_RBUTTON 	0x02
#define VMM_MOUSE_MBUTTON	0x04

/** Representation of a virtual mouse */
struct vmm_vmouse {
	struct dlist head;
	char name[VMM_FIELD_NAME_SIZE];
	bool absolute;
	u32 graphics_width;
	u32 graphics_height;
	u32 graphics_rotation;
	int abs_x;
	int abs_y;
	int abs_z;
	void (*mouse_event) (struct vmm_vmouse *vmou, 
			   int dx, int dy, int dz, int buttons_state);
	void *priv;
};

/** Create a virtual mouse */
struct vmm_vmouse *vmm_vmouse_create(const char *name,
				     bool absolute,
				     void (*mouse_event) (
						   struct vmm_vmouse *vmou, 
						   int dx, int dy, int dz,
						   int buttons_state),
				     void *priv);

/** Destroy a virtual mouse */
int vmm_vmouse_destroy(struct vmm_vmouse *vmou);

/** Retrive private context of virtual mouse */
static inline void *vmm_vmouse_priv(struct vmm_vmouse *vmou)
{
	return (vmou) ? vmou->priv : NULL;
}

/** Trigger virtual mouse event */
int vmm_vmouse_event(struct vmm_vmouse *vmou,
		     int dx, int dy, int dz, int buttons_state);

/** Reset virtual mouse */
void vmm_vmouse_reset(struct vmm_vmouse *vmou);

/** Get absolute X position of virtual mouse */
int vmm_vmouse_absolute_x(struct vmm_vmouse *vmou);

/** Get absolute Y position of virtual mouse */
int vmm_vmouse_absolute_y(struct vmm_vmouse *vmou);

/** Get absolute Z position of virtual mouse */
int vmm_vmouse_absolute_z(struct vmm_vmouse *vmou);

/** Check whether virtual mouse uses absolute positioning */
bool vmm_vmouse_is_absolute(struct vmm_vmouse *vmou);

/** Set graphics width for virtual mouse 
 *  Note: This is required for relative virtual mouse
 */
void vmm_vmouse_set_graphics_width(struct vmm_vmouse *vmou, u32 width);

/** Get graphics width for virtual mouse 
 *  Note: This is required for relative virtual mouse
 */
u32 vmm_vmouse_get_graphics_width(struct vmm_vmouse *vmou);

/** Set graphics height for virtual mouse 
 *  Note: This is required for relative virtual mouse
 */
void vmm_vmouse_set_graphics_height(struct vmm_vmouse *vmou, u32 height);

/** Get graphics height for virtual mouse 
 *  Note: This is required for relative virtual mouse
 */
u32 vmm_vmouse_get_graphics_height(struct vmm_vmouse *vmou);

/** Set graphics rotation angle for virtual mouse  */
void vmm_vmouse_set_graphics_rotation(struct vmm_vmouse *vmou, u32 rotation);

/** Get graphics rotation angle for virtual mouse */
u32 vmm_vmouse_get_graphics_rotation(struct vmm_vmouse *vmou);

/** Find a virtual mouse with given name */
struct vmm_vmouse *vmm_vmouse_find(const char *name);

/** Iterate over each virtual mouse */
int vmm_vmouse_iterate(struct vmm_vmouse *start, void *data,
		       int (*fn)(struct vmm_vmouse *vmou, void *data));

/** Count of available virtual mouses */
u32 vmm_vmouse_count(void);

#endif /* __VMM_VINPUT_H_ */

