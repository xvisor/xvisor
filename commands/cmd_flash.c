/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
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
 * @file cmd_flash.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Flash command module
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_cmdmgr.h>
#include <vmm_modules.h>
#include <linux/mtd/mtd.h>

#define MODULE_DESC			"Command Flash"
#define MODULE_AUTHOR			"Jimmy Durand Wesolowski"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_flash_init
#define	MODULE_EXIT			cmd_flash_exit


static void cmd_flash_usage(struct vmm_chardev *cdev);

static int cmd_flash_list(struct vmm_chardev *cdev,
			  int __unused argc,
			  char __unused **argv)
{
	unsigned int i = 0;
	struct mtd_info *mtd = NULL;

	mtd = mtd_get_device(i++);
	if (!mtd)
		vmm_cprintf(cdev, "No MTD device registered\n");
	while (mtd) {
		vmm_cprintf(cdev, "MTD %d: %s\n", i, mtd->name);
		mtd = mtd_get_device(i++);
	}

	return VMM_OK;
}

typedef struct flash_op
{
	struct mtd_info *mtd;
	int id;
	off_t offset;
	size_t len;
	u_char *buf;
	size_t buf_len;
} flash_op;

#define DUMP_COUNT	8
static void dump_buf(struct vmm_chardev *cdev,
		     flash_op *op)
{
	size_t idx = 0;

	while ((op->buf_len - idx) > DUMP_COUNT) {
		vmm_cprintf(cdev, "0x%08X: 0x%02X 0x%02X 0x%02X 0x%02X "
			    "0x%02X 0x%02X 0x%02X 0x%02X\n",
			    idx + op->offset,
			    op->buf[idx],
			    op->buf[idx + 1],
			    op->buf[idx + 2],
			    op->buf[idx + 3],
			    op->buf[idx + 4],
			    op->buf[idx + 5],
			    op->buf[idx + 6],
			    op->buf[idx + 7]);
		idx += DUMP_COUNT;
	}

	if (op->buf_len - idx) {
		vmm_cprintf(cdev, "0x%08X:", idx + op->offset);

		while (idx < op->len) {
			vmm_cprintf(cdev, " 0x%02X", op->buf[idx++]);
		}

		vmm_cprintf(cdev, "\n");
	}
}

static int flash_args_common(struct vmm_chardev *cdev,
			     int argc,
			     char **argv,
			     flash_op *op)
{
	memset(op, 0, sizeof (flash_op));

	if (argc < 3) {
		cmd_flash_usage(cdev);
		return VMM_EFAIL;
	}

	op->id = atoi(argv[2]);
	if (NULL == (op->mtd = mtd_get_device(op->id))) {
		vmm_cprintf(cdev, "MTD device id %d does not exists\n",
			    op->id);
		return VMM_ENODEV;
	}

	op->offset = 0;
	if (argc >= 4) {
		op->offset = strtoull(argv[3], NULL, 0);
	}

	return VMM_OK;
}

static int flash_args(struct vmm_chardev *cdev,
		      int argc,
		      char **argv,
		      flash_op *op)
{
	int err = 0;

	if (VMM_OK != (err = flash_args_common(cdev, argc, argv, op))) {
		return err;
	}

	if (argc >= 5) {
		op->len = strtoull(argv[4], NULL, 0);
		if (1 > op->len) {
			vmm_cprintf(cdev, "Ucorrect length %d\n", op->len);
			return VMM_EFAIL;
		}
	}
	else {
		if (MTD_NANDFLASH == op->mtd->type) {
			op->len = op->mtd->writesize;
		}
		else {
			op->len = op->mtd->erasesize;
		}
	}

	op->buf_len = op->len;
	if (VMM_PAGE_SIZE < op->buf_len) {
		op->buf_len = VMM_PAGE_SIZE;
	}

	return VMM_OK;
}

static int cmd_flash_read(struct vmm_chardev *cdev,
			  int argc,
			  char **argv)
{
	int err = 0;
	size_t retlen = 0;
	flash_op op;

	if (VMM_OK != (err = flash_args(cdev, argc, argv, &op))) {
		return err;
	}

	if (NULL == (op.buf = vmm_malloc(op.buf_len))) {
		vmm_cprintf(cdev, "Failed to allocate read buffer\n");
		return VMM_ENOMEM;
	}

	while (op.len) {
		vmm_cprintf(cdev, "Reading flash %s from 0x%08X 0x%X bytes\n",
			    op.mtd->name, op.offset, op.buf_len);
		if (0 != (err = mtd_read(op.mtd, op.offset, op.buf_len,
					 &retlen, op.buf))) {
			vmm_cprintf(cdev, "Failed to read the mtd device %s\n",
				    op.mtd->name);
			vmm_free(op.buf);
			return err;
		}
		if (retlen < op.buf_len) {
			op.buf_len = retlen;
			op.len = op.buf_len;
		}
		dump_buf(cdev, &op);
		op.offset += op.buf_len;
		op.len -= op.buf_len;
	}
	vmm_free(op.buf);

	return VMM_OK;
}

static void flash_erase_cb(struct erase_info *info)
{
	struct vmm_chardev *cdev = (struct vmm_chardev*)info->priv;

	vmm_cprintf(cdev, "Done\n");
}

static int flash_question(struct vmm_chardev *cdev)
{
	char ans = 0;

	vmm_cprintf(cdev, "Continue [Y/n]?\n");
	ans = vmm_cgetc(cdev, FALSE);
	vmm_printf("\n");
	return (ans == '\n') || (ans == 'y') || (ans == 'Y');
}

