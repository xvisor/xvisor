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
#define INDIRECT_BLOCKS			12
/* Maximum lenght of a pathname.  */
#define EXT2_PATH_MAX			4096
/* Maximum nesting of symlinks, used to prevent a loop.  */
#define	EXT2_MAX_SYMLINKCNT		8

/* Bits used as offset in sector */
#define SECTOR_BITS			9
#define SECTOR_SIZE			512

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
	u32 block_id;
	u32 inode_id;
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
			u32 dir_blocks[INDIRECT_BLOCKS];
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

/* Information about a "mounted" ext2 filesystem.  */
struct ext2fs_control {
	struct ext2_sblock sblock;

	u32 log2_block_size;
	u32 block_size;
	u32 dir_blklast;
	u32 indir_blklast;
	u32 dindir_blklast;

	u32 inode_size;
	u32 inodes_per_block;

	u32 group_count;
	struct ext2_block_group *groups;
};

/* Information for accessing a ext2 file/directory */
struct ext2fs_node {
	/* Parent Ext2 control */
	struct ext2fs_control *ctrl;

	/* Underlying Inode */
	struct ext2_inode inode;
	u32 inode_no;

	/* Cached data block
	 * Allocated on demand. Must be freed in vput()
	 */
	u32 cached_blkno;
	u8 *cached_block;

	/* Indirect block
	 * Allocated on demand. Must be freed in vpuf()
	 */
	u32 *indir_block;
	u32 indir_blkno;

	/* Double-Indirect level1 block
	 * Allocated on demand. Must be freed in vput()
	 */
	u32 *dindir1_block;
	u32 dindir1_blkno;

	/* Double-Indirect level2 block
	 * Allocated on demand. Must be freed in vput()
	 */
	u32 *dindir2_block;
	u32 dindir2_blkno;
};

/* 
 * Helper routines 
 */

static int ext2fs_devread(struct vmm_blockdev *bdev, u32 sector,
			  int byte_offset, int byte_len, char *buf)
{
	u64 off, len;

	off = (sector << 9) + byte_offset;
	len = byte_len;
	len = vmm_blockdev_read(bdev, (u8 *)buf, off, len);

	return (len == byte_len) ? VMM_OK : VMM_EIO;
}

