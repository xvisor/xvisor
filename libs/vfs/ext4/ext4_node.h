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
 * @file ext4_node.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for Ext4 node functions
 */
#ifndef _EXT4_NODE_H__
#define _EXT4_NODE_H__

#include <vmm_types.h>

#include "ext4_common.h"

#define EXT4_NODE_LOOKUP_SIZE		4

/* Information for accessing a ext4fs file/directory. */
struct ext4fs_node {
	/* Parent ext4fs control */
	struct ext4fs_control *ctrl;

	/* Underlying Inode */
	struct ext2_inode inode;
	u32 inode_no;
	bool inode_dirty;

	/* Cached data block
	 * Allocated on demand. Must be freed in vput()
	 */
	u32 cached_blkno;
	u8 *cached_block;
	bool cached_dirty;

	/* Indirect block
	 * Allocated on demand. Must be freed in vpuf()
	 */
	u32 *indir_block;
	u32 indir_blkno;
	bool indir_dirty;

	/* Double-Indirect level1 block
	 * Allocated on demand. Must be freed in vput()
	 */
	u32 *dindir1_block;
	u32 dindir1_blkno;
	bool dindir1_dirty;

	/* Double-Indirect level2 block
	 * Allocated on demand. Must be freed in vput()
	 */
	u32 *dindir2_block;
	u32 dindir2_blkno;
	bool dindir2_dirty;

	/* Child directory entry lookup table */
	u32 lookup_victim;
	char lookup_name[EXT4_NODE_LOOKUP_SIZE][VFS_MAX_NAME];
	struct ext2_dirent lookup_dent[EXT4_NODE_LOOKUP_SIZE];
};

u64 ext4fs_node_get_size(struct ext4fs_node *node);

void ext4fs_node_set_size(struct ext4fs_node *node, u64 size);

int ext4fs_node_read_blk(struct ext4fs_node *node,
			 u32 blkno, u32 blkoff, u32 blklen, char *buf);

int ext4fs_node_write_blk(struct ext4fs_node *node,
			  u32 blkno, u32 blkoff, u32 blklen, char *buf);

int ext4fs_node_sync(struct ext4fs_node *node);

int ext4fs_node_read_blkno(struct ext4fs_node *node, u32 blkpos, u32 *blkno);

int ext4fs_node_write_blkno(struct ext4fs_node *node, u32 blkpos, u32 blkno);

u32 ext4fs_node_read(struct ext4fs_node *node, u64 pos, u32 len, char *buf);

u32 ext4fs_node_write(struct ext4fs_node *node, u64 pos, u32 len, char *buf);

int ext4fs_node_truncate(struct ext4fs_node *node, u64 pos);

int ext4fs_node_load(struct ext4fs_control *ctrl, 
		     u32 inode_no, struct ext4fs_node *node);

int ext4fs_node_init(struct ext4fs_node *node);

int ext4fs_node_exit(struct ext4fs_node *node);

int ext4fs_node_read_dirent(struct ext4fs_node *dnode, 
			    loff_t off, struct dirent *d);

int ext4fs_node_find_dirent(struct ext4fs_node *dnode, 
			    const char *name, struct ext2_dirent *dent);

int ext4fs_node_add_dirent(struct ext4fs_node *dnode, 
			   const char *name, u32 inode_no, u8 type);

int ext4fs_node_del_dirent(struct ext4fs_node *dnode, const char *name);

#endif
