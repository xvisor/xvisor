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
 * @file cmd_fb.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Implementation of fb command
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_devtree.h>
#include <vmm_modules.h>
#include <vmm_cmdmgr.h>
#include <vmm_delay.h>
#include <libs/stringlib.h>
#include <libs/image_loader.h>
#include <drv/fb.h>

#include "cmd_fb_logo.h"

#define MODULE_DESC			"Command fb"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		0
#define	MODULE_INIT			cmd_fb_init
#define	MODULE_EXIT			cmd_fb_exit

static void cmd_fb_usage(struct vmm_chardev *cdev)
{
	vmm_cprintf(cdev, "Usage:\n");
	vmm_cprintf(cdev, "   fb help\n");
	vmm_cprintf(cdev, "   fb list\n");
	vmm_cprintf(cdev, "   fb info <fb_name>\n");
	vmm_cprintf(cdev, "   fb blank <fb_name> <value>\n");
	vmm_cprintf(cdev, "   fb fillrect <fb_name> <x> <y> <w> <h> <c> "
		    "[<rop>]\n");
	vmm_cprintf(cdev, "   fb logo <fb_name> [<x>] [<y>] [<w>] [<h>]\n");
	vmm_cprintf(cdev, "   fb image <fb_name> <image_path> [<x>] [<y>]\n");
}

static void cmd_fb_list(struct vmm_chardev *cdev)
{
	int rc, num, count;
	char path[256];
	struct fb_info *info;
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	vmm_cprintf(cdev, " %-16s %-20s %-40s\n", 
			  "Name", "ID", "Device Path");
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
	count = fb_count();
	for (num = 0; num < count; num++) {
		info = fb_get(num);
		if (info->dev.parent && info->dev.parent->node) {
			rc = vmm_devtree_getpath(path, sizeof(path),
						 info->dev.parent->node);
			if (rc) {
				vmm_snprintf(path, sizeof(path),
					     "----- (error %d)", rc);
			}
		} else {
			strcpy(path, "-----");
		}
		vmm_cprintf(cdev, " %-16s %-20s %-40s\n", 
				  info->name, info->fix.id, path);
	}
	vmm_cprintf(cdev, "----------------------------------------"
			  "----------------------------------------\n");
}

static int cmd_fb_info(struct vmm_chardev *cdev, struct fb_info *info)
{
	u32 i;
	const char *str;

	vmm_cprintf(cdev, "Name   : %s\n", info->name);
	vmm_cprintf(cdev, "ID     : %s\n", info->fix.id);

	switch (info->fix.type) {
	case FB_TYPE_PACKED_PIXELS:
		str = "Packed Pixels";
		break;
	case FB_TYPE_PLANES:
		str = "Non interleaved planes";
		break;
	case FB_TYPE_INTERLEAVED_PLANES:
		str = "Interleaved planes";
		break;
	case FB_TYPE_TEXT:
		str = "Text/attributes";
		break;
	case FB_TYPE_VGA_PLANES:
		str = "EGA/VGA planes";
		break;
	default:
		str = "Unknown";
		break;
	};
	vmm_cprintf(cdev, "Type   : %s\n", str);

	switch (info->fix.visual) {
	case FB_VISUAL_MONO01:
		str = "Monochrome 1=Black 0=White";
		break;
	case FB_VISUAL_MONO10:
		str = "Monochrome 0=Black 1=White";
		break;
	case FB_VISUAL_TRUECOLOR:
		str = "True color";
		break;
	case FB_VISUAL_PSEUDOCOLOR:
		str = "Pseudo color";
		break;
	case FB_VISUAL_DIRECTCOLOR:
		str = "Direct color";
		break;
	case FB_VISUAL_STATIC_PSEUDOCOLOR:
		str = "Pseudo color readonly";
		break;
	default:
		str = "Unknown";
		break;
	};
	vmm_cprintf(cdev, "Visual : %s\n", str);
	vmm_cprintf(cdev, "Xres   : %d\n", info->var.xres);
	vmm_cprintf(cdev, "Yres   : %d\n", info->var.yres);
	vmm_cprintf(cdev, "BPP    : %d\n", info->var.bits_per_pixel);

	if (info->fix.visual == FB_VISUAL_TRUECOLOR ||
	    info->fix.visual == FB_VISUAL_DIRECTCOLOR) {
		vmm_cprintf(cdev, "CMAP   : \n");
		for (i = info->cmap.start; i < info->cmap.len; i++) {
			vmm_cprintf(cdev, "  color%d: red=0x%x green=0x%x "
				    "blue=0x%x\n", i, info->cmap.red[i],
				    info->cmap.green[i], info->cmap.blue[i]);
		}
		vmm_cprintf(cdev, "\n");
	}

	return VMM_OK;
}

