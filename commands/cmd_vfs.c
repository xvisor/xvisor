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
 * @file cmd_vfs.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of vfs command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_wallclock.h>
#include <vmm_host_aspace.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <libs/stringlib.h>
#include <libs/vfs.h>

#define MODULE_DESC			"Command vfs"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VFS_IPRIORITY+1)
#define	MODULE_INIT			cmd_vfs_init
#define	MODULE_EXIT			cmd_vfs_exit

static void cmd_vfs_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   vfs help\n");
	vmm_cprintf(cdev, "   vfs fslist\n");
	vmm_cprintf(cdev, "   vfs mplist\n");
	vmm_cprintf(cdev, "   vfs mount <bdev_name> <path_to_mount>\n");
	vmm_cprintf(cdev, "   vfs umount <path_to_unmount>\n");
	vmm_cprintf(cdev, "   vfs ls <path_to_dir>\n");
	vmm_cprintf(cdev, "   vfs mv <old_path> <new_path>\n");
	vmm_cprintf(cdev, "   vfs rm <path_to_file>\n");
	vmm_cprintf(cdev, "   vfs mkdir <path_to_dir>\n");
	vmm_cprintf(cdev, "   vfs rmdir <path_to_dir>\n");	
	vmm_cprintf(cdev, "   vfs load <phys_addr> <path_to_file> "
			  "[<file_offset>] [<byte_count>]\n");
}

int cmd_vfs_fslist(struct vmm_chardev *cdev)
{
	int num, count;
	struct filesystem *fs;

	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_cprintf(cdev, " %-9s %-69s\n", "Num", "Name");
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	count = vfs_filesystem_count();
	for (num = 0; num < count; num++) {
		fs = vfs_filesystem_get(num);
		vmm_cprintf(cdev, " %-9d %-69s\n", num, fs->name);
	}
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");

	return VMM_OK;
}

static int cmd_vfs_mplist(struct vmm_chardev *cdev)
{
	int num, count;
	const char *mode;
	struct mount *m;

	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_cprintf(cdev, " %-15s %-11s %-11s %-39s\n", 
			  "BlockDev", "Filesystem", "Mode", "Path");
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	count = vfs_mount_count();
	for (num = 0; num < count; num++) {
		m = vfs_mount_get(num);
		switch (m->m_flags & MOUNT_MASK) {
		case MOUNT_RDONLY:
			mode = "read-only";
			break;
		case MOUNT_RW:
			mode = "read-write";
			break;
		default:
			mode = "unknown";
			break;
		};
		vmm_cprintf(cdev, " %-15s %-11s %-11s %-39s\n", 
				  m->m_dev->name, m->m_fs->name, 
				  mode, m->m_path);
	}
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");

	return VMM_OK;
}

static int cmd_vfs_mount(struct vmm_chardev *cdev, 
			 const char *dev, const char *path)
{
	int rc;
	bool found;
	int fd, num, count;
	struct vmm_blockdev *bdev;
	struct filesystem *fs;

	bdev = vmm_blockdev_find(dev);
	if (!bdev) {
		vmm_cprintf(cdev, "Block device %s not found\n", dev);
		return VMM_ENODEV;
	}

	if (strcmp(path, "/") != 0) {
		fd = vfs_opendir(path);
		if (fd < 0) {
			vmm_cprintf(cdev, "Directory %s not found\n", path);
			return fd;
		} else {
			vfs_closedir(fd);
		}
	}

	found = FALSE;
	count = vfs_filesystem_count();
	vmm_cprintf(cdev, "Trying:");
	for (num = 0; num < count; num++) {
		fs = vfs_filesystem_get(num);
		vmm_cprintf(cdev, " %s", fs->name);
		rc = vfs_mount(path, fs->name, dev, MOUNT_RW);
		if (!rc) {
			found = TRUE;
			vmm_cprintf(cdev, "\n"); 
			break;
		}
	}

	if (!found) {
		vmm_cprintf(cdev, "\nMount failed\n");
		return VMM_ENOSYS;
	}

	vmm_cprintf(cdev, "Mounted %s using %s at %s\n", dev, fs->name, path);

	return VMM_OK;
}

static int cmd_vfs_umount(struct vmm_chardev *cdev, const char *path)
{
	int rc;

	rc = vfs_unmount(path);
	if (rc) {
		vmm_cprintf(cdev, "Unmount failed\n"); 
	} else {
		vmm_cprintf(cdev, "Unmount successful\n"); 
	}

	return rc;
}

