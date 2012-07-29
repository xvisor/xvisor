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
 * @file vmm_input.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Input device framework source
 *
 * The source has been largely adapted from Linux 3.x or higher:
 * drivers/input/input.c
 *
 * Copyright (c) 1999-2002 Vojtech Pavlik
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_string.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <input/vmm_input.h>
#include <input/vmm_input_mt.h>

#define MODULE_VARID			input_framework_module
#define MODULE_NAME			"Input Device Framework"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_IPRIORITY		VMM_INPUT_IPRIORITY
#define	MODULE_INIT			vmm_input_init
#define	MODULE_EXIT			vmm_input_exit

struct vmm_input_ctrl {
	vmm_spinlock_t dev_list_lock;
	struct dlist dev_list;
	vmm_spinlock_t hnd_list_lock;
	struct dlist hnd_list;
	vmm_spinlock_t hnd_conn_lock[EV_CNT];
	struct dlist hnd_conn[EV_CNT];
	u32 hnd_conn_count[EV_CNT];
};

static struct vmm_input_ctrl ictrl;

static inline int is_event_supported(unsigned int code,
				     unsigned long *bm, unsigned int max)
{
	return code <= max && test_bit(code, bm);
}

static int input_defuzz_abs_event(int value, int old_val, int fuzz)
{
	if (fuzz) {
		if (value > old_val - fuzz / 2 && value < old_val + fuzz / 2)
			return old_val;

		if (value > old_val - fuzz && value < old_val + fuzz)
			return (old_val * 3 + value) / 4;

		if (value > old_val - fuzz * 2 && value < old_val + fuzz * 2)
			return (old_val + value) / 2;
	}

	return value;
}

