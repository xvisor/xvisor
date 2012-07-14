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
 * @file vmm_input_mt.h
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

#ifndef __VMM_INPUT_MT_H_
#define __VMM_INPUT_MT_H_

#include <input/vmm_input.h>

#define TRKID_MAX	0xffff

/**
 * struct vmm_input_mt_slot - represents the state of an input MT slot
 * @abs: holds current values of ABS_MT axes for this slot
 */
struct vmm_input_mt_slot {
	int abs[ABS_MT_LAST - ABS_MT_FIRST + 1];
};

static inline void vmm_input_mt_set_value(struct vmm_input_mt_slot *slot,
				      unsigned code, int value)
{
	slot->abs[code - ABS_MT_FIRST] = value;
}

static inline int vmm_input_mt_get_value(const struct vmm_input_mt_slot *slot,
				     unsigned code)
{
	return slot->abs[code - ABS_MT_FIRST];
}

/**
 * Initialize MT input slots
 * @idev: input device supporting MT events and finger tracking
 * @num_slots: number of slots used by the device
 *
 * This function allocates all necessary memory for MT slot handling
 * in the input device, prepares the ABS_MT_SLOT and
 * ABS_MT_TRACKING_ID events for use and sets up appropriate buffers.
 * May be called repeatedly. Returns -EINVAL if attempting to
 * reinitialize with a different number of slots.
 */
int vmm_input_mt_init_slots(struct vmm_input_dev *idev, 
			    unsigned int num_slots);

/**
 * Frees the MT slots of the input device
 * @dev: input device with allocated MT slots
 *
 * This function is only needed in error path as the input core will
 * automatically free the MT slots when the device is destroyed.
 */
void vmm_input_mt_destroy_slots(struct vmm_input_dev *idev);

static inline int vmm_input_mt_new_trkid(struct vmm_input_dev *idev)
{
	return idev->trkid++ & TRKID_MAX;
}

static inline void vmm_input_mt_slot(struct vmm_input_dev *idev, int slot)
{
	vmm_input_event(idev, EV_ABS, ABS_MT_SLOT, slot);
}

static inline bool vmm_input_is_mt_axis(int axis)
{
	return axis == ABS_MT_SLOT ||
		(axis >= ABS_MT_FIRST && axis <= ABS_MT_LAST);
}

/**
 * Rreport contact state
 * @idev: input device with allocated MT slots
 * @tool_type: the tool type to use in this slot
 * @active: true if contact is active, false otherwise
 *
 * Reports a contact via ABS_MT_TRACKING_ID, and optionally
 * ABS_MT_TOOL_TYPE. If active is true and the slot is currently
 * inactive, or if the tool type is changed, a new tracking id is
 * assigned to the slot. The tool type is only reported if the
 * corresponding absbit field is set.
 */
void vmm_input_mt_report_slot_state(struct vmm_input_dev *idev,
				unsigned int tool_type, bool active);

/**
 * Report contact count
 * @idev: input device with allocated MT slots
 * @count: the number of contacts
 *
 * Reports the contact count via BTN_TOOL_FINGER, BTN_TOOL_DOUBLETAP,
 * BTN_TOOL_TRIPLETAP and BTN_TOOL_QUADTAP.
 *
 * The input core ensures only the KEY events already setup for
 * this device will produce output.
 */
void vmm_input_mt_report_finger_count(struct vmm_input_dev *idev, int count);

/**
 * Common pointer emulation
 * @idev: input device with allocated MT slots
 * @use_count: report number of active contacts as finger count
 *
 * Performs legacy pointer emulation via BTN_TOUCH, ABS_X, ABS_Y and
 * ABS_PRESSURE. Touchpad finger count is emulated if use_count is true.
 *
 * The input core ensures only the KEY and ABS axes already setup for
 * this device will produce output.
 */
void vmm_input_mt_report_pointer_emulation(struct vmm_input_dev *idev, 
					   bool use_count);

#endif /* __VMM_INPUT_MT_H_ */
