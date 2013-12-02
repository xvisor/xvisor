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
 * @file input-mt.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Input Multitouch Library
 *
 * The source has been largely adapted from Linux 3.x or higher:
 * drivers/input/input-mt.c
 *
 * Copyright (c) 2008-2010 Henrik Rydberg
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <drv/input/mt.h>

#define TRKID_SGN	((TRKID_MAX + 1) >> 1)

int input_mt_init_slots(struct input_dev *idev, unsigned int num_slots)
{
	int i;

	if (!num_slots)
		return 0;
	if (idev->mt)
		return idev->mtsize != num_slots ? VMM_EINVALID : 0;

	idev->mt = vmm_malloc(num_slots * sizeof(struct input_mt_slot));
	if (!idev->mt)
		return VMM_ENOMEM;

	idev->mtsize = num_slots;
	input_set_abs_params(idev, ABS_MT_SLOT, 0, num_slots - 1, 0, 0);
	input_set_abs_params(idev, ABS_MT_TRACKING_ID, 0, TRKID_MAX, 0, 0);
	input_set_events_per_packet(idev, 6 * num_slots);

	/* Mark slots as 'unused' */
	for (i = 0; i < num_slots; i++)
		input_mt_set_value(&idev->mt[i], ABS_MT_TRACKING_ID, -1);

	return 0;
}
VMM_EXPORT_SYMBOL(input_mt_init_slots);

void input_mt_destroy_slots(struct input_dev *idev)
{
	vmm_free(idev->mt);
	idev->mt = NULL;
	idev->mtsize = 0;
	idev->slot = 0;
	idev->trkid = 0;
}
VMM_EXPORT_SYMBOL(input_mt_destroy_slots);

void input_mt_report_slot_state(struct input_dev *idev,
				    unsigned int tool_type, bool active)
{
	struct input_mt_slot *mt;
	int id;

	if (!idev->mt || !active) {
		input_event(idev, EV_ABS, ABS_MT_TRACKING_ID, -1);
		return;
	}

	mt = &idev->mt[idev->slot];
	id = input_mt_get_value(mt, ABS_MT_TRACKING_ID);
	if (id < 0 || input_mt_get_value(mt, ABS_MT_TOOL_TYPE) != tool_type)
		id = input_mt_new_trkid(idev);

	input_event(idev, EV_ABS, ABS_MT_TRACKING_ID, id);
	input_event(idev, EV_ABS, ABS_MT_TOOL_TYPE, tool_type);
}
VMM_EXPORT_SYMBOL(input_mt_report_slot_state);

void input_mt_report_finger_count(struct input_dev *idev, int count)
{
	input_event(idev, EV_KEY, BTN_TOOL_FINGER, count == 1);
	input_event(idev, EV_KEY, BTN_TOOL_DOUBLETAP, count == 2);
	input_event(idev, EV_KEY, BTN_TOOL_TRIPLETAP, count == 3);
	input_event(idev, EV_KEY, BTN_TOOL_QUADTAP, count == 4);
}
VMM_EXPORT_SYMBOL(input_mt_report_finger_count);

void input_mt_report_pointer_emulation(struct input_dev *idev, 
					   bool use_count)
{
	struct input_mt_slot *oldest = 0;
	int oldid = idev->trkid;
	int count = 0;
	int i;

	for (i = 0; i < idev->mtsize; ++i) {
		struct input_mt_slot *ps = &idev->mt[i];
		int id = input_mt_get_value(ps, ABS_MT_TRACKING_ID);

		if (id < 0)
			continue;
		if ((id - oldid) & TRKID_SGN) {
			oldest = ps;
			oldid = id;
		}
		count++;
	}

	input_event(idev, EV_KEY, BTN_TOUCH, count > 0);
	if (use_count) {
		input_mt_report_finger_count(idev, count);
	}

	if (oldest) {
		int x = input_mt_get_value(oldest, ABS_MT_POSITION_X);
		int y = input_mt_get_value(oldest, ABS_MT_POSITION_Y);
		int p = input_mt_get_value(oldest, ABS_MT_PRESSURE);

		input_event(idev, EV_ABS, ABS_X, x);
		input_event(idev, EV_ABS, ABS_Y, y);
		input_event(idev, EV_ABS, ABS_PRESSURE, p);
	} else {
		input_event(idev, EV_ABS, ABS_PRESSURE, 0);
	}
}
VMM_EXPORT_SYMBOL(input_mt_report_pointer_emulation);

