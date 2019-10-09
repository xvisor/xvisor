/**
 * Copyright (c) 2019 Anup Patel.
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
 * @file virtio_input.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief VirtIO based input device emulator.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_spinlocks.h>
#include <vmm_macros.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>
#include <vmm_host_io.h>
#include <vio/vmm_keymaps.h>
#include <vio/vmm_vinput.h>
#include <vio/vmm_virtio.h>
#include <vio/vmm_virtio_input.h>
#include <libs/stringlib.h>

/* For defines related to linux-style event types and events */
#include <drv/input.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)			vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

#define MODULE_DESC			"VirtIO Input Emulator"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VMM_VIRTIO_IPRIORITY + \
					 VMM_VINPUT_IPRIORITY + 1)
#define MODULE_INIT			virtio_input_init
#define MODULE_EXIT			virtio_input_exit

#define VIRTIO_INPUT_QUEUE_SIZE		128
#define VIRTIO_INPUT_EVENT_QUEUE	0
#define VIRTIO_INPUT_STATUS_QUEUE	1
#define VIRTIO_INPUT_NUM_QUEUES		2

struct virtio_input_dev {
	struct vmm_virtio_device 	*vdev;

	struct vmm_virtio_queue 	vqs[VIRTIO_INPUT_NUM_QUEUES];
	struct vmm_virtio_iovec		event_iov[VIRTIO_INPUT_QUEUE_SIZE];
	struct vmm_virtio_iovec		status_iov[VIRTIO_INPUT_QUEUE_SIZE];
	u64 				features;

	struct vmm_virtio_input_config 	config;

	vmm_spinlock_t			event_lock;
	int				event_vkeycode_offset;
	int				event_buttons_state;

	struct vmm_vkeyboard		*vkbd;
	struct vmm_vmouse		*vmou;
};

static u64 virtio_input_get_host_features(struct vmm_virtio_device *dev)
{
	return	1ULL << VMM_VIRTIO_F_VERSION_1
		| 1UL << VMM_VIRTIO_RING_F_EVENT_IDX;
#if 0
		| 1UL << VMM_VIRTIO_RING_F_INDIRECT_DESC;
#endif
}

static void virtio_input_set_guest_features(struct vmm_virtio_device *dev,
					    u32 select, u32 features)
{
	struct virtio_input_dev *videv = dev->emu_data;

	DPRINTF("%s: dev=%s select=%d features=0x%x\n",
		__func__, dev->name, select, features);

	if (1 < select)
		return;

	videv->features &= ~((u64)UINT_MAX << (select * 32));
	videv->features |= ((u64)features << (select * 32));
}

static int virtio_input_init_vq(struct vmm_virtio_device *dev,
				u32 vq, u32 page_size, u32 align, u32 pfn)
{
	int rc;
	struct virtio_input_dev *videv = dev->emu_data;

	DPRINTF("%s: dev=%s vq=%d page_size=0x%x align=0x%x pfn=0x%x\n",
		__func__, dev->name, vq, page_size, align, pfn);

	switch (vq) {
	case VIRTIO_INPUT_EVENT_QUEUE:
	case VIRTIO_INPUT_STATUS_QUEUE:
		rc = vmm_virtio_queue_setup(&videv->vqs[vq], dev->guest,
			pfn, page_size, VIRTIO_INPUT_QUEUE_SIZE, align);
		break;
	default:
		rc = VMM_EINVALID;
		break;
	};

	return rc;
}

static int virtio_input_get_pfn_vq(struct vmm_virtio_device *dev, u32 vq)
{
	int rc;
	struct virtio_input_dev *videv = dev->emu_data;

	DPRINTF("%s: dev=%s vq=%d\n", __func__, dev->name, vq);

	switch (vq) {
	case VIRTIO_INPUT_EVENT_QUEUE:
	case VIRTIO_INPUT_STATUS_QUEUE:
		rc = vmm_virtio_queue_guest_pfn(&videv->vqs[vq]);
		break;
	default:
		rc = VMM_EINVALID;
		break;
	};

	return rc;
}

static int virtio_input_get_size_vq(struct vmm_virtio_device *dev, u32 vq)
{
	int rc;

	DPRINTF("%s: dev=%s vq=%d\n", __func__, dev->name, vq);

	switch (vq) {
	case VIRTIO_INPUT_EVENT_QUEUE:
	case VIRTIO_INPUT_STATUS_QUEUE:
		rc = VIRTIO_INPUT_QUEUE_SIZE;
		break;
	default:
		rc = 0;
		break;
	};

	return rc;
}

