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

#include <vmm_modules.h>
#include <drv/fb.h>
#include <libs/vfs.h>
#include <libs/stringlib.h>
#include <libs/image_loader.h>

struct format format_rgb565 = {
	.byte_size = 2,
	.red = {
		.offset = 0,
		.length = 5,
		.msb_right = 0,
	},
	.green = {
		.offset = 5,
		.length = 6,
		.msb_right = 0,
	},
	.blue = {
		.offset = 11,
		.length = 5,
		.msb_right = 0,
	},
	.transp = {
		.offset = 0,
		.length = 0,
		.msb_right = 0,
	},
};

typedef	int (*parser)(char *buf,
		      size_t len,
		      struct fb_image *image,
		      struct format *output_format);

static int isnewline(const char	*buf)
{
	return (('\r' == *buf) || ('\n' == *buf));
}

static char *next_token(char *buf, char	**token)
{
	char	*comment = NULL;

	while (buf) {
		buf = skip_spaces(buf);
		if ('#' != *buf)
			break;
		comment = buf;
		while (!isnewline(buf))
			++buf;
		*buf++ = '\0';
		vmm_printf("Comment: %s\n", comment);
		while (isnewline(buf))
			++buf;
	}

	*token = buf;
	while (!isspace(*buf))
		++buf;
	*buf = '\0';
	return buf + 1;
}

static char* ppm_header(char *buf,
			size_t len,
			struct fb_image *image,
			int *color_bytes)
{
	char	*width = NULL;
	char	*height = NULL;
	char	*maxval = NULL;
	int	colors = 0;

	if (buf[1] == '3') {
		vmm_printf("P3 PPM not managed yet\n");
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

	vmm_printf("Width: %s\n", width);
	vmm_printf("Width: %s\n", height);
	vmm_printf("Maxval: %s\n", maxval);

	image->width = strtoul(width, NULL, 10);
	image->height = strtoul(height, NULL, 10);
	colors = strtoul(maxval, NULL, 10);

	if (colors >= 256)
		*color_bytes = 2;
	else
		*color_bytes = 1;

	return buf;
}

int ppm_parser(char *buf,
		size_t len,
		struct fb_image *image,
		struct format *output_format)
{
	int	color_bytes = 0;
	size_t	i = 0;
	size_t	j = 0;
	size_t	size = 0;
	char	red, green, blue;
	unsigned short	*out = NULL;

	if (NULL == (buf = ppm_header(buf, len, image, &color_bytes)))
		return VMM_EFAIL;

	/* Overwritting in the correct format */
	out = (unsigned short*)buf;
	size = image->width * image->height;
	image->depth = output_format->byte_size * 8;

	for (i = 0; (i < size) && (j + 2 < len); ++i) {
		red = buf[j + 2] >> (8 - output_format->red.length);
		green = buf[j + 1] >> (8 - output_format->green.length);
		blue = buf[j] >> (8 - output_format->blue.length);
		out[i] = red << output_format->red.offset |
			green << output_format->green.offset |
			blue << output_format->blue.offset;
		j += 3;
	}
	image->data = buf;

	return VMM_OK;
}

parser		parser_get(const char		*buf,
			   size_t		len)
{
	if (!buf)
		return NULL;

	switch (buf[0]) {
	case 'P':
		if ((buf[1] == '3') || (buf[1] == '6'))
			return ppm_parser;
	}

	return NULL;
}

int image_load(const char *path,
	       struct format *output_format,
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
		vmm_printf("Unsupported format\n");
		err = VMM_EFAIL;
		goto out;
	}

	err = parse_func(buf, len, image, output_format);

out:
	if ((VMM_OK != err) && buf)
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

VMM_DECLARE_MODULE("Image loader library",
		   "Jimmy Durand Wesolowski",
		   "GPL",
		   IMAGE_LOADER_IPRIORITY,
		   NULL,
		   NULL);