static int cmd_fb_fillrect(struct vmm_chardev *cdev, struct fb_info *info,
			   int argc, char **argv)
{
	u32 color_start, color_len;
	struct fb_fillrect rect;

	if (argc < 5) {
		cmd_fb_usage(cdev);
		return VMM_EFAIL;
	}

	memset(&rect, 0, sizeof (struct fb_fillrect));
	rect.dx = strtoul(argv[0], NULL, 10);
	rect.dy = strtoul(argv[1], NULL, 10);
	rect.width = strtoul(argv[2], NULL, 10);
	rect.height = strtoul(argv[3], NULL, 10);
	rect.color = strtoul(argv[4], NULL, 16);

	if (info->var.xres <= rect.dx) {
		vmm_cprintf(cdev, "Error: x should be less than %d\n",
			    info->var.xres);
		return VMM_EINVALID;
	}

	if (info->var.yres <= rect.dy) {
		vmm_cprintf(cdev, "Error: y should be less than %d\n",
			    info->var.yres);
		return VMM_EINVALID;
	}

	if (info->var.xres <= (rect.dx + rect.width)) {
		vmm_cprintf(cdev, "Error: x+width should be less than %d\n",
			    info->var.xres);
		return VMM_EINVALID;
	}

	if (info->var.yres <= (rect.dy + rect.height)) {
		vmm_cprintf(cdev, "Error: y+height should be less than %d\n",
			    info->var.yres);
		return VMM_EINVALID;
	}

	if (info->fix.visual == FB_VISUAL_TRUECOLOR ||
	    info->fix.visual == FB_VISUAL_DIRECTCOLOR) {
		color_start = info->cmap.start;
		color_len = info->cmap.len;
	} else {
		color_start = 0;
		color_len = (1 << info->var.bits_per_pixel);
	}

	if ((rect.color < color_start) ||
	    ((color_start + color_len) <= rect.color)) {
		vmm_cprintf(cdev, "Color error, it should be "
			    "0x%x <= color < 0x%x\n",
			    color_start, color_start + color_len);
		return VMM_EINVALID;
	}

	if (argc > 5) {
		rect.rop = strtol(argv[5], NULL, 10);
	}

	if (!info->fbops || !info->fbops->fb_fillrect) {
		vmm_cprintf(cdev, "FB fillrect operation not defined\n");
		return VMM_ENOTAVAIL;
	}

	vmm_cprintf(cdev, "X: %d, Y: %d, W: %d, H: %d, color: %d\n",
		    rect.dx, rect.dy, rect.width, rect.height, rect.color);
	info->fbops->fb_fillrect(info, &rect);

	return VMM_OK;
}

#if IS_ENABLED(CONFIG_CMD_FB_LOGO) || IS_ENABLED(CONFIG_CMD_FB_IMAGE)
/**
 * Display images on the framebuffer.
 * The image and the framebuffer must have the same color space and color map.
 */
static int fb_write_image(struct fb_info *info, const struct fb_image *image,
			  unsigned int x, unsigned int y, unsigned int w,
			  unsigned int h)
{
	const char *data = image->data;
	char *screen = info->screen_base;
	unsigned int img_stride = image->width * image->depth / 8;
	unsigned int screen_stride = info->fix.line_length;

	x *= image->depth / 8;

	if (0 == w)
		w = img_stride;
	else
		w *= image->depth / 8;

	if (unlikely(w > screen_stride))
		w = screen_stride;

	if (0 == h)
		h = image->height;

	screen += screen_stride * y;

	while (h--) {
		memcpy(screen + x, data, w);
		data += img_stride;
		screen += screen_stride;
	}

	return VMM_OK;
}
#endif

