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
 * @file fat_node.c
 * @author Anup Patel. (anup@brainfault.org)
 * @brief source file for FAT node functions
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>

#include "fat_control.h"
#include "fat_node.h"

static int fatfs_node_find_lookup_dirent(struct fatfs_node *dnode, 
					 const char *name, 
					 struct fat_dirent *dent,
					 u32 *off, u32 *len)
{
	int idx;

	if (name[0] == '\0') {
		return -1;
	}

	for (idx = 0; idx < FAT_NODE_LOOKUP_SIZE; idx++) {
		if (!strcmp(dnode->lookup_name[idx], name)) {
			memcpy(dent, &dnode->lookup_dent[idx], sizeof(*dent));
			*off = dnode->lookup_off[idx];
			*len = dnode->lookup_len[idx];
			return idx;
		}
	}

	return -1;
}

static void fatfs_node_add_lookup_dirent(struct fatfs_node *dnode, 
					 const char *name, 
					 struct fat_dirent *dent, 
					 u32 off, u32 len)
{
	int idx;
	bool found = FALSE;

	if (name[0] == '\0') {
		return;
	}

	for (idx = 0; idx < FAT_NODE_LOOKUP_SIZE; idx++) {
		if (!strcmp(dnode->lookup_name[idx], name)) {
			found = TRUE;
			break;
		}
	}

	if (!found) {
		idx = dnode->lookup_victim;
		dnode->lookup_victim++;
		if (dnode->lookup_victim == FAT_NODE_LOOKUP_SIZE) {
			dnode->lookup_victim = 0;
		}
		if (strlcpy(&dnode->lookup_name[idx][0], name,
		    sizeof(dnode->lookup_name[idx])) >=
		    sizeof(dnode->lookup_name[idx])) {
			return;
		}
		memcpy(&dnode->lookup_dent[idx], dent, sizeof(*dent));
		dnode->lookup_off[idx] = off;
		dnode->lookup_len[idx] = len;
	}
}

#if 0 /* TODO: To be used later. */
static void fatfs_node_del_lookup_dirent(struct fatfs_node *dnode, 
					  const char *name)
{
	int idx;

	if (name[0] == '\0') {
		return;
	}

	for (idx = 0; idx < FAT_NODE_LOOKUP_SIZE; idx++) {
		if (!strcmp(dnode->lookup_name[idx], name)) {
			dnode->lookup_name[idx][0] = '\0';
			dnode->lookup_off[idx] = 0;
			dnode->lookup_len[idx] = 0;
			break;
		}
	}

}
#endif

static int fatfs_node_sync_cached_cluster(struct fatfs_node *node)
{
	u64 wlen, woff;
	struct fatfs_control *ctrl = node->ctrl;

	if (node->cached_dirty) {
		woff = (ctrl->first_data_sector * ctrl->bytes_per_sector) + 
			((node->cached_clust - 2) * ctrl->bytes_per_cluster);

		wlen = vmm_blockdev_write(ctrl->bdev, 
					node->cached_data, 
					woff, ctrl->bytes_per_cluster);
		if (wlen != ctrl->bytes_per_cluster) {
			return VMM_EIO;
		}

		node->cached_dirty = FALSE;
	}

	return VMM_OK;
}

static int fatfs_node_sync_parent_dent(struct fatfs_node *node)
{
	u32 woff, wlen;

	if (!node->parent || !node->parent_dent_dirty) {
		return VMM_OK;
	}

	woff = node->parent_dent_off + 
		node->parent_dent_len - 
		sizeof(node->parent_dent);
	if (woff < node->parent_dent_off) {
		return VMM_EFAIL;
	}

	/* FIXME: Someone else may be accessing the parent node. 
	 * This code does not protect the parent node.
	 */

	wlen = fatfs_node_write(node->parent, woff, 
			sizeof(node->parent_dent), (u8 *)&node->parent_dent);
	if (wlen != sizeof(node->parent_dent)) {
		return VMM_EIO;
	}

	node->parent_dent_dirty = FALSE;

	return VMM_OK;
}

u32 fatfs_node_get_size(struct fatfs_node *node)
{
	if (!node) {
		return 0;
	}

	return __le32(node->parent_dent.file_size);
}

