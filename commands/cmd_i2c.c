/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the
 * Free Software Foundation
 * 51 Franklin Street, Fifth Floor
 * Boston, MA  02110-1301, USA.
 *
 * The I2C detection and functionality parts are an Xvisor adaptation of the
 * userland i2cdetect tool
 *  Copyright (C) 1999-2004  Frodo Looijaard <frodol@dds.nl>, and
 *                           Mark D. Studebaker <mdsxyz123@yahoo.com>
 *  Copyright (C) 2004-2012  Jean Delvare <jdelvare@suse.de>
 *
 * @file cmd_i2c.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief I2C commands
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_cmdmgr.h>
#include <vmm_modules.h>
#include <linux/i2c.h>

#define MODULE_DESC			"I2C command"
#define MODULE_AUTHOR			"Jimmy Durand Wesolowski"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_i2c_init
#define	MODULE_EXIT			cmd_i2c_exit

static void cmd_i2c_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   i2c list - Display i2c device list\n");
	vmm_cprintf(cdev, "   i2c detect <id> [quick|read] - Detect i2c "
		    "client devices on I2C bus \"id\"\n");
	vmm_cprintf(cdev, "   i2c funcs <id> - Get i2c bus \"id\" "
		    "functionalities\n");
}

static int cmd_i2c_help(struct vmm_chardev *cdev,
			 int __unused argc,
			 char __unused **argv)
{
	cmd_i2c_usage(cdev);
	return VMM_OK;
}

static int i2c_print_dev(struct device *dev, void *data)
{
	struct vmm_chardev *cdev = data;
	struct i2c_adapter *adap = NULL;

	if (NULL != (adap = i2c_verify_adapter(dev))) {
		vmm_cprintf(cdev, " %2d %-16s %-16s", adap->nr, dev->name,
			    "adapter");
	} else {
		vmm_cprintf(cdev, "    %-16s %-16s", dev->name, "client");
	}

	if (dev->parent) {
		vmm_cprintf(cdev, " %-16s\n", dev->parent->name);
	} else {
		vmm_cprintf(cdev, " ----------------\n");
	}

	return 0;
}

static int cmd_i2c_list(struct vmm_chardev *cdev,
			int __unused argc,
			char __unused **argv)
{
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_cprintf(cdev, "%-2s %-16s %-16s %-16s\n", "ID", "Name", "Type",
		    "Parent");
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	i2c_for_each_dev(cdev, i2c_print_dev);
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");

	return 0;
}

static int cmd_i2c_common(struct vmm_chardev *cdev,
			  int argc,
			  char **argv,
			  struct i2c_adapter **adap,
			  unsigned int *funcs)
{
	int id = -1;

	if (argc < 3) {
		cmd_i2c_usage(cdev);
		return VMM_EFAIL;
	}

	id = atoi(argv[2]);
	if (id < 0) {
		cmd_i2c_usage(cdev);
		return VMM_EFAIL;
	}

	if (NULL == (*adap = i2c_get_adapter(id))) {
		vmm_cprintf(cdev, "Failed to get adapter %d\n", id);
		return VMM_ENODEV;
	}

	*funcs = i2c_get_functionality(*adap);
	return VMM_OK;
}

#define MODE_AUTO	0
#define MODE_QUICK	1
#define MODE_READ	2

