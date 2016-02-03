/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file vsdaemon_chardev.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief vserial daemon chardev transport implementation
 */

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_heap.h>
#include <vmm_chardev.h>
#include <vmm_stdio.h>
#include <vmm_spinlocks.h>
#include <vmm_modules.h>
#include <libs/vsdaemon.h>

#define MODULE_DESC			"vsdaemon chardev transport"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(VSDAEMON_IPRIORITY + 1)
#define	MODULE_INIT			vsdaemon_chardev_init
#define	MODULE_EXIT			vsdaemon_chardev_exit

struct vsdaemon_chardev {
	/* character device pointer */
	struct vmm_chardev *cdev;
};

static void vsdaemon_chardev_receive_char(struct vsdaemon *vsd, u8 ch)
{
	struct vsdaemon_chardev *vcdev = vsdaemon_transport_get_data(vsd);

	vmm_cputc(vcdev->cdev, ch);
}

static int vsdaemon_chardev_main_loop(struct vsdaemon *vsd)
{
	char ch;
	struct vsdaemon_chardev *vcdev = vsdaemon_transport_get_data(vsd);

	while (1) {
		if (!vmm_scanchars(vcdev->cdev, &ch, 1, TRUE)) {
			while (!vmm_vserial_send(vsd->vser, (u8 *)&ch, 1)) ;
		}
	}

	return VMM_OK;
}

static int vsdaemon_chardev_setup(struct vsdaemon *vsd, int argc, char **argv)
{
	struct vsdaemon_chardev *vcdev;

	if (argc < 1) {
		return VMM_EINVALID;
	}

	vcdev = vmm_zalloc(sizeof(*vcdev));
	if (!vcdev) {
		return VMM_ENOMEM;
	}

	vcdev->cdev = vmm_chardev_find(argv[0]);
	if (!vcdev->cdev) {
		vmm_free(vcdev);
		return VMM_EINVALID;
	}

	vsdaemon_transport_set_data(vsd, vcdev);

	return VMM_OK;
}

static void vsdaemon_chardev_cleanup(struct vsdaemon *vsd)
{
	struct vsdaemon_chardev *vcdev = vsdaemon_transport_get_data(vsd);

	vsdaemon_transport_set_data(vsd, NULL);

	vmm_free(vcdev);
}

static struct vsdaemon_transport chardev = {
	.name = "chardev",
	.setup = vsdaemon_chardev_setup,
	.cleanup = vsdaemon_chardev_cleanup,
	.main_loop = vsdaemon_chardev_main_loop,
	.receive_char = vsdaemon_chardev_receive_char,
};

static int __init vsdaemon_chardev_init(void)
{
	return vsdaemon_transport_register(&chardev);
}

static void __exit vsdaemon_chardev_exit(void)
{
	vsdaemon_transport_unregister(&chardev);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