static int flash_erase(struct vmm_chardev *cdev,
		       flash_op *op)
{
	struct erase_info info;

	if (op->len & op->mtd->erasesize_mask) {
		vmm_cprintf(cdev, "Uncorrect length 0x%X, a block size is "
			    "0x%08X\n", op->len, op->mtd->erasesize);
		return VMM_EFAIL;
	}

	op->offset &= ~op->mtd->erasesize_mask;

	while (op->len) {
		if (mtd_block_isbad(op->mtd, op->offset)) {
			vmm_cprintf(cdev, "%s block at 0x%08X is bad, "
				    "skipping...", op->mtd->name, op->offset);
			op->offset += op->mtd->erasesize;
			op->len -= op->mtd->erasesize;
			continue;
		}

		vmm_cprintf(cdev, "This will erasing the %s block at "
			    "0x%08X\n", op->mtd->name, op->offset);
		if (!flash_question(cdev)) {
			vmm_cprintf(cdev, "Skipping...\n");
			op->offset += op->mtd->erasesize;
			op->len -= op->mtd->erasesize;
			continue;
		}
		vmm_cprintf(cdev, "Erasing...\n");
		memset(&info, 0, sizeof (struct erase_info));
		info.mtd = op->mtd;
		info.addr = op->offset;
		info.len = op->len;
		info.priv = (u_long)cdev;
		info.callback = flash_erase_cb;
		vmm_printf("Sizeof %d, Addr 0x%08X, cb 0x%08X\n",
			   sizeof (struct erase_info),
			   &info, info.callback);
		mtd_erase(op->mtd, &info);
	}
	return VMM_OK;
}

static int cmd_flash_erase(struct vmm_chardev *cdev,
			   int argc,
			   char **argv)
{
	int err = VMM_OK;
	flash_op op;

	if (VMM_OK != flash_args(cdev, argc, argv, &op)) {
		return err;
	}
	if (VMM_OK != flash_erase(cdev, &op)) {
		return err;
	}
	return VMM_OK;
}

static int cmd_flash_write(struct vmm_chardev *cdev,
			   int  argc,
			   char **argv)
{
	int err = VMM_OK;
	int idx = 0;
	size_t retlen = 0;
	flash_op op;
	u_char *buf = NULL;

	if (VMM_OK != (err = flash_args_common(cdev, argc, argv, &op))) {
		return err;
	}

	vmm_cprintf(cdev, "Before writing, the %s block at 0x%08X must have "
		    "been erased?\n", op.mtd->name,
		    op.offset & ~op.mtd->erasesize_mask);
	if (!flash_question(cdev)) {
		vmm_cprintf(cdev, "Exiting...\n");
		return VMM_OK;
	}

	if (argc - 4 <= 0) {
		vmm_cprintf(cdev, "Nothing to write, exiting\n");
		return VMM_OK;
	}

	if (NULL == (buf = vmm_malloc(argc - 5))) {
		return VMM_ENOMEM;
	}

	for (idx = 0; idx < argc - 4; ++idx) {
		buf[idx] = strtoull(argv[idx + 4], NULL, 16);
		vmm_cprintf(cdev, "Writing at 0x%08X 0x%02X\n",
			    op.offset + idx, buf[idx]);
	}
	if (0 != mtd_write(op.mtd, op.offset, argc - 4, &retlen, buf)) {
		vmm_cprintf(cdev, "Failed to write %s at 0x%08X\n",
			    op.mtd->name, op.offset);
	}
	vmm_free(buf);

	return err;
}

static void cmd_flash_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   flash read <ID> [offset] [length] - Read flash "
		    "device 'ID' \n"
		    "  from 'offset' (0 by default), up to 'length' bytes "
		    "(a page size for NAND, a block size for NOR) by "
		    "default)\n");
	vmm_cprintf(cdev, "   flash erase <ID> [offset] [length] - Erase "
		    "flash device 'ID' \n"
		    "  from 'offset' (0 by default), up to 'length' bytes "
		    "(the length should be block aligned)\n");
	vmm_cprintf(cdev, "   flash write <ID> <offset> <bytes> ... - Write "
		    "on flash device 'ID' \n"
		    "  at 'offset' the given bytes given in hexadecimal format"
		    "\n");
	vmm_cprintf(cdev, "   flash list - List flash device with their ID\n");
}

static int cmd_flash_help(struct vmm_chardev *cdev,
			  int __unused argc,
			  char __unused **argv)
{
	cmd_flash_usage(cdev);
	return VMM_OK;
}

static const struct {
	char *name;
	int (*function) (struct vmm_chardev *, int, char **);
} const command[] = {
	{"help", cmd_flash_help},
	{"list", cmd_flash_list},
	{"read", cmd_flash_read},
	{"write", cmd_flash_write},
	{"erase", cmd_flash_erase},
	{NULL, NULL},
};

static int cmd_flash_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	int index = 0;

	while (command[index].name) {
		if (strcmp(argv[1], command[index].name) == 0) {
			return command[index].function(cdev, argc, argv);
		}
		index++;
	}

	cmd_flash_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_flash = {
	.name = "flash",
	.desc = "control commands for flash operations",
	.usage = cmd_flash_usage,
	.exec = cmd_flash_exec,
};

static int __init cmd_flash_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_flash);
}

static void __exit cmd_flash_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_flash);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