static int i2c_scan_bus(struct vmm_chardev *cdev,
			struct i2c_adapter *adap,
			int mode,
			unsigned int funcs,
			int first,
			int last)
{
	int i = 0;
	int j = 0;
	int cmd = 0;
	int res = 0;
	union i2c_smbus_data data;

	vmm_cprintf(cdev, "I2C detect on %s\n", adap->name);
	vmm_cprintf(cdev, "     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  "
		    "f\n");

	for (i = 0; i < 128; i += 16) {
		vmm_cprintf(cdev, "%02x: ", i);
		for(j = 0; j < 16; j++) {
			cmd = mode;

			/* Select detection command for this address */
			if (MODE_AUTO == mode) {
				if ((i+j >= 0x30 && i+j <= 0x37)
				    || (i+j >= 0x50 && i+j <= 0x5F)) {
				 	cmd = MODE_READ;
				} else {
					cmd = MODE_QUICK;
				}
			}

			/* Skip unwanted addresses */
			if (i + j < first || i + j > last
			    || (cmd == MODE_READ &&
				!(funcs & I2C_FUNC_SMBUS_READ_BYTE))
			    || (cmd == MODE_QUICK &&
				!(funcs & I2C_FUNC_SMBUS_QUICK))) {
				vmm_cprintf(cdev, "   ");
				continue;
			}

			if (MODE_READ == cmd) {
				/* This is known to lock SMBus on various
				   write-only chips (mainly clock chips) */
				res = i2c_smbus_xfer(adap, i + j, 0,
						     I2C_SMBUS_READ, 0,
						     I2C_SMBUS_BYTE, &data);
			} else {
				/* MODE_QUICK */
				/* This is known to corrupt the Atmel AT24RF08
				   EEPROM */
				res = i2c_smbus_xfer(adap, i + j, 0,
						     I2C_SMBUS_WRITE, 0,
						     I2C_SMBUS_QUICK, NULL);
			}

			if (res < 0)
				vmm_cprintf(cdev, "-- ");
			else
				vmm_cprintf(cdev, "%02x ", i + j);
		}
		vmm_cprintf(cdev, "\n");
	}

	return VMM_OK;
}

static int cmd_i2c_detect(struct vmm_chardev *cdev,
			  int argc,
			  char **argv)
{
	char ans = 0;
	int err = VMM_OK;
	int mode = MODE_AUTO;
	int first = 0x03;
	int last = 0x77;
	unsigned int funcs = 0;
	struct i2c_adapter *adap = NULL;

	err = cmd_i2c_common(cdev, argc, argv, &adap, &funcs);
	if (VMM_OK != err) {
		return err;
	}

	if (argc >= 4) {
		if (!strcmp(argv[3], "read")) {
			mode = MODE_READ;
		} else if (!strcmp(argv[3], "quick")) {
			mode = MODE_QUICK;
		} else {
			vmm_cprintf(cdev, "Unknown mode 0x%x\n", mode);
		}
	}

	if (!(funcs & (I2C_FUNC_SMBUS_QUICK | I2C_FUNC_SMBUS_READ_BYTE))) {
		vmm_cprintf(cdev, "Error: Bus doesn't support detection "
			    "commands\n");
		err = VMM_EFAIL;
		goto out;
	}

	if (mode == MODE_AUTO) {
		if (!(funcs & I2C_FUNC_SMBUS_QUICK))
			vmm_cprintf(cdev, "Warning: Can't use SMBus Quick "
				    "Write command, will skip some "
				    "addresses\n");
		if (!(funcs & I2C_FUNC_SMBUS_READ_BYTE))
			vmm_cprintf(cdev, "Warning: Can't use SMBus Receive "
				    "Byte command, will skip some "
				    "addresses\n");
	}

	vmm_cprintf(cdev, "WARNING! This program can confuse your I2C "
		    "bus, cause data loss and worse!\n");
	vmm_cprintf(cdev, "Probing %s", adap->name);
	if (MODE_QUICK == mode) {
		vmm_cprintf(cdev, " using quick write commands\n");
	} else if (MODE_READ == mode)  {
		vmm_cprintf(cdev, " using receive byte commands\n");
	} else {
		vmm_cprintf(cdev, " (auto)\n");
	}
	vmm_cprintf(cdev, "  address range 0x%02x-0x%02x.\n", first, last);
	vmm_cprintf(cdev, "Continue? [Y/n] ");

	ans = vmm_cgetc(cdev, 0);
	if ('\n' != ans && 'y' != ans && 'Y' != ans) {
		vmm_cprintf(cdev, "Aborting on user request.\n");
		goto out;
	}

	err = i2c_scan_bus(cdev, adap, mode, funcs, first, last);
out:
	i2c_put_adapter(adap);

	return err;
}

