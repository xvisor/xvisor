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
 * @file vmm_fbmem.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Frame buffer managment framework
 *
 * The source has been largely adapted from Linux 3.x or higher:
 * drivers/video/fbmem.c
 *
 *  Copyright (C) 1994 Martin Schaller
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
#include <vmm_modules.h>
#include <vmm_devdrv.h>
#include <fb/vmm_fb.h>

#include <arch_atomic.h>

#define MODULE_DESC			"Frame Buffer Framework"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		VMM_FB_CLASS_IPRIORITY
#define	MODULE_INIT			vmm_fb_init
#define	MODULE_EXIT			vmm_fb_exit

#define FBPIXMAPSIZE  (1024 * 8)

static void get_fb_info(struct vmm_fb_info *fb_info)
{
	if (!fb_info)
		return;

	arch_atomic_add(&fb_info->count, 1);
}

static void put_fb_info(struct vmm_fb_info *fb_info)
{
	if (arch_atomic_sub_return(&fb_info->count, 1))
		return;
	if (fb_info->fbops->fb_destroy)
		fb_info->fbops->fb_destroy(fb_info);
}

static int vmm_fb_check_foreignness(struct vmm_fb_info *fi)
{
	const bool foreign_endian = fi->flags & FBINFO_FOREIGN_ENDIAN;

	fi->flags &= ~FBINFO_FOREIGN_ENDIAN;

#ifdef CONFIG_CPU_BE
	fi->flags |= foreign_endian ? 0 : FBINFO_BE_MATH;
#else
	fi->flags |= foreign_endian ? FBINFO_BE_MATH : 0;
#endif /* CONFIG_CPU_BE */

	if (fi->flags & FBINFO_BE_MATH && !vmm_fb_be_math(fi)) {
		vmm_printf("%s: enable CONFIG_FB_BIG_ENDIAN to "
		       "support this framebuffer\n", fi->fix.id);
		return VMM_ENOSYS;
	} else if (!(fi->flags & FBINFO_BE_MATH) && vmm_fb_be_math(fi)) {
		vmm_printf("%s: enable CONFIG_FB_LITTLE_ENDIAN to "
		       "support this framebuffer\n", fi->fix.id);
		return VMM_ENOSYS;
	}

	return VMM_OK;
}

static int vmm_fb_check_caps(struct vmm_fb_info *info, 
			     struct vmm_fb_var_screeninfo *var,
			     u32 activate)
{
	struct vmm_fb_blit_caps caps, fbcaps;
	int err = 0;

	vmm_memset(&caps, 0, sizeof(caps));
	vmm_memset(&fbcaps, 0, sizeof(fbcaps));
	caps.flags = (activate & FB_ACTIVATE_ALL) ? 1 : 0;
	info->fbops->fb_get_caps(info, &fbcaps, var);

	if (((fbcaps.x ^ caps.x) & caps.x) ||
	    ((fbcaps.y ^ caps.y) & caps.y) ||
	    (fbcaps.len < caps.len))
		err = VMM_EINVALID;

	return err;
}

int vmm_fb_set_var(struct vmm_fb_info *info, struct vmm_fb_var_screeninfo *var)
{
	int ret = 0;

	if (var->activate & FB_ACTIVATE_INV_MODE) {
		struct vmm_fb_videomode mode1, mode2;

		vmm_fb_var_to_videomode(&mode1, var);
		vmm_fb_var_to_videomode(&mode2, &info->var);
		/* make sure we don't delete the videomode of current var */
		ret = vmm_fb_mode_is_equal(&mode1, &mode2);

		if (!ret)
		    vmm_fb_delete_videomode(&mode1, &info->modelist);

		ret = (ret) ? VMM_EINVALID : 0;
		goto done;
	}

	if ((var->activate & FB_ACTIVATE_FORCE) ||
	    vmm_memcmp(&info->var, var, sizeof(struct vmm_fb_var_screeninfo))) {
		u32 activate = var->activate;

		if (!info->fbops->fb_check_var) {
			vmm_memcpy(var, &info->var, sizeof(*var));
			goto done;
		}

		ret = info->fbops->fb_check_var(var, info);

		if (ret)
			goto done;

		if ((var->activate & FB_ACTIVATE_MASK) == FB_ACTIVATE_NOW) {
			struct vmm_fb_var_screeninfo old_var;
			struct vmm_fb_videomode mode;

			if (info->fbops->fb_get_caps) {
				ret = vmm_fb_check_caps(info, var, activate);

				if (ret)
					goto done;
			}

			vmm_memcpy(&old_var, &info->var, sizeof(old_var));
			vmm_memcpy(&info->var, var, sizeof(info->var));

			if (info->fbops->fb_set_par) {
				ret = info->fbops->fb_set_par(info);

				if (ret) {
					vmm_memcpy(&info->var, &old_var, sizeof(info->var));
					vmm_printf("detected fb_set_par error,"
						   " error code: %d\n", ret);
					goto done;
				}
			}

			vmm_fb_pan_display(info, &info->var);
			vmm_fb_set_cmap(&info->cmap, info);
			vmm_fb_var_to_videomode(&mode, &info->var);

			if (info->modelist.prev && info->modelist.next &&
			    !list_empty(&info->modelist))
				ret = vmm_fb_add_videomode(&mode, &info->modelist);
		}
	}

 done:
	return ret;
}