static int ext2fs_read_inode(struct vmm_blockdev *bdev, 
			     struct ext2fs_control *ctrl, 
			     u32 inode_no, struct ext2_inode *inode)
{
	int rc;
	struct ext2_block_group *blkgrp;
	struct ext2_sblock *sblock = &ctrl->sblock;
	u32 blkno, blkoff, group;

	inode_no--;
	group = udiv32(inode_no, __le32(sblock->inodes_per_group));
	blkgrp = &ctrl->groups[group];

	blkno = umod32(inode_no, __le32(sblock->inodes_per_group));
	blkno = udiv32(blkno, ctrl->inodes_per_block);
	blkno += __le32(blkgrp->inode_table_id);
	blkoff = umod32(inode_no, ctrl->inodes_per_block) * ctrl->inode_size;

	/* Read the inode.  */
	rc = ext2fs_devread(bdev, blkno << ctrl->log2_block_size, 
			blkoff, sizeof(struct ext2_inode), (char *)inode);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

static int ext2fs_read_blkno(struct vmm_blockdev *bdev,
			     struct ext2fs_control *ctrl,
			     struct ext2fs_node *node, 
			     u32 blkpos, u32 *blkno) 
{
	int rc;
	struct ext2_inode *inode = &node->inode;

	if (blkpos < ctrl->dir_blklast) {
		/* Direct blocks.  */
		*blkno = __le32(inode->b.blocks.dir_blocks[blkpos]);
	} else if (blkpos < ctrl->indir_blklast) {
		/* Indirect.  */
		u32 indir_blkno = __le32(inode->b.blocks.indir_block) << 
							ctrl->log2_block_size;
		u32 indir_blkpos = blkpos - ctrl->dir_blklast;

		if (!node->indir_block) {
			node->indir_block = vmm_malloc(ctrl->block_size);
			if (!node->indir_block) {
				return VMM_ENOMEM;
			}
			node->indir_blkno = 0xFFFFFFFF;
		}
		if (indir_blkno != node->indir_blkno) {
			rc = ext2fs_devread(bdev, indir_blkno, 0, 
				ctrl->block_size, (char *)node->indir_block);
			if (rc) {
				return rc;
			}
			node->indir_blkno = indir_blkno;
		}

		*blkno = __le32(node->indir_block[indir_blkpos]);
	} else if (blkpos < ctrl->dindir_blklast) {
		/* Double indirect.  */
		u32 dindir1_blkno = __le32(inode->b.blocks.double_indir_block) 
							<< ctrl->log2_block_size;
		u32 dindir1_blkpos = udiv32(blkpos - ctrl->indir_blklast, 
							ctrl->block_size / 4);
		u32 dindir2_blkno;
		u32 dindir2_blkpos = umod32(blkpos - ctrl->indir_blklast, 
							ctrl->block_size / 4);

		if (!node->dindir1_block) {
			node->dindir1_block = vmm_malloc(ctrl->block_size);
			if (!node->dindir1_block) {
				return VMM_ENOMEM;
			}
			node->dindir1_blkno = 0xFFFFFFFF;
		}
		if (dindir1_blkno != node->dindir1_blkno) {
			rc = ext2fs_devread(bdev, dindir1_blkno, 0,
				ctrl->block_size, (char *)node->dindir1_block);
			if (rc) {
				return rc;
			}
			node->dindir1_blkno = dindir1_blkno;
		}

		dindir2_blkno = __le32(node->dindir1_block[dindir1_blkpos]) 
						<< ctrl->log2_block_size;

		if (!node->dindir2_block) {
			node->dindir2_block = vmm_malloc(ctrl->block_size);
			if (!node->dindir2_block) {
				return VMM_ENOMEM;
			}
			node->dindir2_blkno = 0xFFFFFFFF;
		}
		if (dindir2_blkno != node->dindir2_blkno) {
			rc = ext2fs_devread(bdev, dindir2_blkno, 0,  
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

static int ext2fs_read_file(struct vmm_blockdev *bdev,
			    struct ext2fs_control *ctrl,
			    struct ext2fs_node *node, 
			    u32 pos, u32 len, char *buf) 
{
	int rc;
	u32 filesize = __le32(node->inode.size);
	u32 i, blkno, blkoff, blklen;
	u32 last_blkpos, last_blklen;
	u32 first_blkpos, first_blkoff, first_blklen;

	/* Sanity checks */
	if ((pos > filesize) ||
	    ((pos + len) > filesize)) {
		return VMM_EINVALID;
	}

	first_blkpos = udiv32(pos, ctrl->block_size);
	first_blkoff = umod32(pos, ctrl->block_size);
	first_blklen = ctrl->block_size - first_blkoff;
	if (len < first_blklen) {
		first_blklen = len;
	}

	last_blkpos = udiv32(((len + pos) + ctrl->block_size - 1), ctrl->block_size);
	last_blklen = umod32((len + pos), ctrl->block_size);

	i = first_blkpos;
	while (len) {
		rc = ext2fs_read_blkno(bdev, ctrl, node, i, &blkno);
		if (rc) {
			return rc;
		}
		blkno = blkno << ctrl->log2_block_size;

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

		/* If the block number is 0 then 
		 * this block is not stored on disk
		 * but is zero filled instead.  
		 */
		if (blkno) {
			if (!node->cached_block) {
				node->cached_block = 
						vmm_zalloc(ctrl->block_size);
				if (!node->cached_block) {
					return VMM_ENOMEM;
				}
			}
			if (node->cached_blkno != blkno) {
				rc = ext2fs_devread(bdev, blkno, 0,
						   ctrl->block_size, 
						   (char *)node->cached_block);
				if (rc) {
					return rc;
				}
				node->cached_blkno = blkno;
			}
			memcpy(buf, &node->cached_block[blkoff], blklen);			
		} else {
			memset(buf, 0, blklen);
		}

		buf += blklen;
		len -= blklen;
		i++;
	}

	return VMM_OK;
}

/* TODO: */
#if 0
static char *ext2fs_read_symlink(struct vmm_blockdev *bdev, 
				 struct ext2fs_ctrl *ctrl,
				 struct ext2fs_node *node)
{
	int rc;
	char *symlink;

	symlink = vmm_malloc(__le32(node->inode.size) + 1);
	if (!symlink) {
		return NULL;
	}

	/* If the filesize of the symlink is bigger than
	   60 the symlink is stored in a separate block,
	   otherwise it is stored in the inode.  */
	if (__le32(node->inode.size) <= 60) {
		strncpy(symlink, node->inode.b.symlink,
			 __le32(node->inode.size));
	} else {
		rc = ext2fs_read_file(bdev, ctrl, node, 0,
					__le32(node->inode.size), symlink);
		if (rc) {
			vmm_free(symlink);
			return NULL;
		}
	}
	symlink[__le32(node->inode.size)] = '\0';

	return (symlink);
}
#endif

/* 
 * Mount point operations 
 */

static int ext2fs_mount(struct mount *m, const char *dev, u32 flags)
{
	int rc;
	u16 rootmode;
	u32 blkno, blkoff, desc_per_blk, group;
	struct ext2fs_control *ctrl;
	struct ext2fs_node *root;

	ctrl = vmm_zalloc(sizeof(struct ext2fs_control));
	if (!ctrl) {
		return VMM_ENOMEM;
	}

	/* Read the superblock.  */
	rc = ext2fs_devread(m->m_dev, 1 * 2, 0, 
			   sizeof(struct ext2_sblock),
			   (char *)&ctrl->sblock);
	if (rc) {
		goto fail;
	}

	/* Make sure this is an ext2 filesystem.  */
	if (__le16(ctrl->sblock.magic) != EXT2_MAGIC) {
		rc = VMM_ENOSYS;
		goto fail;
	}

	/* Pre-compute frequently required values */
	ctrl->log2_block_size = __le32((ctrl)->sblock.log2_block_size) + 1;
	ctrl->block_size = 1 << (__le32((ctrl)->sblock.log2_block_size) + 10);
	ctrl->dir_blklast = INDIRECT_BLOCKS;
	ctrl->indir_blklast = INDIRECT_BLOCKS + (ctrl->block_size / 4);
	ctrl->dindir_blklast = INDIRECT_BLOCKS + 
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
					sizeof(struct ext2_block_group));
	if (!ctrl->groups) {
		rc = VMM_ENOMEM;
		goto fail;
	}
	desc_per_blk = udiv32(ctrl->block_size, 
					sizeof(struct ext2_block_group));
	for (group = 0; group < ctrl->group_count; group++) {
		blkno = __le32(ctrl->sblock.first_data_block) + 
					1 + udiv32(group, desc_per_blk);
		blkoff = umod32(group, desc_per_blk) * 
					sizeof(struct ext2_block_group);
		rc = ext2fs_devread(m->m_dev, 
				    blkno << ctrl->log2_block_size, 
				    blkoff, sizeof(struct ext2_block_group), 
				    (char *)&ctrl->groups[group]);
		if (rc) {
			goto fail1;
		}
	}

	/* Setup root node */
	root = m->m_root->v_data;
	root->ctrl = ctrl;
	root->inode_no = 2;

	rc = ext2fs_read_inode(m->m_dev, ctrl, root->inode_no, &root->inode);
	if (rc) {
		goto fail1;
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

	m->m_root->v_size = __le32(root->inode.size);

	/* Save control as mount point data */
	m->m_data = ctrl;

	return VMM_OK;

fail1:
	vmm_free(ctrl->groups);
fail:
	vmm_free(ctrl);
	return rc;
}

static int ext2fs_unmount(struct mount *m)
{
	struct ext2fs_control *ctrl = m->m_data;

	if (!ctrl) {
		return VMM_EFAIL;
	}

	vmm_free(ctrl->groups);
	vmm_free(ctrl);

	return VMM_OK;
}

/* FIXME: */
static int ext2fs_sync(struct mount *m)
{
	return VMM_EFAIL;
}

static int ext2fs_vget(struct mount *m, struct vnode *v)
{
	struct ext2fs_node *node;

	node = vmm_zalloc(sizeof(struct ext2fs_node));
	if (!node) {
		return VMM_ENOMEM;
	}

	v->v_data = node;

	return VMM_OK;
}

static int ext2fs_vput(struct mount *m, struct vnode *v)
{
	struct ext2fs_node *node = v->v_data;

	if (!node) {
		return VMM_EFAIL;
	}

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

	vmm_free(node);

	return VMM_OK;
}

/* 
 * Vnode operations 
 */

static int ext2fs_open(struct vnode *v, struct file *f)
{
	/* For now nothing to do here. */
	/* In future, can be used to prefect file data blocks */
	return VMM_OK;
}

static int ext2fs_close(struct vnode *v, struct file *f)
{
	/* For now nothing to do here. */
	return VMM_OK;
}

static size_t ext2fs_read(struct vnode *v, struct file *f, 
				void *buf, size_t len)
{
	int rc;
	struct ext2fs_node *node = v->v_data;
	struct ext2fs_control *ctrl = node->ctrl;
	u32 filesize = __le32(node->inode.size);

	if (filesize <= f->f_offset) {
		return 0;
	}

	if (filesize < (len + f->f_offset)) {
		len = filesize - f->f_offset;
	}

	rc = ext2fs_read_file(v->v_mount->m_dev, ctrl, node, 
				f->f_offset, len, buf);
	if (rc) {
		return 0;
	}

	return len;
}

/* FIXME: */
static size_t ext2fs_write(struct vnode *v, struct file *f, 
				void *buf, size_t len)
{
	return 0;
}

/* FIXME: */
static bool ext2fs_seek(struct vnode *v, struct file *f, loff_t off)
{
	return (off > (loff_t)(v->v_size)) ? FALSE : TRUE;
}

/* FIXME: */
static int ext2fs_fsync(struct vnode *v, struct file *f)
{
	return VMM_EFAIL;
}

static int ext2fs_readdir(struct vnode *dv, struct file *f, struct dirent *d)
{
	int rc;
	struct ext2_dirent dent;
	struct ext2fs_node *dnode = dv->v_data;
	struct ext2fs_control *dctrl = dnode->ctrl;
	u32 filesize = __le32(dnode->inode.size);
	u32 fileoff = f->f_offset;

	if (filesize <= f->f_offset) {
		return VMM_ENOENT;
	}

	if (filesize < (sizeof(struct ext2_dirent) + f->f_offset)) {
		return VMM_ENOENT;
	}

	d->d_reclen = 0;

	do {
		rc = ext2fs_read_file(dv->v_mount->m_dev, dctrl, dnode,  
			fileoff, sizeof(struct ext2_dirent), (char *)&dent);
		if (rc) {
			return rc;
		}

		if (dent.namelen > (VFS_MAX_NAME - 1)) {
			dent.namelen = (VFS_MAX_NAME - 1);
		}
		rc = ext2fs_read_file(dv->v_mount->m_dev, dctrl, dnode, 
				fileoff + sizeof(struct ext2_dirent),
				dent.namelen, d->d_name);
		if (rc) {
			return rc;
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

	d->d_off = f->f_offset;

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
	u16  filemode;
	bool filefound;
	char filename[VFS_MAX_NAME];
	struct ext2_dirent dent;
	struct ext2fs_node *node = v->v_data;
	struct ext2fs_node *dnode = dv->v_data;
	struct ext2fs_control *dctrl = dnode->ctrl;
	u32 fileoff, filesize = __le32(dnode->inode.size);

	fileoff = 0;
	filefound = FALSE;
	while (fileoff < filesize) {
		rc = ext2fs_read_file(dv->v_mount->m_dev, dctrl, dnode,  
			fileoff, sizeof(struct ext2_dirent), (char *)&dent);
		if (rc) {
			return rc;
		}

		if (dent.namelen > (VFS_MAX_NAME - 1)) {
			dent.namelen = (VFS_MAX_NAME - 1);
		}
		rc = ext2fs_read_file(dv->v_mount->m_dev, dctrl, dnode, 
				fileoff + sizeof(struct ext2_dirent),
				dent.namelen, filename);
		if (rc) {
			return rc;
		}
		filename[dent.namelen] = '\0';

		fileoff += dent.direntlen;

		if ((strcmp(filename, ".") == 0) ||
		    (strcmp(filename, "..") == 0)) {
			continue;
		}

		if (strcmp(filename, name) == 0) {
			filefound = TRUE;
			break;
		}
	}

	if (!filefound) {
		return VMM_ENOENT;
	}

	node->ctrl = dnode->ctrl;

	rc = ext2fs_read_inode(dv->v_mount->m_dev, dctrl, 
				__le32(dent.inode), &node->inode);
	if (rc) {
		return rc;
	}
	node->inode_no = __le32(dent.inode);

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

	v->v_size = __le32(node->inode.size);

	return VMM_OK;
}

/* FIXME: */
static int ext2fs_create(struct vnode *dv, const char *filename, u32 mode)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int ext2fs_remove(struct vnode *dv, struct vnode *v, const char *name)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int ext2fs_rename(struct vnode *dv1, struct vnode *v1, const char *sname, 
			struct vnode *dv2, struct vnode *v2, const char *dname)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int ext2fs_mkdir(struct vnode *dv, const char *name, u32 mode)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int ext2fs_rmdir(struct vnode *dv, struct vnode *v, const char *name)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int ext2fs_getattr(struct vnode *v, struct vattr *a)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int ext2fs_setattr(struct vnode *v, struct vattr *a)
{
	return VMM_EFAIL;
}

/* FIXME: */
static int ext2fs_truncate(struct vnode *v, loff_t off)
{
	return VMM_EFAIL;
}

/* ext2fs filesystem */
static struct filesystem ext2fs = {
	.name		= "ext2",

	/* Mount point operations */
	.mount		= ext2fs_mount,
	.unmount	= ext2fs_unmount,
	.sync		= ext2fs_sync,
	.vget		= ext2fs_vget,
	.vput		= ext2fs_vput,

	/* Vnode operations */
	.open		= ext2fs_open,
	.close		= ext2fs_close,
	.read		= ext2fs_read,
	.write		= ext2fs_write,
	.seek		= ext2fs_seek,
	.fsync		= ext2fs_fsync,
	.readdir	= ext2fs_readdir,
	.lookup		= ext2fs_lookup,
	.create		= ext2fs_create,
	.remove		= ext2fs_remove,
	.rename		= ext2fs_rename,
	.mkdir		= ext2fs_mkdir,
	.rmdir		= ext2fs_rmdir,
	.getattr	= ext2fs_getattr,
	.setattr	= ext2fs_setattr,
	.truncate	= ext2fs_truncate
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