#define INPUT_DO_TOGGLE(dev, type, bits, on)				\
	do {								\
		int i;							\
		bool active;						\
									\
		if (!test_bit(EV_##type, dev->evbit))			\
			break;						\
									\
		for (i = 0; i < type##_MAX; i++) {			\
			if (!test_bit(i, dev->bits##bit))		\
				continue;				\
									\
			active = test_bit(i, dev->bits);		\
			if (!active && !on)				\
				continue;				\
									\
			dev->event(dev, EV_##type, i, on ? active : 0);	\
		}							\
	} while (0)

static void input_dev_toggle(struct vmm_input_dev *idev, bool activate)
{
	if (!idev->event)
		return;

	INPUT_DO_TOGGLE(idev, LED, led, activate);
	INPUT_DO_TOGGLE(idev, SND, snd, activate);

	if (activate && test_bit(EV_REP, idev->evbit)) {
		idev->event(idev, EV_REP, REP_PERIOD, idev->rep[REP_PERIOD]);
		idev->event(idev, EV_REP, REP_DELAY, idev->rep[REP_DELAY]);
	}
}

/* Pass event to all relevant input handlers. This function is called with
 * idev->event_lock held and interrupts disabled.
 */
static void input_pass_event(struct vmm_input_dev *idev,
			     unsigned int type, unsigned int code, int value)
{
	irq_flags_t flags;
	struct dlist *l;
	struct vmm_input_handler *handler;

	vmm_spin_lock_irqsave(&ictrl.hnd_conn_lock[type], flags);

	list_for_each(l, &ictrl.hnd_conn[type]) {
		handler = list_entry(l, struct vmm_input_handler, head);
		handler->event(handler, idev, type, code, value);
	}

	vmm_spin_unlock_irqrestore(&ictrl.hnd_conn_lock[type], flags);
}

/*
 * Generate software autorepeat event. Note that we take
 * idev->event_lock here to avoid racing with input_event
 * which may cause keys get "stuck".
 */
static void input_repeat_key(struct vmm_timer_event *ev)
{
	u64 duration;
	irq_flags_t flags;
	struct vmm_input_dev *idev = (void *)ev->priv;

	vmm_spin_lock_irqsave(&idev->event_lock, flags);

	if (test_bit(idev->repeat_key, idev->key) &&
	    is_event_supported(idev->repeat_key, idev->keybit, KEY_MAX)) {

		input_pass_event(idev, EV_KEY, idev->repeat_key, 2);

		if (idev->sync) {
			/*
			 * Only send SYN_REPORT if we are not in a middle
			 * of driver parsing a new hardware packet.
			 * Otherwise assume that the driver will send
			 * SYN_REPORT once it's done.
			 */
			input_pass_event(idev, EV_SYN, SYN_REPORT, 1);
		}

		if (idev->rep[REP_PERIOD]) {
			duration = idev->rep[REP_PERIOD];
			duration = duration * 1000000;
			vmm_timer_event_start(&idev->repeat_ev, duration);
		}
	}

	vmm_spin_unlock_irqrestore(&idev->event_lock, flags);
}

static void input_start_autorepeat(struct vmm_input_dev *idev, int code)
{
	u32 duration;
	if (test_bit(EV_REP, idev->evbit) &&
	    idev->rep[REP_PERIOD] && idev->rep[REP_DELAY] &&
	    idev->repeat_ev.priv) {
		idev->repeat_key = code;
		duration = idev->rep[REP_DELAY];
		duration = duration * 1000000;
		vmm_timer_event_start(&idev->repeat_ev, duration);
	}
}

static void input_stop_autorepeat(struct vmm_input_dev *idev)
{
	vmm_timer_event_stop(&idev->repeat_ev);
}

#define INPUT_IGNORE_EVENT	0
#define INPUT_PASS_TO_HANDLERS	1
#define INPUT_PASS_TO_DEVICE	2
#define INPUT_PASS_TO_ALL	(INPUT_PASS_TO_HANDLERS | INPUT_PASS_TO_DEVICE)

static int input_handle_abs_event(struct vmm_input_dev *idev,
				  unsigned int code, int *pval)
{
	bool is_mt_event;
	int *pold;

	if (code == ABS_MT_SLOT) {
		/*
		 * "Stage" the event; we'll flush it later, when we
		 * get actual touch data.
		 */
		if (*pval >= 0 && *pval < idev->mtsize)
			idev->slot = *pval;

		return INPUT_IGNORE_EVENT;
	}

	is_mt_event = code >= ABS_MT_FIRST && code <= ABS_MT_LAST;

	if (!is_mt_event) {
		pold = &idev->absinfo[code].value;
	} else if (idev->mt) {
		struct vmm_input_mt_slot *mtslot = &idev->mt[idev->slot];
		pold = &mtslot->abs[code - ABS_MT_FIRST];
	} else {
		/*
		 * Bypass filtering for multi-touch events when
		 * not employing slots.
		 */
		pold = NULL;
	}

	if (pold) {
		*pval = input_defuzz_abs_event(*pval, *pold,
						idev->absinfo[code].fuzz);
		if (*pold == *pval)
			return INPUT_IGNORE_EVENT;

		*pold = *pval;
	}

	/* Flush pending "slot" event */
	if (is_mt_event && idev->slot != vmm_input_abs_get_val(idev, ABS_MT_SLOT)) {
		vmm_input_abs_set_val(idev, ABS_MT_SLOT, idev->slot);
		input_pass_event(idev, EV_ABS, ABS_MT_SLOT, idev->slot);
	}

	return INPUT_PASS_TO_HANDLERS;
}

static void input_handle_event(struct vmm_input_dev *idev,
			       unsigned int type, unsigned int code, int value)
{
	int disposition = INPUT_IGNORE_EVENT;

	switch (type) {

	case EV_SYN:
		switch (code) {
		case SYN_CONFIG:
			disposition = INPUT_PASS_TO_ALL;
			break;

		case SYN_REPORT:
			if (!idev->sync) {
				idev->sync = TRUE;
				disposition = INPUT_PASS_TO_HANDLERS;
			}
			break;
		case SYN_MT_REPORT:
			idev->sync = FALSE;
			disposition = INPUT_PASS_TO_HANDLERS;
			break;
		}
		break;

	case EV_KEY:
		if (is_event_supported(code, idev->keybit, KEY_MAX) &&
		    !!test_bit(code, idev->key) != value) {

			if (value != 2) {
				__change_bit(code, idev->key);
				if (value)
					input_start_autorepeat(idev, code);
				else
					input_stop_autorepeat(idev);
			}

			disposition = INPUT_PASS_TO_HANDLERS;
		}
		break;

	case EV_SW:
		if (is_event_supported(code, idev->swbit, SW_MAX) &&
		    !!test_bit(code, idev->sw) != value) {

			disposition = INPUT_PASS_TO_HANDLERS;
		}
		break;

	case EV_ABS:
		if (is_event_supported(code, idev->absbit, ABS_MAX))
			disposition = input_handle_abs_event(idev, code, &value);

		break;

	case EV_REL:
		if (is_event_supported(code, idev->relbit, REL_MAX) && value)
			disposition = INPUT_PASS_TO_HANDLERS;

		break;

	case EV_MSC:
		if (is_event_supported(code, idev->mscbit, MSC_MAX))
			disposition = INPUT_PASS_TO_ALL;

		break;

	case EV_LED:
		if (is_event_supported(code, idev->ledbit, LED_MAX) &&
		    !!test_bit(code, idev->led) != value) {

			__change_bit(code, idev->led);
			disposition = INPUT_PASS_TO_ALL;
		}
		break;

	case EV_SND:
		if (is_event_supported(code, idev->sndbit, SND_MAX)) {

			if (!!test_bit(code, idev->snd) != !!value)
				__change_bit(code, idev->snd);
			disposition = INPUT_PASS_TO_ALL;
		}
		break;

	case EV_REP:
		if (code <= REP_MAX && value >= 0 && idev->rep[code] != value) {
			idev->rep[code] = value;
			disposition = INPUT_PASS_TO_ALL;
		}
		break;

	case EV_FF:
		if (value >= 0)
			disposition = INPUT_PASS_TO_ALL;
		break;

	case EV_PWR:
		disposition = INPUT_PASS_TO_ALL;
		break;
	};

	if (disposition != INPUT_IGNORE_EVENT && type != EV_SYN)
		idev->sync = FALSE;

	if ((disposition & INPUT_PASS_TO_DEVICE) && idev->event)
		idev->event(idev, type, code, value);

	if (disposition & INPUT_PASS_TO_HANDLERS)
		input_pass_event(idev, type, code, value);
}

void vmm_input_event(struct vmm_input_dev *idev, 
		     unsigned int type, unsigned int code, int value)
{
	irq_flags_t flags;

	if (is_event_supported(type, idev->evbit, EV_MAX)) {
		vmm_spin_lock_irqsave(&idev->event_lock, flags);
		input_handle_event(idev, type, code, value);
		vmm_spin_unlock_irqrestore(&idev->event_lock, flags);
	}
}

void vmm_input_set_capability(struct vmm_input_dev *idev, 
			      unsigned int type, unsigned int code)
{
	switch (type) {
	case EV_KEY:
		__set_bit(code, idev->keybit);
		break;

	case EV_REL:
		__set_bit(code, idev->relbit);
		break;

	case EV_ABS:
		__set_bit(code, idev->absbit);
		break;

	case EV_MSC:
		__set_bit(code, idev->mscbit);
		break;

	case EV_SW:
		__set_bit(code, idev->swbit);
		break;

	case EV_LED:
		__set_bit(code, idev->ledbit);
		break;

	case EV_SND:
		__set_bit(code, idev->sndbit);
		break;

	case EV_FF:
		__set_bit(code, idev->ffbit);
		break;

	case EV_PWR:
		/* do nothing */
		break;

	default:
		vmm_panic("%s: unknown type %u (code %u)\n", 
			   __func__, type, code);
		return;
	}

	__set_bit(type, idev->evbit);
}

int vmm_input_scancode_to_scalar(const struct vmm_input_keymap_entry *ke,
				 unsigned int *scancode)
{
	switch (ke->len) {
	case 1:
		*scancode = *((u8 *)ke->scancode);
		break;

	case 2:
		*scancode = *((u16 *)ke->scancode);
		break;

	case 4:
		*scancode = *((u32 *)ke->scancode);
		break;

	default:
		return VMM_EINVALID;
	}

	return VMM_OK;
}

static unsigned int input_fetch_keycode(struct vmm_input_dev *idev,
					unsigned int index)
{
	switch (idev->keycodesize) {
	case 1:
		return ((u8 *)idev->keycode)[index];

	case 2:
		return ((u16 *)idev->keycode)[index];

	default:
		return ((u32 *)idev->keycode)[index];
	}
}

static int input_default_getkeycode(struct vmm_input_dev *idev,
				    struct vmm_input_keymap_entry *ke)
{
	unsigned int index;
	int error;

	if (!idev->keycodesize) {
		return VMM_EINVALID;
	}

	if (ke->flags & INPUT_KEYMAP_BY_INDEX) {
		index = ke->index;
	} else {
		error = vmm_input_scancode_to_scalar(ke, &index);
		if (error) {
			return error;
		}
	}

	if (index >= idev->keycodemax)
		return VMM_EINVALID;

	ke->keycode = input_fetch_keycode(idev, index);
	ke->index = index;
	ke->len = sizeof(index);
	vmm_memcpy(ke->scancode, &index, sizeof(index));

	return 0;
}

static int input_default_setkeycode(struct vmm_input_dev *idev,
				    const struct vmm_input_keymap_entry *ke,
				    unsigned int *old_keycode)
{
	unsigned int index;
	int error;
	int i;

	if (!idev->keycodesize) {
		return VMM_EINVALID;
	}

	if (ke->flags & INPUT_KEYMAP_BY_INDEX) {
		index = ke->index;
	} else {
		error = vmm_input_scancode_to_scalar(ke, &index);
		if (error) {
			return error;
		}
	}

	if (index >= idev->keycodemax)
		return VMM_EINVALID;

	if (idev->keycodesize < sizeof(ke->keycode) &&
	    (ke->keycode >> (idev->keycodesize * 8)))
		return VMM_EINVALID;

	switch (idev->keycodesize) {
		case 1: {
			u8 *k = (u8 *)idev->keycode;
			*old_keycode = k[index];
			k[index] = ke->keycode;
			break;
		}
		case 2: {
			u16 *k = (u16 *)idev->keycode;
			*old_keycode = k[index];
			k[index] = ke->keycode;
			break;
		}
		default: {
			u32 *k = (u32 *)idev->keycode;
			*old_keycode = k[index];
			k[index] = ke->keycode;
			break;
		}
	}

	__clear_bit(*old_keycode, idev->keybit);
	__set_bit(ke->keycode, idev->keybit);

	for (i = 0; i < idev->keycodemax; i++) {
		if (input_fetch_keycode(idev, i) == *old_keycode) {
			__set_bit(*old_keycode, idev->keybit);
			break; /* Setting the bit twice is useless, so break */
		}
	}

	return 0;
}

void vmm_input_alloc_absinfo(struct vmm_input_dev *idev)
{
	if (!idev->absinfo) {
		idev->absinfo = 
			vmm_malloc(ABS_CNT * sizeof(struct vmm_input_absinfo));
	}

	BUG_ON(!idev->absinfo, "%s(): vmm_malloc() failed?\n", __func__);
}

void vmm_input_set_abs_params(struct vmm_input_dev *idev, unsigned int axis,
			  int min, int max, int fuzz, int flat)
{
	struct vmm_input_absinfo *absinfo;

	vmm_input_alloc_absinfo(idev);
	if (!idev->absinfo)
		return;

	absinfo = &idev->absinfo[axis];
	absinfo->minimum = min;
	absinfo->maximum = max;
	absinfo->fuzz = fuzz;
	absinfo->flat = flat;

	idev->absbit[BIT_WORD(axis)] |= BIT_MASK(axis);
}

int vmm_input_get_keycode(struct vmm_input_dev *idev, 
			  struct vmm_input_keymap_entry *ke)
{
	irq_flags_t flags;
	int rc;

	vmm_spin_lock_irqsave(&idev->event_lock, flags);
	rc = idev->getkeycode(idev, ke);
	vmm_spin_unlock_irqrestore(&idev->event_lock, flags);

	return rc;
}

int vmm_input_set_keycode(struct vmm_input_dev *idev,
			  const struct vmm_input_keymap_entry *ke)
{
	int rc;
	irq_flags_t flags;
	unsigned int old_keycode;

	if (ke->keycode > KEY_MAX)
		return VMM_EINVALID;

	vmm_spin_lock_irqsave(&idev->event_lock, flags);

	rc = idev->setkeycode(idev, ke, &old_keycode);
	if (rc) {
		goto out;
	}

	/* Make sure KEY_RESERVED did not get enabled. */
	__clear_bit(KEY_RESERVED, idev->keybit);

	/*
	 * Simulate keyup event if keycode is not present
	 * in the keymap anymore
	 */
	if (test_bit(EV_KEY, idev->evbit) &&
	    !is_event_supported(old_keycode, idev->keybit, KEY_MAX) &&
	    __test_and_clear_bit(old_keycode, idev->key)) {

		input_pass_event(idev, EV_KEY, old_keycode, 0);
		if (idev->sync) {
			input_pass_event(idev, EV_SYN, SYN_REPORT, 1);
		}
	}

 out:
	vmm_spin_unlock_irqrestore(&idev->event_lock, flags);

	return rc;
}

struct vmm_input_dev *vmm_input_alloc_device(void)
{
	struct vmm_input_dev *idev;

	idev = vmm_malloc(sizeof(struct vmm_input_dev));
	if (!idev) {
		return NULL;
	}

	vmm_memset(idev, 0, sizeof(struct vmm_input_dev));

	INIT_LIST_HEAD(&idev->head);
	INIT_SPIN_LOCK(&idev->event_lock);
	INIT_SPIN_LOCK(&idev->ops_lock);

	return idev;
}

void vmm_input_free_device(struct vmm_input_dev *idev)
{
	if (!idev) {
		vmm_free(idev);
	}
}

static unsigned int input_estimate_events_per_packet(struct vmm_input_dev *idev)
{
	int mt_slots;
	int i;
	unsigned int events;

	if (idev->mtsize) {
		mt_slots = idev->mtsize;
	} else if (test_bit(ABS_MT_TRACKING_ID, idev->absbit)) {
		mt_slots = idev->absinfo[ABS_MT_TRACKING_ID].maximum -
			   idev->absinfo[ABS_MT_TRACKING_ID].minimum + 1,
		mt_slots = clamp(mt_slots, 2, 32);
	} else if (test_bit(ABS_MT_POSITION_X, idev->absbit)) {
		mt_slots = 2;
	} else {
		mt_slots = 0;
	}

	events = mt_slots + 1; /* count SYN_MT_REPORT and SYN_REPORT */

	for (i = 0; i < ABS_CNT; i++) {
		if (test_bit(i, idev->absbit)) {
			if (vmm_input_is_mt_axis(i)) {
				events += mt_slots;
			} else {
				events++;
			}
		}
	}

	for (i = 0; i < REL_CNT; i++) {
		if (test_bit(i, idev->relbit)) {
			events++;
		}
	}

	return events;
}

#define INPUT_CLEANSE_BITMASK(dev, type, bits)				\
	do {								\
		if (!test_bit(EV_##type, dev->evbit))			\
			vmm_memset(dev->bits##bit, 0,			\
				sizeof(dev->bits##bit));		\
	} while (0)

static void input_cleanse_bitmasks(struct vmm_input_dev *idev)
{
	INPUT_CLEANSE_BITMASK(idev, KEY, key);
	INPUT_CLEANSE_BITMASK(idev, REL, rel);
	INPUT_CLEANSE_BITMASK(idev, ABS, abs);
	INPUT_CLEANSE_BITMASK(idev, MSC, msc);
	INPUT_CLEANSE_BITMASK(idev, LED, led);
	INPUT_CLEANSE_BITMASK(idev, SND, snd);
	INPUT_CLEANSE_BITMASK(idev, FF, ff);
	INPUT_CLEANSE_BITMASK(idev, SW, sw);
}

int vmm_input_register_device(struct vmm_input_dev *idev)
{
	int i, rc;
	irq_flags_t flags, flags1;
	struct vmm_classdev *cd;

	if (!(idev && idev->name)) {
		return VMM_EFAIL;
	}

	cd = vmm_malloc(sizeof(struct vmm_classdev));
	if (!cd) {
		return VMM_EFAIL;
	}

	vmm_memset(cd, 0, sizeof(struct vmm_classdev));

	INIT_LIST_HEAD(&cd->head);
	vmm_strcpy(cd->name, idev->name);
	cd->dev = idev->dev;
	cd->priv = idev;

	rc = vmm_devdrv_register_classdev(VMM_INPUT_DEV_CLASS_NAME, cd);
	if (rc != VMM_OK) {
		vmm_free(cd);
		return rc;
	}

	/* Every input device generates EV_SYN/SYN_REPORT events. */
	__set_bit(EV_SYN, idev->evbit);

	/* KEY_RESERVED is not supposed to be transmitted to userspace. */
	__clear_bit(KEY_RESERVED, idev->keybit);

	/* Make sure that bitmasks not mentioned in dev->evbit are clean. */
	input_cleanse_bitmasks(idev);

	if (!idev->hint_events_per_packet) {
		idev->hint_events_per_packet =
				input_estimate_events_per_packet(idev);
	}

	/* If delay and period are pre-set by the driver, then autorepeating
	 * is handled by the driver itself and we don't do it in input.c.
	 */
	INIT_TIMER_EVENT(&idev->repeat_ev, input_repeat_key, idev);
	if (!idev->rep[REP_DELAY] && !idev->rep[REP_PERIOD]) {
		idev->rep[REP_DELAY] = 250;
		idev->rep[REP_PERIOD] = 33;
	}

	if (!idev->getkeycode) {
		idev->getkeycode = input_default_getkeycode;
	}

	if (!idev->setkeycode) {
		idev->setkeycode = input_default_setkeycode;
	}

	vmm_spin_lock_irqsave(&idev->ops_lock, flags);
	idev->users = 0;
	for (i = 0; i < EV_CNT; i++) {
		if (!test_bit(i, &idev->evbit[0])) {
			continue;
		}
		vmm_spin_lock_irqsave(&ictrl.hnd_conn_lock[i], flags1);
		idev->users += ictrl.hnd_conn_count[i];
		vmm_spin_unlock_irqrestore(&ictrl.hnd_conn_lock[i], flags1);
	}
	if (idev->users && idev->open) {
		rc = idev->open(idev);
	}
	vmm_spin_unlock_irqrestore(&idev->ops_lock, flags);

	vmm_spin_lock_irqsave(&ictrl.dev_list_lock, flags);
	list_add_tail(&idev->head, &ictrl.dev_list);
	vmm_spin_unlock_irqrestore(&ictrl.dev_list_lock, flags);

	return rc;
}

int vmm_input_unregister_device(struct vmm_input_dev *idev)
{
	int rc;
	irq_flags_t flags;
	struct vmm_classdev *cd;

	if (!idev) {
		return VMM_EFAIL;
	}

	vmm_spin_lock_irqsave(&ictrl.dev_list_lock, flags);
	list_del(&idev->head);
	vmm_spin_unlock_irqrestore(&ictrl.dev_list_lock, flags);	

	vmm_timer_event_stop(&idev->repeat_ev);

	vmm_spin_lock_irqsave(&idev->ops_lock, flags);
	if (idev->users && idev->close) {
		idev->users = 0;
		idev->close(idev);
	}
	vmm_spin_unlock_irqrestore(&idev->ops_lock, flags);

	cd = vmm_devdrv_find_classdev(VMM_INPUT_DEV_CLASS_NAME, idev->name);
	if (!cd) {
		return VMM_EFAIL;
	}

	rc = vmm_devdrv_unregister_classdev(VMM_INPUT_DEV_CLASS_NAME, cd);
	if (rc == VMM_OK) {
		vmm_free(cd);
	}

	return rc;
}

/*
 * Simulate keyup events for all keys that are marked as pressed.
 * The function must be called with dev->event_lock held.
 */
static void input_dev_release_keys(struct vmm_input_dev *idev)
{
	int code;

	if (is_event_supported(EV_KEY, idev->evbit, EV_MAX)) {
		for (code = 0; code <= KEY_MAX; code++) {
			if (is_event_supported(code, idev->keybit, KEY_MAX) &&
			    __test_and_clear_bit(code, idev->key)) {
				input_pass_event(idev, EV_KEY, code, 0);
			}
		}
		input_pass_event(idev, EV_SYN, SYN_REPORT, 1);
	}
}

void vmm_input_reset_device(struct vmm_input_dev *idev)
{
	irq_flags_t flags, flags1;

	vmm_spin_lock_irqsave(&idev->ops_lock, flags);

	if (idev && idev->users) {
		input_dev_toggle(idev, TRUE);

		/* Keys that have been pressed at suspend time are unlikely
		 * to be still pressed when we resume.
		 */
		vmm_spin_lock_irqsave(&idev->event_lock, flags1);
		input_dev_release_keys(idev);
		vmm_spin_unlock_irqrestore(&idev->event_lock, flags1);
	}

	vmm_spin_unlock_irqrestore(&idev->ops_lock, flags);
}

int vmm_input_flush_device(struct vmm_input_dev *idev)
{
	int rc = VMM_OK;
	irq_flags_t flags;

	if (!idev) {
		return VMM_EFAIL;
	}

	if (idev->flush) {
		vmm_spin_lock_irqsave(&idev->ops_lock, flags);
		rc = idev->flush(idev);
		vmm_spin_unlock_irqrestore(&idev->ops_lock, flags);
	}

	return rc;
}

struct vmm_input_dev *vmm_input_find_device(const char *name)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_find_classdev(VMM_INPUT_DEV_CLASS_NAME, name);
	if (!cd) {
		return NULL;
	}

	return cd->priv;
}

struct vmm_input_dev *vmm_input_get_device(int index)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_classdev(VMM_INPUT_DEV_CLASS_NAME, index);
	if (!cd) {
		return NULL;
	}

	return cd->priv;
}

u32 vmm_input_count_device(void)
{
	return vmm_devdrv_classdev_count(VMM_INPUT_DEV_CLASS_NAME);
}

int vmm_input_register_handler(struct vmm_input_handler *ihnd)
{
	int i;
	bool found;
	irq_flags_t flags;
	struct dlist *l;
	struct vmm_input_handler *ih;

	if (!(ihnd && ihnd->name && ihnd->event)) {
		return VMM_EFAIL;
	}

	ih = NULL;
	found = FALSE;

	vmm_spin_lock_irqsave(&ictrl.hnd_list_lock, flags);

	list_for_each(l, &ictrl.hnd_list) {
		ih = list_entry(l, struct vmm_input_handler, head);
		if (vmm_strcmp(ih->name, ihnd->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (found) {
		vmm_spin_unlock_irqrestore(&ictrl.hnd_list_lock, flags);
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&ihnd->head);
	ihnd->connected = FALSE;
	for (i = 0; i < EV_CNT; i++) {
		INIT_LIST_HEAD(&ihnd->conn_head[i]);
	}

	list_add_tail(&ihnd->head, &ictrl.hnd_list);

	vmm_spin_unlock_irqrestore(&ictrl.hnd_list_lock, flags);

	return VMM_OK;
}

int vmm_input_unregister_handler(struct vmm_input_handler *ihnd)
{
	bool found;
	irq_flags_t flags;
	struct dlist *l;
	struct vmm_input_handler *ih;

	if (!ihnd) {
		return VMM_EFAIL;
	}

	vmm_spin_lock_irqsave(&ictrl.hnd_list_lock, flags);

	if (list_empty(&ictrl.hnd_list)) {
		vmm_spin_unlock_irqrestore(&ictrl.hnd_list_lock, flags);
		return VMM_EFAIL;
	}

	ih = NULL;
	found = FALSE;
	list_for_each(l, &ictrl.hnd_list) {
		ih = list_entry(l, struct vmm_input_handler, head);
		if (vmm_strcmp(ih->name, ihnd->name) == 0) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		vmm_spin_unlock_irqrestore(&ictrl.hnd_list_lock, flags);
		return VMM_ENOTAVAIL;
	}

	list_del(&ih->head);

	vmm_spin_unlock_irqrestore(&ictrl.hnd_list_lock, flags);

	return VMM_OK;
}

int vmm_input_connect_handler(struct vmm_input_handler *ihnd)
{
	int i, rc;
	irq_flags_t flags, flags1;
	struct dlist *l;
	struct vmm_input_dev *idev;

	if (!ihnd || ihnd->connected) {
		return VMM_EFAIL;
	}

	for (i = 0; i < EV_CNT; i++) {
		if (!test_bit(i, &ihnd->evbit[0])) {
			continue;
		}

		vmm_spin_lock_irqsave(&ictrl.hnd_conn_lock[i], flags);
		INIT_LIST_HEAD(&ihnd->conn_head[i]);
		list_add_tail(&ihnd->conn_head[i], &ictrl.hnd_conn[i]);
		ictrl.hnd_conn_count[i]++;
		vmm_spin_unlock_irqrestore(&ictrl.hnd_conn_lock[i], flags);

		vmm_spin_lock_irqsave(&ictrl.dev_list_lock, flags);
		list_for_each(l, &ictrl.dev_list) {
			idev = list_entry(l, struct vmm_input_dev, head);
			if (!test_bit(i, &idev->evbit[0])) {
				continue;
			}
			vmm_spin_lock_irqsave(&idev->ops_lock, flags1);
			if (!idev->users && idev->open) {
				rc = idev->open(idev);
				if (rc) {
					vmm_printf("%s: failed to open %s", 
						   __func__, idev->name);
				}
			}
			idev->users++;
			vmm_spin_unlock_irqrestore(&idev->ops_lock, flags1);
		}
		vmm_spin_unlock_irqrestore(&ictrl.dev_list_lock, flags);
	}

	ihnd->connected = TRUE;

	return VMM_OK;
}

int vmm_input_disconnect_handler(struct vmm_input_handler *ihnd)
{
	int i;
	irq_flags_t flags, flags1;
	struct dlist *l;
	struct vmm_input_dev *idev;

	if (!ihnd || !ihnd->connected) {
		return VMM_EFAIL;
	}

	for (i = 0; i < EV_CNT; i++) {
		if (!test_bit(i, &ihnd->evbit[0])) {
			continue;
		}

		vmm_spin_lock_irqsave(&ictrl.hnd_conn_lock[i], flags);
		list_del(&ihnd->conn_head[i]);
		if (ictrl.hnd_conn_count[i]) {
			ictrl.hnd_conn_count[i]--;
		}
		vmm_spin_unlock_irqrestore(&ictrl.hnd_conn_lock[i], flags);

		vmm_spin_lock_irqsave(&ictrl.dev_list_lock, flags);
		list_for_each(l, &ictrl.dev_list) {
			idev = list_entry(l, struct vmm_input_dev, head);
			if (!test_bit(i, &idev->evbit[0])) {
				continue;
			}
			vmm_spin_lock_irqsave(&idev->ops_lock, flags1);
			if ((idev->users == 1) && idev->close) {
				idev->close(idev);
			}
			if (idev->users) {
				idev->users--;
			}
			vmm_spin_unlock_irqrestore(&idev->ops_lock, flags1);
		}
		vmm_spin_unlock_irqrestore(&ictrl.dev_list_lock, flags);
	}

	ihnd->connected = FALSE;

	return VMM_OK;
}

struct vmm_input_handler *vmm_input_find_handler(const char *name)
{
	bool found;
	irq_flags_t flags;
	struct dlist *l;
	struct vmm_input_handler *ihnd;

	if (!name) {
		return NULL;
	}

	found = FALSE;
	ihnd = NULL;

	vmm_spin_lock_irqsave(&ictrl.hnd_list_lock, flags);

	list_for_each(l, &ictrl.hnd_list) {
		ihnd = list_entry(l, struct vmm_input_handler, head);
		if (vmm_strcmp(ihnd->name, name) == 0) {
			found = TRUE;
			break;
		}
	}

	vmm_spin_unlock_irqrestore(&ictrl.hnd_list_lock, flags);

	if (!found) {
		return NULL;
	}

	return ihnd;
}

struct vmm_input_handler *vmm_input_get_handler(int index)
{
	bool found;
	irq_flags_t flags;
	struct dlist *l;
	struct vmm_input_handler *ret;

	if (index < 0) {
		return NULL;
	}

	ret = NULL;
	found = FALSE;

	vmm_spin_lock_irqsave(&ictrl.hnd_list_lock, flags);

	list_for_each(l, &ictrl.hnd_list) {
		ret = list_entry(l, struct vmm_input_handler, head);
		if (!index) {
			found = TRUE;
			break;
		}
		index--;
	}

	vmm_spin_unlock_irqrestore(&ictrl.hnd_list_lock, flags);

	if (!found) {
		return NULL;
	}

	return ret;
}

u32 vmm_input_count_handler(void)
{
	u32 retval = 0;
	irq_flags_t flags;
	struct dlist *l;

	vmm_spin_lock_irqsave(&ictrl.hnd_list_lock, flags);

	list_for_each(l, &ictrl.hnd_list) {
		retval++;
	}

	vmm_spin_unlock_irqrestore(&ictrl.hnd_list_lock, flags);

	return retval;
}

static int __init vmm_input_init(void)
{
	int i, rc;
	struct vmm_class *c;

	vmm_printf("Initialize Input Device Framework\n");

	vmm_memset(&ictrl, 0, sizeof(struct vmm_input_ctrl));

	INIT_SPIN_LOCK(&ictrl.dev_list_lock);
	INIT_LIST_HEAD(&ictrl.dev_list);
	INIT_SPIN_LOCK(&ictrl.hnd_list_lock);
	INIT_LIST_HEAD(&ictrl.hnd_list);
	for (i = 0; i < EV_CNT; i++) {
		INIT_SPIN_LOCK(&ictrl.hnd_conn_lock[i]);
		INIT_LIST_HEAD(&ictrl.hnd_conn[i]);
		ictrl.hnd_conn_count[i] = 0;
	}

	c = vmm_malloc(sizeof(struct vmm_class));
	if (!c) {
		return VMM_EFAIL;
	}

	vmm_memset(c, 0, sizeof(struct vmm_class));

	INIT_LIST_HEAD(&c->head);
	vmm_strcpy(c->name, VMM_INPUT_DEV_CLASS_NAME);
	INIT_LIST_HEAD(&c->classdev_list);

	rc = vmm_devdrv_register_class(c);
	if (rc != VMM_OK) {
		vmm_free(c);
	}

	return rc;
}

static void vmm_input_exit(void)
{
	int rc;
	struct vmm_class *c;

	c = vmm_devdrv_find_class(VMM_INPUT_DEV_CLASS_NAME);
	if (!c) {
		return;
	}

	rc = vmm_devdrv_unregister_class(c);
	if (rc) {
		return;
	}

	vmm_free(c);
}

VMM_DECLARE_MODULE(MODULE_VARID,
		   MODULE_NAME,
		   MODULE_AUTHOR,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
