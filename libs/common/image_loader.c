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

typedef	int (*parser)(char *buf,
		      size_t len,
		      struct fb_image *image,
		      struct image_format *fmt);

static int isnewline(const char *buf)
{
	return (('\r' == *buf) || ('\n' == *buf));
}

static char *next_token(char *buf, char **token)
{
	while (buf) {
		buf = skip_spaces(buf);
		if ('#' != *buf)
			break;
		while (!isnewline(buf))
			++buf;
		*buf++ = '\0';
		while (isnewline(buf))
			++buf;
	}

	*token = buf;
	while (!isspace(*buf))
		++buf;
	*buf = '\0';
	return buf + 1;
}

static unsigned int get_number(char **buf)
{
	char *val = NULL;

	*buf = next_token(*buf, &val);
	if (NULL == val) {
		return 0;
	}

	return strtoul(val, NULL, 10);
}

static unsigned int get_number255(char **buf)
{
	return get_number(buf) & 0xFF;
}

static char* ppm_header(char *buf,
			size_t len,
			struct fb_image *image,
			int *color_bytes)
{
	char *width = NULL;
	char *height = NULL;
	char *maxval = NULL;
	int colors = 0;

	if (buf[1] != '3') {
		DPRINTF("Only PPM allowed\n");
		return NULL;
	}

	buf += 3;

	buf = next_token(buf, &width);
	if (NULL == width) {
		return NULL;
	}

	buf = next_token(buf, &height);
	if (NULL == height) {
		return NULL;
	}

	buf = next_token(buf, &maxval);
	if (NULL == maxval) {
		return NULL;
	}

	image->width = strtoul(width, NULL, 10);
	image->height = strtoul(height, NULL, 10);
	colors = strtoul(maxval, NULL, 10);

	DPRINTF("Width: %d\n", (int)image->width);
	DPRINTF("Height: %d\n", (int)image->height);
	DPRINTF("Maxval: %d\n", colors);

	if (colors >= 256)
		*color_bytes = 2;
	else
		*color_bytes = 1;

	return buf;
}

int ppm_parser(char *buf,
		size_t len,
		struct fb_image *image,
		struct image_format *fmt)
{
	int color_bytes = 0;
	size_t i, size;
	char *nbuf;
	unsigned char red, green, blue;
	unsigned int outv;
	void *out = NULL;

	if (NULL == (nbuf = ppm_header(buf, len, image, &color_bytes)))
		return VMM_EFAIL;
	len = len - (size_t)(nbuf - buf);

	size = image->width * image->height;
	image->depth = fmt->bits_per_pixel;
	out = vmm_zalloc(size * (fmt->bits_per_pixel / 8));
	if (!out)
		return VMM_ENOMEM;
	image->data = out;

	for (i = 0; i < size; i++) {
		red = get_number255(&buf);
		red = red >> (8 - fmt->red.length);
		green = get_number255(&buf);
		green = green >> (8 - fmt->green.length);
		blue = get_number255(&buf);
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

static parser parser_get(const char *buf, size_t len)
{
	if (!buf)
		return NULL;

	switch (buf[0]) {
	case 'P':
		if (buf[1] == '3')
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
	int fd = 0;
	int err = 0;
	char *buf = NULL;
	size_t len = 0;
	struct stat stat;
	parser parse_func;

	if (NULL == path)
		return VMM_EFAIL;

	if (0 > (fd = vfs_open(path, O_RDONLY, 0)))
		return fd;

	if (VMM_OK != (err = vfs_fstat(fd, &stat))) {
		goto out;
	}

	if (NULL == (buf = vmm_malloc(stat.st_size))) {
		err = VMM_ENOMEM;
		goto out;
	}

	len = vfs_read(fd, buf, stat.st_size);

	if (NULL == (parse_func = parser_get(buf, len))) {
		DPRINTF("Unsupported format\n");
		err = VMM_EINVALID;
		goto out;
	}

	err = parse_func(buf, len, image, fmt);

out:
	vmm_free(buf);
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
