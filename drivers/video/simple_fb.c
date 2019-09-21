/**
 * Copyright (c) 2019 Ashutosh Sharma.
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
 * @file simple_fb.c
 * @author Ashutosh Sharma (ashutosh.sharma.0204@gmail.com)
 * @brief  simple Framebuffer Driver
 *
 * This source is largely adapted from Linux sources:
 * <linux>/drivers/video/simple_fb.c
 *
 * Copyright (C) 2010 Broadcom
 *
 * The original code is licensed under the GPL.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/list.h>
#include <linux/clk.h>
#include <linux/hardirq.h>
#include <asm/sizes.h>

#define MODULE_DESC		"Simple Framebuffer Driver"
#define MODULE_AUTHOR		"Ashutosh Sharma"
#define MODULE_LICENSE		"GPL"
#define MODULE_IPRIORITY	(FB_CLASS_IPRIORITY+1)
#define MODULE_INIT		simple_fb_init
#define MODULE_EXIT		simple_fb_exit

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)		vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

static const char *simple_name = "Simple FB";

struct simple_fb_info {
	u32 xres, yres, xres_virtual, yres_virtual;
	u32 pitch, bpp;
	u32 xoffset, yoffset;
	virtual_addr_t base;
	u32 screen_size;
	u16 cmap[256];
};

/* save simple framebuffer parameters received from device tree */
struct simple_fb_data {
	u32  reg, width, height, depth, stride;
	char status[8];
	char format[16];
};

struct simple_fb {
	struct fb_info fb;
	struct platform_device *dev;
	struct simple_fb_data data;
	struct simple_fb_info info;
	virtual_addr_t dma;
	u32 cmap[16];
};

#define to_simple(info)	container_of(info, struct simple_fb, fb)

static int simple_fb_set_bitfields(struct fb_var_screeninfo *var)
{
	int ret = 0;

	memset(&var->transp, 0, sizeof(var->transp));

	var->red.msb_right   = 0;
	var->green.msb_right = 0;
	var->blue.msb_right  = 0;

	switch (var->bits_per_pixel) {
	case 1:
	case 2:
	case 4:
	case 8:
		var->red.length   = var->bits_per_pixel;
		var->red.offset   = 0;
		var->green.length = var->bits_per_pixel;
		var->green.offset = 0;
		var->blue.length  = var->bits_per_pixel;
		var->blue.offset  = 0;
		break;
	case 16:
		var->red.length  = 5;
		var->blue.length = 5;
		/*
		 * Green length can be 5 or 6 depending whether
		 * we're operating in RGB555 or RGB565 mode.
		 */
		if (var->green.length != 5 && var->green.length != 6)
			var->green.length = 6;
		break;
	case 24:
		var->red.length   = 8;
		var->blue.length  = 8;
		var->green.length = 8;
		break;
	case 32:
		var->red.length    = 8;
		var->green.length  = 8;
		var->blue.length   = 8;
		var->transp.length = 8;
		break;
	default:
		ret = -EINVAL;
		break;
	}

	/*
	 * >= 16bpp displays have separate colour component bitfields
	 * encoded in the pixel data.  Calculate their position from
	 * the bitfield length defined above.
	 */
	if (ret == 0 && var->bits_per_pixel >= 24) {
		var->red.offset = 0;
		var->green.offset = var->red.offset + var->red.length;
		var->blue.offset = var->green.offset + var->green.length;
		var->transp.offset = var->blue.offset + var->blue.length;
	} else if (ret == 0 && var->bits_per_pixel >= 16) {
		var->blue.offset = 0;
		var->green.offset = var->blue.offset + var->blue.length;
		var->red.offset = var->green.offset + var->green.length;
		var->transp.offset = var->red.offset + var->red.length;
	}

	return ret;
}

static int simple_fb_check_var(struct fb_var_screeninfo *var,
				struct fb_info *info)
{
	/* info input, var output */
	int yres;