struct func
{
	long value;
	const char* name;
};

static const struct func all_func[] = {
	{ .value = I2C_FUNC_I2C,
	  .name = "I2C" },
	{ .value = I2C_FUNC_SMBUS_QUICK,
	  .name = "SMBus Quick Command" },
	{ .value = I2C_FUNC_SMBUS_WRITE_BYTE,
	  .name = "SMBus Send Byte" },
	{ .value = I2C_FUNC_SMBUS_READ_BYTE,
	  .name = "SMBus Receive Byte" },
	{ .value = I2C_FUNC_SMBUS_WRITE_BYTE_DATA,
	  .name = "SMBus Write Byte" },
	{ .value = I2C_FUNC_SMBUS_READ_BYTE_DATA,
	  .name = "SMBus Read Byte" },
	{ .value = I2C_FUNC_SMBUS_WRITE_WORD_DATA,
	  .name = "SMBus Write Word" },
	{ .value = I2C_FUNC_SMBUS_READ_WORD_DATA,
	  .name = "SMBus Read Word" },
	{ .value = I2C_FUNC_SMBUS_PROC_CALL,
	  .name = "SMBus Process Call" },
	{ .value = I2C_FUNC_SMBUS_WRITE_BLOCK_DATA,
	  .name = "SMBus Block Write" },
	{ .value = I2C_FUNC_SMBUS_READ_BLOCK_DATA,
	  .name = "SMBus Block Read" },
	{ .value = I2C_FUNC_SMBUS_BLOCK_PROC_CALL,
	  .name = "SMBus Block Process Call" },
	{ .value = I2C_FUNC_SMBUS_PEC,
	  .name = "SMBus PEC" },
	{ .value = I2C_FUNC_SMBUS_WRITE_I2C_BLOCK,
	  .name = "I2C Block Write" },
	{ .value = I2C_FUNC_SMBUS_READ_I2C_BLOCK,
	  .name = "I2C Block Read" },
	{ .value = 0, .name = "" }
};

static int cmd_i2c_funcs(struct vmm_chardev *cdev,
			 int argc,
			 char **argv)
{
	int i = VMM_OK;
	int err = VMM_OK;
	unsigned int funcs = 0;
	struct i2c_adapter *adap = NULL;

	err = cmd_i2c_common(cdev, argc, argv, &adap, &funcs);
	if (VMM_OK != err) {
		return err;
	}

	for (i = 0; all_func[i].value; i++) {
		vmm_cprintf(cdev, "%-32s %s\n", all_func[i].name,
			    (funcs & all_func[i].value) ? "yes" : "no");
	}

	return VMM_OK;
}

static const struct {
	char *name;
	int (*function) (struct vmm_chardev *, int, char **);
} const command[] = {
	{"help", cmd_i2c_help},
	{"list", cmd_i2c_list},
	{"detect", cmd_i2c_detect},
	{"funcs", cmd_i2c_funcs},
	{NULL, NULL},
};

static int cmd_i2c_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	int index = 0;

	if (argc < 2) {
		goto fail;
	}

	while (command[index].name) {
		if (strcmp(argv[1], command[index].name) == 0) {
			return command[index].function(cdev, argc, argv);
		}
		index++;
	}

fail:
	cmd_i2c_usage(cdev);
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_i2c = {
	.name = "i2c",
	.desc = "control commands for i2c devices",
	.usage = cmd_i2c_usage,
	.exec = cmd_i2c_exec,
};

static int __init cmd_i2c_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_i2c);
}

static void __exit cmd_i2c_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_i2c);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