u32 fatfs_node_read(struct fatfs_node *node, u32 pos, u32 len, u8 *buf)
{
	int rc;
	u64 rlen, roff;
	u32 r, cl_pos, cl_off, cl_num, cl_len;
	struct fatfs_control *ctrl = node->ctrl;

	if (!node->parent && ctrl->type != FAT_TYPE_32) {
		rlen = (u64)ctrl->bytes_per_sector * ctrl->root_sectors;
		if (pos >= rlen) {
			return 0;
		}
		if ((pos + len) > rlen) {
			rlen = rlen - pos;
		} else {
			rlen = len;
		}
		roff = (u64)ctrl->first_root_sector * ctrl->bytes_per_sector;
		roff += pos;
		return vmm_blockdev_read(ctrl->bdev, (u8 *)buf, roff, rlen);
	}

	r = 0;
	while (r < len) {
		/* Get the next cluster */
		if (r == 0) {
			cl_pos = udiv32(pos, ctrl->bytes_per_cluster); 
			cl_off = pos - cl_pos * ctrl->bytes_per_cluster;
			rc = fatfs_control_nth_cluster(ctrl, 
							node->first_cluster,
							cl_pos, &cl_num);
			if (rc) {
				return 0;
			}
			cl_len = ctrl->bytes_per_cluster - cl_off;
			cl_len = (cl_len < len) ? cl_len : len;
		} else {
			cl_pos++;
			cl_off = 0;
			rc = fatfs_control_nth_cluster(ctrl, 
							cl_num, 1, &cl_num);
			if (rc) {
				return r;
			}
			cl_len = (ctrl->bytes_per_cluster < len) ? 
						ctrl->bytes_per_cluster : len;
		}

		/* Make sure cached cluster is updated */
		if (!node->cached_data) {
			return 0;
		}
		if (node->cached_clust != cl_num) {
			if (fatfs_node_sync_cached_cluster(node)) {
				return 0;
			}

			node->cached_clust = cl_num;

			roff = (u64)ctrl->first_data_sector * 
						ctrl->bytes_per_sector;
			roff += (u64)(cl_num - 2) * ctrl->bytes_per_cluster;
			rlen = vmm_blockdev_read(ctrl->bdev, 
						node->cached_data, 
						roff, ctrl->bytes_per_cluster);
			if (rlen != ctrl->bytes_per_cluster) {
				return r;
			}
		}

		/* Read from cached cluster */
		memcpy(buf, &node->cached_data[cl_off], cl_len);

		/* Update iteration */
		r += cl_len;
		buf += cl_len;
	}

	return r;
}

u32 fatfs_node_write(struct fatfs_node *node, u32 pos, u32 len, u8 *buf)
{
	int rc;
	u64 woff, wlen;
	u32 w, wstartcl, wendcl;
	u32 cl_off, cl_num, cl_len;
	u32 year, mon, day, hour, min, sec;
	struct fatfs_control *ctrl = node->ctrl;

	if (!node->parent && ctrl->type != FAT_TYPE_32) {
		wlen = (u64)ctrl->bytes_per_sector * ctrl->root_sectors;
		if (pos >= wlen) {
			return 0;
		}
		if ((pos + len) > wlen) {
			wlen = wlen - pos;
		}
		woff = (u64)ctrl->first_root_sector * ctrl->bytes_per_sector;
		woff += pos;
		return vmm_blockdev_write(ctrl->bdev, (u8 *)buf, woff, wlen);
	}

	wstartcl = udiv32(pos, ctrl->bytes_per_cluster);
	wendcl = udiv32(pos + len - 1, ctrl->bytes_per_cluster);

	/* Sync and zero-out cached cluster buffer */
	if (node->cached_clust) {
		if (fatfs_node_sync_cached_cluster(node)) {
			return 0;
		}
		node->cached_clust = 0;
		memset(node->cached_data, 0, ctrl->bytes_per_cluster);
	}

	/* Make room for new data by appending free clusters */
	cl_num = node->first_cluster;
	for (w = 0; w <= (wendcl - wstartcl); w++) {
		if (w == 0) {
			rc = fatfs_control_nth_cluster(ctrl, cl_num,
							wstartcl, &cl_num);
			if (!rc) {
				continue;
			}
		} else {
			rc = fatfs_control_nth_cluster(ctrl, cl_num, 1, &cl_num);
			if (!rc) {
				continue;
			}
		}

		/* Add new cluster */
		rc = fatfs_control_append_free_cluster(ctrl, cl_num, &cl_num);
		if (rc) {
			return 0;
		}

		/* Write zeros to new cluster */
		woff = (u64)ctrl->first_data_sector * ctrl->bytes_per_sector;
		woff += (u64)(cl_num - 2) * ctrl->bytes_per_cluster;
		wlen = vmm_blockdev_write(ctrl->bdev, 
					node->cached_data, 
					woff, ctrl->bytes_per_cluster);
		if (wlen != ctrl->bytes_per_cluster) {
			return 0;
		}
	}

	/* Write data to required location */
	w = 0;
	rc = fatfs_control_nth_cluster(ctrl, node->first_cluster, 
					wstartcl, &cl_num);
	if (rc) {
		return 0;
	}
	while (w < len) {
		/* Current cluster info */
		cl_off = umod64(pos + w, ctrl->bytes_per_cluster);
		cl_len = ctrl->bytes_per_cluster - cl_off;
		cl_len = (len < cl_len) ? len : cl_len;

		/* Write next cluster */
		woff = (u64)ctrl->first_data_sector * ctrl->bytes_per_sector;
		woff += (u64)(cl_num - 2) * ctrl->bytes_per_cluster;
		woff += cl_off;
		wlen = vmm_blockdev_write(ctrl->bdev, buf, woff, cl_len);
		if (wlen != cl_len) {
			break;
		}

		/* Update iteration */
		w += cl_len;
		buf += cl_len;

		/* Go to next cluster */
		rc = fatfs_control_nth_cluster(ctrl, cl_num, 1, &cl_num);
		if (rc) {
			break;
		}
	}

	/* Update node size */
	if (!(node->parent_dent.file_attributes & FAT_DIRENT_SUBDIR)) {
		if (__le32(node->parent_dent.file_size) < (pos + len)) {
			node->parent_dent.file_size = __le32(pos + len);
		}
	}

	/* Update node modify time */
	fatfs_current_timestamp(&year, &mon, &day, &hour, &min, &sec);
	node->parent_dent.lmodify_date_year = year;
	node->parent_dent.lmodify_date_month = mon;
	node->parent_dent.lmodify_date_day = day;
	node->parent_dent.lmodify_time_hours = min;
	node->parent_dent.lmodify_time_minutes = min;
	node->parent_dent.lmodify_time_seconds = sec;

	/* Mark node directory entry as dirty */
	node->parent_dent_dirty = TRUE;

	return w;
}