	/* info input, var output */
	DPRINTF("%s: info(%p) %dx%d (%dx%d), %d, %d\n", __func__, info,
		info->var.xres, info->var.yres, info->var.xres_virtual,
		info->var.yres_virtual, (int)info->screen_size,
		info->var.bits_per_pixel);
	DPRINTF("%s: var(%p) %dx%d (%dx%d), %d\n", __func__, var,
		var->xres, var->yres, var->xres_virtual, var->yres_virtual,
		var->bits_per_pixel);

	if (!var->bits_per_pixel)
		var->bits_per_pixel = 32; //16;

	if (simple_fb_set_bitfields(var) != 0) {
		vmm_printf("simple_fb_check_var: invalid bits_per_pixel %d\n",
			   var->bits_per_pixel);
		return -EINVAL;
	}

	if (var->xres_virtual < var->xres)
		var->xres_virtual = var->xres;
	/* use highest possible virtual resolution */
	if (var->yres_virtual == -1) {
		var->yres_virtual = 480;
		DPRINTF("%s: virtual resolution set to maximum of %dx%d\n",
			__func__, var->xres_virtual, var->yres_virtual);
	}
	if (var->yres_virtual < var->yres)
		var->yres_virtual = var->yres;

	if (var->xoffset < 0)
		var->xoffset = 0;
	if (var->yoffset < 0)
		var->yoffset = 0;

	/* truncate xoffset and yoffset to maximum if too high */
	if (var->xoffset > var->xres_virtual - var->xres)
		var->xoffset = var->xres_virtual - var->xres - 1;
	if (var->yoffset > var->yres_virtual - var->yres)
		var->yoffset = var->yres_virtual - var->yres - 1;

	yres = var->yres;
	if (var->vmode & FB_VMODE_DOUBLE)
		yres *= 2;
	else if (var->vmode & FB_VMODE_INTERLACED)
		yres = (yres + 1) / 2;

	if (yres > 1200) {
		vmm_printf("%s: ERROR: VerticalTotal >= 1200; "
			   "special treatment required! (TODO)\n", __func__);
		return -EINVAL;
	}

	return 0;
}

static int simple_fb_set_par(struct fb_info *info)
{
	struct simple_fb *fb = to_simple(info);
	struct simple_fb_info *fbinfo = &fb->info;

	fbinfo->xres		= info->var.xres;
	fbinfo->yres		= info->var.yres;
	fbinfo->xres_virtual	= info->var.xres_virtual;
	fbinfo->yres_virtual	= info->var.yres_virtual;
	fbinfo->bpp		= info->var.bits_per_pixel;
	fbinfo->xoffset		= info->var.xoffset;
	fbinfo->yoffset		= info->var.yoffset;
	fbinfo->base		= fb->dma;
	fbinfo->pitch		= fb->data.stride;

	DPRINTF("%s: info(%p) %dx%d (%dx%d), %d, %d\n", __func__, info,
		info->var.xres, info->var.yres, info->var.xres_virtual,
		info->var.yres_virtual, (int)info->screen_size,
		info->var.bits_per_pixel);

	fb->fb.fix.line_length = fbinfo->pitch;

	if (info->var.bits_per_pixel <= 8)
		fb->fb.fix.visual = FB_VISUAL_PSEUDOCOLOR;
	else
		fb->fb.fix.visual = FB_VISUAL_TRUECOLOR;

	fb->fb.fix.smem_start	= fb->dma;
	fb->fb.fix.smem_len	= fbinfo->pitch * fbinfo->yres_virtual;
	fb->fb.screen_size	= fbinfo->screen_size;
	fb->fb.screen_base	= (void *)fb->dma;
	if (!fb->fb.screen_base) {
		BUG();/* what can we do here */
	}
	fb->fb.screen_size = fbinfo->xres*fbinfo->yres;
	DPRINTF("%s: start= %p,%p width=%d, height=%d, bpp=%d, "
		"pitch=%d size=%d\n", __func__,
		(void *)fb->fb.screen_base, (void *)fb->fb.fix.smem_start,
		fbinfo->xres, fbinfo->yres, fbinfo->bpp,
		fbinfo->pitch, fb->fb.screen_size);

	return 0;
}

