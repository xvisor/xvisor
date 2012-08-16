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
 * @file vmm_fbcmap.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Colormap handling for frame buffer devices
 *
 * The source has been largely adapted from Linux 3.x or higher:
 * drivers/video/fbcmap.c
 *
 *	Created 15 Jun 1997 by Geert Uytterhoeven
 *
 *	2001 - Documented with DocBook
 *	- Brad Douglas <brad@neruo.com>
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_stdio.h>
#include <fb/vmm_fb.h>

static u16 red2[] __read_mostly = {
    0x0000, 0xaaaa
};
static u16 green2[] __read_mostly = {
    0x0000, 0xaaaa
};
static u16 blue2[] __read_mostly = {
    0x0000, 0xaaaa
};

static u16 red4[] __read_mostly = {
    0x0000, 0xaaaa, 0x5555, 0xffff
};
static u16 green4[] __read_mostly = {
    0x0000, 0xaaaa, 0x5555, 0xffff
};
static u16 blue4[] __read_mostly = {
    0x0000, 0xaaaa, 0x5555, 0xffff
};

static u16 red8[] __read_mostly = {
    0x0000, 0x0000, 0x0000, 0x0000, 0xaaaa, 0xaaaa, 0xaaaa, 0xaaaa
};
static u16 green8[] __read_mostly = {
    0x0000, 0x0000, 0xaaaa, 0xaaaa, 0x0000, 0x0000, 0x5555, 0xaaaa
};
static u16 blue8[] __read_mostly = {
    0x0000, 0xaaaa, 0x0000, 0xaaaa, 0x0000, 0xaaaa, 0x0000, 0xaaaa
};

static u16 red16[] __read_mostly = {
    0x0000, 0x0000, 0x0000, 0x0000, 0xaaaa, 0xaaaa, 0xaaaa, 0xaaaa,
    0x5555, 0x5555, 0x5555, 0x5555, 0xffff, 0xffff, 0xffff, 0xffff
};
static u16 green16[] __read_mostly = {
    0x0000, 0x0000, 0xaaaa, 0xaaaa, 0x0000, 0x0000, 0x5555, 0xaaaa,
    0x5555, 0x5555, 0xffff, 0xffff, 0x5555, 0x5555, 0xffff, 0xffff
};
static u16 blue16[] __read_mostly = {
    0x0000, 0xaaaa, 0x0000, 0xaaaa, 0x0000, 0xaaaa, 0x0000, 0xaaaa,
    0x5555, 0xffff, 0x5555, 0xffff, 0x5555, 0xffff, 0x5555, 0xffff
};

static const struct vmm_fb_cmap default_2_colors = {
    .len=2, .red=red2, .green=green2, .blue=blue2
};
static const struct vmm_fb_cmap default_8_colors = {
    .len=8, .red=red8, .green=green8, .blue=blue8
};
static const struct vmm_fb_cmap default_4_colors = {
    .len=4, .red=red4, .green=green4, .blue=blue4
};
static const struct vmm_fb_cmap default_16_colors = {
    .len=16, .red=red16, .green=green16, .blue=blue16
};



/**
 *	Allocate a colormap
 *	@cmap: frame buffer colormap structure
 *	@len: length of @cmap
 *	@transp: boolean, 1 if there is transparency, 0 otherwise
 *
 *	Allocates memory for a colormap @cmap.  @len is the
 *	number of entries in the palette.
 *
 *	Returns negative errno on error, or zero on success.
 *
 */

int vmm_fb_alloc_cmap(struct vmm_fb_cmap *cmap, int len, int transp)
{
	int size = len * sizeof(u16);
	int ret = VMM_ENOMEM;

	if (cmap->len != len) {
		vmm_fb_dealloc_cmap(cmap);
		if (!len)
			return 0;

		cmap->red = vmm_malloc(size);
		if (!cmap->red)
			goto fail;
		cmap->green = vmm_malloc(size);
		if (!cmap->green)
			goto fail;
		cmap->blue = vmm_malloc(size);
		if (!cmap->blue)
			goto fail;
		if (transp) {
			cmap->transp = vmm_malloc(size);
			if (!cmap->transp)
				goto fail;
		} else {
			cmap->transp = NULL;
		}
	}
	cmap->start = 0;
	cmap->len = len;
	ret = vmm_fb_copy_cmap(vmm_fb_default_cmap(len), cmap);
	if (ret)
		goto fail;
	return 0;

fail:
	vmm_fb_dealloc_cmap(cmap);
	return ret;
}

