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
 * @file ext4_node.c
 * @author Anup Patel. (anup@brainfault.org)
 * @brief header file for Ext4 node functions
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <libs/vfs.h>

#include "ext4_control.h"
#include "ext4_node.h"

u64 ext4fs_node_get_size(struct ext4fs_node *node)
{
	u64 ret = __le32(node->inode.size);

	if (__le32(node->ctrl->sblock.revision_level) != 0) {
		ret |= ((u64)__le32(node->inode.dir_acl)) << 32;
	}

	return ret;
}

void ext4fs_node_set_size(struct ext4fs_node *node, u64 size)
{
	node->inode.size = __le32((u32)(size & 0xFFFFFFFFULL));
	if (__le32(node->ctrl->sblock.revision_level) != 0) {
		node->inode.dir_acl = __le32((u32)(size >> 32));
	}
	node->inode.blockcnt = __le32((u32)(size >> EXT2_SECTOR_BITS));
	node->inode_dirty = TRUE;
}

int ext4fs_node_read_blk(struct ext4fs_node *node,
			 u32 blkno, u32 blkoff, u32 blklen, char *buf)
{
	int rc;
	struct ext4fs_control *ctrl = node->ctrl;

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
			rc = ext4fs_devwrite(ctrl, node->cached_blkno,
					    0, ctrl->block_size, 
					    (char *)node->cached_block);
			if (rc) {
				return rc;
			}
			node->cached_dirty = FALSE;
		}
		rc = ext4fs_devread(ctrl, blkno,
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

int ext4fs_node_write_blk(struct ext4fs_node *node,
			  u32 blkno, u32 blkoff, u32 blklen, char *buf)
{
	int rc;
	struct ext4fs_control *ctrl = node->ctrl;

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
			rc = ext4fs_devwrite(ctrl, node->cached_blkno,
					    0, ctrl->block_size, 
					    (char *)node->cached_block);
			if (rc) {
				return rc;
			}
			node->cached_dirty = FALSE;
		}
		if (blkoff != 0 ||
		    blklen != ctrl->block_size) {
			rc = ext4fs_devread(ctrl, blkno,
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

int ext4fs_node_sync(struct ext4fs_node *node)
{
	int rc;
	struct ext4fs_control *ctrl = node->ctrl;

	if (node->inode_dirty) {
		rc = ext4fs_control_write_inode(ctrl, 
					node->inode_no, &node->inode);
		if (rc) {
			return rc;
		}
		node->inode_dirty = FALSE;
	}

	if (node->cached_block && node->cached_dirty) {
		rc = ext4fs_devwrite(ctrl, node->cached_blkno, 0, 
				ctrl->block_size, (char *)node->cached_block);
		if (rc) {
			return rc;
		}
		node->cached_dirty = FALSE;
	}

	if (node->indir_block && node->indir_dirty) {
		rc = ext4fs_devwrite(ctrl, node->indir_blkno, 0, 
				ctrl->block_size, (char *)node->indir_block);
		if (rc) {
			return rc;
		}
		node->indir_dirty = FALSE;
	}

	if (node->dindir1_block && node->dindir1_dirty) {
		rc = ext4fs_devwrite(ctrl, node->dindir1_blkno, 0, 
				ctrl->block_size, (char *)node->dindir1_block);
		if (rc) {
			return rc;
		}
		node->dindir1_dirty = FALSE;
	}

	if (node->dindir2_block && node->dindir2_dirty) {
		rc = ext4fs_devwrite(ctrl, node->dindir2_blkno, 0, 
				ctrl->block_size, (char *)node->dindir2_block);
		if (rc) {
			return rc;
		}
		node->dindir2_dirty = FALSE;
	}

	return VMM_OK;
}

int ext4fs_node_read_blkno(struct ext4fs_node *node, u32 blkpos, u32 *blkno)
{
	int rc;
	u32 dindir2_blkno;
	struct ext2_inode *inode = &node->inode;
	struct ext4fs_control *ctrl = node->ctrl;

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
			rc = ext4fs_devread(ctrl, node->indir_blkno, 0, 
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
			rc = ext4fs_devread(ctrl, node->dindir1_blkno, 0,
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
				rc = ext4fs_devwrite(ctrl, node->dindir2_blkno,
						  0, ctrl->block_size, 
						  (char *)node->dindir2_block);
				if (rc) {
					return rc;
				}
				node->dindir2_dirty = FALSE;
			}
			rc = ext4fs_devread(ctrl, dindir2_blkno, 0,  
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

int ext4fs_node_write_blkno(struct ext4fs_node *node, u32 blkpos, u32 blkno)
{
	int rc;
	u32 dindir2_blkno;
	struct ext2_inode *inode = &node->inode;
	struct ext4fs_control *ctrl = node->ctrl;

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
			rc = ext4fs_devread(ctrl, node->indir_blkno, 0, 
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
			rc = ext4fs_devread(ctrl, node->dindir1_blkno, 0,
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
				rc = ext4fs_devwrite(ctrl, node->dindir2_blkno,
						  0, ctrl->block_size, 
						  (char *)node->dindir2_block);
				if (rc) {
					return rc;
				}
				node->dindir2_dirty = FALSE;
			}
			if (!dindir2_blkno) {
				rc = ext4fs_control_alloc_block(ctrl, 
					node->inode_no, &dindir2_blkno);
				if (rc) {
					return rc;
				}
				node->dindir1_block[dindir1_blkpos] = 
							__le32(dindir2_blkno);
				node->dindir1_dirty = TRUE;
				memset(node->dindir2_block, 0, ctrl->block_size);
			} else {
				rc = ext4fs_devread(ctrl, dindir2_blkno, 
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
u32 ext4fs_node_read(struct ext4fs_node *node, u64 pos, u32 len, char *buf)
{
	int rc;
	u64 filesize = ext4fs_node_get_size(node);
	u32 i, rlen, blkno, blkoff, blklen;
	u32 last_blkpos, last_blklen;
	u32 first_blkpos, first_blkoff, first_blklen;
	struct ext4fs_control *ctrl = node->ctrl;

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
		rc = ext4fs_node_read_blkno(node, i, &blkno);
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
		rc = ext4fs_node_read_blk(node, blkno, blkoff, blklen, buf);
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
static char *ext4fs_node_read_symlink(struct ext4fs_node *node)
{
	int rc;
	u32 rlen
	char *symlink;

	symlink = vmm_malloc(ext4fs_node_get_size(node) + 1);
	if (!symlink) {
		return NULL;
	}

	/* If the filesize of the symlink is bigger than
	   60 the symlink is stored in a separate block,
	   otherwise it is stored in the inode.  */
	if (ext4fs_node_get_size(node) <= 60) {
		strncpy(symlink, node->inode.b.symlink,
			ext4fs_node_get_size(node));
	} else {
		rlen = ext4fs_node_read(node, 0,
				ext4fs_node_get_size(node), symlink);
		if (rlen != ext4fs_node_get_size(node)) {
			vmm_free(symlink);
			return NULL;
		}
	}
	symlink[ext4fs_node_get_size(node)] = '\0';

	return (symlink);
}
#endif

u32 ext4fs_node_write(struct ext4fs_node *node, u64 pos, u32 len, char *buf)
{
	int rc;
	bool update_nodesize = FALSE, alloc_newblock = FALSE;
	u32 wlen, blkpos, blkno, blkoff, blklen;
	u64 wpos, filesize = ext4fs_node_get_size(node);
	struct ext4fs_control *ctrl = node->ctrl;

	wlen = len;
	wpos = pos;
	update_nodesize = FALSE;

	while (wlen) {
		/* Note: div result < 32-bit */
		blkpos = udiv64(wpos, ctrl->block_size);
		blkoff = wpos - (blkpos * ctrl->block_size);
		blklen = ctrl->block_size - blkoff;
		blklen = (wlen < blklen) ? wlen : blklen;

		rc = ext4fs_node_read_blkno(node, blkpos, &blkno);
		if (rc) {
			goto done;
		}

		if (!blkno) {
			rc = ext4fs_control_alloc_block(ctrl, 
						node->inode_no, &blkno);
			if (rc) {
				goto done;
			}

			rc = ext4fs_node_write_blkno(node, blkpos, blkno);
			if (rc) {
				return rc;
			}

			alloc_newblock = TRUE;			
		} else {
			alloc_newblock = FALSE;
		}

		rc = ext4fs_node_write_blk(node, blkno, blkoff, blklen, buf);
		if (rc) {
			if (alloc_newblock) {
				ext4fs_control_free_block(ctrl, blkno);
				ext4fs_node_write_blkno(node, blkpos, 0);
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
		ext4fs_node_set_size(node, filesize);
	}
	if (len - wlen) {
		/* Update node modify time */
		node->inode.mtime = __le32(ext4fs_current_timestamp());
		node->inode_dirty = TRUE;
	}

	return len - wlen;
}

int ext4fs_node_truncate(struct ext4fs_node *node, u64 pos) 
{
	int rc;
	u32 blkpos, blkno, blkcnt;
	u32 first_blkpos, first_blkoff;
	u64 filesize = ext4fs_node_get_size(node);
	struct ext4fs_control *ctrl = node->ctrl;

	if (filesize <= pos) {
		return VMM_OK;
	}

	/* Note: div result < 32-bit */
	first_blkpos = udiv64(pos, ctrl->block_size); 
	first_blkoff = pos - (first_blkpos * ctrl->block_size);

	/* Note: div result < 32-bit */
	blkcnt = udiv64(filesize, ctrl->block_size);
	if (filesize > ((u64)blkcnt * (u64)ctrl->block_size)) {
		blkcnt++;
	}

	/* If first block to truncate will have some data left
	 * then do not free first block
	 */
	if (first_blkoff) {
		blkpos = first_blkpos + 1;
	} else {
		blkpos = first_blkpos;
	}

	/* Free node blocks */
	while (blkpos < blkcnt) {
		rc = ext4fs_node_read_blkno(node, blkpos, &blkno);
		if (rc) {
			return rc;
		}

		rc = ext4fs_control_free_block(ctrl, blkno);
		if (rc) {
			return rc;
		}

		rc = ext4fs_node_write_blkno(node, blkpos, 0);
		if (rc) {
			return rc;
		}

		blkpos++;
	}

	/* FIXME: Free indirect & double indirect blocks */

	if (pos != filesize) {
		/* Update node mtime */
		node->inode.mtime = __le32(ext4fs_current_timestamp());
		node->inode_dirty = TRUE;
		/* Update node size */
		ext4fs_node_set_size(node, pos);
	}

	return VMM_OK;
}

int ext4fs_node_load(struct ext4fs_control *ctrl, 
		     u32 inode_no, struct ext4fs_node *node)
{
	int rc;

	node->ctrl = ctrl;

	node->inode_no = inode_no;
	rc = ext4fs_control_read_inode(ctrl, node->inode_no, &node->inode);
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

int ext4fs_node_init(struct ext4fs_node *node)
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

int ext4fs_node_exit(struct ext4fs_node *node)
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

int ext4fs_node_find_dirent(struct ext4fs_node *dnode, 
			    const char *name, struct ext2_dirent *dent)
{
	bool found;
	u32 rlen;
	char filename[VFS_MAX_NAME];
	u64 off, filesize = ext4fs_node_get_size(dnode);

	/* Find desired directoy entry such that we ignore
	 * "." and ".." in search process
	 */
	off = 0;
	found = FALSE;
	while (off < filesize) {
		rlen = ext4fs_node_read(dnode, off, 
				sizeof(struct ext2_dirent), (char *)dent);
		if (rlen != sizeof(struct ext2_dirent)) {
			return VMM_EIO;
		}

		if (dent->namelen > (VFS_MAX_NAME - 1)) {
			dent->namelen = (VFS_MAX_NAME - 1);
		}
		rlen = ext4fs_node_read(dnode, 
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

int ext4fs_node_add_dirent(struct ext4fs_node *dnode, 
			   const char *name, u32 inode_no, u8 type)
{
	bool found;
	u16 direntlen;
	u32 rlen, wlen;
	char filename[VFS_MAX_NAME];
	struct ext2_dirent dent;
	struct ext4fs_control *ctrl = dnode->ctrl;
	u64 off, filesize = ext4fs_node_get_size(dnode);

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
		rlen = ext4fs_node_read(dnode, off, 
				sizeof(struct ext2_dirent), (char *)&dent);
		if (rlen != sizeof(struct ext2_dirent)) {
			return VMM_EIO;
		}

		if (direntlen < (__le16(dent.direntlen) - 
				dent.namelen - sizeof(struct ext2_dirent))) {
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
			wlen = ext4fs_node_write(dnode, off + rlen, 
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
		direntlen = (__le16(dent.direntlen) - 
				dent.namelen - sizeof(struct ext2_dirent));
		dent.direntlen = __le16(__le16(dent.direntlen) - direntlen);

		wlen = ext4fs_node_write(dnode, off, 
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

	wlen = ext4fs_node_write(dnode, off, 
			 sizeof(struct ext2_dirent), (char *)&dent);
	if (wlen != sizeof(struct ext2_dirent)) {
		return VMM_EIO;
	}

	off += sizeof(struct ext2_dirent);

	wlen = ext4fs_node_write(dnode, off, 
			 strlen(filename), (char *)filename);
	if (wlen != strlen(filename)) {
		return VMM_EIO;
	}

	/* Increment nlinks field of inode */
	dnode->inode.nlinks = __le16(__le16(dnode->inode.nlinks) + 1);
	dnode->inode_dirty = TRUE;

	return VMM_OK;
}

int ext4fs_node_del_dirent(struct ext4fs_node *dnode, const char *name)
{
	bool found;
	u32 rlen, wlen;
	char filename[VFS_MAX_NAME];
	struct ext2_dirent pdent, dent;
	u64 poff, off, filesize = ext4fs_node_get_size(dnode);

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
		rlen = ext4fs_node_read(dnode, off, 
				sizeof(struct ext2_dirent), (char *)&dent);
		if (rlen != sizeof(struct ext2_dirent)) {
			return VMM_EIO;
		}

		if (dent.namelen > (VFS_MAX_NAME - 1)) {
			dent.namelen = (VFS_MAX_NAME - 1);
		}
		rlen = ext4fs_node_read(dnode, 
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
	wlen = ext4fs_node_write(dnode, poff, 
				 sizeof(struct ext2_dirent), (char *)&pdent);
	if (wlen != sizeof(struct ext2_dirent)) {
		return VMM_EIO;
	}

	/* Decrement nlinks field of inode */
	dnode->inode.nlinks = __le16(__le16(dnode->inode.nlinks) - 1);
	dnode->inode_dirty = TRUE;

	return VMM_OK;
}

