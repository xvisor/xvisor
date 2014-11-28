/*
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * Adapted from Linux Kernel 3.13.6 include/linux/spi/flash.h
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation
 * 51 Franklin Street, Fifth Floor
 * Boston, MA  02110-1301, USA.
 *
 * @file flash.h
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief SPI flash header
 */
#ifndef LINUX_SPI_FLASH_H
#define LINUX_SPI_FLASH_H

struct mtd_partition;

/**
 * struct flash_platform_data: board-specific flash data
 * @name: optional flash device name (eg, as used with mtdparts=)
 * @parts: optional array of mtd_partitions for static partitioning
 * @nr_parts: number of mtd_partitions for static partitoning
 * @type: optional flash device type (e.g. m25p80 vs m25p64), for use
 *	with chips that can't be queried for JEDEC or other IDs
 *
 * Board init code (in arch/.../mach-xxx/board-yyy.c files) can
 * provide information about SPI flash parts (such as DataFlash) to
 * help set up the device and its appropriate default partitioning.
 *
 * Note that for DataFlash, sizes for pages, blocks, and sectors are
 * rarely powers of two; and partitions should be sector-aligned.
 */
struct flash_platform_data {
	char		*name;
	struct mtd_partition *parts;
	unsigned int	nr_parts;

	char		*type;

	/* we'll likely add more ... use JEDEC IDs, etc */
};

#endif
