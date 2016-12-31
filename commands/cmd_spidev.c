/**
 * Copyright (c) 2016 Chaitanya Dhere.
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
 * @file cmd_spidev.c
 * @author Chaitanya Dhere (chaitanyadhere1@gmail.com)
 * @brief Implementation of spidev command
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_stdio.h>
#include <vmm_cmdmgr.h>
#include <vmm_modules.h>
#include <drv/spi/spidev.h>

#define MODULE_DESC                     "SPIDEV command"
#define MODULE_AUTHOR                   "Chaitanya Dhere"
#define MODULE_LICENSE                  "GPL"
#define MODULE_IPRIORITY                0
#define MODULE_INIT                     cmd_spidev_init
#define MODULE_EXIT                     cmd_spidev_exit

#define MAX_BUFLEN 256

static void cmd_spidev_usage(struct vmm_chardev *cdev)
{
        vmm_cprintf(cdev, "Usage:\n");
        vmm_cprintf(cdev, "   spidev list - Display SPI device list \n");
        vmm_cprintf(cdev, "   spidev xfer <mode> <output_frequency> "
			  "<bits_per_word> <id_num> <data_to_transfer> \n");
	vmm_cprintf(cdev, "Available modes - 0,1,2,3 \n Read supported "
			  "frequencies from SoC datasheet / manual,\n"
			  "Mode0 can be used for normal/loopback operations\n"
			  "Example command:\n"
			  "1. spidev xfer 0 0x12 (Uses the default mode, "
			      "frequency and bits per word)\n"
			  "2. spidev xfer 0 500000 8 "
			      "(Uses user defined values)\n"
			  "NOTE: Please use user defined options in the same "
			  "order and format as mentioned in Example2\n");
}

static int cmd_spidev_help(struct vmm_chardev *cdev,
			   int __unused argc, char __unused **argv)
{
        cmd_spidev_usage(cdev);
        return VMM_OK;
}

static int cmd_spidev_list(struct vmm_chardev *cdev,
			   int __unused argc, char __unused **argv)
{
	int id = 0, num, i;
	struct spidev *spidev;

	if (argc < 1) {
		cmd_spidev_usage(cdev);
                return VMM_EFAIL;
	}

	id = atoi(argv[1]);
	if (id < 0) {
		cmd_spidev_usage(cdev);
		return VMM_EFAIL;
	}

	num = spidev_count();
	vmm_cprintf(cdev, "Total %d spidev instances found : \n", num);
	for(i = 0; i < num; i++) {
		spidev = spidev_get(i);
		vmm_cprintf(cdev,"\n id = %d and spidev instance = %s\n",
			    i, spidev_name(spidev));
	}

	return VMM_OK;
}

static int cmd_spidev_xfer(struct vmm_chardev *cdev,
			   int argc, char **argv)
{
	int id = 0, ret = 0, index = 0, num = 0;
	struct spidev_xfer_data xfer;
	struct spidev *spidev;

	xfer.mode = -1;
	if (argc < 4) {
		ret = VMM_EINVALID;
		goto fail;
	} else if (argc > 4 && argc != 7) {
		ret = VMM_EINVALID;
		goto fail;
	}
	if (argc > 4) {
		index = 5;
		xfer.mode = atoi(argv[2]);
		if (xfer.mode < 0 || xfer.mode > 3) {
			ret = VMM_EINVALID;
			goto fail;
		}
		xfer.out_frequency = atoi(argv[3]);
		if (xfer.out_frequency < 0) {
			ret = VMM_EINVALID;
			goto fail;
		}
		xfer.bits_per_word = atoi(argv[4]);
		if (xfer.bits_per_word < 0) {
			ret = VMM_EINVALID;
			goto fail;
		}
	} else {
		index = 2;
	}

	num = spidev_count();
	id = atoi(argv[index]);
	if (id < 0) {
		ret = VMM_EINVALID;
		goto fail;
	} else if (id > num) {
		vmm_cprintf(cdev, "Please enter a valid ID using: "
				  "spidev list command\n");
		ret = VMM_EINVALID;
		goto fail;
	}
	spidev = spidev_get(id);
	if (!spidev) {
		vmm_cprintf(cdev, "Failed to get spidev from ID %d\n", id);
		ret = VMM_EINVALID;
		goto fail;
	}

	xfer.tx_buf = vmm_zalloc(MAX_BUFLEN);
	if (xfer.tx_buf == NULL) {
		vmm_cprintf(cdev, "Failed to allocate buffer for Tx data \n");
		ret = VMM_ENOMEM;
		goto fail;
	}

	xfer.rx_buf = vmm_zalloc(MAX_BUFLEN);
	if (xfer.rx_buf == NULL) {
		vmm_cprintf(cdev, "Failed to allocate buffer for Rx data \n");
		vmm_free(xfer.tx_buf);
		ret = VMM_ENOMEM;
		goto fail;
	}

	/* TODO: We should parse bytes instead of strcpy */
	strcpy((char*)xfer.tx_buf, argv[index+1]);
	xfer.len = strlen((const char *)xfer.tx_buf)+1;

	vmm_cprintf(cdev, "Submitting: %s to SPI device \n", xfer.tx_buf);
	ret = spidev_xfer(spidev, &xfer);
	if (ret < 0) {
		vmm_cprintf(cdev, "Failed submit data to the SPIDEV\n");
		vmm_free(xfer.rx_buf);
		vmm_free(xfer.tx_buf);
		return ret;
	}

	vmm_cprintf(cdev, "Received: %s as a reply from device \n",
		    xfer.rx_buf);

	vmm_free(xfer.rx_buf);
	vmm_free(xfer.tx_buf);

	return VMM_OK;

fail:
	cmd_spidev_usage(cdev);
	return ret;
}

static const struct {
	char *name;
	int (*function) (struct vmm_chardev *, int, char **);
} const command[] = {
	{"help", cmd_spidev_help},
	{"list", cmd_spidev_list},
	{"xfer", cmd_spidev_xfer},
	{NULL, NULL},
};

static int cmd_spidev_exec(struct vmm_chardev *cdev, int argc, char **argv)
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
	cmd_spidev_usage(cdev);
	return VMM_EFAIL;

}

static struct vmm_cmd cmd_spidev = {
	.name = "spidev",
	.desc = "control commands for SPIDEV devices",
	.usage = cmd_spidev_usage,
	.exec = cmd_spidev_exec,
};

static int __init cmd_spidev_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_spidev);
}

static void __exit cmd_spidev_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_spidev);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