int fatfs_node_truncate(struct fatfs_node *node, u32 pos)
{
	int rc;
	u32 cl_pos, cl_off, cl_num;
	u32 year, mon, day, hour, min, sec;
	struct fatfs_control *ctrl = node->ctrl;

	if (!node->parent && ctrl->type != FAT_TYPE_32) {
		/* For FAT12 and FAT16 ignore root node truncation */
		return VMM_OK;
	}

	/* Determine last cluster after truncation */
	cl_pos = udiv32(pos, ctrl->bytes_per_cluster);
	cl_off = pos - cl_pos * ctrl->bytes_per_cluster;
	if (cl_off) {
		cl_pos += 1;
	}
	rc = fatfs_control_nth_cluster(ctrl, node->first_cluster, 
					cl_pos, &cl_num);
	if (rc) {
		return rc;
	}

	/* Remove all clusters after last cluster */
	rc = fatfs_control_truncate_clusters(ctrl, cl_num);
	if (rc) {
		return rc;
	}

	/* Update node size */
	if (!(node->parent_dent.file_attributes & FAT_DIRENT_SUBDIR)) {
		if (pos < __le32(node->parent_dent.file_size)) {
			node->parent_dent.file_size = __le32(pos);
		}
	}

	/* Update node modify time */
	fatfs_current_timestamp(&year, &mon, &day, &hour, &min, &sec);
	node->parent_dent.lmodify_date_year = year;
	node->parent_dent.lmodify_date_month = mon;
	node->parent_dent.lmodify_date_day = day;
	node->parent_dent.lmodify_time_hours = min;
	node->parent_dent.lmodify_time_minutes = min;
	node->parent_dent.lmodify_time_seconds = sec;

	/* Mark node directory entry as dirty */
	node->parent_dent_dirty = TRUE;

	return VMM_OK;
}

int fatfs_node_sync(struct fatfs_node *node)
{
	int rc;

	rc = fatfs_node_sync_cached_cluster(node);
	if (rc) {
		return rc;
	}

	rc = fatfs_node_sync_parent_dent(node);
	if (rc) {
		return rc;
	}

	return VMM_OK;
}

int fatfs_node_init(struct fatfs_control *ctrl, struct fatfs_node *node)
{
	int idx;

	node->ctrl = ctrl;
	node->parent = NULL;
	node->parent_dent_off = 0;
	node->parent_dent_len = 0;
	memset(&node->parent_dent, 0, sizeof(struct fat_dirent));
	node->parent_dent_dirty = TRUE;
	node->first_cluster = 0;

	node->cached_clust = 0;
	node->cached_data = vmm_zalloc(ctrl->bytes_per_cluster);
	node->cached_dirty = FALSE;

	node->lookup_victim = 0;
	for (idx = 0; idx < FAT_NODE_LOOKUP_SIZE; idx++) {
		node->lookup_name[idx][0] = '\0';
		node->lookup_off[idx] = 0;
		node->lookup_len[idx] = 0;
	}

	return VMM_OK;
}

