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
 * @file amba-clcd.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief ARM PrimeCell PL110 Color LCD Controller
 *
 * The source has been largely adapted from Linux 3.x or higher:
 * drivers/video/amba-clcd.c
 *
 * Copyright (C) 2001 ARM Limited, by David A Rusling
 * Updated to 2.5, Deep Blue Solutions Ltd.
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
#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>
#include <linux/clk.h>
#include <linux/hardirq.h>

#include <asm/sizes.h>

#define MODULE_DESC			"AMBA CLCD Driver"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(FB_CLASS_IPRIORITY+1)
#define	MODULE_INIT			amba_clcdfb_init
#define	MODULE_EXIT			amba_clcdfb_exit

#define to_clcd(info)	container_of(info, struct clcd_fb, fb)

/* This is limited to 16 characters when displayed by X startup */
static const char *clcd_name = "CLCD FB";

/*
 * Unfortunately, the enable/disable functions may be called either from
 * process or IRQ context, and we _need_ to delay.  This is _not_ good.
 */
static inline void clcdfb_sleep(unsigned int ms)
{
	if (in_atomic()) {
		mdelay(ms);
	} else {
		msleep(ms);
	}
}

static inline void clcdfb_set_start(struct clcd_fb *fb)
{
	unsigned long ustart = fb->fb.fix.smem_start;
	unsigned long lstart;

	ustart += fb->fb.var.yoffset * fb->fb.fix.line_length;
	lstart = ustart + fb->fb.var.yres * fb->fb.fix.line_length / 2;

	writel(ustart, fb->regs + CLCD_UBAS);
	writel(lstart, fb->regs + CLCD_LBAS);
}

static void clcdfb_disable(struct clcd_fb *fb)
{
	u32 val;

	if (fb->board->disable)
		fb->board->disable(fb);

	val = readl(fb->regs + fb->off_cntl);
	if (val & CNTL_LCDPWR) {
		val &= ~CNTL_LCDPWR;
		writel(val, fb->regs + fb->off_cntl);

		clcdfb_sleep(20);
	}
	if (val & CNTL_LCDEN) {
		val &= ~CNTL_LCDEN;
		writel(val, fb->regs + fb->off_cntl);
	}

	/*
	 * Disable CLCD clock source.
	 */
	if (fb->clk_enabled) {
		fb->clk_enabled = false;
		clk_disable(fb->clk);
	}
}

static void clcdfb_enable(struct clcd_fb *fb, u32 cntl)
{
	/*
	 * Enable the CLCD clock source.
	 */
	if (!fb->clk_enabled) {
		fb->clk_enabled = true;
		clk_enable(fb->clk);
	}

	/*
	 * Bring up by first enabling..
	 */
	cntl |= CNTL_LCDEN;
	writel(cntl, fb->regs + fb->off_cntl);

	clcdfb_sleep(20);

	/*
	 * and now apply power.
	 */
	cntl |= CNTL_LCDPWR;
	writel(cntl, fb->regs + fb->off_cntl);

	/*
	 * finally, enable the interface.
	 */
	if (fb->board->enable)
		fb->board->enable(fb);
}