static int virtio_input_set_size_vq(struct vmm_virtio_device *dev,
				    u32 vq, int size)
{
	DPRINTF("%s: dev=%s vq=%d size=%d\n", __func__, dev->name, vq, size);

	/* FIXME: dynamic */
	return size;
}

/* NOTE: This must be called with videv->event_lock held */
static void __virtio_input_do_events(struct virtio_input_dev *videv,
				     struct vmm_virtio_input_event *events,
				     int events_count)
{
	u16 head;
	int rc, i = 0;
	u32 iov_cnt, total_len, wr_len;
	struct vmm_virtio_device *dev = videv->vdev;
	struct vmm_virtio_queue *vq = &videv->vqs[VIRTIO_INPUT_EVENT_QUEUE];
	struct vmm_virtio_iovec *iov = videv->event_iov;

	DPRINTF("%s: dev=%s events_count=%d\n",
		__func__, dev->name, events_count);

	while ((i < events_count) && vmm_virtio_queue_available(vq)) {
		head = iov_cnt = total_len = wr_len = 0;

		rc = vmm_virtio_queue_get_iovec(vq, iov,
						&iov_cnt, &total_len, &head);
		if (rc) {
			vmm_printf("%s: failed to get iovec for event[%d] "
				   "(error %d)\n", __func__, i, rc);
			return;
		}

		DPRINTF("%s: dev=%s iov_cnt=%d total_len=%d\n",
			__func__, dev->name, iov_cnt, total_len);

		if (iov_cnt == 0) {
			vmm_printf("%s: empty iovec for event[%d]\n",
				   __func__, i);
			return;
		}

		wr_len = vmm_virtio_buf_to_iovec_write(dev, iov, iov_cnt,
						&events[i], sizeof(*events));
		if (wr_len != sizeof(*events)) {
			vmm_printf("%s: failed to write event[%d]\n",
				   __func__, i);
			return;
		}

		vmm_virtio_queue_set_used_elem(vq, head, wr_len);

		i++;
	}

	if (vmm_virtio_queue_should_signal(vq)) {
		dev->tra->notify(dev, VIRTIO_INPUT_EVENT_QUEUE);
	}
}

static void virtio_input_keyboard_event(struct vmm_vkeyboard *vkbd,
					int vkeycode, int vkey)
{
	irq_flags_t flags;
	int vkeyvalue, cnt = 0;
	struct vmm_virtio_input_event events[2];
	struct virtio_input_dev *videv = vmm_vkeyboard_priv(vkbd);

	DPRINTF("%s: dev=%s vkeycode=0x%x vkey=%d\n",
		__func__, videv->vdev->name, vkeycode, vkey);

	vmm_spin_lock_irqsave(&videv->event_lock, flags);

	if (vkeycode == SCANCODE_EMUL0) {
		videv->event_vkeycode_offset = SCANCODE_KEYCODEMASK + 1;
		goto done;
	}

	vkeyvalue = (vkeycode & SCANCODE_UP) ? 0 : 1;
	vkeycode = vkeycode & SCANCODE_KEYCODEMASK;
	vkeycode += videv->event_vkeycode_offset;
	videv->event_vkeycode_offset = 0;

	events[cnt].type = vmm_cpu_to_le16(EV_KEY);
	events[cnt].code = vmm_cpu_to_le16((u16)vkeycode);
	events[cnt].value = vmm_cpu_to_le32((u32)vkeyvalue);
	cnt++;

	events[cnt].type = vmm_cpu_to_le16(EV_SYN);
	events[cnt].code = 0;
	events[cnt].value = 0;
	cnt++;

	__virtio_input_do_events(videv, events, cnt);

done:
	vmm_spin_unlock_irqrestore(&videv->event_lock, flags);
}