static inline u32 convert_bitfield(int val, struct fb_bitfield *bf)
{
	unsigned int mask = (1 << bf->length) - 1;
	return (val >> (16 - bf->length) & mask) << bf->offset;
}

static int simple_fb_setcolreg(unsigned int regno, unsigned int red,
				unsigned int green, unsigned int blue,
				unsigned int transp, struct fb_info *info)
{
	struct simple_fb *fb = to_simple(info);

	DPRINTF("%s: setcolreg %d:(%02x,%02x,%02x,%02x) %x\n", __func__,
		regno, red, green, blue, transp, fb->fb.fix.visual);
	if (fb->fb.var.bits_per_pixel <= 8) {
		if (regno < 256) {
			/* blue [0:4], green [5:10], red [11:15] */
			fb->info.cmap[regno] = ((red   >> (16-5)) & 0x1f) << 11 |
						((green >> (16-6)) & 0x3f) << 5 |
						((blue  >> (16-5)) & 0x1f) << 0;
		}
		/* Hack: we need to tell GPU the palette has changed, but currently
		 * simple_fb_set_par takes noticable time when called for every (256)
		 * colour. So just call it for what looks like the last colour in a
		 * list for now.
		 */
		if (regno == 15 || regno == 255)
			simple_fb_set_par(info);
	} else if (regno < 16) {
		fb->cmap[regno] = convert_bitfield(transp, &fb->fb.var.transp) |
		    convert_bitfield(blue, &fb->fb.var.blue) |
		    convert_bitfield(green, &fb->fb.var.green) |
		    convert_bitfield(red, &fb->fb.var.red);
	}
	return regno > 255;
}

static int simple_fb_blank(int blank_mode, struct fb_info *info)
{
	u32 i, screen_size;

	/*
	 * We cannot change FB hardware state so just clear
	 * the screen by writing zeros.
	 */
	screen_size = info->screen_size * (info->var.bits_per_pixel >> 3);
	for(i = 0; i < screen_size; i++)
		info->screen_base[i] = 0;

	return VMM_OK;
}

static struct fb_ops simple_fb_ops = {
	.fb_check_var	= simple_fb_check_var,
	.fb_set_par	= simple_fb_set_par,
	.fb_setcolreg	= simple_fb_setcolreg,
	.fb_blank	= simple_fb_blank,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};

static int simple_fb_register(struct simple_fb *fb)
{
	int ret;

	fb->fb.fbops = &simple_fb_ops;
	fb->fb.flags = FBINFO_FLAG_DEFAULT;
	fb->fb.pseudo_palette = fb->cmap;

	strncpy(fb->fb.fix.id, simple_name, sizeof(fb->fb.fix.id));
	fb->fb.fix.type		= FB_TYPE_PACKED_PIXELS;
	fb->fb.fix.type_aux	= 0;
	fb->fb.fix.xpanstep	= 0;
	fb->fb.fix.ypanstep	= 0;
	fb->fb.fix.ywrapstep	= 0;
	fb->fb.fix.accel	= FB_ACCEL_NONE;

	fb->fb.var.xres		= fb->data.width;
	fb->fb.var.yres		= fb->data.height;
	fb->fb.var.xres_virtual	= fb->data.width;
	fb->fb.var.yres_virtual	= fb->data.height;
	fb->fb.var.bits_per_pixel = fb->data.depth;
	fb->fb.var.vmode	= FB_VMODE_NONINTERLACED;
	fb->fb.var.activate	= FB_ACTIVATE_NOW;
	fb->fb.var.nonstd	= 0;
	fb->fb.var.height	= -1; /* height of picture in mm */
	fb->fb.var.width	= -1; /* width of picture in mm */
	fb->fb.var.accel_flags	= 0;

	fb->fb.monspecs.hfmin	= 0;
	fb->fb.monspecs.hfmax	= 100000;
	fb->fb.monspecs.vfmin	= 0;
	fb->fb.monspecs.vfmax	= 400;
	fb->fb.monspecs.dclkmin = 1000000;
	fb->fb.monspecs.dclkmax = 100000000;

	simple_fb_set_bitfields(&fb->fb.var);

	/* Allocate colourmap. */
	ret = fb_alloc_cmap(&fb->fb.cmap, 256, 0);
	if (ret) {
		vmm_printf("fb_alloc_cmap fail\n");
		goto out;
	}

	fb_set_var(&fb->fb, &fb->fb.var);

	ret = register_framebuffer(&fb->fb);
	if (ret == 0)
		goto out;

	vmm_linfo("simple_fb", "registered framebuffer (%dx%d@%d)\n",
		  fb->data.width, fb->data.height, fb->data.depth);
out:
	return ret;
}