int vmm_fb_pan_display(struct vmm_fb_info *info, struct vmm_fb_var_screeninfo *var)
{
	struct vmm_fb_fix_screeninfo *fix = &info->fix;
	unsigned int yres = info->var.yres;
	int err = 0;

	if (var->yoffset > 0) {
		if (var->vmode & FB_VMODE_YWRAP) {
			if (!fix->ywrapstep || umod32(var->yoffset, fix->ywrapstep))
				err = VMM_EINVALID;
			else
				yres = 0;
		} else if (!fix->ypanstep || umod32(var->yoffset, fix->ypanstep))
			err = VMM_EINVALID;
	}

	if (var->xoffset > 0 && (!fix->xpanstep ||
				umod32(var->xoffset, fix->xpanstep)))
		err = VMM_EINVALID;

	if (err || !info->fbops->fb_pan_display ||
	    var->yoffset > info->var.yres_virtual - yres ||
	    var->xoffset > info->var.xres_virtual - info->var.xres)
		return VMM_EINVALID;

	if ((err = info->fbops->fb_pan_display(var, info)))
		return err;
	info->var.xoffset = var->xoffset;
	info->var.yoffset = var->yoffset;
	if (var->vmode & FB_VMODE_YWRAP)
		info->var.vmode |= FB_VMODE_YWRAP;
	else
		info->var.vmode &= ~FB_VMODE_YWRAP;
	return 0;
}

int vmm_fb_blank(struct vmm_fb_info *info, int blank)
{	
 	int ret = VMM_EINVALID;

 	if (blank > FB_BLANK_POWERDOWN)
 		blank = FB_BLANK_POWERDOWN;

	if (info->fbops->fb_blank)
 		ret = info->fbops->fb_blank(blank, info);

 	return ret;
}

int vmm_lock_fb_info(struct vmm_fb_info *info)
{
	vmm_mutex_lock(&info->lock);
	if (!info->fbops) {
		vmm_mutex_unlock(&info->lock);
		return 0;
	}
	return 1;
}

void vmm_unlock_fb_info(struct vmm_fb_info *info)
{
	vmm_mutex_unlock(&info->lock);
}

void vmm_fb_set_suspend(struct vmm_fb_info *info, int state)
{
	if (!vmm_lock_fb_info(info))
		return;
	if (state) {
		info->state = FBINFO_STATE_SUSPENDED;
	} else {
		info->state = FBINFO_STATE_RUNNING;
	}
	vmm_unlock_fb_info(info);
}

int vmm_fb_open(struct vmm_fb_info *info)
{
	int res = 0;

	if (!info) {
		return VMM_EFAIL;
	}

	get_fb_info(info);

	vmm_mutex_lock(&info->lock);
	if (info->fbops->fb_open) {
		/* Note: we don't have userspace so, 
		 * always call fb_open with user=0
		 */
		res = info->fbops->fb_open(info, 0);
	}
	vmm_mutex_unlock(&info->lock);

	if (res) {
		put_fb_info(info);
	}

	return res;
}

int vmm_fb_release(struct vmm_fb_info *info)
{
	if (!info) {
		return VMM_EFAIL;
	}

	vmm_mutex_lock(&info->lock);
	if (info->fbops->fb_release) {
		/* Note: we don't have userspace so, 
		 * always call fb_release with user=0
		 */
		info->fbops->fb_release(info, 0);
	}
	vmm_mutex_unlock(&info->lock);

	put_fb_info(info);

	return VMM_OK;
}