static int cmd_vfs_ls(struct vmm_chardev *cdev, const char *path)
{
	char type[11];
	char dpath[VFS_MAX_PATH];
	int fd, rc, plen, total_ent;
	struct vmm_timeinfo ti;
	struct stat st;
	struct dirent d;

	fd = vfs_opendir(path);
	if (fd < 0) {
		vmm_cprintf(cdev, "Failed to opendir %s\n", path);
		return fd;
	}

	strncpy(dpath, path, sizeof(dpath));
	plen = strlen(dpath);
	if (path[plen-1] != '/') {
		strncat(dpath, "/", sizeof(dpath));
		plen++;
	}

	total_ent = 0;
	while (!vfs_readdir(fd, &d)) {
		dpath[plen] = '\0';
		strncat(dpath, d.d_name, sizeof(dpath));
		rc = vfs_stat(dpath, &st);
		if (rc) {
			vfs_closedir(fd);
			vmm_cprintf(cdev, "Failed to get %s stat\n", dpath);
			return rc;
		}
		strcpy(type, "----------");
		if (st.st_mode & S_IFDIR) {
			type[0]= 'd';
		} else if (st.st_mode & S_IFCHR) {
			type[0]= 'c';
		} else if (st.st_mode & S_IFBLK) {
			type[0]= 'b';
		} else if (st.st_mode & S_IFLNK) {
			type[0]= 'l';
		}
		if (st.st_mode & S_IRUSR) {
			type[1] = 'r';
		}
		if (st.st_mode & S_IWUSR) {
			type[2] = 'w';
		}
		if (st.st_mode & S_IXUSR) {
			type[3] = 'x';
		}
		if (st.st_mode & S_IRGRP) {
			type[4] = 'r';
		}
		if (st.st_mode & S_IWGRP) {
			type[5] = 'w';
		}
		if (st.st_mode & S_IXGRP) {
			type[6] = 'x';
		}
		if (st.st_mode & S_IROTH) {
			type[7] = 'r';
		}
		if (st.st_mode & S_IWOTH) {
			type[8] = 'w';
		}
		if (st.st_mode & S_IXOTH) {
			type[9] = 'x';
		}
		vmm_cprintf(cdev, "%10s ", type);
		vmm_cprintf(cdev, "%10ll ", st.st_size);
		vmm_wallclock_mkinfo(st.st_mtime, 0, &ti);
		switch (ti.tm_mon) {
		case 0:
			vmm_cprintf(cdev, "%s ", "Jan");
			break;
		case 1:
			vmm_cprintf(cdev, "%s ", "Feb");
			break;
		case 2:
			vmm_cprintf(cdev, "%s ", "Mar");
			break;
		case 3:
			vmm_cprintf(cdev, "%s ", "Apr");
			break;
		case 4:
			vmm_cprintf(cdev, "%s ", "May");
			break;
		case 5:
			vmm_cprintf(cdev, "%s ", "Jun");
			break;
		case 6:
			vmm_cprintf(cdev, "%s ", "Jul");
			break;
		case 7:
			vmm_cprintf(cdev, "%s ", "Aug");
			break;
		case 8:
			vmm_cprintf(cdev, "%s ", "Sep");
			break;
		case 9:
			vmm_cprintf(cdev, "%s ", "Oct");
			break;
		case 10:
			vmm_cprintf(cdev, "%s ", "Nov");
			break;
		case 11:
			vmm_cprintf(cdev, "%s ", "Dec");
			break;
		default:
			break;
		};
		vmm_cprintf(cdev, "%02d %d %02d:%02d:%02d ", 
				  ti.tm_mday, ti.tm_year + 1900, 
				  ti.tm_hour, ti.tm_min, ti.tm_sec);
		vmm_cprintf(cdev, "%s\n", d.d_name);
		total_ent++;
	}
	vmm_cprintf(cdev, "total %d\n", total_ent);
	rc = vfs_closedir(fd);
	if (rc) {
		vmm_cprintf(cdev, "Failed to closedir %s\n", path);
		return rc;
	}

	return VMM_OK;
}

static int cmd_vfs_mv(struct vmm_chardev *cdev, 
			const char *old_path, const char *new_path)
{
	int rc;
	struct stat st;

	rc = vfs_stat(old_path, &st);
	if (rc) {
		vmm_cprintf(cdev, "Path %s does not exist.\n", old_path);
		return rc;
	}

	rc = vfs_rename(old_path, new_path);
	if (rc) {
		vmm_cprintf(cdev, "Failed to rename.\n");
		return rc;
	}

	return VMM_OK;
}

static int cmd_vfs_rm(struct vmm_chardev *cdev, const char *path)
{
	int rc;
	struct stat st;

	rc = vfs_stat(path, &st);
	if (rc) {
		vmm_cprintf(cdev, "Path %s does not exist.\n", path);
		return rc;
	}

	if (!(st.st_mode & S_IFREG)) {
		vmm_cprintf(cdev, "Path %s should be regular file.\n", path);
		return VMM_EINVALID;
	}

	return vfs_unlink(path);
}

static int cmd_vfs_mkdir(struct vmm_chardev *cdev, const char *path)
{
	int rc;
	struct stat st;

	rc = vfs_stat(path, &st);
	if (!rc) {
		vmm_cprintf(cdev, "Path %s already exist.\n", path);
		return VMM_EEXIST;
	}

	return vfs_mkdir(path, S_IRWXU|S_IRWXG|S_IRWXO);
}