static void virtio_input_mouse_event(struct vmm_vmouse *vmou,
				     int dx, int dy, int dz,
				     int buttons_state)
{
	u32 value;
	irq_flags_t flags;
	int buttons_change, cnt = 0;
	struct vmm_virtio_input_event events[7];
	struct virtio_input_dev *videv = vmm_vmouse_priv(vmou);

	DPRINTF("%s: dev=%s dx=%d dy=%d dz=%d buttons_state=%d\n",
		__func__, videv->vdev->name, dx, dy, dz, buttons_state);

	vmm_spin_lock_irqsave(&videv->event_lock, flags);

	events[cnt].type = vmm_cpu_to_le16(EV_REL);
	events[cnt].code = vmm_cpu_to_le16(REL_X);
	events[cnt].value = vmm_cpu_to_le32((u32)dx);
	cnt++;

	events[cnt].type = vmm_cpu_to_le16(EV_REL);
	events[cnt].code = vmm_cpu_to_le16(REL_Y);
	events[cnt].value = vmm_cpu_to_le32((u32)dy);
	cnt++;

	events[cnt].type = vmm_cpu_to_le16(EV_REL);
	events[cnt].code = vmm_cpu_to_le16(REL_Z);
	events[cnt].value = vmm_cpu_to_le32((u32)dz);
	cnt++;

	buttons_change = videv->event_buttons_state ^ buttons_state;
	if (buttons_change) {
		if (buttons_change & VMM_MOUSE_LBUTTON) {
			value = (buttons_state & VMM_MOUSE_LBUTTON) ? 1 : 0;
			events[cnt].type = vmm_cpu_to_le16(EV_KEY);
			events[cnt].code = vmm_cpu_to_le16(BTN_LEFT);
			events[cnt].value = vmm_cpu_to_le32(value);
			cnt++;
		}

		if (buttons_change & VMM_MOUSE_MBUTTON) {
			value = (buttons_state & VMM_MOUSE_MBUTTON) ? 1 : 0;
			events[cnt].type = vmm_cpu_to_le16(EV_KEY);
			events[cnt].code = vmm_cpu_to_le16(BTN_MIDDLE);
			events[cnt].value = vmm_cpu_to_le32(value);
			cnt++;
		}

		if (buttons_change & VMM_MOUSE_RBUTTON) {
			value = (buttons_state & VMM_MOUSE_RBUTTON) ? 1 : 0;
			events[cnt].type = vmm_cpu_to_le16(EV_KEY);
			events[cnt].code = vmm_cpu_to_le16(BTN_RIGHT);
			events[cnt].value = vmm_cpu_to_le32(value);
			cnt++;
		}

		videv->event_buttons_state = buttons_state;
	}

	events[cnt].type = vmm_cpu_to_le16(EV_SYN);
	events[cnt].code = 0;
	events[cnt].value = 0;
	cnt++;

	__virtio_input_do_events(videv, events, cnt);

	vmm_spin_unlock_irqrestore(&videv->event_lock, flags);
}

static void virtio_input_do_status(struct vmm_virtio_device *dev,
				   struct virtio_input_dev *videv)
{
	u16 head;
	int rc, ledstate, ledmask;
	u32 i, iov_cnt, total_len, rd_len;
	struct vmm_virtio_queue *vq = &videv->vqs[VIRTIO_INPUT_STATUS_QUEUE];
	struct vmm_virtio_iovec *iov = videv->status_iov;
	struct vmm_virtio_input_event event;

	DPRINTF("%s: dev=%s\n", __func__, dev->name);

	ledstate = vmm_vkeyboard_get_ledstate(videv->vkbd);

	while (vmm_virtio_queue_available(vq)) {
		rc = vmm_virtio_queue_get_iovec(vq, iov,
						&iov_cnt, &total_len, &head);
		if (rc) {
			vmm_printf("%s: failed to get iovec (error %d)\n",
				   __func__, rc);
			continue;
		}

		DPRINTF("%s: dev=%s iov_cnt=%d total_len=%d\n",
			__func__, dev->name, iov_cnt, total_len);

		for (i = 0; i < iov_cnt; i++) {
			rd_len = vmm_virtio_iovec_to_buf_read(dev, &iov[i], 1,
							&event, sizeof(event));
			if (rd_len != sizeof(event)) {
				vmm_printf("%s: failed to read event%d\n",
					   __func__, i);
				continue;
			}

			event.type = vmm_le16_to_cpu(event.type);
			event.code = vmm_le16_to_cpu(event.code);
			event.value = vmm_le16_to_cpu(event.value);

			DPRINTF("%s: dev=%s type=0x%x code=0x%x value=0x%x\n",
				__func__, dev->name, event.type,
				event.code, event.value);

			if (event.type == EV_LED) {
				switch (event.code) {
				case LED_NUML:
					ledmask = VMM_NUM_LOCK_LED;
					break;
				case LED_CAPSL:
					ledmask = VMM_CAPS_LOCK_LED;
					break;
				case LED_SCROLLL:
					ledmask = VMM_SCROLL_LOCK_LED;
					break;
				default:
					ledmask = 0;
					break;
				}

				if (event.value)
					ledstate |= ledmask;
				else
					ledstate &= ~ledmask;
			}
		}

		vmm_virtio_queue_set_used_elem(vq, head, total_len);
	}

