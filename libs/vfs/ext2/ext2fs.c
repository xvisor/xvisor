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
 * @file ext2fs.c
 * @author Anup Patel. (anup@brainfault.org)
 * @brief Ext2 filesystem driver
 *
 * Ext2 is a very widely used filesystem in all unix-like OSes such
 * as Linux, FreeBSD, NetBSD, etc.
 * 
 * For more info, visit http://www.nongnu.org/ext2-doc/ext2.html
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <vmm_wallclock.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <libs/vfs.h>

#define MODULE_DESC			"Ext2 Filesystem Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VFS_IPRIORITY + 1)
#define	MODULE_INIT			ext2fs_init
#define	MODULE_EXIT			ext2fs_exit

#define __le32(x)			vmm_le32_to_cpu(x)
#define __le16(x)			vmm_le16_to_cpu(x)

/* Magic value used to identify an ext2 filesystem.  */
#define	EXT2_MAGIC			0xEF53

/* Amount of indirect blocks in an inode.  */
#define EXT2_DIRECT_BLOCKS		12

/* Bits used as offset in sector */
#define EXT2_SECTOR_BITS		9
#define EXT2_SECTOR_SIZE		512

/* Maximum file size (2 TB) */
#define EXT2_MAX_FILE_SIZE		0x20000000000ULL

/* The ext2 superblock.  */
struct ext2_sblock {
	u32 total_inodes;
	u32 total_blocks;
	u32 reserved_blocks;
	u32 free_blocks;
	u32 free_inodes;
	u32 first_data_block;
	u32 log2_block_size;
	u32 log2_fragment_size;
	u32 blocks_per_group;
	u32 fragments_per_group;
	u32 inodes_per_group;
	u32 mtime;
	u32 utime;
	u16 mnt_count;
	u16 max_mnt_count;
	u16 magic;
	u16 fs_state;
	u16 error_handling;
	u16 minor_revision_level;
	u32 lastcheck;
	u32 checkinterval;
	u32 creator_os;
	u32 revision_level;
	u16 uid_reserved;
	u16 gid_reserved;
	u32 first_inode;
	u16 inode_size;
	u16 block_group_number;
	u32 feature_compatibility;
	u32 feature_incompat;
	u32 feature_ro_compat;
	u32 unique_id[4];
	char volume_name[16];
	char last_mounted_on[64];
	u32 compression_info;
}__packed;

/* FS States */
#define EXT2_VALID_FS			1	/* Unmounted cleanly */
#define EXT2_ERROR_FS			2	/* Errors detected */

/* Error Handling */
#define EXT2_ERRORS_CONTINUE		1	/* continue as if nothing happened */
#define EXT2_ERRORS_RO			2	/* remount read-only */
#define EXT2_ERRORS_PANIC		3	/* cause a kernel panic */

/* Creator OS */
#define EXT2_OS_LINUX			0	/* Linux */
#define EXT2_OS_HURD			1	/* GNU HURD */
#define EXT2_OS_MASIX			2	/* MASIX */
#define EXT2_OS_FREEBSD			3	/* FreeBSD */
#define EXT2_OS_LITES			4	/* Lites */

/* Revision Level */
#define EXT2_GOOD_OLD_REV		0	/* Revision 0 */
#define EXT2_DYNAMIC_REV		1	/* Revision 1 with variable inode sizes, extended attributes, etc. */

/* Feature Compatibility */
#define EXT2_FEAT_COMPAT_DIR_PREALLOC	0x0001	/* Block pre-allocation for new directories */
#define EXT2_FEAT_COMPAT_IMAGIC_INODES	0x0002	 
#define EXT3_FEAT_COMPAT_HAS_JOURNAL	0x0004	/* An Ext3 journal exists */
#define EXT2_FEAT_COMPAT_EXT_ATTR	0x0008	/* Extended inode attributes are present */
#define EXT2_FEAT_COMPAT_RESIZE_INO	0x0010	/* Non-standard inode size used */
#define EXT2_FEAT_COMPAT_DIR_INDEX	0x0020	/* Directory indexing (HTree) */

/* Feature Incompatibility */
#define EXT2_FEAT_INCOMPAT_COMPRESSION	0x0001	/* Disk/File compression is used */
#define EXT2_FEAT_INCOMPAT_FILETYPE	0x0002	 
#define EXT3_FEAT_INCOMPAT_RECOVER	0x0004	 
#define EXT3_FEAT_INCOMPAT_JOURNAL_DEV	0x0008	 
#define EXT2_FEAT_INCOMPAT_META_BG	0x0010

/* Feature Read-Only Compatibility */
#define EXT2_FEAT_RO_COMPAT_SPARS_SUPER	0x0001	/* Sparse Superblock */
#define EXT2_FEAT_RO_COMPAT_LARGE_FILE	0x0002	/* Large file support, 64-bit file size */
#define EXT2_FEAT_RO_COMPAT_BTREE_DIR	0x0004	/* Binary tree sorted directory files */

/* Compression Algo Bitmap */
#define EXT2_LZV1_ALG			0	/* Binary value of 0x00000001 */
#define EXT2_LZRW3A_ALG			1	/* Binary value of 0x00000002 */
#define EXT2_GZIP_ALG			2	/* Binary value of 0x00000004 */
#define EXT2_BZIP2_ALG			3	/* Binary value of 0x00000008 */
#define EXT2_LZO_ALG			4	/* Binary value of 0x00000010 */

/* The ext2 blockgroup.  */
struct ext2_block_group {
	u32 block_bmap_id;
	u32 inode_bmap_id;
	u32 inode_table_id;
	u16 free_blocks;
	u16 free_inodes;
	u16 used_dir_cnt;
	u32 reserved[3];
}__packed;

/* The ext2 inode.  */
struct ext2_inode {
	u16 mode;
	u16 uid;
	u32 size;
	u32 atime;
	u32 ctime;
	u32 mtime;
	u32 dtime;
	u16 gid;
	u16 nlinks;
	u32 blockcnt;	/* Blocks of 512 bytes!! */
	u32 flags;
	u32 osd1;
	union {
		struct datablocks {
			u32 dir_blocks[EXT2_DIRECT_BLOCKS];
			u32 indir_block;
			u32 double_indir_block;
			u32 tripple_indir_block;
		} blocks;
		char symlink[60];
	} b;
	u32 version;
	u32 acl;
	u32 dir_acl;
	u32 fragment_addr;
	u32 osd2[3];
}__packed;

/* Inode modes */
#define EXT2_S_IFMASK			0xF000  /* Inode type mask */
#define EXT2_S_IFSOCK			0xC000	/* socket */
#define EXT2_S_IFLNK			0xA000	/* symbolic link */
#define EXT2_S_IFREG			0x8000	/* regular file */
#define EXT2_S_IFBLK			0x6000	/* block device */
#define EXT2_S_IFDIR			0x4000	/* directory */
#define EXT2_S_IFCHR			0x2000	/* character device */
#define EXT2_S_IFIFO			0x1000	/* fifo */
	/* -- process execution user/group override -- */
#define EXT2_S_ISUID			0x0800	/* Set process User ID */
#define EXT2_S_ISGID			0x0400	/* Set process Group ID */
#define EXT2_S_ISVTX			0x0200	/* sticky bit */
	/* -- access rights -- */
#define EXT2_S_IRUSR			0x0100	/* user read */
#define EXT2_S_IWUSR			0x0080	/* user write */
#define EXT2_S_IXUSR			0x0040	/* user execute */
#define EXT2_S_IRGRP			0x0020	/* group read */
#define EXT2_S_IWGRP			0x0010	/* group write */
#define EXT2_S_IXGRP			0x0008	/* group execute */
#define EXT2_S_IROTH			0x0004	/* others read */
#define EXT2_S_IWOTH			0x0002	/* others write */
#define EXT2_S_IXOTH			0x0001	/* others execute */

