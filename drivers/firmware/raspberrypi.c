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
 * @file raspberrypi.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief RaspberryPi BCM2835 firmware API implementation.
 *
 * The source has been largely adapted from Linux
 * drivers/firmware/raspberrypi.c
 *
 * Defines interfaces for interacting wtih the Raspberry Pi firmware's
 * property channel.
 *
 * Copyright (c) 2015 Broadcom
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_stdio.h>
#include <vmm_mutex.h>
#include <vmm_completion.h>
#include <vmm_wallclock.h>
#include <vmm_devdrv.h>
#include <vmm_devres.h>
#include <vmm_platform.h>
#include <vmm_host_aspace.h>
#include <vmm_modules.h>
#include <arch_barrier.h>
#include <libs/stringlib.h>
#include <drv/mailbox_client.h>
#include <drv/soc/bcm2835/raspberrypi-firmware.h>

#define MODULE_DESC			"RaspberryPi Firmware Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		RPI_FIRMWARE_IPRIORITY
#define	MODULE_INIT			rpi_firmware_init
#define	MODULE_EXIT			rpi_firmware_exit

#define MBOX_MSG(chan, data28)		(((data28) & ~0xf) | ((chan) & 0xf))
#define MBOX_CHAN(msg)			((msg) & 0xf)
#define MBOX_DATA28(msg)		((msg) & ~0xf)
#define MBOX_CHAN_PROPERTY		8

struct rpi_firmware {
	struct mbox_client cl;
	struct mbox_chan *chan; /* The property channel. */
	struct vmm_completion c;
	u32 enabled;
};

static DEFINE_MUTEX(transaction_lock);

static void response_callback(struct mbox_client *cl, void *msg)
{
	struct rpi_firmware *fw = container_of(cl, struct rpi_firmware, cl);
	vmm_completion_complete(&fw->c);
}

/*
 * Sends a request to the firmware through the BCM2835 mailbox driver,
 * and synchronously waits for the reply.
 */
static int
rpi_firmware_transaction(struct rpi_firmware *fw, u32 chan, u32 data)
{
	u32 message = MBOX_MSG(chan, data);
	int ret;

	WARN_ON(data & 0xf);

	vmm_mutex_lock(&transaction_lock);
	REINIT_COMPLETION(&fw->c);
	ret = mbox_send_message(fw->chan, &message);
	if (ret >= 0) {
		vmm_completion_wait(&fw->c);
		ret = 0;
	} else {
		vmm_lerror(fw->cl.dev->name,
			   "mbox_send_message returned %d\n", ret);
	}
	vmm_mutex_unlock(&transaction_lock);

	return ret;
}

/**
 * rpi_firmware_property_list - Submit firmware property list
 * @fw:		Pointer to firmware structure from rpi_firmware_get().
 * @data:	Buffer holding tags.
 * @tag_size:	Size of tags buffer.
 *
 * Submits a set of concatenated tags to the VPU firmware through the
 * mailbox property interface.
 *
 * The buffer header and the ending tag are added by this function and
 * don't need to be supplied, just the actual tags for your operation.
 * See struct rpi_firmware_property_tag_header for the per-tag
 * structure.
 */
int rpi_firmware_property_list(struct rpi_firmware *fw,
			       void *data, size_t tag_size)
{
	size_t size = tag_size + 12;
	u32 *buf;
	virtual_addr_t buf_va;
	physical_addr_t buf_pa;
	int i, ret;

	/* Packets are processed a dword at a time. */
	if (((virtual_addr_t)data & 3) || (size & 3))
		return VMM_EINVALID;

	buf_va = vmm_host_alloc_pages(VMM_SIZE_TO_PAGE(size),
				      VMM_MEMORY_FLAGS_NORMAL_NOCACHE);
	if (!buf_va)
		return VMM_ENOMEM;
	buf = (u32 *)buf_va;

	ret = vmm_host_va2pa(buf_va, &buf_pa);
	if (ret) {
		vmm_host_free_pages(buf_va, VMM_SIZE_TO_PAGE(size));
		return ret;
	}

	/* The firmware will error out without parsing in this case. */
	WARN_ON(size >= 1024 * 1024);

	buf[0] = size;
	buf[1] = RPI_FIRMWARE_STATUS_REQUEST;
	for (i = 0; i < (tag_size / 4); i++) {
		buf[2 + i] = ((u32 *)data)[i];
	}
	buf[size / 4 - 1] = RPI_FIRMWARE_PROPERTY_END;
	arch_smp_wmb();

	ret = rpi_firmware_transaction(fw, MBOX_CHAN_PROPERTY, (u32)buf_pa);

	arch_smp_rmb();
	for (i = 0; i < (tag_size / 4); i++) {
		((u32 *)data)[i] = buf[2 + i];
	}
	if (ret == 0 && buf[1] != RPI_FIRMWARE_STATUS_SUCCESS) {
		/*
		 * The tag name here might not be the one causing the
		 * error, if there were multiple tags in the request.
		 * But single-tag is the most common, so go with it.
		 */
		vmm_lerror(fw->cl.dev->name,
			   "Request 0x%08x returned status 0x%08x\n",
			   buf[2], buf[1]);
		ret = VMM_EINVALID;
	}

	vmm_host_free_pages(buf_va, VMM_SIZE_TO_PAGE(size));

	return ret;
}
VMM_EXPORT_SYMBOL_GPL(rpi_firmware_property_list);