	if (vmm_virtio_queue_should_signal(vq)) {
		dev->tra->notify(dev, VIRTIO_INPUT_STATUS_QUEUE);
	}

	vmm_vkeyboard_set_ledstate(videv->vkbd, ledstate);
}

static int virtio_input_notify_vq(struct vmm_virtio_device *dev, u32 vq)
{
	int rc = VMM_OK;
	struct virtio_input_dev *videv = dev->emu_data;

	DPRINTF("%s: dev=%s vq=%d\n", __func__, dev->name, vq);

	switch (vq) {
	case VIRTIO_INPUT_STATUS_QUEUE:
		virtio_input_do_status(dev, videv);
		break;
	default:
		rc = VMM_EINVALID;
		break;
	};

	return rc;
}

static void virtio_input_status_changed(struct vmm_virtio_device *dev,
					u32 new_status)
{
	/* Nothing to do here. */
}

static void virtio_input_update_config(struct virtio_input_dev *videv)
{
	u32 i;
	struct vmm_virtio_input_config *cfg = &videv->config;

	/* First clear the config */
	memset(&cfg->u, 0, sizeof(cfg->u));
	cfg->size = 0;

	switch (cfg->select) {
	case VMM_VIRTIO_INPUT_CFG_ID_NAME:
		if (cfg->subsel) {
			break;
		}
		/* Set name in u.string */
		strcpy(cfg->u.string, "Xvisor VirtIO Input");
		cfg->size = strlen(cfg->u.string);
		break;
	case VMM_VIRTIO_INPUT_CFG_ID_SERIAL:
		if (cfg->subsel) {
			break;
		}
		/* Set serial in u.string */
		strcpy(cfg->u.string, "0000-0000-0000-0000");
		cfg->size = strlen(cfg->u.string);
		break;
	case VMM_VIRTIO_INPUT_CFG_EV_BITS:
		/* Set supported events in u.bitmap */
		switch (cfg->subsel) {
		case EV_KEY:
			for (i = 0 ; i < KEY_CNT; i++) {
				cfg->u.bitmap[i / 8] |= (1U << i % 8);
			}
			cfg->size = KEY_CNT / 8;
			break;
		case EV_REL:
			cfg->u.bitmap[REL_X / 8] |= (1U << REL_X % 8);
			cfg->u.bitmap[REL_Y / 8] |= (1U << REL_Y % 8);
			cfg->u.bitmap[REL_Z / 8] |= (1U << REL_Z % 8);
			cfg->size = REL_CNT / 8;
			break;
		case EV_LED:
			cfg->u.bitmap[LED_NUML / 8] |= (1U << LED_NUML % 8);
			cfg->u.bitmap[LED_CAPSL / 8] |= (1U << LED_CAPSL % 8);
			cfg->u.bitmap[LED_SCROLLL / 8] |= (1U << LED_SCROLLL % 8);
			cfg->size = LED_CNT / 8;
			break;
		default:
			break;
		};
		break;
	case VMM_VIRTIO_INPUT_CFG_ID_DEVIDS:
	case VMM_VIRTIO_INPUT_CFG_PROP_BITS:
	case VMM_VIRTIO_INPUT_CFG_ABS_INFO:
	default:
		/* No DEVIDS, PROP_BITS, ABS_INFO for now */
		break;
	}
}

static int virtio_input_read_config(struct vmm_virtio_device *dev,
				    u32 offset, void *dst, u32 dst_len)
{
	struct virtio_input_dev *videv = dev->emu_data;
	u32 i, src_len = sizeof(videv->config);
	u8 *src = (u8 *)&videv->config;

	DPRINTF("%s: dev=%s offset=%d dst=%p dst_len=%d\n",
		__func__, dev->name, offset, dst, dst_len);

	if (sizeof(videv->config) < (offset + dst_len))
		return VMM_EINVALID;

	for (i = 0; (i < dst_len) && ((offset + i) < src_len); i++) {
		*((u8 *)dst + i) = src[offset + i];
	}

	return VMM_OK;
}

static int virtio_input_write_config(struct vmm_virtio_device *dev,
				     u32 offset, void *src, u32 src_len)
{
	u8 data8;
	struct virtio_input_dev *videv = dev->emu_data;

	DPRINTF("%s: dev=%s offset=%d src=%p src_len=%d\n",
		__func__, dev->name, offset, src, src_len);

	if (src_len != sizeof(u8))
		return VMM_EINVALID;
	data8 = *((u8 *)src);

	switch (offset) {
	case offsetof(struct vmm_virtio_input_config, select):
		videv->config.select = data8;
		virtio_input_update_config(videv);
		break;
	case offsetof(struct vmm_virtio_input_config, subsel):
		videv->config.subsel = data8;
		virtio_input_update_config(videv);
		break;
	default:
		return VMM_EINVALID;
	};

	return VMM_OK;
}