/* Inode flags */
#define EXT2_SECRM_FL			0x00000001	/* secure deletion */
#define EXT2_UNRM_FL			0x00000002	/* record for undelete */
#define EXT2_COMPR_FL			0x00000004	/* compressed file */
#define EXT2_SYNC_FL			0x00000008	/* synchronous updates */
#define EXT2_IMMUTABLE_FL		0x00000010	/* immutable file */
#define EXT2_APPEND_FL			0x00000020	/* append only */
#define EXT2_NODUMP_FL			0x00000040	/* do not dump/delete file */
#define EXT2_NOATIME_FL			0x00000080	/* do not update .i_atime */
	/* -- Reserved for compression usage -- */
#define EXT2_DIRTY_FL			0x00000100	/* Dirty (modified) */
#define EXT2_COMPRBLK_FL		0x00000200	/* compressed blocks */
#define EXT2_NOCOMPR_FL			0x00000400	/* access raw compressed data */
#define EXT2_ECOMPR_FL			0x00000800	/* compression error */
	/* -- End of compression flags -- */
#define EXT2_BTREE_FL			0x00001000	/* b-tree format directory */
#define EXT2_INDEX_FL			0x00001000	/* hash indexed directory */
#define EXT2_IMAGIC_FL			0x00002000	/* AFS directory */
#define EXT3_JOURNAL_DATA_FL		0x00004000	/* journal file data */
#define EXT2_RESERVED_FL		0x80000000	/* reserved for ext2 library */

/* The ext2 directory entry. */
struct ext2_dirent {
	u32 inode;
	u16 direntlen;
	u8 namelen;
	u8 filetype;
}__packed;

/* Directory entry file types */
#define EXT2_FT_UNKNOWN			0	/* Unknown File Type */
#define EXT2_FT_REG_FILE		1	/* Regular File */
#define EXT2_FT_DIR			2	/* Directory File */
#define EXT2_FT_CHRDEV			3	/* Character Device */
#define EXT2_FT_BLKDEV			4	/* Block Device */
#define EXT2_FT_FIFO			5	/* Buffer File */
#define EXT2_FT_SOCK			6	/* Socket File */
#define EXT2_FT_SYMLINK			7	/* Symbolic Link */

/* Information for accessing a ext2 file/directory. */
struct ext2fs_node {
	/* Parent Ext2 control */
	struct ext2fs_control *ctrl;

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
};

/* Information for accessing block groups. */
struct ext2fs_group {
	/* lock to protect group */
	struct vmm_mutex grp_lock;
	struct ext2_block_group grp;

	u8 *block_bmap;
	u8 *inode_bmap;

	bool grp_dirty;
};

/* Information about a "mounted" ext2 filesystem. */
struct ext2fs_control {
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
	struct ext2fs_group *groups;
};

/* 
 * Helper routines 
 */

static u32 ext2fs_current_timestamp(void)
{
	struct vmm_timeval tv;

	vmm_wallclock_get_local_time(&tv);

	return (u32)tv.tv_sec;
}

static int ext2fs_devread(struct ext2fs_control *ctrl, 
			  u32 blkno, u32 blkoff, 
			  u32 buf_len, char *buf)
{
	u64 off, len;

	off = (blkno << (ctrl->log2_block_size + EXT2_SECTOR_BITS));
	off += blkoff;
	len = buf_len;
	len = vmm_blockdev_read(ctrl->bdev, (u8 *)buf, off, len);

	return (len == buf_len) ? VMM_OK : VMM_EIO;
}

static int ext2fs_devwrite(struct ext2fs_control *ctrl, 
			   u32 blkno, u32 blkoff, 
			   u32 buf_len, char *buf)
{
	u64 off, len;

	off = (blkno << (ctrl->log2_block_size + EXT2_SECTOR_BITS));
	off += blkoff;
	len = buf_len;
	len = vmm_blockdev_write(ctrl->bdev, (u8 *)buf, off, len);

	return (len == buf_len) ? VMM_OK : VMM_EIO;
}