#if IS_ENABLED(CONFIG_CMD_FB_LOGO)
static int cmd_fb_logo(struct vmm_chardev *cdev, struct fb_info *info,
		       int argc, char *argv[])
{
	unsigned int x = 0;
	unsigned int y = 0;
	unsigned int w = 0;
	unsigned int h = 0;
	const struct fb_image *image = &cmd_fb_logo_image;

	if (!info->fbops || !info->fbops->fb_blank) {
		vmm_cprintf(cdev, "FB 'blank' operation not defined\n");
		return VMM_EFAIL;
	}

	if (info->fbops->fb_blank(FB_BLANK_UNBLANK, info)) {
		vmm_cprintf(cdev, "FB 'blank' operation failed\n");
		return VMM_EFAIL;
	}

	if (argc >= 1)
		x = strtol(argv[0], NULL, 10);
	else if (image->width < info->var.xres)
		x = (info->var.xres - image->width) / 2;
	else
		x = 0;

	if (argc >= 2)
		y = strtol(argv[1], NULL, 10);
	else if (image->height < info->var.yres)
		y = (info->var.yres - image->height) / 2;
	else
		y = 0;

	if (argc >= 3)
		w = strtol(argv[2], NULL, 10);
	else if (image->width < info->var.xres)
		w = image->width;
	else
		w = info->var.xres - 1;

	if (argc >= 4)
		h = strtol(argv[3], NULL, 10);
	else if (image->height < info->var.yres)
		h = image->height;
	else
		h = info->var.yres - 1;

	if (info->var.xres <= x) {
		vmm_cprintf(cdev, "Error: x should be less than %d\n",
			    info->var.xres);
		return VMM_EINVALID;
	}

	if (info->var.yres <= y) {
		vmm_cprintf(cdev, "Error: y should be less than %d\n",
			    info->var.yres);
		return VMM_EINVALID;
	}

	if (info->var.xres <= (x + w)) {
		vmm_cprintf(cdev, "Error: x+width should be less than %d\n",
			    info->var.xres);
		return VMM_EINVALID;
	}

	if (info->var.yres <= (y + h)) {
		vmm_cprintf(cdev, "Error: y+height should be less than %d\n",
			    info->var.yres);
		return VMM_EINVALID;
	}

	return fb_write_image(info, image, x, y, w, h);
}
#else
static int cmd_fb_logo(struct vmm_chardev *cdev, struct fb_info *info,
		       int argc, char *argv[])
{
	vmm_cprintf(cdev, "fb logo command is not enabled.\n");
	return VMM_EFAIL;
}
#endif /* CONFIG_CMD_FB_LOGO */

#if IS_ENABLED(CONFIG_CMD_FB_IMAGE)
static int cmd_fb_image(struct vmm_chardev *cdev, struct fb_info *info,
			int argc, char **argv)
{
	int err = VMM_OK;
	unsigned int w = 0;
	unsigned int h = 0;
	struct fb_image image;

	if (argc < 1) {
		cmd_fb_usage(cdev);
		return VMM_EFAIL;
	}

	memset(&image, 0, sizeof(image));

	if (VMM_OK != (err = image_load(argv[0], &format_rgb565, &image))) {
		vmm_cprintf(cdev, "Error, failed to load image \"%s\" (%d)\n",
			    argv[0], err);
		return err;
	}

	if (argc >= 2)
		image.dx = strtol(argv[1], NULL, 10);
	else if (image.width < info->var.xres)
		image.dx = (info->var.xres - image.width) / 2;
	else
		image.dx = 0;

	if (argc >= 3)
		image.dy = strtol(argv[2], NULL, 10);
	else if (image.height < info->var.yres)
		image.dy = (info->var.yres - image.height) / 2;
	else
		image.dy = 0;

	if (image.width < info->var.xres)
		w = image.width;
	else
		w = info->var.xres - 1;

	if (image.height < info->var.yres)
		h = image.height;
	else
		h = info->var.yres - 1;

	err = fb_write_image(info, &image, image.dx, image.dy, w, h);

	image_release(&image);

	return err;
}
#else
static int cmd_fb_image(struct vmm_chardev *cdev, struct fb_info *info,
			int argc, char **argv)
{
	vmm_cprintf(cdev, "fb image command is not available\n");
	return VMM_EFAIL;
}
#endif /* CONFIG_CMD_FB_IMAGE */