int fatfs_node_exit(struct fatfs_node *node)
{
	vmm_free(node->cached_data);
	node->cached_clust = 0;
	node->cached_data = NULL;
	node->cached_dirty = FALSE;

	return VMM_OK;
}

int fatfs_node_read_dirent(struct fatfs_node *dnode, 
			    loff_t off, struct dirent *d)
{
	u32 i, rlen, len;
	char lname[VFS_MAX_NAME];
	struct fat_dirent dent;
	struct fat_longname lfn;
	u32 fileoff = (u32)off;

	if (umod32(fileoff, sizeof(struct fat_dirent))) {
		return VMM_EINVALID;
	}

	memset(lname, 0, sizeof(lname));
	d->d_off = off;
	d->d_reclen = 0;

	do {
		rlen = fatfs_node_read(dnode, fileoff, 
				sizeof(struct fat_dirent), (u8 *)&dent);
		if (rlen != sizeof(struct fat_dirent)) {
			return VMM_EIO;
		}

		if (dent.dos_file_name[0] == 0x0) {
			return VMM_ENOENT;
		}

		d->d_reclen += sizeof(struct fat_dirent);
		fileoff += sizeof(struct fat_dirent);

		if ((dent.dos_file_name[0] == 0xE5) ||
		    (dent.dos_file_name[0] == 0x2E)) {
			continue;
		}

		if (dent.file_attributes == FAT_LONGNAME_ATTRIBUTE) {
			memcpy(&lfn, &dent, sizeof(struct fat_longname));
			if (FAT_LONGNAME_LASTSEQ(lfn.seqno)) {
				memset(lname, 0, sizeof(lname));
				lfn.seqno = FAT_LONGNAME_SEQNO(lfn.seqno);
			}
			if ((lfn.seqno < FAT_LONGNAME_MINSEQ) ||
			    (FAT_LONGNAME_MAXSEQ < lfn.seqno)) {
				continue;
			}
			len = (lfn.seqno - 1) * 13;
			lname[len + 0] = (char)__le16(lfn.name_utf16_1[0]);
			lname[len + 1] = (char)__le16(lfn.name_utf16_1[1]);
			lname[len + 2] = (char)__le16(lfn.name_utf16_1[2]);
			lname[len + 3] = (char)__le16(lfn.name_utf16_1[3]);
			lname[len + 4] = (char)__le16(lfn.name_utf16_1[4]);
			lname[len + 5] = (char)__le16(lfn.name_utf16_2[0]);
			lname[len + 6] = (char)__le16(lfn.name_utf16_2[1]);
			lname[len + 7] = (char)__le16(lfn.name_utf16_2[2]);
			lname[len + 8] = (char)__le16(lfn.name_utf16_2[3]);
			lname[len + 9] = (char)__le16(lfn.name_utf16_2[4]);
			lname[len + 10] = (char)__le16(lfn.name_utf16_2[5]);
			lname[len + 11] = (char)__le16(lfn.name_utf16_3[0]);
			lname[len + 12] = (char)__le16(lfn.name_utf16_3[1]);
			continue;
		}

		if (dent.file_attributes & FAT_DIRENT_VOLLABLE) {
			continue;
		}

		if (!strlen(lname)) {
			i = 8;
			while (i && (dent.dos_file_name[i-1] == ' ')) {
				dent.dos_file_name[i-1] = '\0';
				i--;
			}
			i = 3;
			while (i && (dent.dos_extension[i-1] == ' ')) {
				dent.dos_extension[i-1] = '\0';
				i--;
			}
			memcpy(lname, dent.dos_file_name, 8);
			if (dent.dos_extension[0] != '\0') {
				len = strlen(lname);
				lname[len] = '.';
				lname[len + 1] = dent.dos_extension[0];
				lname[len + 2] = dent.dos_extension[1];
				lname[len + 3] = dent.dos_extension[2];
				lname[len + 4] = '\0';
			}
		}

		if (strlcpy(d->d_name, lname, sizeof(d->d_name)) >=
		    sizeof(d->d_name)) {
			return VMM_EOVERFLOW;
		}

		break;
	} while (1);

	if (dent.file_attributes & FAT_DIRENT_SUBDIR) {
		d->d_type = DT_DIR;
	} else {
		d->d_type = DT_REG;
	}

	/* Add dent to lookup table */
	fatfs_node_add_lookup_dirent(dnode, d->d_name, 
				     &dent, d->d_off, d->d_reclen);

	return VMM_OK;
}

