/**
 * Copyright (c) 2014 Himanshu Chauhan
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
 * @file piix3_ide.c
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief PIIX3 IDE Suport
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_delay.h>
#include <vmm_timer.h>
#include <vmm_host_io.h>
#include <vmm_host_irq.h>
#include <vmm_devtree.h>
#include <vmm_devdrv.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <asm/io.h>
#include <drv/ide/ide.h>
#include <drv/ide/ata.h>

#define MODULE_DESC			"PIIX3 IDE"
#define MODULE_AUTHOR			"Himanshu Chauhan"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			piix3_ide_init
#define	MODULE_EXIT			piix3_ide_exit

struct piix3_ide_device {
	u8 bus;
	u8 device;
	u8 function;
};

struct piix3_ide_device piix3_ide_devices[2] = {
	/* QEMU PIIX3 */
	{
		.bus = 0,
		.device = 1,
		.function = 1
	},
	/* vmplayer PIIX3 */
	{
		.bus = 0,
		.device = 7,
		.function = 1
	}
};

#define TO_BDF(__b, __d, __f)	((__b << 16) | \
				 (__d << 11) | \
				 (__f << 8))

#define PIIX3_BDF(__dev)	(TO_BDF(__dev.bus,		\
					__dev.device,		\
					__dev.function))

static int piix3_ide_probe(struct vmm_device *dev,
			   const struct vmm_devtree_nodeid *devid)
{
	struct ide_host_controller *controller;
	struct ide_drive *drive;
	int i, j;

	for (j = 0; j < array_size(piix3_ide_devices); j++) {
		/* send the parameters */
		outl((1 << 31) | PIIX3_BDF(piix3_ide_devices[j]) | 8, 0xCF8);
		/* If device exists class won't be 0xFFFF */
		if ((inl(0xCFC) >> 16) != 0xFFFF) {
			vmm_printf("PIIX3: Found PIIX3 IDE Controller.\n");
			controller =
				vmm_zalloc(sizeof(struct ide_host_controller));
			if (controller == NULL) {
				vmm_printf("ERROR: Failed to allocate host "
					   "controller instance.\n");
				return VMM_ENOMEM;
			}

			controller->bar0 = 0x1f0;
			controller->bar1 = 0x3f6;
			controller->bar2 = 0x170;
			controller->bar3 = 0x376;

			if (ide_initialize(controller) != VMM_OK) {
				vmm_free(controller);
				vmm_printf("ERROR: Failed to initialize IDE "
					   "controller.\n");
				return VMM_ENODEV;
			}

			/* Print Summary: */
			for (i = 0; i < MAX_IDE_DRIVES; i++) {
				if (!controller->ide_drives[i].present) {
					continue;
				}

				drive = &controller->ide_drives[i];

				drive->dev = dev;

				vmm_printf(" Found %s Drive %dMB - [%s %s] %s\n",
					   (const char *[]){"ATA", "ATAPI"}[drive->type],
					   drive->size/1024/2,
					   (const char *[]){"Primary", "Secondary"}[drive->channel->id],
					   /* Same as above, using the drive */
					   (const char *[]){"Master", "Slave"}[drive->drive],
					   drive->model);

				if (ide_add_drive(drive) != VMM_OK) {
					vmm_printf("ERROR: Failed to add drive to block layer.\n");
					return VMM_EFAIL;
				}
			}

			return VMM_OK;
		}
	}

	return VMM_EFAIL;
}

static struct vmm_devtree_nodeid piix3_ide_devid_table[] = {
        { .compatible = "piix3_ide" },
	{ /* end of list */ },
};

static struct vmm_driver piix3_ide_driver = {
        .name = "piix3_ide",
        .match_table = piix3_ide_devid_table,
        .probe = piix3_ide_probe,
};

static int __init piix3_ide_init(void)
{
        return vmm_devdrv_register_driver(&piix3_ide_driver);
}

static void piix3_ide_exit(void)
{
	vmm_devdrv_unregister_driver(&piix3_ide_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