static int cmd_fb_blank(struct vmm_chardev *cdev, struct fb_info *info,
			int argc, char **argv)
{
	int blank = 0;

	if (argc < 1) {
		cmd_fb_usage(cdev);
		return VMM_EFAIL;
	}

	if (!info->fbops || !info->fbops->fb_blank) {
		vmm_cprintf(cdev, "FB 'blank' operation not defined\n");
		return VMM_EFAIL;
	}

	blank = strtol(argv[0], NULL, 10);

	switch (blank) {
	case FB_BLANK_POWERDOWN:
		vmm_cprintf(cdev, "Setting '%s' blank to power down\n",
			    info->name);
		break;
	case FB_BLANK_VSYNC_SUSPEND:
		vmm_cprintf(cdev, "Setting '%s' blank to vsync suspend\n",
			    info->name);
		break;
	case FB_BLANK_HSYNC_SUSPEND:
		vmm_cprintf(cdev, "Setting '%s' blank to hsync suspend\n",
			    info->name);
		break;
	case FB_BLANK_NORMAL:
		vmm_cprintf(cdev, "Setting '%s' blank to normal\n",
			    info->name);
		break;
	case FB_BLANK_UNBLANK:
		vmm_cprintf(cdev, "Setting '%s' blank to unblank\n",
			    info->name);
		break;
	}

	if (info->fbops->fb_blank(blank, info)) {
		return VMM_EFAIL;
	}
	return VMM_OK;
}

static int cmd_fb_exec(struct vmm_chardev *cdev, int argc, char **argv)
{
	struct fb_info *info = NULL;

	if (argc == 2) {
		if (strcmp(argv[1], "help") == 0) {
			cmd_fb_usage(cdev);
			return VMM_OK;
		} else if (strcmp(argv[1], "list") == 0) {
			cmd_fb_list(cdev);
			return VMM_OK;
		}
	}
	if (argc <= 2) {
		cmd_fb_usage(cdev);
		return VMM_EFAIL;
	}

	info = fb_find(argv[2]);
	if (!info) {
		vmm_cprintf(cdev, "Error: Invalid FB %s\n", argv[2]);
		return VMM_EFAIL;
	}

	if (strcmp(argv[1], "info") == 0) {
		return cmd_fb_info(cdev, info);
	} else if (0 == strcmp(argv[1], "blank")) {
		return cmd_fb_blank(cdev, info, argc - 3, argv + 3);
	} else if (0 == strcmp(argv[1], "fillrect")) {
		return cmd_fb_fillrect(cdev, info, argc - 3, argv + 3);
	} else if (0 == strcmp(argv[1], "logo")) {
		return cmd_fb_logo(cdev, info, argc - 3, argv + 3);
	} else if (0 == strcmp(argv[1], "image")) {
		return cmd_fb_image(cdev, info, argc - 3, argv + 3);
	}
	return VMM_EFAIL;
}

static struct vmm_cmd cmd_fb = {
	.name = "fb",
	.desc = "frame buffer commands",
	.usage = cmd_fb_usage,
	.exec = cmd_fb_exec,
};

static int __init cmd_fb_init(void)
{
	return vmm_cmdmgr_register_cmd(&cmd_fb);
}

static void __exit cmd_fb_exit(void)
{
	vmm_cmdmgr_unregister_cmd(&cmd_fb);
}

VMM_DECLARE_MODULE(MODULE_DESC, 
			MODULE_AUTHOR, 
			MODULE_LICENSE, 
			MODULE_IPRIORITY, 
			MODULE_INIT, 
			MODULE_EXIT);
