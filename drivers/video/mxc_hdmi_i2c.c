/*
 * Copyright (C) 2016 Open Wide
 * Copyright (C) 2016 Institut de Recherche Technologique SystemX
 *
 *
 * Copyright (C) 2011-2014 Freescale Semiconductor, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * Copyright (C) 2010, Guennadi Liakhovetski <g.liakhovetski@gmx.de>
 *
 * SH-Mobile High-Definition Multimedia Interface (HDMI) driver
 * for SLISHDMI13T and SLIPHDMIT IP cores
 *
 *
 * @file mxc_hdmi_i2c.c
 * @author Jean Guyomarc'h (jean.guyomarch@openwide.fr)
 */

#include <vmm_modules.h>
#include <vmm_devdrv.h>
#include <vmm_devres.h>
#include <vmm_devtree.h>
#include <vmm_notifier.h>

#include <linux/types.h>
#include <linux/printk.h>
#include <linux/fb.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/i2c.h>
#include <linux/mod_devicetable.h>

#include <video/ipu_pixfmt.h>

#include <imx-common.h>
#include <mxc_dispdrv.h>


#define MODULE_AUTHOR		"Jean Guyomarc'h"
#define MODULE_DESC		"MXC HDMI I2C driver"
#define MODULE_LICENSE		"GPL"
#define MODULE_IPRIORITY	I2C_IPRIORITY
#define MODULE_INIT		hdmi_i2c_init
#define MODULE_EXIT		hdmi_i2c_exit

static struct i2c_client *hdmi_i2c = NULL;

struct i2c_client *mxc_hdmi_get_i2c_client(void)
{
	return hdmi_i2c;
}
EXPORT_SYMBOL(mxc_hdmi_get_i2c_client);

static int hdmi_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_BYTE | I2C_FUNC_I2C)) {
		vmm_lerror(NULL, "Failed to get I2C client\n");
		return VMM_ENODEV;
	}

	hdmi_i2c = client;
	return VMM_OK;
}

static int hdmi_i2c_remove(struct i2c_client *client)
{
	hdmi_i2c = NULL;
	return VMM_OK;
}

static const struct i2c_device_id hdmi_i2c_id[] = {
	{ "mxc_hdmi_i2c", 0 },
	{ /* sentinel */ }
};

static const struct of_device_id imx_hdmi_i2c_match[] = {
	{ .compatible = "fsl,imx6-hdmi-i2c", },
	{ /* sentinel */ }
};

static struct i2c_driver hdmi_i2c_driver = {
	.driver = {
		.name =  "mxc_hdmi_i2c",
		.match_table = imx_hdmi_i2c_match,
	},
	.probe = hdmi_i2c_probe,
	.remove = hdmi_i2c_remove,
	.id_table = hdmi_i2c_id,
};

static int __init hdmi_i2c_init(void)
{
	return i2c_add_driver(&hdmi_i2c_driver);
}

static void __exit hdmi_i2c_exit(void)
{
	i2c_del_driver(&hdmi_i2c_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