/**
 *      Deallocate a colormap
 *      @cmap: frame buffer colormap structure
 *
 *      Deallocates a colormap that was previously allocated with
 *      fb_alloc_cmap().
 *
 */

void vmm_fb_dealloc_cmap(struct vmm_fb_cmap *cmap)
{
	vmm_free(cmap->red);
	vmm_free(cmap->green);
	vmm_free(cmap->blue);
	vmm_free(cmap->transp);

	cmap->red = cmap->green = cmap->blue = cmap->transp = NULL;
	cmap->len = 0;
}

/**
 *	Copy a colormap
 *	@from: frame buffer colormap structure
 *	@to: frame buffer colormap structure
 *
 *	Copy contents of colormap from @from to @to.
 */

int vmm_fb_copy_cmap(const struct vmm_fb_cmap *from, struct vmm_fb_cmap *to)
{
	int tooff = 0, fromoff = 0;
	int size;

	if (to->start > from->start)
		fromoff = to->start - from->start;
	else
		tooff = from->start - to->start;
	size = to->len - tooff;
	if (size > (int) (from->len - fromoff))
		size = from->len - fromoff;
	if (size <= 0)
		return VMM_EINVALID;
	size *= sizeof(u16);

	vmm_memcpy(to->red+tooff, from->red+fromoff, size);
	vmm_memcpy(to->green+tooff, from->green+fromoff, size);
	vmm_memcpy(to->blue+tooff, from->blue+fromoff, size);
	if (from->transp && to->transp)
		vmm_memcpy(to->transp+tooff, from->transp+fromoff, size);
	return 0;
}

/**
 *	Set the colormap
 *	@cmap: frame buffer colormap structure
 *	@info: frame buffer info structure
 *
 *	Sets the colormap @cmap for a screen of device @info.
 *
 *	Returns negative errno on error, or zero on success.
 *
 */

int vmm_fb_set_cmap(struct vmm_fb_cmap *cmap, struct vmm_fb_info *info)
{
	int i, start, rc = 0;
	u16 *red, *green, *blue, *transp;
	unsigned hred, hgreen, hblue, htransp = 0xffff;

	red = cmap->red;
	green = cmap->green;
	blue = cmap->blue;
	transp = cmap->transp;
	start = cmap->start;

	if (start < 0 || (!info->fbops->fb_setcolreg &&
			  !info->fbops->fb_setcmap))
		return VMM_EINVALID;
	if (info->fbops->fb_setcmap) {
		rc = info->fbops->fb_setcmap(cmap, info);
	} else {
		for (i = 0; i < cmap->len; i++) {
			hred = *red++;
			hgreen = *green++;
			hblue = *blue++;
			if (transp)
				htransp = *transp++;
			if (info->fbops->fb_setcolreg(start++,
						      hred, hgreen, hblue,
						      htransp, info))
				break;
		}
	}
	if (rc == 0)
		vmm_fb_copy_cmap(cmap, &info->cmap);

	return rc;
}

/**
 *	Get default colormap
 *	@len: size of palette for a depth
 *
 *	Gets the default colormap for a specific screen depth.  @len
 *	is the size of the palette for a particular screen depth.
 *
 *	Returns pointer to a frame buffer colormap structure.
 *
 */

const struct vmm_fb_cmap *vmm_fb_default_cmap(int len)
{
    if (len <= 2)
	return &default_2_colors;
    if (len <= 4)
	return &default_4_colors;
    if (len <= 8)
	return &default_8_colors;
    return &default_16_colors;
}


/**
 *	Invert all defaults colormaps
 */

void vmm_fb_invert_cmaps(void)
{
    unsigned i;

    for (i = 0; i < array_size(red2); i++) {
	red2[i] = ~red2[i];
	green2[i] = ~green2[i];
	blue2[i] = ~blue2[i];
    }
    for (i = 0; i < array_size(red4); i++) {
	red4[i] = ~red4[i];
	green4[i] = ~green4[i];
	blue4[i] = ~blue4[i];
    }
    for (i = 0; i < array_size(red8); i++) {
	red8[i] = ~red8[i];
	green8[i] = ~green8[i];
	blue8[i] = ~blue8[i];
    }
    for (i = 0; i < array_size(red16); i++) {
	red16[i] = ~red16[i];
	green16[i] = ~green16[i];
	blue16[i] = ~blue16[i];
    }
}

