/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file fat_node.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for FAT node functions
 */
#ifndef _FAT_NODE_H__
#define _FAT_NODE_H__

#include <vmm_types.h>

#include "fat_common.h"

/* Information for accessing a FAT file/directory. */
struct fatfs_node {
	/* Parent FAT control */
	struct fatfs_control *ctrl;

	/* Parent directory FAT node */
	struct fatfs_node *parent;
	u32 parent_dirent_off;
	u32 parent_dirent_len;

	/* Parent directory entry */
	struct fat_dirent dirent;

	/* First cluster */
	u32 first_cluster;

	/* Cached cluster */
	u8 *cached_data;
	u32 cached_cluster;
	bool cached_dirty;
};

u32 fatfs_node_read(struct fatfs_node *node, u64 pos, u32 len, char *buf);

u64 fatfs_node_get_size(struct fatfs_node *node);

int fatfs_node_sync(struct fatfs_node *node);

int fatfs_node_init(struct fatfs_node *node);

int fatfs_node_exit(struct fatfs_node *node);

int fatfs_node_find_dirent(struct fatfs_node *dnode, 
			   const char *name,
			   struct fat_dirent *dent, 
			   u32 *dent_off, u32 *dent_len);

#endif