static int
clcdfb_set_bitfields(struct clcd_fb *fb, struct fb_var_screeninfo *var)
{
	u32 caps;
	int ret = 0;

	if (fb->panel->caps && fb->board->caps)
		caps = fb->panel->caps & fb->board->caps;
	else {
		/* Old way of specifying what can be used */
		caps = fb->panel->cntl & CNTL_BGR ?
			CLCD_CAP_BGR : CLCD_CAP_RGB;
		/* But mask out 444 modes as they weren't supported */
		caps &= ~CLCD_CAP_444;
	}

	/* Only TFT panels can do RGB888/BGR888 */
	if (!(fb->panel->cntl & CNTL_LCDTFT))
		caps &= ~CLCD_CAP_888;

	memset(&var->transp, 0, sizeof(var->transp));

	var->red.msb_right = 0;
	var->green.msb_right = 0;
	var->blue.msb_right = 0;

	switch (var->bits_per_pixel) {
	case 1:
	case 2:
	case 4:
	case 8:
		/* If we can't do 5551, reject */
		caps &= CLCD_CAP_5551;
		if (!caps) {
			ret = -EINVAL;
			break;
		}

		var->red.length		= var->bits_per_pixel;
		var->red.offset		= 0;
		var->green.length	= var->bits_per_pixel;
		var->green.offset	= 0;
		var->blue.length	= var->bits_per_pixel;
		var->blue.offset	= 0;
		break;

	case 16:
		/* If we can't do 444, 5551 or 565, reject */
		if (!(caps & (CLCD_CAP_444 | CLCD_CAP_5551 | CLCD_CAP_565))) {
			ret = -EINVAL;
			break;
		}

		/*
		 * Green length can be 4, 5 or 6 depending whether
		 * we're operating in 444, 5551 or 565 mode.
		 */
		if (var->green.length == 4 && caps & CLCD_CAP_444)
			caps &= CLCD_CAP_444;
		if (var->green.length == 5 && caps & CLCD_CAP_5551)
			caps &= CLCD_CAP_5551;
		else if (var->green.length == 6 && caps & CLCD_CAP_565)
			caps &= CLCD_CAP_565;
		else {
			/*
			 * PL110 officially only supports RGB555,
			 * but may be wired up to allow RGB565.
			 */
			if (caps & CLCD_CAP_565) {
				var->green.length = 6;
				caps &= CLCD_CAP_565;
			} else if (caps & CLCD_CAP_5551) {
				var->green.length = 5;
				caps &= CLCD_CAP_5551;
			} else {
				var->green.length = 4;
				caps &= CLCD_CAP_444;
			}
		}

		if (var->green.length >= 5) {
			var->red.length = 5;
			var->blue.length = 5;
		} else {
			var->red.length = 4;
			var->blue.length = 4;
		}
		break;
	case 32:
		/* If we can't do 888, reject */
		caps &= CLCD_CAP_888;
		if (!caps) {
			ret = -EINVAL;
			break;
		}

		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
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
	if (ret == 0 && var->bits_per_pixel >= 16) {
		bool bgr, rgb;

		bgr = caps & CLCD_CAP_BGR && var->blue.offset == 0;
		rgb = caps & CLCD_CAP_RGB && var->red.offset == 0;

		if (!bgr && !rgb)
			/*
			 * The requested format was not possible, try just
			 * our capabilities.  One of BGR or RGB must be
			 * supported.
			 */
			bgr = caps & CLCD_CAP_BGR;

		if (bgr) {
			var->blue.offset = 0;
			var->green.offset = var->blue.offset + var->blue.length;
			var->red.offset = var->green.offset + var->green.length;
		} else {
			var->red.offset = 0;
			var->green.offset = var->red.offset + var->red.length;
			var->blue.offset = var->green.offset + var->green.length;
		}
	}

	return ret;
}

static int clcdfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct clcd_fb *fb = to_clcd(info);
	int ret = -EINVAL;

	if (fb->board->check)
		ret = fb->board->check(fb, var);

	if (ret == 0 &&
	    var->xres_virtual * var->bits_per_pixel / 8 *
	    var->yres_virtual > fb->fb.fix.smem_len)
		ret = -EINVAL;

	if (ret == 0)
		ret = clcdfb_set_bitfields(fb, var);

	return ret;
}