int vmm_fb_register(struct vmm_fb_info *info)
{
	int rc;
	struct vmm_fb_videomode mode;
	struct vmm_classdev *cd;

	if (info == NULL) {
		return VMM_EFAIL;
	}
	if (info->fbops == NULL) {
		return VMM_EFAIL;
	}

	if ((rc = vmm_fb_check_foreignness(info))) {
		return rc;
	}

	arch_atomic_write(&info->count, 1);
	INIT_MUTEX(&info->lock);

	if (info->pixmap.addr == NULL) {
		info->pixmap.addr = vmm_malloc(FBPIXMAPSIZE);
		if (info->pixmap.addr) {
			info->pixmap.size = FBPIXMAPSIZE;
			info->pixmap.buf_align = 1;
			info->pixmap.scan_align = 1;
			info->pixmap.access_align = 32;
			info->pixmap.flags = FB_PIXMAP_DEFAULT;
		}
	}	
	info->pixmap.offset = 0;

	if (!info->pixmap.blit_x)
		info->pixmap.blit_x = ~(u32)0;

	if (!info->pixmap.blit_y)
		info->pixmap.blit_y = ~(u32)0;

	if (!info->modelist.prev || !info->modelist.next) {
		INIT_LIST_HEAD(&info->modelist);
	}

	vmm_fb_var_to_videomode(&mode, &info->var);
	vmm_fb_add_videomode(&mode, &info->modelist);

	cd = vmm_malloc(sizeof(struct vmm_classdev));
	if (!cd) {
		rc = VMM_EFAIL;
		goto free_pixmap;
	}

	INIT_LIST_HEAD(&cd->head);
	vmm_strcpy(cd->name, info->dev->node->name);
	cd->dev = info->dev;
	cd->priv = info;

	rc = vmm_devdrv_register_classdev(VMM_FB_CLASS_NAME, cd);
	if (rc) {
		goto free_classdev;
	}

	return VMM_OK;

free_classdev:
	cd->dev = NULL;
	cd->priv = NULL;
	vmm_free(cd);
free_pixmap:
	if (info->pixmap.flags & FB_PIXMAP_DEFAULT) {
		vmm_free(info->pixmap.addr);
	}
	return rc;
}

int vmm_fb_unregister(struct vmm_fb_info *info)
{
	int rc;
	struct vmm_classdev *cd;

	if (info == NULL) {
		return VMM_EFAIL;
	}
	if (info->dev == NULL) {
		return VMM_EFAIL;
	}

	cd = vmm_devdrv_find_classdev(VMM_FB_CLASS_NAME, info->dev->node->name);
	if (!cd) {
		return VMM_EFAIL;
	}

	rc = vmm_devdrv_unregister_classdev(VMM_FB_CLASS_NAME, cd);
	if (!rc) {
		vmm_free(cd);
	}

	if (info->pixmap.addr &&
	    (info->pixmap.flags & FB_PIXMAP_DEFAULT)) {
		vmm_free(info->pixmap.addr);
	}
	vmm_fb_destroy_modelist(&info->modelist);

	return rc;
}

struct vmm_fb_info *vmm_fb_find(const char *name)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_find_classdev(VMM_FB_CLASS_NAME, name);

	if (!cd) {
		return NULL;
	}

	return cd->priv;
}

struct vmm_fb_info *vmm_fb_get(int num)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_classdev(VMM_FB_CLASS_NAME, num);

	if (!cd) {
		return NULL;
	}

	return cd->priv;
}

u32 vmm_fb_count(void)
{
	return vmm_devdrv_classdev_count(VMM_FB_CLASS_NAME);
}

static int __init vmm_fb_init(void)
{
	int rc;
	struct vmm_class *c;

	vmm_printf("Initialize Frame Buffer Framework\n");

	c = vmm_malloc(sizeof(struct vmm_class));
	if (!c) {
		return VMM_EFAIL;
	}

	INIT_LIST_HEAD(&c->head);
	vmm_strcpy(c->name, VMM_FB_CLASS_NAME);
	INIT_LIST_HEAD(&c->classdev_list);

	rc = vmm_devdrv_register_class(c);
	if (rc) {
		vmm_free(c);
		return rc;
	}

	return VMM_OK;
}

static void __exit vmm_fb_exit(void)
{
	int rc;
	struct vmm_class *c;

	c = vmm_devdrv_find_class(VMM_FB_CLASS_NAME);
	if (!c) {
		return;
	}

	rc = vmm_devdrv_unregister_class(c);
	if (rc) {
		return;
	}

	vmm_free(c);
}

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
