/**
 * Copyright (c) 2017 Anup Patel.
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
 * @file vmm_virtio_config.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief VirtIO device feature and status defines
 */
#ifndef __VMM_VIRTIO_CONFIG_H__
#define __VMM_VIRTIO_CONFIG_H__

/* Status byte for guest to report progress, and synchronize features. */
/* We have seen device and processed generic fields (VIRTIO_CONFIG_F_VIRTIO) */
#define VMM_VIRTIO_CONFIG_S_ACKNOWLEDGE		1
/* We have found a driver for the device. */
#define VMM_VIRTIO_CONFIG_S_DRIVER		2
/* Driver has used its parts of the config, and is happy */
#define VMM_VIRTIO_CONFIG_S_DRIVER_OK		4
/* Driver has finished configuring features */
#define VMM_VIRTIO_CONFIG_S_FEATURES_OK		8
/* Device entered invalid state, driver must reset it */
#define VMM_VIRTIO_CONFIG_S_NEEDS_RESET		0x40
/* We've given up on this device. */
#define VMM_VIRTIO_CONFIG_S_FAILED		0x80

/* Some virtio feature bits (currently bits 28 through 32) are reserved
 * for the transport being used (eg. virtio_ring), the rest are per-device
 * feature bits.
 */
#define VMM_VIRTIO_TRANSPORT_F_START		28
#define VMM_VIRTIO_TRANSPORT_F_END		34

#ifndef VMM_VIRTIO_CONFIG_NO_LEGACY
/* Do we get callbacks when the ring is completely used, even if we've
 * suppressed them? */
#define VMM_VIRTIO_F_NOTIFY_ON_EMPTY		24

/* Can the device handle any descriptor layout? */
#define VMM_VIRTIO_F_ANY_LAYOUT			27
#endif /* VIRTIO_CONFIG_NO_LEGACY */

/* v1.0 compliant. */
#define VMM_VIRTIO_F_VERSION_1			32

/*
 * If clear - device has the IOMMU bypass quirk feature.
 * If set - use platform tools to detect the IOMMU.
 *
 * Note the reverse polarity (compared to most other features),
 * this is for compatibility with legacy systems.
 */
#define VMM_VIRTIO_F_IOMMU_PLATFORM		33

#endif /* __VMM_VIRTIO_CONFIG_H__ */