static int clcdfb_set_par(struct fb_info *info)
{
	struct clcd_fb *fb = to_clcd(info);
	struct clcd_regs regs;

	fb->fb.fix.line_length = fb->fb.var.xres_virtual *
				 fb->fb.var.bits_per_pixel / 8;

	if (fb->fb.var.bits_per_pixel <= 8)
		fb->fb.fix.visual = FB_VISUAL_PSEUDOCOLOR;
	else
		fb->fb.fix.visual = FB_VISUAL_TRUECOLOR;

	fb->board->decode(fb, &regs);

	clcdfb_disable(fb);

	writel(regs.tim0, fb->regs + CLCD_TIM0);
	writel(regs.tim1, fb->regs + CLCD_TIM1);
	writel(regs.tim2, fb->regs + CLCD_TIM2);
	writel(regs.tim3, fb->regs + CLCD_TIM3);

	clcdfb_set_start(fb);

	clk_set_rate(fb->clk, udiv32(1000000000, regs.pixclock) * 1000);

	fb->clcd_cntl = regs.cntl;

	clcdfb_enable(fb, regs.cntl);

#ifdef DEBUG
	printk(KERN_INFO
	       "CLCD: Registers set to\n"
	       "  %08x %08x %08x %08x\n"
	       "  %08x %08x %08x %08x\n",
		readl(fb->regs + CLCD_TIM0), readl(fb->regs + CLCD_TIM1),
		readl(fb->regs + CLCD_TIM2), readl(fb->regs + CLCD_TIM3),
		readl(fb->regs + CLCD_UBAS), readl(fb->regs + CLCD_LBAS),
		readl(fb->regs + fb->off_ienb), readl(fb->regs + fb->off_cntl));
#endif

	return 0;
}

static int clcdfb_set_smem(struct fb_info *info,
			   unsigned long start, u32 len)
{
	struct clcd_fb *fb = to_clcd(info);

	if (info->var.xres_virtual * info->var.bits_per_pixel / 8 *
	    info->var.yres_virtual > len)
		return -EINVAL;

	clcdfb_disable(fb);

	fb->fb.fix.smem_start = start;
	fb->fb.fix.smem_len = len;

	clcdfb_set_start(fb);

	clcdfb_enable(fb, fb->clcd_cntl);

	return 0;
}

static inline u32 convert_bitfield(int val, struct fb_bitfield *bf)
{
	unsigned int mask = (1 << bf->length) - 1;

	return (val >> (16 - bf->length) & mask) << bf->offset;
}

/*
 *  Set a single color register. The values supplied have a 16 bit
 *  magnitude.  Return != 0 for invalid regno.
 */
static int
clcdfb_setcolreg(unsigned int regno, unsigned int red, unsigned int green,
		 unsigned int blue, unsigned int transp, struct fb_info *info)
{
	struct clcd_fb *fb = to_clcd(info);

	if (regno < 16)
		fb->cmap[regno] = convert_bitfield(transp, &fb->fb.var.transp) |
				  convert_bitfield(blue, &fb->fb.var.blue) |
				  convert_bitfield(green, &fb->fb.var.green) |
				  convert_bitfield(red, &fb->fb.var.red);

	if (fb->fb.fix.visual == FB_VISUAL_PSEUDOCOLOR && regno < 256) {
		int hw_reg = CLCD_PALETTE + ((regno * 2) & ~3);
		u32 val, mask, newval;

		newval  = (red >> 11)  & 0x001f;
		newval |= (green >> 6) & 0x03e0;
		newval |= (blue >> 1)  & 0x7c00;

		/*
		 * 3.2.11: if we're configured for big endian
		 * byte order, the palette entries are swapped.
		 */
		if (fb->clcd_cntl & CNTL_BEBO)
			regno ^= 1;

		if (regno & 1) {
			newval <<= 16;
			mask = 0x0000ffff;
		} else {
			mask = 0xffff0000;
		}

		val = readl(fb->regs + hw_reg) & mask;
		writel(val | newval, fb->regs + hw_reg);
	}

	return regno > 255;
}

/*
 *  Blank the screen if blank_mode != 0, else unblank. If blank == NULL
 *  then the caller blanks by setting the CLUT (Color Look Up Table) to all
 *  black. Return 0 if blanking succeeded, != 0 if un-/blanking failed due
 *  to e.g. a video mode which doesn't support it. Implements VESA suspend
 *  and powerdown modes on hardware that supports disabling hsync/vsync:
 *    blank_mode == 2: suspend vsync
 *    blank_mode == 3: suspend hsync
 *    blank_mode == 4: powerdown
 */
static int clcdfb_blank(int blank_mode, struct fb_info *info)
{
	struct clcd_fb *fb = to_clcd(info);

	if (blank_mode != 0) {
		clcdfb_disable(fb);
	} else {
		clcdfb_enable(fb, fb->clcd_cntl);
	}
	return 0;
}

