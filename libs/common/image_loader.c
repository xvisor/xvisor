/**
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 * Modified by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
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
 * @file image_loader.c
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief Simple image loader.
 */

#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <drv/fb.h>
#include <libs/vfs.h>
#include <libs/stringlib.h>
#include <libs/image_loader.h>

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)		vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

typedef	int (*parser_func_t)(int fd,
		      struct fb_image *image,
		      struct image_format *fmt);

static int isnewline(char ch)
{
	return (('\r' == ch) || ('\n' == ch));
}

static int next_token(int fd, char *buf, size_t buf_len)
{
	char ch;
	size_t pos, rdcnt;

	if (!buf || !buf_len) {
		return VMM_EINVALID;
	}

	rdcnt = vfs_read(fd, &ch, 1);
	if (!rdcnt) {
		return VMM_ENOTAVAIL;
	}
	while (ch) {
		while ((ch == ' ') || (ch == '\t') || isnewline(ch)) {
			rdcnt = vfs_read(fd, &ch, 1);
			if (!rdcnt) {
				return VMM_ENOTAVAIL;
			}
		}
		if (ch != '#')
			break;
		while (!isnewline(ch)) {
			rdcnt = vfs_read(fd, &ch, 1);
			if (!rdcnt) {
				return VMM_ENOTAVAIL;
			}
		}
	}

	pos = 0;
	while (!isspace(ch) && (pos < (buf_len - 1))) {
		buf[pos++] = ch;
		rdcnt = vfs_read(fd, &ch, 1);
		if (!rdcnt) {
			break;
		}
	}
	buf[pos] = '\0';

	return VMM_OK;
}

static unsigned int get_number(int fd)
{
	int rc;
	char val[128];

	rc = next_token(fd, val, sizeof(val));
	if (rc != VMM_OK) {
		return 0;
	}

	return strtoul(val, NULL, 10);
}

static unsigned int get_number255(int fd)
{
	return get_number(fd) & 0xFF;
}

static int ppm_header(int fd, struct fb_image *image, int *color_bytes)
{
	char val[128];
	int rc, colors = 0;

	rc = next_token(fd, val, sizeof(val));
	if (rc != VMM_OK) {
		return rc;
	}
	image->width = strtoul(val, NULL, 10);

	rc = next_token(fd, val, sizeof(val));
	if (rc != VMM_OK) {
		return rc;
	}
	image->height = strtoul(val, NULL, 10);

	rc = next_token(fd, val, sizeof(val));
	if (rc != VMM_OK) {
		return rc;
	}
	colors = strtoul(val, NULL, 10);

	DPRINTF("Width: %d\n", (int)image->width);
	DPRINTF("Height: %d\n", (int)image->height);
	DPRINTF("Maxval: %d\n", colors);

	if (colors >= 256)
		*color_bytes = 2;
	else
		*color_bytes = 1;

	return VMM_OK;
}

static int ppm_parser(int fd,
		      struct fb_image *image,
		      struct image_format *fmt)
{
	int err, color_bytes = 0;
	size_t i, size;
	unsigned char red, green, blue;
	unsigned int outv;
	void *out = NULL;

	err = ppm_header(fd, image, &color_bytes);
	if (err != VMM_OK) {
		return err;
	}

	if (color_bytes != 1) {
		return VMM_ENOTSUPP;
	}

	size = image->width * image->height;
	image->depth = fmt->bits_per_pixel;
	out = vmm_zalloc(size * (fmt->bits_per_pixel / 8));
	if (!out)
		return VMM_ENOMEM;
	image->data = out;

	for (i = 0; i < size; i++) {
		red = get_number255(fd);
		red = red >> (8 - fmt->red.length);
		green = get_number255(fd);
		green = green >> (8 - fmt->green.length);
		blue = get_number255(fd);
		blue = blue >> (8 - fmt->blue.length);
		outv = red << fmt->red.offset |
			green << fmt->green.offset |
			blue << fmt->blue.offset;
		DPRINTF("pos=%d red=0x%x green=0x%x blue=0x%x outv=0x%x\n",
			i, red, green, blue, outv);
		switch (fmt->bits_per_pixel) {
		case 8:
			((u8 *)out)[i] = (u8)outv;
			break;
		case 16:
			((u16 *)out)[i] = (u16)outv;
			break;
		case 24:
			((u32 *)out)[i] = (u32)outv;
			break;
		default:
			break;
		};
	}

	return VMM_OK;
}

static parser_func_t parser_get(int fd)
{
	char ch[2];
	size_t rdcnt;

	rdcnt = vfs_read(fd, ch, sizeof(ch));
	if (rdcnt < 2) {
		return NULL;
	}

	switch (ch[0]) {
	case 'P':
		if (ch[1] == '3')
			return ppm_parser;
		break;
	default:
		break;
	}

	return NULL;
}

int image_load(const char *path,
	       struct image_format *fmt,
	       struct fb_image *image)
{
	int fd = 0, err = 0;
	struct stat st;
	parser_func_t parse_func;

	if (!path) {
		return VMM_EINVALID;
	}

	if ((fd = vfs_open(path, O_RDONLY, 0)) < 0) {
		return fd;
	}

	if ((err = vfs_fstat(fd, &st)) != VMM_OK) {
		goto out;
	}

	if (!(st.st_mode & S_IFREG)) {
		DPRINTF("Cannot read %s\n", path);
		err = VMM_EINVALID;
		goto out;
	}

	if (!(parse_func = parser_get(fd))) {
		DPRINTF("Unsupported format\n");
		err = VMM_EINVALID;
		goto out;
	}

	err = parse_func(fd, image, fmt);

out:
	if (fd >= 0)
		vfs_close(fd);

	return err;
}
VMM_EXPORT_SYMBOL(image_load);

void image_release(struct fb_image *image)
{
	if (!image)
		return;

	if (image->data)
		vmm_free((void*)image->data);
	image->data = NULL;
}
VMM_EXPORT_SYMBOL(image_release);

/**
 * Display images on the framebuffer.
 * The image and the framebuffer must have the same color space and color map.
 */
int image_draw(struct fb_info *info, const struct fb_image *image,
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
VMM_EXPORT_SYMBOL(image_draw);

VMM_DECLARE_MODULE("Image loader library",
		   "Jimmy Durand Wesolowski",
		   "GPL",
		   IMAGE_LOADER_IPRIORITY,
		   NULL,
		   NULL);
