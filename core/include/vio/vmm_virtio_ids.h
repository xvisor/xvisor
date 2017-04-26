/**
 * Copyright (c) 2017 Pranav Sawargaonkar.
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
 * @file vmm_virtio_ids.h
 * @author Pranav Sawargaonkar (pranav.sawargaonkar@gmail.com)
 * @brief VirtIO Device IDs
 */
#ifndef __VMM_VIRTIO_IDS_H__
#define __VMM_VIRTIO_IDS_H__

enum vmm_virtio_id {
	VMM_VIRTIO_ID_NET		=  1, /* Network card */
	VMM_VIRTIO_ID_BLOCK		=  2, /* Block device */
	VMM_VIRTIO_ID_CONSOLE		=  3, /* Console */
	VMM_VIRTIO_ID_RNG		=  4, /* Entropy source */
	VMM_VIRTIO_ID_BALLOON		=  5, /* Memory ballooning (traditional) */
	VMM_VIRTIO_ID_IO_MEMORY		=  6, /* ioMemory */
	VMM_VIRTIO_ID_RPMSG		=  7, /* rpmsg (remote processor messaging) */
	VMM_VIRTIO_ID_SCSI		=  8, /* SCSI host */
	VMM_VIRTIO_ID_9P		=  9, /* 9P transport */
	VMM_VIRTIO_ID_MAC_VLAN		= 10, /* mac 802.11 Vlan */
	VMM_VIRTIO_ID_RPROC_SERIAL	= 11, /* rproc serial */
	VMM_VIRTIO_ID_CAIF		= 12, /* virtio CAIF */
	VMM_VIRTIO_ID_BALLOON_NEW	= 13, /* New memory ballooning */
	VMM_VIRTIO_ID_GPU		= 16, /* GPU device */
	VMM_VIRTIO_ID_TIMER		= 17, /* Timer/Clock device */
	VMM_VIRTIO_ID_INPUT		= 18, /* Input device */
};

#endif /* __VMM_VIRTIO_IDS_H__ */