int fatfs_node_find_dirent(struct fatfs_node *dnode, 
			   const char *name,
			   struct fat_dirent *dent, 
			   u32 *dent_off, u32 *dent_len)
{
	u32 i, off, rlen, len, lfn_off, lfn_len;
	struct fat_longname lfn;
	char lname[VFS_MAX_NAME];

	/* Try to find in lookup table */
	if (fatfs_node_find_lookup_dirent(dnode, name, 
					  dent, dent_off, dent_len) > -1) {
		return VMM_OK;
	}

	lfn_off = 0;
	lfn_len = 0;
	memset(lname, 0, sizeof(lname));

	off = 0;
	while (1) {
		rlen = fatfs_node_read(dnode, off, 
				sizeof(struct fat_dirent), (u8 *)dent);
		if (rlen != sizeof(struct fat_dirent)) {
			return VMM_EIO;
		}

		if (dent->dos_file_name[0] == 0x0) {
			return VMM_ENOENT;
		}

		off += sizeof(struct fat_dirent);

		if ((dent->dos_file_name[0] == 0xE5) ||
		    (dent->dos_file_name[0] == 0x2E)) {
			continue;
		}

		if (dent->file_attributes == FAT_LONGNAME_ATTRIBUTE) {
			memcpy(&lfn, dent, sizeof(struct fat_longname));
			if (FAT_LONGNAME_LASTSEQ(lfn.seqno)) {
				lfn.seqno = FAT_LONGNAME_SEQNO(lfn.seqno);
				lfn_off = off - sizeof(struct fat_dirent);
				lfn_len = lfn.seqno * sizeof(struct fat_longname);
				memset(lname, 0, sizeof(lname));
			}
			if ((lfn.seqno < FAT_LONGNAME_MINSEQ) ||
			    (FAT_LONGNAME_MAXSEQ < lfn.seqno)) {
				continue;
			}
			len = (lfn.seqno - 1) * 13;
			lname[len + 0] = (char)__le16(lfn.name_utf16_1[0]);
			lname[len + 1] = (char)__le16(lfn.name_utf16_1[1]);
			lname[len + 2] = (char)__le16(lfn.name_utf16_1[2]);
			lname[len + 3] = (char)__le16(lfn.name_utf16_1[3]);
			lname[len + 4] = (char)__le16(lfn.name_utf16_1[4]);
			lname[len + 5] = (char)__le16(lfn.name_utf16_2[0]);
			lname[len + 6] = (char)__le16(lfn.name_utf16_2[1]);
			lname[len + 7] = (char)__le16(lfn.name_utf16_2[2]);
			lname[len + 8] = (char)__le16(lfn.name_utf16_2[3]);
			lname[len + 9] = (char)__le16(lfn.name_utf16_2[4]);
			lname[len + 10] = (char)__le16(lfn.name_utf16_2[5]);
			lname[len + 11] = (char)__le16(lfn.name_utf16_3[0]);
			lname[len + 12] = (char)__le16(lfn.name_utf16_3[1]);
			continue;
		}

		if (dent->file_attributes & FAT_DIRENT_VOLLABLE) {
			continue;
		}

		if (!strlen(lname)) {
			lfn_off = off - sizeof(struct fat_dirent);
			lfn_len = 0;
			i = 8;
			while (i && (dent->dos_file_name[i-1] == ' ')) {
				dent->dos_file_name[i-1] = '\0';
				i--;
			}
			i = 3;
			while (i && (dent->dos_extension[i-1] == ' ')) {
				dent->dos_extension[i-1] = '\0';
				i--;
			}
			memcpy(lname, dent->dos_file_name, 8);
			if (dent->dos_extension[0] != '\0') {
				len = strlen(lname);
				lname[len] = '.';
				lname[len + 1] = dent->dos_extension[0];
				lname[len + 2] = dent->dos_extension[1];
				lname[len + 3] = dent->dos_extension[2];
				lname[len + 4] = '\0';
			}
		}

		if (!strncmp(lname, name, VFS_MAX_NAME)) {
			*dent_off = lfn_off;
			*dent_len = sizeof(struct fat_dirent) + lfn_len;
			break;
		}

		lfn_off = off;
		lfn_len = 0;
		memset(lname, 0, sizeof(lname));
	}

	/* Add dent to lookup table */
	fatfs_node_add_lookup_dirent(dnode, lname, 
				     dent, *dent_off, *dent_len);

	return VMM_OK;
}