static int virtio_input_reset(struct vmm_virtio_device *dev)
{
	int rc;
	irq_flags_t flags;
	struct virtio_input_dev *videv = dev->emu_data;

	DPRINTF("%s: dev=%s\n", __func__, dev->name);

	videv->config.select = VMM_VIRTIO_INPUT_CFG_UNSET;
	videv->config.subsel = 0,
	virtio_input_update_config(videv);

	rc = vmm_virtio_queue_cleanup(&videv->vqs[VIRTIO_INPUT_EVENT_QUEUE]);
	if (rc) {
		return rc;
	}

	rc = vmm_virtio_queue_cleanup(&videv->vqs[VIRTIO_INPUT_STATUS_QUEUE]);
	if (rc) {
		return rc;
	}

	vmm_spin_lock_irqsave(&videv->event_lock, flags);
	videv->event_vkeycode_offset = 0;
	videv->event_buttons_state = 0;
	vmm_spin_unlock_irqrestore(&videv->event_lock, flags);

	return VMM_OK;
}

static int virtio_input_connect(struct vmm_virtio_device *dev,
			      struct vmm_virtio_emulator *emu)
{
	char name[VMM_FIELD_NAME_SIZE];
	struct virtio_input_dev *videv;

	DPRINTF("%s: dev=%s emu=%s\n", __func__, dev->name, emu->name);

	videv = vmm_zalloc(sizeof(struct virtio_input_dev));
	if (!videv) {
		vmm_printf("Failed to allocate virtio input device\n");
		return VMM_ENOMEM;
	}
	videv->vdev = dev;

	INIT_SPIN_LOCK(&videv->event_lock);

	vmm_snprintf(name, VMM_FIELD_NAME_SIZE, "%s/keyboard", dev->name);
	videv->vkbd = vmm_vkeyboard_create(name, virtio_input_keyboard_event,
					   videv);
	if (!videv->vkbd) {
		vmm_printf("Failed to create virtio input keyboard\n");
		vmm_free(videv);
		return VMM_EFAIL;
	}

	vmm_snprintf(name, VMM_FIELD_NAME_SIZE, "%s/mouse", dev->name);
	videv->vmou = vmm_vmouse_create(name, TRUE, virtio_input_mouse_event,
					videv);
	if (!videv->vmou) {
		vmm_printf("Failed to create virtio input mouse\n");
		vmm_vkeyboard_destroy(videv->vkbd);
		vmm_free(videv);
		return VMM_EFAIL;
	}

	dev->emu_data = videv;

	return VMM_OK;
}

static void virtio_input_disconnect(struct vmm_virtio_device *dev)
{
	struct virtio_input_dev *videv = dev->emu_data;

	DPRINTF("%s: dev=%s\n", __func__, dev->name);

	vmm_vmouse_destroy(videv->vmou);
	vmm_vkeyboard_destroy(videv->vkbd);
	vmm_free(videv);
}

struct vmm_virtio_device_id virtio_input_emu_id[] = {
	{ .type = VMM_VIRTIO_ID_INPUT },
	{ },
};

struct vmm_virtio_emulator virtio_input = {
	.name = "virtio_input",
	.id_table = virtio_input_emu_id,

	/* VirtIO operations */
	.get_host_features      = virtio_input_get_host_features,
	.set_guest_features     = virtio_input_set_guest_features,
	.init_vq                = virtio_input_init_vq,
	.get_pfn_vq             = virtio_input_get_pfn_vq,
	.get_size_vq            = virtio_input_get_size_vq,
	.set_size_vq            = virtio_input_set_size_vq,
	.notify_vq              = virtio_input_notify_vq,
	.status_changed         = virtio_input_status_changed,

	/* Emulator operations */
	.read_config = virtio_input_read_config,
	.write_config = virtio_input_write_config,
	.reset = virtio_input_reset,
	.connect = virtio_input_connect,
	.disconnect = virtio_input_disconnect,
};

static int __init virtio_input_init(void)
{
	return vmm_virtio_register_emulator(&virtio_input);
}

static void __exit virtio_input_exit(void)
{
	vmm_virtio_unregister_emulator(&virtio_input);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