static struct fb_ops clcdfb_ops = {
	.fb_check_var	= clcdfb_check_var,
	.fb_set_par	= clcdfb_set_par,
	.fb_set_smem	= clcdfb_set_smem,
	.fb_setcolreg	= clcdfb_setcolreg,
	.fb_blank	= clcdfb_blank,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};

static int clcdfb_register(struct clcd_fb *fb)
{
	int ret;
	physical_addr_t mmio_pa;
	physical_size_t mmio_sz;

	/*
	 * ARM PL111 always has IENB at 0x1c; it's only PL110
	 * which is reversed on some platforms.
	 */
	if (amba_manf(fb->dev) == 0x41 && amba_part(fb->dev) == 0x111) {
		fb->off_ienb = CLCD_PL111_IENB;
		fb->off_cntl = CLCD_PL111_CNTL;
	} else {
#ifdef CONFIG_BOARD_VERSATILE
		fb->off_ienb = CLCD_PL111_IENB;
		fb->off_cntl = CLCD_PL111_CNTL;
#else
		fb->off_ienb = CLCD_PL110_IENB;
		fb->off_cntl = CLCD_PL110_CNTL;
#endif
	}

	fb->clk = clk_get(fb->dev, NULL);
	if (!fb->clk) {
		ret = PTR_ERR(fb->clk);
		goto out;
	}

	ret = clk_prepare(fb->clk);
	if (ret)
		goto free_clk;

	fb->fb.dev.parent	= fb->dev;

	ret = vmm_devtree_regaddr(fb->dev->node, &mmio_pa, 0);
	if (ret) {
		goto clk_unprep;
	}
	fb->fb.fix.mmio_start	= mmio_pa;

	ret = vmm_devtree_regsize(fb->dev->node, &mmio_sz, 0);
	if (ret) {
		goto clk_unprep;
	}
	fb->fb.fix.mmio_len	= mmio_sz;

	ret = vmm_devtree_regmap(fb->dev->node, (virtual_addr_t *)&fb->regs, 0);
	if (ret) {
		printk(KERN_ERR "CLCD: unable to map registers\n");
		ret = -ENOMEM;
		goto clk_unprep;
	}

	fb->fb.fbops		= &clcdfb_ops;
	fb->fb.flags		= FBINFO_FLAG_DEFAULT;
	fb->fb.pseudo_palette	= fb->cmap;

	if (strlcpy(fb->fb.fix.id, clcd_name, sizeof(fb->fb.fix.id)) >=
	    sizeof(fb->fb.fix.id)) {
		ret = -EOVERFLOW;
		goto unmap;
	}

	fb->fb.fix.type		= FB_TYPE_PACKED_PIXELS;
	fb->fb.fix.type_aux	= 0;
	fb->fb.fix.xpanstep	= 0;
	fb->fb.fix.ypanstep	= 0;
	fb->fb.fix.ywrapstep	= 0;
	fb->fb.fix.accel	= FB_ACCEL_NONE;

	fb->fb.var.xres		= fb->panel->mode.xres;
	fb->fb.var.yres		= fb->panel->mode.yres;
	fb->fb.var.xres_virtual	= fb->panel->mode.xres;
	fb->fb.var.yres_virtual	= fb->panel->mode.yres;
	fb->fb.var.bits_per_pixel = fb->panel->bpp;
	fb->fb.var.grayscale	= fb->panel->grayscale;
	fb->fb.var.pixclock	= fb->panel->mode.pixclock;
	fb->fb.var.left_margin	= fb->panel->mode.left_margin;
	fb->fb.var.right_margin	= fb->panel->mode.right_margin;
	fb->fb.var.upper_margin	= fb->panel->mode.upper_margin;
	fb->fb.var.lower_margin	= fb->panel->mode.lower_margin;
	fb->fb.var.hsync_len	= fb->panel->mode.hsync_len;
	fb->fb.var.vsync_len	= fb->panel->mode.vsync_len;
	fb->fb.var.sync		= fb->panel->mode.sync;
	fb->fb.var.vmode	= fb->panel->mode.vmode;
	fb->fb.var.activate	= FB_ACTIVATE_NOW;
	fb->fb.var.nonstd	= 0;
	fb->fb.var.height	= fb->panel->height;
	fb->fb.var.width	= fb->panel->width;
	fb->fb.var.accel_flags	= 0;

	fb->fb.monspecs.hfmin	= 0;
	fb->fb.monspecs.hfmax   = 100000;
	fb->fb.monspecs.vfmin	= 0;
	fb->fb.monspecs.vfmax	= 400;
	fb->fb.monspecs.dclkmin = 1000000;
	fb->fb.monspecs.dclkmax	= 100000000;

	/*
	 * Make sure that the bitfields are set appropriately.
	 */
	clcdfb_set_bitfields(fb, &fb->fb.var);

	/*
	 * Allocate colourmap.
	 */
	ret = fb_alloc_cmap(&fb->fb.cmap, 256, 0);
	if (ret)
		goto unmap;

	/*
	 * Ensure interrupts are disabled.
	 */
	writel(0, fb->regs + fb->off_ienb);

	fb_set_var(&fb->fb, &fb->fb.var);

	dev_info(fb->dev, "%s hardware, %s display\n",
	         fb->board->name, fb->panel->mode.name);

	ret = register_framebuffer(&fb->fb);
	if (ret == 0)
		goto out;

	printk(KERN_ERR "CLCD: cannot register framebuffer (%d)\n", ret);

	fb_dealloc_cmap(&fb->fb.cmap);
 unmap:
	vmm_devtree_regunmap(fb->dev->node, (virtual_addr_t)fb->regs, 0);
 clk_unprep:
	clk_unprepare(fb->clk);
 free_clk:
	clk_put(fb->clk);
 out:
	return ret;
}