static int simple_fb_probe(struct vmm_device *dev)
{
	int ret = VMM_OK;
	struct simple_fb *fb;
	const char *format;
	const char *status;

	fb = kmalloc(sizeof(struct simple_fb), GFP_KERNEL);
	if (!fb) {
		vmm_printf("Could not allocate new simple_fb struct\n");
		ret = -ENOMEM;
		goto out;
	}
	memset(fb, 0, sizeof(struct simple_fb));

	fb->dev = dev;

	/* Get base address of simple framebuffer */
	ret = vmm_devtree_request_regmap(dev->of_node, &fb->dma,
					 0, "SIMPLE_FB");
	if (ret) {
		vmm_printf("%s: Could get base address\n", simple_name);
		goto fail_destroy_fb;
	}

	/* Get simple framebuffer parameters */
	format = fb->data.format;
	status = fb->data.status;

	ret = of_property_read_string(dev->of_node,"status", &status);
	if (ret) {
		vmm_printf("%s: Not able to get status\n", simple_name);
		goto fail_destroy_fb;
	}
	/* make sure status is okay */
	if (strcmp(status, "okay") != 0) {
		vmm_printf("%s: status: %s", simple_name, status);
		goto fail_destroy_fb;
	}

	ret = of_property_read_u32(dev->of_node,"width", &fb->data.width);
	if (ret) {
		vmm_printf("%s: Not able to get width\n", simple_name);
		goto fail_destroy_fb;
	}

	ret = of_property_read_u32(dev->of_node,"height", &fb->data.height);
	if (ret) {
		vmm_printf("%s: Not able to get height\n", simple_name);
		goto fail_destroy_fb;
	}

	ret = of_property_read_u32(dev->of_node,"stride", &fb->data.stride);
	if (ret) {
		vmm_printf("%s: Not able to get stride/pitch\n", simple_name);
		goto fail_destroy_fb;
	}

	ret = of_property_read_string(dev->of_node, "format", &format);
	if (ret) {
		vmm_printf("%s: Not able to get fb format\n", simple_name);
		goto fail_destroy_fb;
	}

	/* add support for more graphics options */
	if (strcmp(format, "a8r8g8b8") == 0) {
		fb->data.depth = 32;
	} else {
		vmm_printf("%s: [%s] format not supported by this driver\n",
			   format, simple_name);
		goto fail_destroy_fb;
	}

	ret = simple_fb_register(fb);
	if (ret == 0) {
		platform_set_drvdata(dev, fb);
		goto out;
	}

fail_destroy_fb:
	kfree(fb);
out:
	return ret;
}

static int simple_fb_remove(struct vmm_device *dev)
{
	struct simple_fb *fb = platform_get_drvdata(dev);

	platform_set_drvdata(dev, NULL);

	unregister_framebuffer(&fb->fb);
	fb_dealloc_cmap(&fb->fb.cmap);
	kfree(fb);

	return 0;
}

static struct vmm_devtree_nodeid simple_fb_devid_table[] = {
	{ .compatible = "simple-framebuffer" },
	{ /* end of list */ },
};

static struct vmm_driver simple_fb_driver = {
	.name = "simple_fb",
	.match_table = simple_fb_devid_table,
	.probe = simple_fb_probe,
	.remove = simple_fb_remove,
};

static int __init simple_fb_init(void)
{
	return vmm_devdrv_register_driver(&simple_fb_driver);
}

static void __exit simple_fb_exit(void)
{
	vmm_devdrv_unregister_driver(&simple_fb_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
