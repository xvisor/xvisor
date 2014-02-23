/**
 * Copyright (c) 2012 Anup Patel.
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
 * @file mt.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Input Multitouch Library header
 *
 * The source has been largely adapted from Linux 3.x or higher:
 * include/linux/input/mt.h
 *
 * Copyright (c) 2010 Henrik Rydberg
 *
 * The original code is licensed under the GPL.
 */

#ifndef __DRV_INPUT_MT_H_
#define __DRV_INPUT_MT_H_

#include <drv/input.h>

#define TRKID_MAX	0xffff

#define INPUT_MT_POINTER	0x0001	/* pointer device, e.g. trackpad */
#define INPUT_MT_DIRECT		0x0002	/* direct device, e.g. touchscreen */
#define INPUT_MT_DROP_UNUSED	0x0004	/* drop contacts not seen in frame */
#define INPUT_MT_TRACK		0x0008	/* use in-kernel tracking */

/**
 * struct input_mt_slot - represents the state of an input MT slot
 * @abs: holds current values of ABS_MT axes for this slot
 * @frame: last frame at which input_mt_report_slot_state() was called
 * @key: optional driver designation of this slot
 */
struct input_mt_slot {
	int abs[ABS_MT_LAST - ABS_MT_FIRST + 1];
	unsigned int frame;
	unsigned int key;
};

/**
 * struct input_mt - state of tracked contacts
 * @trkid: stores MT tracking ID for the next contact
 * @num_slots: number of MT slots the device uses
 * @slot: MT slot currently being transmitted
 * @flags: input_mt operation flags
 * @frame: increases every time input_mt_sync_frame() is called
 * @red: reduced cost matrix for in-kernel tracking
 * @slots: array of slots holding current values of tracked contacts
 */
struct input_mt {
	int trkid;
	int num_slots;
	int slot;
	unsigned int flags;
	unsigned int frame;
	int *red;
	struct input_mt_slot slots[];
};

static inline void input_mt_set_value(struct input_mt_slot *slot,
				      unsigned code, int value)
{
	slot->abs[code - ABS_MT_FIRST] = value;
}

static inline int input_mt_get_value(const struct input_mt_slot *slot,
				     unsigned code)
{
	return slot->abs[code - ABS_MT_FIRST];
}

static inline bool input_mt_is_active(const struct input_mt_slot *slot)
{
	return input_mt_get_value(slot, ABS_MT_TRACKING_ID) >= 0;
}

/**
 * Initialize MT input slots
 * @dev: input device supporting MT events and finger tracking
 * @num_slots: number of slots used by the device
 * @flags: mt tasks to handle in core
 *
 * This function allocates all necessary memory for MT slot handling
 * in the input device, prepares the ABS_MT_SLOT and
 * ABS_MT_TRACKING_ID events for use and sets up appropriate buffers.
 * Depending on the flags set, it also performs pointer emulation and
 * frame synchronization.
 *
 * May be called repeatedly. Returns VMM_EINVAL if attempting to
 * reinitialize with a different number of slots.
 */
int input_mt_init_slots(struct input_dev *dev, unsigned int num_slots,
			unsigned int flags);

/**
 * Frees the MT slots of the input device
 * @dev: input device with allocated MT slots
 *
 * This function is only needed in error path as the input core will
 * automatically free the MT slots when the device is destroyed.
 */
void input_mt_destroy_slots(struct input_dev *dev);

static inline int input_mt_new_trkid(struct input_mt *mt)
{
	return mt->trkid++ & TRKID_MAX;
}

static inline void input_mt_slot(struct input_dev *dev, int slot)
{
	input_event(dev, EV_ABS, ABS_MT_SLOT, slot);
}

static inline bool input_is_mt_value(int axis)
{
	return axis >= ABS_MT_FIRST && axis <= ABS_MT_LAST;
}

static inline bool input_is_mt_axis(int axis)
{
	return axis == ABS_MT_SLOT || input_is_mt_value(axis);
}

/**
 * Report contact state
 * @dev: input device with allocated MT slots
 * @tool_type: the tool type to use in this slot
 * @active: true if contact is active, false otherwise
 *
 * Reports a contact via ABS_MT_TRACKING_ID, and optionally
 * ABS_MT_TOOL_TYPE. If active is true and the slot is currently
 * inactive, or if the tool type is changed, a new tracking id is
 * assigned to the slot. The tool type is only reported if the
 * corresponding absbit field is set.
 */
void input_mt_report_slot_state(struct input_dev *dev,
				unsigned int tool_type, bool active);

/**
 * Report contact count
 * @dev: input device with allocated MT slots
 * @count: the number of contacts
 *
 * Reports the contact count via BTN_TOOL_FINGER, BTN_TOOL_DOUBLETAP,
 * BTN_TOOL_TRIPLETAP and BTN_TOOL_QUADTAP.
 *
 * The input core ensures only the KEY events already setup for
 * this device will produce output.
 */
void input_mt_report_finger_count(struct input_dev *dev, int count);

/**
 * Common pointer emulation
 * @dev: input device with allocated MT slots
 * @use_count: report number of active contacts as finger count
 *
 * Performs legacy pointer emulation via BTN_TOUCH, ABS_X, ABS_Y and
 * ABS_PRESSURE. Touchpad finger count is emulated if use_count is true.
 *
 * The input core ensures only the KEY and ABS axes already setup for
 * this device will produce output.
 */
void input_mt_report_pointer_emulation(struct input_dev *dev, 
					bool use_count);

void input_mt_sync_frame(struct input_dev *dev);

/**
 * struct input_mt_pos - contact position
 * @x: horizontal coordinate
 * @y: vertical coordinate
 */
struct input_mt_pos {
	s16 x, y;
};

/**
 * Perform a best-match assignment
 * @dev: input device with allocated MT slots
 * @slots: the slot assignment to be filled
 * @pos: the position array to match
 * @num_pos: number of positions
 *
 * Performs a best match against the current contacts and returns
 * the slot assignment list. New contacts are assigned to unused
 * slots.
 *
 * Returns zero on success, or negative error in case of failure.
 */
int input_mt_assign_slots(struct input_dev *dev, int *slots,
			  const struct input_mt_pos *pos, int num_pos);

/**
 * Return slot matching key
 * @dev: input device with allocated MT slots
 * @key: the key of the sought slot
 *
 * Returns the slot of the given key, if it exists, otherwise
 * set the key on the first unused slot and return.
 *
 * If no available slot can be found, -1 is returned.
 */
int input_mt_get_slot_by_key(struct input_dev *dev, int key);

#endif /* __DRV_INPUT_MT_H_ */
