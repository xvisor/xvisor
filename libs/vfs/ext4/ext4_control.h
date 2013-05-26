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
 * @file ext4_control.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for Ext4 control functions
 */
#ifndef _EXT4_CONTROL_H__
#define _EXT4_CONTROL_H__

#include <vmm_mutex.h>
#include <vmm_host_io.h>
#include <block/vmm_blockdev.h>

#include "ext4_common.h"

#define __le32(x)			vmm_le32_to_cpu(x)
#define __le16(x)			vmm_le16_to_cpu(x)

/* Information for accessing block groups. */
struct ext4fs_group {
	/* lock to protect group */
	struct vmm_mutex grp_lock;
	struct ext2_block_group grp;

	u8 *block_bmap;
	u8 *inode_bmap;

	bool grp_dirty;
};

/* Information about a "mounted" ext filesystem. */
struct ext4fs_control {
	struct vmm_blockdev *bdev;

	/* lock to protect:
	 * sblock.free_blocks, 
	 * sblock.free_inodes,
	 * sblock.mtime,
	 * sblock.utime,
	 * sblock_dirty
	 */
	struct vmm_mutex sblock_lock;
	struct ext2_sblock sblock;

	/* flag to show whether sblock,
	 * groups, or bitmaps are updated.
	 */
	bool sblock_dirty;

	u32 log2_block_size;
	u32 block_size;
	u32 dir_blklast;
	u32 indir_blklast;
	u32 dindir_blklast;

	u32 inode_size;
	u32 inodes_per_block;

	u32 group_count;
	u32 group_table_blkno;
	struct ext4fs_group *groups;
};

u32 ext4fs_current_timestamp(void);

int ext4fs_devread(struct ext4fs_control *ctrl, 
		   u32 blkno, u32 blkoff, u32 buf_len, char *buf);

int ext4fs_devwrite(struct ext4fs_control *ctrl, 
		    u32 blkno, u32 blkoff, u32 buf_len, char *buf);

int ext4fs_control_read_inode(struct ext4fs_control *ctrl, 
			      u32 inode_no, struct ext2_inode *inode);

int ext4fs_control_write_inode(struct ext4fs_control *ctrl, 
			       u32 inode_no, struct ext2_inode *inode);

int ext4fs_control_alloc_block(struct ext4fs_control *ctrl,
			       u32 inode_no, u32 *blkno);

int ext4fs_control_free_block(struct ext4fs_control *ctrl, u32 blkno);

int ext4fs_control_alloc_inode(struct ext4fs_control *ctrl,
			       u32 parent_inode_no, u32 *inode_no);

int ext4fs_control_free_inode(struct ext4fs_control *ctrl, u32 inode_no);

int ext4fs_control_sync(struct ext4fs_control *ctrl);

int ext4fs_control_init(struct ext4fs_control *ctrl, 
			struct vmm_blockdev *bdev);

int ext4fs_control_exit(struct ext4fs_control *ctrl);

#endif
