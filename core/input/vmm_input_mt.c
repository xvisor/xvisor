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
 * @file vmm_input_mt.c
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
#include <input/vmm_input_mt.h>

#define TRKID_SGN	((TRKID_MAX + 1) >> 1)

int vmm_input_mt_init_slots(struct vmm_input_dev *idev, unsigned int num_slots)
{
	int i;

	if (!num_slots)
		return 0;
	if (idev->mt)
		return idev->mtsize != num_slots ? VMM_EINVALID : 0;

	idev->mt = vmm_malloc(num_slots * sizeof(struct vmm_input_mt_slot));
	if (!idev->mt)
		return VMM_ENOMEM;

	idev->mtsize = num_slots;
	vmm_input_set_abs_params(idev, ABS_MT_SLOT, 0, num_slots - 1, 0, 0);
	vmm_input_set_abs_params(idev, ABS_MT_TRACKING_ID, 0, TRKID_MAX, 0, 0);
	vmm_input_set_events_per_packet(idev, 6 * num_slots);

	/* Mark slots as 'unused' */
	for (i = 0; i < num_slots; i++)
		vmm_input_mt_set_value(&idev->mt[i], ABS_MT_TRACKING_ID, -1);

	return 0;
}
VMM_EXPORT_SYMBOL(vmm_input_mt_init_slots);

void vmm_input_mt_destroy_slots(struct vmm_input_dev *idev)
{
	vmm_free(idev->mt);
	idev->mt = NULL;
	idev->mtsize = 0;
	idev->slot = 0;
	idev->trkid = 0;
}
VMM_EXPORT_SYMBOL(vmm_input_mt_destroy_slots);

void vmm_input_mt_report_slot_state(struct vmm_input_dev *idev,
				    unsigned int tool_type, bool active)
{
	struct vmm_input_mt_slot *mt;
	int id;

	if (!idev->mt || !active) {
		vmm_input_event(idev, EV_ABS, ABS_MT_TRACKING_ID, -1);
		return;
	}

	mt = &idev->mt[idev->slot];
	id = vmm_input_mt_get_value(mt, ABS_MT_TRACKING_ID);
	if (id < 0 || vmm_input_mt_get_value(mt, ABS_MT_TOOL_TYPE) != tool_type)
		id = vmm_input_mt_new_trkid(idev);

	vmm_input_event(idev, EV_ABS, ABS_MT_TRACKING_ID, id);
	vmm_input_event(idev, EV_ABS, ABS_MT_TOOL_TYPE, tool_type);
}
VMM_EXPORT_SYMBOL(vmm_input_mt_report_slot_state);

void vmm_input_mt_report_finger_count(struct vmm_input_dev *idev, int count)
{
	vmm_input_event(idev, EV_KEY, BTN_TOOL_FINGER, count == 1);
	vmm_input_event(idev, EV_KEY, BTN_TOOL_DOUBLETAP, count == 2);
	vmm_input_event(idev, EV_KEY, BTN_TOOL_TRIPLETAP, count == 3);
	vmm_input_event(idev, EV_KEY, BTN_TOOL_QUADTAP, count == 4);
}
VMM_EXPORT_SYMBOL(vmm_input_mt_report_finger_count);

void vmm_input_mt_report_pointer_emulation(struct vmm_input_dev *idev, 
					   bool use_count)
{
	struct vmm_input_mt_slot *oldest = 0;
	int oldid = idev->trkid;
	int count = 0;
	int i;

	for (i = 0; i < idev->mtsize; ++i) {
		struct vmm_input_mt_slot *ps = &idev->mt[i];
		int id = vmm_input_mt_get_value(ps, ABS_MT_TRACKING_ID);

		if (id < 0)
			continue;
		if ((id - oldid) & TRKID_SGN) {
			oldest = ps;
			oldid = id;
		}
		count++;
	}

	vmm_input_event(idev, EV_KEY, BTN_TOUCH, count > 0);
	if (use_count) {
		vmm_input_mt_report_finger_count(idev, count);
	}

	if (oldest) {
		int x = vmm_input_mt_get_value(oldest, ABS_MT_POSITION_X);
		int y = vmm_input_mt_get_value(oldest, ABS_MT_POSITION_Y);
		int p = vmm_input_mt_get_value(oldest, ABS_MT_PRESSURE);

		vmm_input_event(idev, EV_ABS, ABS_X, x);
		vmm_input_event(idev, EV_ABS, ABS_Y, y);
		vmm_input_event(idev, EV_ABS, ABS_PRESSURE, p);
	} else {
		vmm_input_event(idev, EV_ABS, ABS_PRESSURE, 0);
	}
}
VMM_EXPORT_SYMBOL(vmm_input_mt_report_pointer_emulation);