static int ext2fs_control_read_inode(struct ext2fs_control *ctrl, 
			     u32 inode_no, struct ext2_inode *inode)
{
	int rc;
	u32 g, blkno, blkoff;
	struct ext2fs_group *group;

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
	rc = ext2fs_devread(ctrl, blkno, blkoff,
			    sizeof(struct ext2_inode), (char *)inode);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

static int ext2fs_control_write_inode(struct ext2fs_control *ctrl, 
			     u32 inode_no, struct ext2_inode *inode)
{
	int rc;
	u32 g, blkno, blkoff;
	struct ext2fs_group *group;

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
	rc = ext2fs_devwrite(ctrl, blkno, blkoff,
			    sizeof(struct ext2_inode), (char *)inode);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

static int ext2fs_control_alloc_block(struct ext2fs_control *ctrl,
				u32 inode_no, u32 *blkno) 
{
	bool found;
	u32 g, group_count, b, bmask, blocks_per_group;
	struct ext2fs_group *group;

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

static int ext2fs_control_free_block(struct ext2fs_control *ctrl, u32 blkno) 
{
	u32 g, b;
	struct ext2fs_group *group;

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

static int ext2fs_control_alloc_inode(struct ext2fs_control *ctrl,
				u32 parent_inode_no, u32 *inode_no) 
{
	bool found;
	u32 g, group_count, i, imask, inodes_per_group;
	struct ext2fs_group *group;

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

static int ext2fs_control_free_inode(struct ext2fs_control *ctrl, u32 inode_no)
{
	u32 g, i;
	struct ext2fs_group *group;

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

static int ext2fs_control_sync(struct ext2fs_control *ctrl)
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
		blkno = __le32(ctrl->sblock.first_data_block) + 
					1 + udiv32(g, desc_per_blk);
		blkoff = umod32(g, desc_per_blk) * 
					sizeof(struct ext2_block_group);
		rc = ext2fs_devwrite(ctrl, blkno, blkoff, 
				    sizeof(struct ext2_block_group), 
				    (char *)&ctrl->groups[g].grp);
		if (rc) {
			vmm_mutex_unlock(&ctrl->groups[g].grp_lock);
			return rc;
		}
	
		/* Write block bitmap to block device */
		blkno = __le32(ctrl->groups[g].grp.block_bmap_id);
		blkoff = 0;
		rc = ext2fs_devwrite(ctrl, blkno, blkoff, ctrl->block_size, 
				     (char *)ctrl->groups[g].block_bmap);
		if (rc) {
			vmm_mutex_unlock(&ctrl->groups[g].grp_lock);
			return rc;
		}

		/* Write inode bitmap to block device */
		blkno = __le32(ctrl->groups[g].grp.inode_bmap_id);
		blkoff = 0;
		rc = ext2fs_devwrite(ctrl, blkno, blkoff, ctrl->block_size, 
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

	return VMM_OK;
}

static int ext2fs_control_init(struct ext2fs_control *ctrl,
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
	ctrl->groups = vmm_zalloc(ctrl->group_count * 
						sizeof(struct ext2fs_group));
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
		blkno = __le32(ctrl->sblock.first_data_block) + 
					1 + udiv32(g, desc_per_blk);
		blkoff = umod32(g, desc_per_blk) * 
					sizeof(struct ext2_block_group);
		rc = ext2fs_devread(ctrl, blkno, blkoff, 
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
		rc = ext2fs_devread(ctrl, blkno, blkoff, ctrl->block_size, 
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
		rc = ext2fs_devread(ctrl, blkno, blkoff, ctrl->block_size, 
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

static int ext2fs_control_exit(struct ext2fs_control *ctrl)
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

static u64 ext2fs_node_get_size(struct ext2fs_node *node)
{
	u64 ret = __le32(node->inode.size);

	if (__le32(node->ctrl->sblock.revision_level) != 0) {
		ret |= ((u64)__le32(node->inode.dir_acl)) << 32;
	}

	return ret;
}

static void ext2fs_node_set_size(struct ext2fs_node *node, u64 size)
{
	node->inode.size = __le32((u32)(size & 0xFFFFFFFFULL));
	if (__le32(node->ctrl->sblock.revision_level) != 0) {
		node->inode.dir_acl = __le32((u32)(size >> 32));
	}
	node->inode.blockcnt = __le32((u32)(size >> EXT2_SECTOR_BITS));
	node->inode_dirty = TRUE;
}

static int ext2fs_node_read_blk(struct ext2fs_node *node,
				u32 blkno, u32 blkoff, 
				u32 blklen, char *buf)
{
	int rc;
	struct ext2fs_control *ctrl = node->ctrl;

	if (blklen > ctrl->block_size) {
		return VMM_EINVALID;
	}

	/* If the block number is 0 then 
	 * this block is not stored on disk
	 * but is zero filled instead.  
	 */
	if (!blkno) {
		memset(buf, 0, blklen);
		return VMM_OK;
	}

	if (!node->cached_block) {
		node->cached_block = vmm_zalloc(ctrl->block_size);
		if (!node->cached_block) {
			return VMM_ENOMEM;
		}
	}
	if (node->cached_blkno != blkno) {
		if (node->cached_dirty) {
			rc = ext2fs_devwrite(ctrl, node->cached_blkno,
					   0, ctrl->block_size, 
					   (char *)node->cached_block);
			if (rc) {
				return rc;
			}
			node->cached_dirty = FALSE;
		}
		rc = ext2fs_devread(ctrl, blkno,
				    0, ctrl->block_size, 
				    (char *)node->cached_block);
		if (rc) {
			return rc;
		}
		node->cached_blkno = blkno;
	}

	memcpy(buf, &node->cached_block[blkoff], blklen);			

	return VMM_OK;
}

static int ext2fs_node_write_blk(struct ext2fs_node *node,
				u32 blkno, u32 blkoff, 
				u32 blklen, char *buf)
{
	int rc;
	struct ext2fs_control *ctrl = node->ctrl;

	if (blklen > ctrl->block_size) {
		return VMM_EINVALID;
	}

	/* We skip writes to block number 0
	 * since its expected to be zero filled.
	 */
	if (!blkno) {
		return VMM_OK;
	}

	if (!node->cached_block) {
		node->cached_block = vmm_zalloc(ctrl->block_size);
		if (!node->cached_block) {
			return VMM_ENOMEM;
		}
	}
	if (node->cached_blkno != blkno) {
		if (node->cached_dirty) {
			rc = ext2fs_devwrite(ctrl, node->cached_blkno,
					   0, ctrl->block_size, 
					   (char *)node->cached_block);
			if (rc) {
				return rc;
			}
			node->cached_dirty = FALSE;
		}
		if (blkoff != 0 ||
		    blklen != ctrl->block_size) {
			rc = ext2fs_devread(ctrl, blkno,
					    0, ctrl->block_size, 
					    (char *)node->cached_block);
			if (rc) {
				return rc;
			}
			node->cached_blkno = blkno;
		}
	}

	memcpy(&node->cached_block[blkoff], buf, blklen);
	node->cached_dirty = TRUE;

	return VMM_OK;
}

static int ext2fs_node_sync(struct ext2fs_node *node)
{
	int rc;
	struct ext2fs_control *ctrl = node->ctrl;

	if (node->inode_dirty) {
		rc = ext2fs_control_write_inode(ctrl, 
					node->inode_no, &node->inode);
		if (rc) {
			return rc;
		}
		node->inode_dirty = FALSE;
	}

	if (node->cached_block && node->cached_dirty) {
		rc = ext2fs_devwrite(ctrl, node->cached_blkno, 0, 
				ctrl->block_size, (char *)node->cached_block);
		if (rc) {
			return rc;
		}
		node->cached_dirty = FALSE;
	}

	if (node->indir_block && node->indir_dirty) {
		rc = ext2fs_devwrite(ctrl, node->indir_blkno, 0, 
				ctrl->block_size, (char *)node->indir_block);
		if (rc) {
			return rc;
		}
		node->indir_dirty = FALSE;
	}

	if (node->dindir1_block && node->dindir1_dirty) {
		rc = ext2fs_devwrite(ctrl, node->dindir1_blkno, 0, 
				ctrl->block_size, (char *)node->dindir1_block);
		if (rc) {
			return rc;
		}
		node->dindir1_dirty = FALSE;
	}

	if (node->dindir2_block && node->dindir2_dirty) {
		rc = ext2fs_devwrite(ctrl, node->dindir2_blkno, 0, 
				ctrl->block_size, (char *)node->dindir2_block);
		if (rc) {
			return rc;
		}
		node->dindir2_dirty = FALSE;
	}

	return VMM_OK;
}

static int ext2fs_node_read_blkno(struct ext2fs_node *node, 
				  u32 blkpos, u32 *blkno) 
{
	int rc;
	u32 dindir2_blkno;
	struct ext2_inode *inode = &node->inode;
	struct ext2fs_control *ctrl = node->ctrl;

	if (blkpos < ctrl->dir_blklast) {
		/* Direct blocks.  */
		*blkno = __le32(inode->b.blocks.dir_blocks[blkpos]);
	} else if (blkpos < ctrl->indir_blklast) {
		/* Indirect.  */
		u32 indir_blkpos = blkpos - ctrl->dir_blklast;

		if (!node->indir_block) {
			node->indir_block = vmm_malloc(ctrl->block_size);
			if (!node->indir_block) {
				return VMM_ENOMEM;
			}
			rc = ext2fs_devread(ctrl, node->indir_blkno, 0, 
				ctrl->block_size, (char *)node->indir_block);
			if (rc) {
				return rc;
			}
		}

		*blkno = __le32(node->indir_block[indir_blkpos]);
	} else if (blkpos < ctrl->dindir_blklast) {
		/* Double indirect.  */
		u32 t = blkpos - ctrl->indir_blklast;
		u32 dindir1_blkpos = udiv32(t, ctrl->block_size / 4);
		u32 dindir2_blkpos = t - dindir1_blkpos * (ctrl->block_size / 4);

		if (!node->dindir1_block) {
			node->dindir1_block = vmm_malloc(ctrl->block_size);
			if (!node->dindir1_block) {
				return VMM_ENOMEM;
			}
			rc = ext2fs_devread(ctrl, node->dindir1_blkno, 0,
				ctrl->block_size, (char *)node->dindir1_block);
			if (rc) {
				return rc;
			}
		}

		dindir2_blkno = __le32(node->dindir1_block[dindir1_blkpos]);

		if (!node->dindir2_block) {
			node->dindir2_block = vmm_malloc(ctrl->block_size);
			if (!node->dindir2_block) {
				return VMM_ENOMEM;
			}
			node->dindir2_blkno = 0;
		}
		if (dindir2_blkno != node->dindir2_blkno) {
			if (node->dindir2_dirty) {
				rc = ext2fs_devwrite(ctrl, node->dindir2_blkno,
						  0, ctrl->block_size, 
						  (char *)node->dindir2_block);
				if (rc) {
					return rc;
				}
				node->dindir2_dirty = FALSE;
			}
			rc = ext2fs_devread(ctrl, dindir2_blkno, 0,  
				ctrl->block_size, (char *)node->dindir2_block);
			if (rc) {
				return rc;
			}
			node->dindir2_blkno = dindir2_blkno;
		}

		*blkno = __le32(node->dindir2_block[dindir2_blkpos]);
	} else {
		/* Tripple indirect.  */
		return VMM_EFAIL;
	}

	return VMM_OK;
}

static int ext2fs_node_write_blkno(struct ext2fs_node *node, 
				   u32 blkpos, u32 blkno) 
{
	int rc;
	u32 dindir2_blkno;
	struct ext2_inode *inode = &node->inode;
	struct ext2fs_control *ctrl = node->ctrl;

	if (blkpos < ctrl->dir_blklast) {
		/* Direct blocks.  */
		inode->b.blocks.dir_blocks[blkpos] = __le32(blkno);
		node->inode_dirty = TRUE;
	} else if (blkpos < ctrl->indir_blklast) {
		/* Indirect.  */
		u32 indir_blkpos = blkpos - ctrl->dir_blklast;

		if (!node->indir_block) {
			node->indir_block = vmm_malloc(ctrl->block_size);
			if (!node->indir_block) {
				return VMM_ENOMEM;
			}
			rc = ext2fs_devread(ctrl, node->indir_blkno, 0, 
				ctrl->block_size, (char *)node->indir_block);
			if (rc) {
				return rc;
			}
		}

		node->indir_block[indir_blkpos] = __le32(blkno);
		node->indir_dirty = TRUE;
	} else if (blkpos < ctrl->dindir_blklast) {
		/* Double indirect.  */
		u32 t = blkpos - ctrl->indir_blklast;
		u32 dindir1_blkpos = udiv32(t, ctrl->block_size / 4);
		u32 dindir2_blkpos = t - dindir1_blkpos * (ctrl->block_size / 4);

		if (!node->dindir1_block) {
			node->dindir1_block = vmm_malloc(ctrl->block_size);
			if (!node->dindir1_block) {
				return VMM_ENOMEM;
			}
			rc = ext2fs_devread(ctrl, node->dindir1_blkno, 0,
				ctrl->block_size, (char *)node->dindir1_block);
			if (rc) {
				return rc;
			}
		}

		dindir2_blkno = __le32(node->dindir1_block[dindir1_blkpos]);

		if (!node->dindir2_block) {
			node->dindir2_block = vmm_malloc(ctrl->block_size);
			if (!node->dindir2_block) {
				return VMM_ENOMEM;
			}
			node->dindir2_blkno = 0;
		}
		if (dindir2_blkno != node->dindir2_blkno) {
			if (node->dindir2_dirty) {
				rc = ext2fs_devwrite(ctrl, node->dindir2_blkno,
						  0, ctrl->block_size, 
						  (char *)node->dindir2_block);
				if (rc) {
					return rc;
				}
				node->dindir2_dirty = FALSE;
			}
			if (!dindir2_blkno) {
				rc = ext2fs_control_alloc_block(ctrl, 
					node->inode_no, &dindir2_blkno);
				if (rc) {
					return rc;
				}
				node->dindir1_block[dindir1_blkpos] = 
							__le32(dindir2_blkno);
				node->dindir1_dirty = TRUE;
				memset(node->dindir2_block, 0, ctrl->block_size);
			} else {
				rc = ext2fs_devread(ctrl, dindir2_blkno, 
						0, ctrl->block_size, 
						(char *)node->dindir2_block);
				if (rc) {
					return rc;
				}
			}
			node->dindir2_blkno = dindir2_blkno;
		}

		node->dindir2_block[dindir2_blkpos] = __le32(blkno);
		node->dindir2_dirty = TRUE;
	} else {
		/* Tripple indirect.  */
		return VMM_EFAIL;
	}

	return VMM_OK;
}

/* Note: Node position has to be 64-bit */
static u32 ext2fs_node_read(struct ext2fs_node *node, 
			    u64 pos, u32 len, char *buf) 
{
	int rc;
	u64 filesize = ext2fs_node_get_size(node);
	u32 i, rlen, blkno, blkoff, blklen;
	u32 last_blkpos, last_blklen;
	u32 first_blkpos, first_blkoff, first_blklen;
	struct ext2fs_control *ctrl = node->ctrl;

	if (filesize <= pos) {
		return 0;
	}
	if (filesize < (len + pos)) {
		len = filesize - pos;
	}

	/* Note: div result < 32-bit */
	first_blkpos = udiv64(pos, ctrl->block_size); 
	first_blkoff = pos - (first_blkpos * ctrl->block_size);
	first_blklen = ctrl->block_size - first_blkoff;
	if (len < first_blklen) {
		first_blklen = len;
	}

	/* Note: div result < 32-bit */
	last_blkpos = udiv64((len + pos), ctrl->block_size); 
	last_blklen = (len + pos) - (last_blkpos * ctrl->block_size);

	rlen = len;
	i = first_blkpos;
	while (rlen) {
		rc = ext2fs_node_read_blkno(node, i, &blkno);
		if (rc) {
			goto done;
		}

		if (i == first_blkpos) {
			/* First block.  */
			blkoff = first_blkoff;
			blklen = first_blklen;
		} else if (i == last_blkpos) {
			/* Last block.  */
			blkoff = 0;
			blklen = last_blklen;
		} else {
			/* Middle block. */
			blkoff = 0;
			blklen = ctrl->block_size;
		}

		/* Read cached block */
		rc = ext2fs_node_read_blk(node, blkno, blkoff, blklen, buf);
		if (rc) {
			goto done;
		}

		buf += blklen;
		rlen -= blklen;
		i++;
	}

done:
	return len - rlen;
}

/* TODO: */
#if 0
static char *ext2fs_node_read_symlink(struct ext2fs_node *node)
{
	int rc;
	u32 rlen
	char *symlink;

	symlink = vmm_malloc(ext2fs_node_get_size(node) + 1);
	if (!symlink) {
		return NULL;
	}

	/* If the filesize of the symlink is bigger than
	   60 the symlink is stored in a separate block,
	   otherwise it is stored in the inode.  */
	if (ext2fs_node_get_size(node) <= 60) {
		strncpy(symlink, node->inode.b.symlink,
			ext2fs_node_get_size(node));
	} else {
		rlen = ext2fs_node_read(node, 0,
				ext2fs_node_get_size(node), symlink);
		if (rlen != ext2fs_node_get_size(node)) {
			vmm_free(symlink);
			return NULL;
		}
	}
	symlink[ext2fs_node_get_size(node)] = '\0';

	return (symlink);
}
#endif

static u32 ext2fs_node_write(struct ext2fs_node *node, 
			     u64 pos, u32 len, char *buf) 
{
	int rc;
	bool update_nodesize = FALSE, alloc_newblock = FALSE;
	u32 wlen, blkpos, blkno, blkoff, blklen;
	u64 wpos, filesize = ext2fs_node_get_size(node);
	struct ext2fs_control *ctrl = node->ctrl;

	wlen = len;
	wpos = pos;
	update_nodesize = FALSE;

	while (wlen) {
		/* Note: div result < 32-bit */
		blkpos = udiv64(wpos, ctrl->block_size);
		blkoff = wpos - (blkpos * ctrl->block_size);
		blklen = ctrl->block_size - blkoff;
		blklen = (wlen < blklen) ? wlen : blklen;

		rc = ext2fs_node_read_blkno(node, blkpos, &blkno);
		if (rc) {
			goto done;
		}

		if (!blkno) {
			rc = ext2fs_control_alloc_block(ctrl, 
						node->inode_no, &blkno);
			if (rc) {
				goto done;
			}

			rc = ext2fs_node_write_blkno(node, blkpos, blkno);
			if (rc) {
				return rc;
			}

			alloc_newblock = TRUE;			
		} else {
			alloc_newblock = FALSE;
		}

		rc = ext2fs_node_write_blk(node, blkno, 
					   blkoff, blklen, buf);
		if (rc) {
			if (alloc_newblock) {
				ext2fs_control_free_block(ctrl, blkno);
				ext2fs_node_write_blkno(node, blkpos, 0);
			}
			goto done;
		}

		if (wpos >= filesize) {
			update_nodesize = TRUE;
		}

		wpos += blklen;
		buf += blklen;
		wlen -= blklen;
		if (update_nodesize) {
			filesize += blklen;
		}
	}

done:
	if (update_nodesize) {
		/* Update node size */
		ext2fs_node_set_size(node, filesize);
	}
	if (len - wlen) {
		/* Update node modify time */
		node->inode.mtime = __le32(ext2fs_current_timestamp());
		node->inode_dirty = TRUE;
	}

	return len - wlen;
}

static int ext2fs_node_truncate(struct ext2fs_node *node, u64 pos) 
{
	int rc;
	u32 blkpos, blkno;
	u32 last_blkpos;
	u32 first_blkpos, first_blkoff;
	u64 filesize = ext2fs_node_get_size(node);
	struct ext2fs_control *ctrl = node->ctrl;

	if (filesize <= pos) {
		return VMM_OK;
	}

	/* Note: div result < 32-bit */
	first_blkpos = udiv64(pos, ctrl->block_size); 
	first_blkoff = pos - (first_blkpos * ctrl->block_size);

	/* Note: div result < 32-bit */
	last_blkpos = udiv64(filesize, ctrl->block_size);

	/* If first block to truncate will have some data left
	 * then do not free first block
	 */
	if (first_blkoff) {
		blkpos = first_blkpos + 1;
	} else {
		blkpos = first_blkpos;
	}

	/* Free node blocks */
	while (blkpos < last_blkpos) {
		rc = ext2fs_node_read_blkno(node, blkpos, &blkno);
		if (rc) {
			return rc;
		}

		rc = ext2fs_control_free_block(ctrl, blkno);
		if (rc) {
			return rc;
		}

		rc = ext2fs_node_write_blkno(node, blkpos, 0);
		if (rc) {
			return rc;
		}

		blkpos++;
	}

	if (pos != filesize) {
		/* Update node mtime */
		node->inode.mtime = __le32(ext2fs_current_timestamp());
		node->inode_dirty = TRUE;
		/* Update node size */
		ext2fs_node_set_size(node, pos);
	}

	return VMM_OK;
}

static int ext2fs_node_load(struct ext2fs_control *ctrl, 
			    u32 inode_no, struct ext2fs_node *node)
{
	int rc;

	node->ctrl = ctrl;

	node->inode_no = inode_no;
	rc = ext2fs_control_read_inode(ctrl, node->inode_no, &node->inode);
	if (rc) {
		return rc;
	}
	node->inode_dirty = FALSE;

	node->cached_block = NULL;
	node->cached_blkno = 0;
	node->cached_dirty = FALSE;

	node->indir_block = NULL;
	node->indir_blkno = __le32(node->inode.b.blocks.indir_block);
	node->indir_dirty = FALSE;

	node->dindir1_block = NULL;
	node->dindir1_blkno = __le32(node->inode.b.blocks.double_indir_block);
	node->dindir1_dirty = FALSE;

	node->dindir2_block = NULL;
	node->dindir2_blkno = 0;
	node->dindir2_dirty = FALSE;

	return VMM_OK;
}

static int ext2fs_node_init(struct ext2fs_node *node)
{
	node->inode_no = 0;
	node->inode_dirty = FALSE;

	node->cached_block = NULL;
	node->cached_blkno = 0;
	node->cached_dirty = FALSE;

	node->indir_block = NULL;
	node->indir_blkno = 0;
	node->indir_dirty = FALSE;

	node->dindir1_block = NULL;
	node->dindir1_blkno = 0;
	node->dindir1_dirty = FALSE;

	node->dindir2_block = NULL;
	node->dindir2_blkno = 0;
	node->dindir2_dirty = FALSE;

	return VMM_OK;
}

static int ext2fs_node_exit(struct ext2fs_node *node)
{
	if (node->cached_block) {
		vmm_free(node->cached_block);
	}

	if (node->indir_block) {
		vmm_free(node->indir_block);
	}

	if (node->dindir1_block) {
		vmm_free(node->dindir1_block);
	}

	if (node->dindir2_block) {
		vmm_free(node->dindir2_block);
	}

	return VMM_OK;
}

static int ext2fs_node_find_dirent(struct ext2fs_node *dnode, 
				   const char *name,
				   struct ext2_dirent *dent)
{
	bool found;
	u32 rlen;
	char filename[VFS_MAX_NAME];
	u64 off, filesize = ext2fs_node_get_size(dnode);

	/* Find desired directoy entry such that we ignore
	 * "." and ".." in search process
	 */
	off = 0;
	found = FALSE;
	while (off < filesize) {
		rlen = ext2fs_node_read(dnode, off, 
				sizeof(struct ext2_dirent), (char *)dent);
		if (rlen != sizeof(struct ext2_dirent)) {
			return VMM_EIO;
		}

		if (dent->namelen > (VFS_MAX_NAME - 1)) {
			dent->namelen = (VFS_MAX_NAME - 1);
		}
		rlen = ext2fs_node_read(dnode, 
				off + sizeof(struct ext2_dirent),
				dent->namelen, filename);
		if (rlen != dent->namelen) {
			return VMM_EIO;
		}
		filename[dent->namelen] = '\0';

		if ((strcmp(filename, ".") != 0) &&
		    (strcmp(filename, "..") != 0)) {
			if (strcmp(filename, name) == 0) {
				found = TRUE;
				break;
			}
		}

		off += __le16(dent->direntlen);
	}

	if (!found) {
		return VMM_ENOENT;
	}

	return VMM_OK;
}

static int ext2fs_node_add_dirent(struct ext2fs_node *dnode, 
				  const char *name, u32 inode_no, u8 type)
{
	bool found;
	u16 direntlen;
	u32 rlen, wlen;
	char filename[VFS_MAX_NAME];
	struct ext2_dirent dent;
	struct ext2fs_control *ctrl = dnode->ctrl;
	u64 off, filesize = ext2fs_node_get_size(dnode);

	/* Sanity check */
	if (!strcmp(name, ".") || !strcmp(name, "..")) {
		return VMM_EINVALID;
	}

	/* Compute size of directory entry required */
	direntlen = sizeof(struct ext2_dirent) + strlen(name);

	/* Find directory entry to split */
	off = 0;
	found = FALSE;
	while (off < filesize) {
		rlen = ext2fs_node_read(dnode, off, 
				sizeof(struct ext2_dirent), (char *)&dent);
		if (rlen != sizeof(struct ext2_dirent)) {
			return VMM_EIO;
		}

		if (direntlen < (__le16(dent.direntlen) - dent.namelen)) {
			found = TRUE;
			break;
		}

		off += __le16(dent.direntlen);
	}

	if (!found) {
		/* Add space at end of directory to make space for
		 * new directory entry
		 */
		if ((off != filesize) ||
		    umod64(filesize, ctrl->block_size)) {
			/* Sum of length of all directory enteries 
			 * should be equal to directory filesize.
			 */
			/* Directory filesize should always be
			 * multiple of block size.
			 */
			return VMM_EUNKNOWN;
		}

		memset(filename, 0, VFS_MAX_NAME);
		for (rlen = 0; rlen < ctrl->block_size; rlen += VFS_MAX_NAME) {
			wlen = ext2fs_node_write(dnode, off + rlen, 
					VFS_MAX_NAME, (char *)filename);
			if (wlen != VFS_MAX_NAME) {
				return VMM_EIO;
			}
		}

		direntlen = ctrl->block_size;
	} else {
		/* Split existing directory entry to make space for 
		 * new directory entry
		 */
		direntlen = (__le16(dent.direntlen) - dent.namelen);
		dent.direntlen = __le16(__le16(dent.direntlen) - direntlen);

		wlen = ext2fs_node_write(dnode, off, 
				 sizeof(struct ext2_dirent), (char *)&dent);
		if (wlen != sizeof(struct ext2_dirent)) {
			return VMM_EIO;
		}

		off += __le16(dent.direntlen);
	}

	/* Add new entry at given offset and of given length */
	strncpy(filename, name, VFS_MAX_NAME);
	filename[VFS_MAX_NAME - 1] = '\0';

	dent.inode = __le32(inode_no);
	dent.direntlen = __le16(direntlen);
	dent.namelen = strlen(filename);
	dent.filetype = type;

	wlen = ext2fs_node_write(dnode, off, 
			 sizeof(struct ext2_dirent), (char *)&dent);
	if (wlen != sizeof(struct ext2_dirent)) {
		return VMM_EIO;
	}

	off += sizeof(struct ext2_dirent);

	wlen = ext2fs_node_write(dnode, off, 
			 strlen(filename), (char *)filename);
	if (wlen != sizeof(struct ext2_dirent)) {
		return VMM_EIO;
	}

	return VMM_OK;
}

static int ext2fs_node_del_dirent(struct ext2fs_node *dnode, 
				  const char *name)
{
	bool found;
	u32 rlen, wlen;
	char filename[VFS_MAX_NAME];
	struct ext2_dirent pdent, dent;
	u64 poff, off, filesize = ext2fs_node_get_size(dnode);

	/* Sanity check */
	if (!strcmp(name, ".") || !strcmp(name, "..")) {
		return VMM_EINVALID;
	}

	/* Initialize perivous entry and previous offset */
	poff = 0;
	memset(&pdent, 0, sizeof(pdent));

	/* Find the directory entry and previous entry */
	off = 0;
	found = FALSE;
	while (off < filesize) {
		rlen = ext2fs_node_read(dnode, off, 
				sizeof(struct ext2_dirent), (char *)&dent);
		if (rlen != sizeof(struct ext2_dirent)) {
			return VMM_EIO;
		}

		if (dent.namelen > (VFS_MAX_NAME - 1)) {
			dent.namelen = (VFS_MAX_NAME - 1);
		}
		rlen = ext2fs_node_read(dnode, 
				off + sizeof(struct ext2_dirent),
				dent.namelen, filename);
		if (rlen != dent.namelen) {
			return VMM_EIO;
		}
		filename[dent.namelen] = '\0';

		if ((strcmp(filename, ".") != 0) &&
		    (strcmp(filename, "..") != 0)) {
			if (strcmp(filename, name) == 0) {
				found = TRUE;
				break;
			}
		}

		poff = off;
		memcpy(&pdent, &dent, sizeof(pdent));

		off += __le16(dent.direntlen);
	}

	if (!found || !poff) {
		return VMM_ENOENT;
	}

	/* Stretch previous directory entry to delete directory entry */
	/* TODO: Handle overflow in below 16-bit addition. */
	pdent.direntlen = 
		__le16(__le16(pdent.direntlen) + __le16(dent.direntlen));
	wlen = ext2fs_node_write(dnode, poff, 
				 sizeof(struct ext2_dirent), (char *)&pdent);
	if (wlen != sizeof(struct ext2_dirent)) {
		return VMM_EIO;
	}

	return VMM_OK;
}

/* 
 * Mount point operations 
 */

static int ext2fs_mount(struct mount *m, const char *dev, u32 flags)
{
	int rc;
	u16 rootmode;
	struct ext2fs_control *ctrl;
	struct ext2fs_node *root;

	ctrl = vmm_zalloc(sizeof(struct ext2fs_control));
	if (!ctrl) {
		return VMM_ENOMEM;
	}

	/* Setup control info */
	rc = ext2fs_control_init(ctrl, m->m_dev);
	if (rc) {
		goto fail;
	}

	/* Setup root node */
	root = m->m_root->v_data;
	rc = ext2fs_node_init(root);
	if (rc) {
		goto fail;
	}
	rc = ext2fs_node_load(ctrl, 2, root);
	if (rc) {
		goto fail;
	}

	rootmode = __le16(root->inode.mode);

	switch (rootmode & EXT2_S_IFMASK) {
	case EXT2_S_IFSOCK:
		m->m_root->v_type = VSOCK;
		break;
	case EXT2_S_IFLNK:
		m->m_root->v_type = VLNK;
		break;
	case EXT2_S_IFREG:
		m->m_root->v_type = VREG;
		break;
	case EXT2_S_IFBLK:
		m->m_root->v_type = VBLK;
		break;
	case EXT2_S_IFDIR:
		m->m_root->v_type = VDIR;
		break;
	case EXT2_S_IFCHR:
		m->m_root->v_type = VCHR;
		break;
	case EXT2_S_IFIFO:
		m->m_root->v_type = VFIFO;
		break;
	default:
		m->m_root->v_type = VUNK;
		break;
	};

	m->m_root->v_mode = 0;
	m->m_root->v_mode |= (rootmode & EXT2_S_IRUSR) ? S_IRUSR : 0;
	m->m_root->v_mode |= (rootmode & EXT2_S_IWUSR) ? S_IWUSR : 0;
	m->m_root->v_mode |= (rootmode & EXT2_S_IXUSR) ? S_IXUSR : 0;
	m->m_root->v_mode |= (rootmode & EXT2_S_IRGRP) ? S_IRGRP : 0;
	m->m_root->v_mode |= (rootmode & EXT2_S_IWGRP) ? S_IWGRP : 0;
	m->m_root->v_mode |= (rootmode & EXT2_S_IXGRP) ? S_IXGRP : 0;
	m->m_root->v_mode |= (rootmode & EXT2_S_IROTH) ? S_IROTH : 0;
	m->m_root->v_mode |= (rootmode & EXT2_S_IWOTH) ? S_IWOTH : 0;
	m->m_root->v_mode |= (rootmode & EXT2_S_IXOTH) ? S_IXOTH : 0;

	m->m_root->v_ctime = __le32(root->inode.ctime);
	m->m_root->v_atime = __le32(root->inode.atime);
	m->m_root->v_mtime = __le32(root->inode.mtime);

	m->m_root->v_size = ext2fs_node_get_size(root);

	/* Save control as mount point data */
	m->m_data = ctrl;

	return VMM_OK;

fail:
	vmm_free(ctrl);
	return rc;
}

static int ext2fs_unmount(struct mount *m)
{
	int rc;
	struct ext2fs_control *ctrl = m->m_data;

	if (!ctrl) {
		return VMM_EFAIL;
	}

	rc = ext2fs_control_exit(ctrl);

	vmm_free(ctrl);

	return rc;
}

static int ext2fs_msync(struct mount *m)
{
	struct ext2fs_control *ctrl = m->m_data;

	if (!ctrl) {
		return VMM_EFAIL;
	}

	return ext2fs_control_sync(ctrl);
}

static int ext2fs_vget(struct mount *m, struct vnode *v)
{
	int rc;
	struct ext2fs_node *node;

	node = vmm_zalloc(sizeof(struct ext2fs_node));
	if (!node) {
		return VMM_ENOMEM;
	}

	rc = ext2fs_node_init(node);

	v->v_data = node;

	return rc;
}

static int ext2fs_vput(struct mount *m, struct vnode *v)
{
	int rc;
	struct ext2fs_node *node = v->v_data;

	if (!node) {
		return VMM_EFAIL;
	}

	rc = ext2fs_node_exit(node);

	vmm_free(node);

	return rc;
}

/* 
 * Vnode operations 
 */

static size_t ext2fs_read(struct vnode *v, loff_t off, void *buf, size_t len)
{
	struct ext2fs_node *node = v->v_data;
	u64 filesize = ext2fs_node_get_size(node);

	if (filesize <= off) {
		return 0;
	}

	if (filesize < (len + off)) {
		len = filesize - off;
	}

	return ext2fs_node_read(node, off, len, buf);
}

static size_t ext2fs_write(struct vnode *v, loff_t off, void *buf, size_t len)
{
	u32 wlen;
	struct ext2fs_node *node = v->v_data;

	wlen = ext2fs_node_write(node, off, len, buf);

	/* Size and mtime might have changed */
	v->v_size = ext2fs_node_get_size(node);
	v->v_mtime = __le32(node->inode.mtime);

	return wlen;
}

static int ext2fs_truncate(struct vnode *v, loff_t off)
{
	int rc;
	struct ext2fs_node *node = v->v_data;
	u64 fileoff = off;
	u64 filesize = ext2fs_node_get_size(node);

	if (filesize <= fileoff) {
		return VMM_EFAIL;
	}

	rc = ext2fs_node_truncate(node, fileoff);
	if (rc) {
		return rc;
	}

	/* Size and mtime might have changed */
	v->v_size = ext2fs_node_get_size(node);
	v->v_mtime = __le32(node->inode.mtime);

	return VMM_OK;
}

static int ext2fs_sync(struct vnode *v)
{
	struct ext2fs_node *node = v->v_data;

	if (!node) {
		return VMM_EFAIL;
	}

	return ext2fs_node_sync(node);
}

static int ext2fs_readdir(struct vnode *dv, loff_t off, struct dirent *d)
{
	u32 readlen;
	struct ext2_dirent dent;
	struct ext2fs_node *dnode = dv->v_data;
	u64 filesize = ext2fs_node_get_size(dnode);
	u64 fileoff = off;

	if (filesize <= fileoff) {
		return VMM_ENOENT;
	}

	if (filesize < (sizeof(struct ext2_dirent) + fileoff)) {
		return VMM_ENOENT;
	}

	d->d_reclen = 0;

	do {
		readlen = ext2fs_node_read(dnode, fileoff, 
				sizeof(struct ext2_dirent), (char *)&dent);
		if (readlen != sizeof(struct ext2_dirent)) {
			return VMM_EIO;
		}

		if (dent.namelen > (VFS_MAX_NAME - 1)) {
			dent.namelen = (VFS_MAX_NAME - 1);
		}
		readlen = ext2fs_node_read(dnode, 
				fileoff + sizeof(struct ext2_dirent),
				dent.namelen, d->d_name);
		if (readlen != dent.namelen) {
			return VMM_EIO;
		}
		d->d_name[dent.namelen] = '\0';

		d->d_reclen += __le16(dent.direntlen);
		fileoff += __le16(dent.direntlen);

		if ((strcmp(d->d_name, ".") == 0) ||
		    (strcmp(d->d_name, "..") == 0)) {
			continue;
		} else {
			break;
		}
	} while (1);

	d->d_off = off;

	switch (dent.filetype) {
	case EXT2_FT_REG_FILE:
		d->d_type = DT_REG;
		break;
	case EXT2_FT_DIR:
		d->d_type = DT_DIR;
		break;
	case EXT2_FT_CHRDEV:
		d->d_type = DT_CHR;
		break;
	case EXT2_FT_BLKDEV:
		d->d_type = DT_BLK;
		break;
	case EXT2_FT_FIFO:
		d->d_type = DT_FIFO;
		break;
	case EXT2_FT_SOCK:
		d->d_type = DT_SOCK;
		break;
	case EXT2_FT_SYMLINK:
		d->d_type = DT_LNK;
		break;
	default:
		d->d_type = DT_UNK;
		break;
	};

	return VMM_OK;
}

static int ext2fs_lookup(struct vnode *dv, const char *name, struct vnode *v)
{
	int rc;
	u16 filemode;
	struct ext2_dirent dent;
	struct ext2fs_node *node = v->v_data;
	struct ext2fs_node *dnode = dv->v_data;

	rc = ext2fs_node_find_dirent(dnode, name, &dent);
	if (rc) {
		return rc;
	}

	rc = ext2fs_node_load(dnode->ctrl, __le32(dent.inode), node);
	if (rc) {
		return rc;
	}

	filemode = __le16(node->inode.mode);

	switch (filemode & EXT2_S_IFMASK) {
	case EXT2_S_IFSOCK:
		v->v_type = VSOCK;
		break;
	case EXT2_S_IFLNK:
		v->v_type = VLNK;
		break;
	case EXT2_S_IFREG:
		v->v_type = VREG;
		break;
	case EXT2_S_IFBLK:
		v->v_type = VBLK;
		break;
	case EXT2_S_IFDIR:
		v->v_type = VDIR;
		break;
	case EXT2_S_IFCHR:
		v->v_type = VCHR;
		break;
	case EXT2_S_IFIFO:
		v->v_type = VFIFO;
		break;
	default:
		v->v_type = VUNK;
		break;
	};

	v->v_mode = 0;
	v->v_mode |= (filemode & EXT2_S_IRUSR) ? S_IRUSR : 0;
	v->v_mode |= (filemode & EXT2_S_IWUSR) ? S_IWUSR : 0;
	v->v_mode |= (filemode & EXT2_S_IXUSR) ? S_IXUSR : 0;
	v->v_mode |= (filemode & EXT2_S_IRGRP) ? S_IRGRP : 0;
	v->v_mode |= (filemode & EXT2_S_IWGRP) ? S_IWGRP : 0;
	v->v_mode |= (filemode & EXT2_S_IXGRP) ? S_IXGRP : 0;
	v->v_mode |= (filemode & EXT2_S_IROTH) ? S_IROTH : 0;
	v->v_mode |= (filemode & EXT2_S_IWOTH) ? S_IWOTH : 0;
	v->v_mode |= (filemode & EXT2_S_IXOTH) ? S_IXOTH : 0;

	v->v_ctime = __le32(node->inode.ctime);
	v->v_atime = __le32(node->inode.atime);
	v->v_mtime = __le32(node->inode.mtime);

	v->v_size = ext2fs_node_get_size(node);

	return VMM_OK;
}

static int ext2fs_create(struct vnode *dv, const char *name, u32 mode)
{
	int rc;
	u16 filemode;
	u32 inode_no;
	struct ext2_dirent dent;
	struct ext2_inode inode;
	struct ext2fs_node *dnode = dv->v_data;

	rc = ext2fs_node_find_dirent(dnode, name, &dent);
	if (rc != VMM_ENOENT) {
		if (!rc) {
			return VMM_EALREADY;
		} else {
			return rc;
		}
	}

	rc = ext2fs_control_alloc_inode(dnode->ctrl, 
					dnode->inode_no, &inode_no);
	if (rc) {
		return rc;
	}

	memset(&inode, 0, sizeof(inode));

	inode.nlinks = __le16(1);

	filemode = EXT2_S_IFREG;
	filemode |= (mode & S_IRUSR) ? EXT2_S_IRUSR : 0;
	filemode |= (mode & S_IWUSR) ? EXT2_S_IWUSR : 0;
	filemode |= (mode & S_IXUSR) ? EXT2_S_IXUSR : 0;
	filemode |= (mode & S_IRGRP) ? EXT2_S_IRGRP : 0;
	filemode |= (mode & S_IWGRP) ? EXT2_S_IWGRP : 0;
	filemode |= (mode & S_IXGRP) ? EXT2_S_IXGRP : 0;
	filemode |= (mode & S_IROTH) ? EXT2_S_IROTH : 0;
	filemode |= (mode & S_IWOTH) ? EXT2_S_IWOTH : 0;
	filemode |= (mode & S_IXOTH) ? EXT2_S_IXOTH : 0;
	inode.mode = __le16(filemode);

	inode.mtime = __le32(ext2fs_current_timestamp());
	inode.atime = __le32(ext2fs_current_timestamp());
	inode.ctime = __le32(ext2fs_current_timestamp());

	rc = ext2fs_control_write_inode(dnode->ctrl, inode_no, &inode);
	if (rc) {
		ext2fs_control_free_inode(dnode->ctrl, inode_no);
		return rc;
	}

	rc = ext2fs_node_add_dirent(dnode, name, inode_no, 0);
	if (rc) {
		ext2fs_control_free_inode(dnode->ctrl, inode_no);
		return rc;
	}

	return VMM_OK;
}

static int ext2fs_remove(struct vnode *dv, struct vnode *v, const char *name)
{
	int rc;
	struct ext2_dirent dent;
	struct ext2fs_node *dnode = dv->v_data;
	struct ext2fs_node *node = v->v_data;

	rc = ext2fs_node_find_dirent(dnode, name, &dent);
	if (rc) {
		return rc;
	}

	if (__le32(dent.inode) != node->inode_no) {
		return VMM_EINVALID;
	}

	rc = ext2fs_node_del_dirent(dnode, name);
	if (rc) {
		return rc;
	}

	rc = ext2fs_control_free_inode(dnode->ctrl, node->inode_no);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

static int ext2fs_rename(struct vnode *sv, const char *sname, 
			 struct vnode *dv, const char *dname)
{
	int rc;
	struct ext2_dirent dent;
	struct ext2fs_node *snode = sv->v_data;
	struct ext2fs_node *dnode = dv->v_data;

	rc = ext2fs_node_find_dirent(dnode, dname, &dent);
	if (rc != VMM_ENOENT) {
		if (!rc) {
			return VMM_EALREADY;
		} else {
			return rc;
		}
	}

	rc = ext2fs_node_find_dirent(snode, sname, &dent);
	if (rc) {
		return rc;
	}

	rc = ext2fs_node_del_dirent(snode, sname);
	if (rc) {
		return rc;
	}

	rc = ext2fs_node_add_dirent(dnode, dname, __le32(dent.inode), 0);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

static int ext2fs_mkdir(struct vnode *dv, const char *name, u32 mode)
{
	int rc;
	u16 filemode;
	u32 i, inode_no, blkno;
	char buf[64];
	struct ext2_dirent dent;
	struct ext2_inode inode;
	struct ext2fs_node *dnode = dv->v_data;
	struct ext2fs_control *ctrl = dnode->ctrl;

	rc = ext2fs_node_find_dirent(dnode, name, &dent);
	if (rc != VMM_ENOENT) {
		if (!rc) {
			return VMM_EALREADY;
		} else {
			return rc;
		}
	}

	rc = ext2fs_control_alloc_inode(ctrl, dnode->inode_no, &inode_no);
	if (rc) {
		return rc;
	}

	memset(&inode, 0, sizeof(inode));

	inode.nlinks = __le16(1);

	filemode = EXT2_S_IFDIR;
	filemode |= (mode & S_IRUSR) ? EXT2_S_IRUSR : 0;
	filemode |= (mode & S_IWUSR) ? EXT2_S_IWUSR : 0;
	filemode |= (mode & S_IXUSR) ? EXT2_S_IXUSR : 0;
	filemode |= (mode & S_IRGRP) ? EXT2_S_IRGRP : 0;
	filemode |= (mode & S_IWGRP) ? EXT2_S_IWGRP : 0;
	filemode |= (mode & S_IXGRP) ? EXT2_S_IXGRP : 0;
	filemode |= (mode & S_IROTH) ? EXT2_S_IROTH : 0;
	filemode |= (mode & S_IWOTH) ? EXT2_S_IWOTH : 0;
	filemode |= (mode & S_IXOTH) ? EXT2_S_IXOTH : 0;
	inode.mode = __le16(filemode);

	inode.mtime = __le32(ext2fs_current_timestamp());
	inode.atime = __le32(ext2fs_current_timestamp());
	inode.ctime = __le32(ext2fs_current_timestamp());

	rc = ext2fs_control_alloc_block(ctrl, dnode->inode_no, &blkno);
	if (rc) {
		goto failed1;
	}

	memset(buf, 0, sizeof(buf));
	for (i = 0; i < ctrl->block_size; i += sizeof(buf)) {
		rc = ext2fs_devwrite(ctrl, blkno, i, sizeof(buf), buf);
		if (rc) {
			goto failed2;
		}
	}
	i = 0;
	dent.inode = __le32(inode_no);
	dent.filetype = 0;
	dent.namelen = 1;
	dent.direntlen = __le16(sizeof(dent) + 1);
	memcpy(&buf[i], &dent, sizeof(dent));
	i += sizeof(dent);
	memcpy(&buf[i], ".", 1);
	i += 1;
	dent.inode = __le32(dnode->inode_no);
	dent.filetype = 0;
	dent.namelen = 2;
	dent.direntlen = __le16(ctrl->block_size - i);
	memcpy(&buf[i], &dent, sizeof(dent));
	i += sizeof(dent);
	memcpy(&buf[i], "..", 2);
	i += 2;

	rc = ext2fs_devwrite(ctrl, blkno, 0, i, buf);
	if (rc) {
		goto failed2;
	}

	inode.b.blocks.dir_blocks[0] = __le32(blkno);
	inode.size = __le32(ctrl->block_size);
	inode.blockcnt = __le32(ctrl->block_size >> EXT2_SECTOR_BITS);

	rc = ext2fs_control_write_inode(ctrl, inode_no, &inode);
	if (rc) {
		goto failed2;
	}

	rc = ext2fs_node_add_dirent(dnode, name, inode_no, 0);
	if (rc) {
		goto failed2;
	}

	return VMM_OK;

failed2:
	ext2fs_control_free_block(ctrl, blkno);
failed1:
	ext2fs_control_free_inode(ctrl, inode_no);
	return rc;
}

static int ext2fs_rmdir(struct vnode *dv, struct vnode *v, const char *name)
{
	int rc;
	struct ext2_dirent dent;
	struct ext2fs_node *dnode = dv->v_data;
	struct ext2fs_node *node = v->v_data;

	rc = ext2fs_node_find_dirent(dnode, name, &dent);
	if (rc) {
		return rc;
	}

	if (__le32(dent.inode) != node->inode_no) {
		return VMM_EINVALID;
	}

	rc = ext2fs_node_truncate(node, 0);
	if (rc) {
		return rc;
	}

	rc = ext2fs_node_del_dirent(dnode, name);
	if (rc) {
		return rc;
	}

	rc = ext2fs_control_free_inode(dnode->ctrl, node->inode_no);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

static int ext2fs_chmod(struct vnode *v, u32 mode)
{
	u16 filemode;
	struct ext2fs_node *node = v->v_data;

	filemode = 0;
	switch (v->v_type) {
	case VSOCK:
		filemode = EXT2_S_IFSOCK;
		break;
	case VLNK:
		filemode = EXT2_S_IFLNK;
		break;
	case VREG:
		filemode = EXT2_S_IFREG;
		break;
	case VBLK:
		filemode = EXT2_S_IFBLK;
		break;
	case VDIR:
		filemode = EXT2_S_IFDIR;
		break;
	case VCHR:
		filemode = EXT2_S_IFCHR;
		break;
	case VFIFO:
		filemode = EXT2_S_IFIFO;
		break;
	default:
		filemode = 0;
		break;
	};

	filemode |= (mode & S_IRUSR) ? EXT2_S_IRUSR : 0;
	filemode |= (mode & S_IWUSR) ? EXT2_S_IWUSR : 0;
	filemode |= (mode & S_IXUSR) ? EXT2_S_IXUSR : 0;
	filemode |= (mode & S_IRGRP) ? EXT2_S_IRGRP : 0;
	filemode |= (mode & S_IWGRP) ? EXT2_S_IWGRP : 0;
	filemode |= (mode & S_IXGRP) ? EXT2_S_IXGRP : 0;
	filemode |= (mode & S_IROTH) ? EXT2_S_IROTH : 0;
	filemode |= (mode & S_IWOTH) ? EXT2_S_IWOTH : 0;
	filemode |= (mode & S_IXOTH) ? EXT2_S_IXOTH : 0;

	node->inode.mode = __le16(filemode);
	node->inode.atime = __le32(ext2fs_current_timestamp());
	node->inode_dirty = TRUE;
	
	v->v_mode &= ~(S_IRWXU|S_IRWXG|S_IRWXO);
	v->v_mode |= mode;

	return VMM_OK;
}

/* ext2fs filesystem */
static struct filesystem ext2fs = {
	.name		= "ext2",

	/* Mount point operations */
	.mount		= ext2fs_mount,
	.unmount	= ext2fs_unmount,
	.msync		= ext2fs_msync,
	.vget		= ext2fs_vget,
	.vput		= ext2fs_vput,

	/* Vnode operations */
	.read		= ext2fs_read,
	.write		= ext2fs_write,
	.truncate	= ext2fs_truncate,
	.sync		= ext2fs_sync,
	.readdir	= ext2fs_readdir,
	.lookup		= ext2fs_lookup,
	.create		= ext2fs_create,
	.remove		= ext2fs_remove,
	.rename		= ext2fs_rename,
	.mkdir		= ext2fs_mkdir,
	.rmdir		= ext2fs_rmdir,
	.chmod		= ext2fs_chmod,
};

static int __init ext2fs_init(void)
{
	return vfs_filesystem_register(&ext2fs);
}

static void __exit ext2fs_exit(void)
{
	vfs_filesystem_unregister(&ext2fs);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