static int cmd_vfs_rmdir(struct vmm_chardev *cdev, const char *path)
{
	int rc;
	struct stat st;

	rc = vfs_stat(path, &st);
	if (rc) {
		vmm_cprintf(cdev, "Path %s does not exist.\n", path);
		return rc;
	}

	if (!(st.st_mode & S_IFDIR)) {
		vmm_cprintf(cdev, "Path %s should be directory.\n", path);
		return VMM_EINVALID;
	}

	return vfs_rmdir(path);
}

#define VFS_LOAD_BUF_SZ		256

static int cmd_vfs_load(struct vmm_chardev *cdev, physical_addr_t pa, 
			const char *path, u32 off, u32 len)
{
	int fd, rc;
	size_t buf_rd, rd_count;
	char buf[VFS_LOAD_BUF_SZ];
	struct stat st;

	fd = vfs_open(path, O_RDONLY, 0);
	if (fd < 0) {
		vmm_cprintf(cdev, "Failed to open %s\n", path);
		return fd;
	}

	rc = vfs_fstat(fd, &st);
	if (rc) {
		vfs_close(fd);
		vmm_cprintf(cdev, "Failed to stat %s\n", path);
		return rc;
	}

	if (!(st.st_mode & S_IFREG)) {
		vfs_close(fd);
		vmm_cprintf(cdev, "Cannot read %s\n", path);
		return VMM_EINVALID;
	}

	if (off >= st.st_size) {
		vfs_close(fd);
		vmm_cprintf(cdev, "Offset greater than file size\n", path);
		return VMM_EINVALID;
	}

	len = ((st.st_size - off) < len) ? (st.st_size - off) : len;

	rd_count = 0;
	while (len) {
		buf_rd = (len < VFS_LOAD_BUF_SZ) ? len : VFS_LOAD_BUF_SZ;
		buf_rd = vfs_read(fd, buf, buf_rd);
		if (buf_rd < 1) {
			break;
		}
		vmm_host_physical_write(pa, buf, buf_rd);
		len -= buf_rd;
		rd_count += buf_rd;
	}

	vmm_cprintf(cdev, "Loaded %d bytes\n", rd_count);

	rc = vfs_close(fd);
	if (rc) {
		vmm_cprintf(cdev, "Failed to close %s\n", path);
		return rc;
	}

	return VMM_OK;
}

int cmd_vfs_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	u32 off, len;
	physical_addr_t pa;
	if (argc < 2) {
		cmd_vfs_usage(cdev);
		return VMM_EFAIL;
	}
	if (strcmp(argv[1], "help") == 0) {
		cmd_vfs_usage(cdev);
		return VMM_OK;
	} else if ((strcmp(argv[1], "fslist") == 0) && (argc == 2)) {
		return cmd_vfs_fslist(cdev);
	} else if ((strcmp(argv[1], "mplist") == 0) && (argc == 2)) {
		return cmd_vfs_mplist(cdev);
	} else if ((strcmp(argv[1], "mount") == 0) && (argc == 4)) {
		return cmd_vfs_mount(cdev, argv[2], argv[3]);
	} else if ((strcmp(argv[1], "umount") == 0) && (argc == 3)) {
		return cmd_vfs_umount(cdev, argv[2]);
	} else if ((strcmp(argv[1], "ls") == 0) && (argc == 3)) {
		return cmd_vfs_ls(cdev, argv[2]);
	} else if ((strcmp(argv[1], "mv") == 0) && (argc == 4)) {
		return cmd_vfs_mv(cdev, argv[2], argv[3]);
	} else if ((strcmp(argv[1], "rm") == 0) && (argc == 3)) {
		return cmd_vfs_rm(cdev, argv[2]);
	} else if ((strcmp(argv[1], "mkdir") == 0) && (argc == 3)) {
		return cmd_vfs_mkdir(cdev, argv[2]);
	} else if ((strcmp(argv[1], "rmdir") == 0) && (argc == 3)) {
		return cmd_vfs_rmdir(cdev, argv[2]);
	} else if ((strcmp(argv[1], "load") == 0) && (argc > 3)) {
		pa = (physical_addr_t)str2ulonglong(argv[2], 10);
		off = (argc > 4) ? str2uint(argv[4], 10) : 0;
		len = (argc > 5) ? str2uint(argv[5], 10) : 0xFFFFFFFF;
		return cmd_vfs_load(cdev, pa, argv[3], off, len);
	}
	cmd_vfs_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_vfs = {
	.name = "vfs",
	.desc = "vfs related commands",
	.usage = cmd_vfs_usage,
	.exec = cmd_vfs_exec,
};

static int __init cmd_vfs_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_vfs);
}

static void __exit cmd_vfs_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_vfs);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