static int clcdfb_probe(struct vmm_device *dev, 
			const struct vmm_devtree_nodeid *id)
{
	struct clcd_board *board = dev->node->system_data;
	struct clcd_fb *fb;
	int ret;

	if (!board)
		return -EINVAL;

	fb = kzalloc(sizeof(struct clcd_fb), GFP_KERNEL);
	if (!fb) {
		printk(KERN_INFO "CLCD: could not allocate new clcd_fb struct\n");
		ret = -ENOMEM;
		goto out;
	}

	fb->dev = dev;
	fb->board = board;

	dev_info(fb->dev, "PL%03x rev%u\n",
		amba_part(dev), amba_rev(dev));

	ret = fb->board->setup(fb);
	if (ret)
		goto free_fb;

	ret = clcdfb_register(fb); 
	if (ret == 0) {
		dev->priv = fb;
		goto out;
	}

	fb->board->remove(fb);
 free_fb:
	kfree(fb);
 out:
	return ret;
}

static int clcdfb_remove(struct vmm_device *dev)
{
	struct clcd_fb *fb = dev->priv;

	dev->priv = NULL;

	clcdfb_disable(fb);
	unregister_framebuffer(&fb->fb);
	if (fb->fb.cmap.len)
		fb_dealloc_cmap(&fb->fb.cmap);
	vmm_devtree_regunmap(fb->dev->node, (virtual_addr_t)fb->regs, 0);
	clk_unprepare(fb->clk);
	clk_put(fb->clk);

	fb->board->remove(fb);

	kfree(fb);

	return 0;
}

static struct vmm_devtree_nodeid clcdfb_devid_table[] = {
	{.type = "fb",.compatible = "arm,pl110"},
	{.type = "fb",.compatible = "arm,pl111"},
	{ /* end of list */ },
};

static struct vmm_driver clcdfb_driver = {
	.name = "clcdfb-pl110",
	.match_table = clcdfb_devid_table,
	.probe = clcdfb_probe,
	.remove = clcdfb_remove,
};

static int __init amba_clcdfb_init(void)
{
	return vmm_devdrv_register_driver(&clcdfb_driver);
}

static void __exit amba_clcdfb_exit(void)
{
	vmm_devdrv_unregister_driver(&clcdfb_driver);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
