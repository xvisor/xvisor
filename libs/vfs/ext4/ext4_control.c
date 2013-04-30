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
 * @file ext4_control.c
 * @author Anup Patel. (anup@brainfault.org)
 * @brief source file for Ext4 control functions
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_wallclock.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>

#include "ext4_control.h"

u32 ext4fs_current_timestamp(void)
{
	struct vmm_timeval tv;

	vmm_wallclock_get_local_time(&tv);

	return (u32)tv.tv_sec;
}

int ext4fs_devread(struct ext4fs_control *ctrl, 
		   u32 blkno, u32 blkoff, u32 buf_len, char *buf)
{
	u64 off, len;

	off = (blkno << (ctrl->log2_block_size + EXT2_SECTOR_BITS));
	off += blkoff;
	len = buf_len;
	len = vmm_blockdev_read(ctrl->bdev, (u8 *)buf, off, len);

	return (len == buf_len) ? VMM_OK : VMM_EIO;
}

int ext4fs_devwrite(struct ext4fs_control *ctrl, 
		    u32 blkno, u32 blkoff, u32 buf_len, char *buf)
{
	u64 off, len;

	off = (blkno << (ctrl->log2_block_size + EXT2_SECTOR_BITS));
	off += blkoff;
	len = buf_len;
	len = vmm_blockdev_write(ctrl->bdev, (u8 *)buf, off, len);

	return (len == buf_len) ? VMM_OK : VMM_EIO;
}