/**
 * rpi_firmware_property - Submit single firmware property
 * @fw:		Pointer to firmware structure from rpi_firmware_get().
 * @tag:	One of enum_mbox_property_tag.
 * @tag_data:	Tag data buffer.
 * @buf_size:	Buffer size.
 *
 * Submits a single tag to the VPU firmware through the mailbox
 * property interface.
 *
 * This is a convenience wrapper around
 * rpi_firmware_property_list() to avoid some of the
 * boilerplate in property calls.
 */
int rpi_firmware_property(struct rpi_firmware *fw,
			  u32 tag, void *tag_data, size_t buf_size)
{
	/* Single tags are very small (generally 8 bytes), so the
	 * stack should be safe.
	 */
	u8 data[buf_size + sizeof(struct rpi_firmware_property_tag_header)];
	struct rpi_firmware_property_tag_header *header =
		(struct rpi_firmware_property_tag_header *)data;
	int ret;

	header->tag = tag;
	header->buf_size = buf_size;
	header->req_resp_size = 0;
	memcpy(data + sizeof(struct rpi_firmware_property_tag_header),
	       tag_data, buf_size);

	ret = rpi_firmware_property_list(fw, &data, sizeof(data));
	memcpy(tag_data,
	       data + sizeof(struct rpi_firmware_property_tag_header),
	       buf_size);

	return ret;
}
VMM_EXPORT_SYMBOL_GPL(rpi_firmware_property);

static void
rpi_firmware_print_firmware_revision(struct rpi_firmware *fw)
{
	u32 packet;
	int ret = rpi_firmware_property(fw,
					RPI_FIRMWARE_GET_FIRMWARE_REVISION,
					&packet, sizeof(packet));

	if (ret == 0) {
		struct vmm_timeinfo tm;

		vmm_wallclock_mkinfo(packet, 0, &tm);

		vmm_linfo(fw->cl.dev->name,
			  "Attached to firmware from %04ld-%02d-%02d %02d:%02d\n",
			  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			  tm.tm_hour, tm.tm_min);
	}
}

static int rpi_firmware_probe(struct vmm_device *dev,
			      const struct vmm_devtree_nodeid *devid)
{
	struct rpi_firmware *fw;

	fw = vmm_devm_zalloc(dev, sizeof(*fw));
	if (!fw)
		return VMM_ENOMEM;

	fw->cl.dev = dev;
	fw->cl.rx_callback = response_callback;
	fw->cl.tx_block = true;

	fw->chan = mbox_request_channel(&fw->cl, 0);
	if (VMM_IS_ERR(fw->chan)) {
		int ret = VMM_PTR_ERR(fw->chan);
		if (ret != VMM_EPROBE_DEFER)
			vmm_lerror(dev->name,
				   "Failed to get mbox channel: %d\n", ret);
		return ret;
	}

	INIT_COMPLETION(&fw->c);

	vmm_devdrv_set_data(dev, fw);

	rpi_firmware_print_firmware_revision(fw);

	return 0;
}

static int rpi_firmware_remove(struct vmm_device *dev)
{
	struct rpi_firmware *fw = vmm_devdrv_get_data(dev);

	mbox_free_channel(fw->chan);

	return 0;
}

/**
 * rpi_firmware_get - Get pointer to rpi_firmware structure.
 * @firmware_node:    Pointer to the firmware Device Tree node.
 *
 * Returns NULL is the firmware device is not ready.
 */
struct rpi_firmware *rpi_firmware_get(struct vmm_devtree_node *firmware_node)
{
	struct vmm_device *dev =
			vmm_platform_find_device_by_node(firmware_node);

	if (!dev)
		return NULL;

	return vmm_devdrv_get_data(dev);
}
VMM_EXPORT_SYMBOL_GPL(rpi_firmware_get);

static struct vmm_devtree_nodeid rpi_firmware_devid_table[] = {
	{ .compatible = "raspberrypi,bcm2835-firmware" },
	{ /* end of list */ },
};

static struct vmm_driver rpi_firmware_driver = {
	.name = "raspberrypi-firmware",
	.match_table = rpi_firmware_devid_table,
	.probe = rpi_firmware_probe,
	.remove = rpi_firmware_remove,
};

static int __init rpi_firmware_init(void)
{
	return vmm_devdrv_register_driver(&rpi_firmware_driver);
}

static void __exit rpi_firmware_exit(void)
{
	vmm_devdrv_unregister_driver(&rpi_firmware_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