int ext4fs_control_read_inode(struct ext4fs_control *ctrl, 
			      u32 inode_no, struct ext2_inode *inode)
{
	int rc;
	u32 g, blkno, blkoff;
	struct ext4fs_group *group;

	/* inodes are addressed from 1 onwards */
	inode_no--;

	/* determine block group */
	g = udiv32(inode_no, __le32(ctrl->sblock.inodes_per_group));
	if (g >= ctrl->group_count) {
		return VMM_EINVALID;
	}
	group = &ctrl->groups[g];

	blkno = umod32(inode_no, __le32(ctrl->sblock.inodes_per_group));
	blkno = udiv32(blkno, ctrl->inodes_per_block);
	blkno += __le32(group->grp.inode_table_id);
	blkoff = umod32(inode_no, ctrl->inodes_per_block) * ctrl->inode_size;

	/* read the inode.  */
	rc = ext4fs_devread(ctrl, blkno, blkoff,
			   sizeof(struct ext2_inode), (char *)inode);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

int ext4fs_control_write_inode(struct ext4fs_control *ctrl, 
			       u32 inode_no, struct ext2_inode *inode)
{
	int rc;
	u32 g, blkno, blkoff;
	struct ext4fs_group *group;

	/* inodes are addressed from 1 onwards */
	inode_no--;

	/* determine block group */
	g = udiv32(inode_no, __le32(ctrl->sblock.inodes_per_group));
	if (g >= ctrl->group_count) {
		return VMM_EINVALID;
	}
	group = &ctrl->groups[g];

	blkno = umod32(inode_no, __le32(ctrl->sblock.inodes_per_group));
	blkno = udiv32(blkno, ctrl->inodes_per_block);
	blkno += __le32(group->grp.inode_table_id);
	blkoff = umod32(inode_no, ctrl->inodes_per_block) * ctrl->inode_size;

	/* write the inode.  */
	rc = ext4fs_devwrite(ctrl, blkno, blkoff,
			    sizeof(struct ext2_inode), (char *)inode);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

int ext4fs_control_alloc_block(struct ext4fs_control *ctrl,
			       u32 inode_no, u32 *blkno) 
{
	bool found;
	u32 g, group_count, b, bmask, blocks_per_group;
	struct ext4fs_group *group;

	/* inodes are addressed from 1 onwards */
	inode_no--;

	/* alloc free indoe from a block group */
	blocks_per_group = __le32(ctrl->sblock.blocks_per_group); 
	g = udiv32(inode_no, __le32(ctrl->sblock.inodes_per_group));
	if (g >= ctrl->group_count) {
		return VMM_EINVALID;
	}
	found = FALSE;
	group_count = ctrl->group_count;
	group = NULL;
	while (group_count) {
		group = &ctrl->groups[g];

		vmm_mutex_lock(&group->grp_lock);
		if (__le16(group->grp.free_blocks)) {
			for (b = 0; b < blocks_per_group; b++) {
				bmask = 1 << (b & 0x7);
				if (!(group->block_bmap[b] & bmask)) {
					break;
				}
			}
			if (b == blocks_per_group) {
				vmm_mutex_unlock(&group->grp_lock);
				return VMM_ENOTAVAIL;
			}
			group->grp.free_blocks = 
				__le16((__le16(group->grp.free_blocks) - 1));
			group->block_bmap[b >> 3] |= bmask;
			group->grp_dirty = TRUE;
			found = TRUE;
			*blkno = b + g * blocks_per_group + 
					__le32(ctrl->sblock.first_data_block);
		}
		vmm_mutex_unlock(&group->grp_lock);

		if (found) {
			break;
		}

		g++;
		if (g >= ctrl->group_count) {
			g = 0;
		}
		group_count--;
	}
	if (!found) {
		return VMM_ENOTAVAIL;
	}

	/* update superblock */
	vmm_mutex_lock(&ctrl->sblock_lock);
	ctrl->sblock.free_blocks = 
				__le32((__le32(ctrl->sblock.free_blocks) - 1));
	ctrl->sblock_dirty = TRUE;
	vmm_mutex_unlock(&ctrl->sblock_lock);

	return VMM_OK;
}

int ext4fs_control_free_block(struct ext4fs_control *ctrl, u32 blkno) 
{
	u32 g, b;
	struct ext4fs_group *group;

	/* blocks are address from 0 onwards */
	/* For 1KB block size, block group 0 starts at block 1 */
	/* For greater than 1KB block size, block group 0 starts at block 0 */
	blkno = blkno - __le32(ctrl->sblock.first_data_block);

	/* determine block group */
	g = udiv32(blkno, __le32(ctrl->sblock.blocks_per_group));
	if (g >= ctrl->group_count) {
		return VMM_EINVALID;
	}
	group = &ctrl->groups[g];

	/* update superblock */
	vmm_mutex_lock(&ctrl->sblock_lock);
	ctrl->sblock.free_blocks = __le32((__le32(ctrl->sblock.free_blocks) + 1));
	ctrl->sblock_dirty = TRUE;
	vmm_mutex_unlock(&ctrl->sblock_lock);

	/* update block group descriptor and block group bitmap */
	vmm_mutex_lock(&group->grp_lock);
	group->grp.free_blocks = __le16((__le16(group->grp.free_blocks) + 1));
	b = umod32(blkno, __le32(ctrl->sblock.blocks_per_group));
	group->block_bmap[b >> 3] &= ~(1 << (b & 0x7));
	group->grp_dirty = TRUE;
	vmm_mutex_unlock(&group->grp_lock);

	return VMM_OK;
}

int ext4fs_control_alloc_inode(struct ext4fs_control *ctrl,
			       u32 parent_inode_no, u32 *inode_no) 
{
	bool found;
	u32 g, group_count, i, imask, inodes_per_group;
	struct ext4fs_group *group;

	/* inodes are addressed from 1 onwards */
	parent_inode_no--;

	/* alloc free indoe from a block group */
	inodes_per_group = __le32(ctrl->sblock.inodes_per_group); 
	g = udiv32(parent_inode_no, inodes_per_group);
	if (g >= ctrl->group_count) {
		return VMM_EINVALID;
	}
	found = FALSE;
	group = NULL;
	group_count = ctrl->group_count;
	while (group_count) {
		group = &ctrl->groups[g];

		vmm_mutex_lock(&group->grp_lock);
		if (__le16(group->grp.free_inodes)) {
			for (i = 0; i < inodes_per_group; i++) {
				imask = 1 << (i & 0x7);
				if (!(group->inode_bmap[i] & imask)) {
					break;
				}
			}
			if (i == inodes_per_group) {
				vmm_mutex_unlock(&group->grp_lock);
				return VMM_ENOTAVAIL;
			}
			group->grp.free_inodes = 
				__le16((__le16(group->grp.free_inodes) - 1));
			group->inode_bmap[i >> 3] |= imask;
			group->grp_dirty = TRUE;
			found = TRUE;
			*inode_no = i + g * inodes_per_group + 1;
		}
		vmm_mutex_unlock(&group->grp_lock);

		if (found) {
			break;
		}

		g++;
		if (g >= ctrl->group_count) {
			g = 0;
		}
		group_count--;
	}
	if (!found) {
		return VMM_ENOTAVAIL;
	}

	/* update superblock */
	vmm_mutex_lock(&ctrl->sblock_lock);
	ctrl->sblock.free_inodes = 
				__le32((__le32(ctrl->sblock.free_inodes) - 1));
	ctrl->sblock_dirty = TRUE;
	vmm_mutex_unlock(&ctrl->sblock_lock);

	return VMM_OK;
}

int ext4fs_control_free_inode(struct ext4fs_control *ctrl, u32 inode_no)
{
	u32 g, i;
	struct ext4fs_group *group;

	/* inodes are addressed from 1 onwards */
	inode_no--;

	/* determine block group */
	g = udiv32(inode_no, __le32(ctrl->sblock.inodes_per_group));
	if (g >= ctrl->group_count) {
		return VMM_EINVALID;
	}
	group = &ctrl->groups[g];

	/* update superblock */
	vmm_mutex_lock(&ctrl->sblock_lock);
	ctrl->sblock.free_inodes = 
				__le32((__le32(ctrl->sblock.free_inodes) + 1));
	ctrl->sblock_dirty = TRUE;
	vmm_mutex_unlock(&ctrl->sblock_lock);

	/* update block group descriptor and block group bitmap */
	vmm_mutex_lock(&group->grp_lock);
	group->grp.free_inodes = __le16((__le16(group->grp.free_inodes) + 1));
	i = umod32(inode_no, __le32(ctrl->sblock.inodes_per_group));
	group->inode_bmap[i >> 3] &= ~(1 << (i & 0x7));
	group->grp_dirty = TRUE;
	vmm_mutex_unlock(&group->grp_lock);

	return VMM_OK;
}

int ext4fs_control_sync(struct ext4fs_control *ctrl)
{
	int rc;
	u32 g, wr;
	u32 blkno, blkoff, desc_per_blk;

	/* Lock sblock */
	vmm_mutex_lock(&ctrl->sblock_lock);

	if (ctrl->sblock_dirty) {
		/* Write superblock to block device */
		wr = vmm_blockdev_write(ctrl->bdev, (u8 *)&ctrl->sblock, 
					1024, sizeof(struct ext2_sblock));
		if (wr != sizeof(struct ext2_sblock)) {
			vmm_mutex_unlock(&ctrl->sblock_lock);
			return VMM_EIO;
		}

		/* Clear sblock_dirty flag */
		ctrl->sblock_dirty = FALSE;
	}

	/* Unlock sblock */
	vmm_mutex_unlock(&ctrl->sblock_lock);

	desc_per_blk = udiv32(ctrl->block_size, 
					sizeof(struct ext2_block_group));
	for (g = 0; g < ctrl->group_count; g++) {
		/* Lock group */
		vmm_mutex_lock(&ctrl->groups[g].grp_lock);

		/* Check group dirty flag */
		if (!ctrl->groups[g].grp_dirty) {
			vmm_mutex_unlock(&ctrl->groups[g].grp_lock);
			continue;
		}

		/* Write group descriptor to block device */
		blkno = ctrl->group_table_blkno + udiv32(g, desc_per_blk);
		blkoff = umod32(g, desc_per_blk) * 
					sizeof(struct ext2_block_group);
		rc = ext4fs_devwrite(ctrl, blkno, blkoff, 
				    sizeof(struct ext2_block_group), 
				    (char *)&ctrl->groups[g].grp);
		if (rc) {
			vmm_mutex_unlock(&ctrl->groups[g].grp_lock);
			return rc;
		}
	
		/* Write block bitmap to block device */
		blkno = __le32(ctrl->groups[g].grp.block_bmap_id);
		blkoff = 0;
		rc = ext4fs_devwrite(ctrl, blkno, blkoff, ctrl->block_size, 
				     (char *)ctrl->groups[g].block_bmap);
		if (rc) {
			vmm_mutex_unlock(&ctrl->groups[g].grp_lock);
			return rc;
		}

		/* Write inode bitmap to block device */
		blkno = __le32(ctrl->groups[g].grp.inode_bmap_id);
		blkoff = 0;
		rc = ext4fs_devwrite(ctrl, blkno, blkoff, ctrl->block_size, 
				     (char *)ctrl->groups[g].inode_bmap);
		if (rc) {
			vmm_mutex_unlock(&ctrl->groups[g].grp_lock);
			return rc;
		}

		/* Clear grp_dirty flag */
		ctrl->groups[g].grp_dirty = FALSE;

		/* Unlock group */
		vmm_mutex_unlock(&ctrl->groups[g].grp_lock);
	}

	/* Flush cached data in device request queue */
	rc = vmm_blockdev_flush_cache(ctrl->bdev);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

int ext4fs_control_init(struct ext4fs_control *ctrl, 
			struct vmm_blockdev *bdev)
{
	int rc;
	u64 sb_read;
	u32 g, blkno, blkoff, desc_per_blk;

	/* Save underlying block device pointer */
	ctrl->bdev = bdev;

	/* Init superblock lock */
	INIT_MUTEX(&ctrl->sblock_lock);

	/* Read the superblock.  */
	sb_read = vmm_blockdev_read(bdev, (u8 *)&ctrl->sblock, 
				    1024, sizeof(struct ext2_sblock));
	if (sb_read != sizeof(struct ext2_sblock)) {
		rc = VMM_EIO;
		goto fail;
	}

	/* Clear the sblock_dirty flag */
	ctrl->sblock_dirty = FALSE;

	/* Make sure this is an ext2 filesystem.  */
	if (__le16(ctrl->sblock.magic) != EXT2_MAGIC) {
		rc = VMM_ENOSYS;
		goto fail;
	}

	/* Directory indexing not supported */
	if (__le32(ctrl->sblock.feature_compatibility) & 
					EXT2_FEAT_COMPAT_DIR_INDEX) {
		rc = VMM_ENOSYS;
		goto fail;
	}

	/* Pre-compute frequently required values */
	ctrl->log2_block_size = __le32((ctrl)->sblock.log2_block_size) + 1;
	ctrl->block_size = 1 << (ctrl->log2_block_size + EXT2_SECTOR_BITS);
	ctrl->dir_blklast = EXT2_DIRECT_BLOCKS;
	ctrl->indir_blklast = EXT2_DIRECT_BLOCKS + (ctrl->block_size / 4);
	ctrl->dindir_blklast = EXT2_DIRECT_BLOCKS + 
			(ctrl->block_size / 4 * (ctrl->block_size / 4 + 1));
	if (__le32(ctrl->sblock.revision_level) == 0) {
		ctrl->inode_size = 128;
	} else {
		ctrl->inode_size = __le16(ctrl->sblock.inode_size);
	}
	ctrl->inodes_per_block = udiv32(ctrl->block_size, ctrl->inode_size);

	/* Setup block groups */
	ctrl->group_count = udiv32(__le32(ctrl->sblock.total_blocks), 
				   __le32(ctrl->sblock.blocks_per_group));
	if (umod32(__le32(ctrl->sblock.total_blocks), 
			__le32(ctrl->sblock.blocks_per_group))) {
		ctrl->group_count++;
	}
	ctrl->group_table_blkno = __le32(ctrl->sblock.first_data_block) + 1;
	ctrl->groups = vmm_zalloc(ctrl->group_count * 
						sizeof(struct ext4fs_group));
	if (!ctrl->groups) {
		rc = VMM_ENOMEM;
		goto fail;
	}
	desc_per_blk = udiv32(ctrl->block_size, 
					sizeof(struct ext2_block_group));
	for (g = 0; g < ctrl->group_count; g++) {
		/* Init group lock */
		INIT_MUTEX(&ctrl->groups[g].grp_lock);

		/* Load descriptor */
		blkno = ctrl->group_table_blkno + udiv32(g, desc_per_blk);
		blkoff = umod32(g, desc_per_blk) * 
					sizeof(struct ext2_block_group);
		rc = ext4fs_devread(ctrl, blkno, blkoff, 
				    sizeof(struct ext2_block_group), 
				    (char *)&ctrl->groups[g].grp);
		if (rc) {
			goto fail1;
		}

		/* Load group block bitmap */
		ctrl->groups[g].block_bmap = vmm_zalloc(ctrl->block_size);
		if (!ctrl->groups[g].block_bmap) {
			rc = VMM_ENOMEM;
			goto fail1;
		}
		blkno = __le32(ctrl->groups[g].grp.block_bmap_id);
		blkoff = 0;
		rc = ext4fs_devread(ctrl, blkno, blkoff, ctrl->block_size, 
				    (char *)ctrl->groups[g].block_bmap);
		if (rc) {
			goto fail1;
		}

		/* Load group inode bitmap */
		ctrl->groups[g].inode_bmap = vmm_zalloc(ctrl->block_size);
		if (!ctrl->groups[g].inode_bmap) {
			rc = VMM_ENOMEM;
			goto fail1;
		}
		blkno = __le32(ctrl->groups[g].grp.inode_bmap_id);
		blkoff = 0;
		rc = ext4fs_devread(ctrl, blkno, blkoff, ctrl->block_size, 
				    (char *)ctrl->groups[g].inode_bmap);
		if (rc) {
			goto fail1;
		}

		/* Clear grp_dirty flag */
		ctrl->groups[g].grp_dirty = FALSE;
	}

	return VMM_OK;

fail1:
	for (g = 0; g < ctrl->group_count; g++) {
		if (ctrl->groups[g].block_bmap) {
			vmm_free(ctrl->groups[g].block_bmap);
			ctrl->groups[g].block_bmap = NULL;
		}
		if (ctrl->groups[g].inode_bmap) {
			vmm_free(ctrl->groups[g].inode_bmap);
			ctrl->groups[g].inode_bmap = NULL;
		}
	}
	vmm_free(ctrl->groups);
fail:
	return rc;
}

int ext4fs_control_exit(struct ext4fs_control *ctrl)
{
	u32 g;

	/* Free group bitmaps */
	for (g = 0; g < ctrl->group_count; g++) {
		if (ctrl->groups[g].block_bmap) {
			vmm_free(ctrl->groups[g].block_bmap);
			ctrl->groups[g].block_bmap = NULL;
		}
		if (ctrl->groups[g].inode_bmap) {
			vmm_free(ctrl->groups[g].inode_bmap);
			ctrl->groups[g].inode_bmap = NULL;
		}
	}

	/* Free groups */
	vmm_free(ctrl->groups);

	return VMM_OK;
}

