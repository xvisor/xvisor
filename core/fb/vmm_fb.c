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
 * @file vmm_fb.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Frame buffer framework source
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_string.h>
#include <vmm_stdio.h>
#include <vmm_modules.h>
#include <vmm_devdrv.h>
#include <fb/vmm_fb.h>

#define MODULE_DESC			"Frame Buffer Framework"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		VMM_FB_CLASS_IPRIORITY
#define	MODULE_INIT			vmm_fb_init
#define	MODULE_EXIT			vmm_fb_exit

#define FBPIXMAPSIZE  (1024 * 8)

static int vmm_fb_check_foreignness(struct vmm_fb *fi)
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

int vmm_fb_lock(struct vmm_fb *fb)
{
	vmm_mutex_lock(&fb->lock);
	if (!fb->fbops) {
		vmm_mutex_unlock(&fb->lock);
		return 0;
	}
	return 1;
}

void vmm_fb_unlock(struct vmm_fb *fb)
{
	vmm_mutex_unlock(&fb->lock);
}

int vmm_fb_register(struct vmm_fb *fb)
{
	int rc;
	struct vmm_fb_videomode mode;
	struct vmm_classdev *cd;

	if (fb == NULL) {
		return VMM_EFAIL;
	}
	if (fb->fbops == NULL) {
		return VMM_EFAIL;
	}

	if ((rc = vmm_fb_check_foreignness(fb))) {
		return rc;
	}

	INIT_MUTEX(&fb->lock);

	if (fb->pixmap.addr == NULL) {
		fb->pixmap.addr = vmm_malloc(FBPIXMAPSIZE);
		if (fb->pixmap.addr) {
			fb->pixmap.size = FBPIXMAPSIZE;
			fb->pixmap.buf_align = 1;
			fb->pixmap.scan_align = 1;
			fb->pixmap.access_align = 32;
			fb->pixmap.flags = FB_PIXMAP_DEFAULT;
		}
	}	
	fb->pixmap.offset = 0;

	if (!fb->pixmap.blit_x)
		fb->pixmap.blit_x = ~(u32)0;

	if (!fb->pixmap.blit_y)
		fb->pixmap.blit_y = ~(u32)0;

	if (!fb->modelist.prev || !fb->modelist.next) {
		INIT_LIST_HEAD(&fb->modelist);
	}

	vmm_fb_var_to_videomode(&mode, &fb->var);
	vmm_fb_add_videomode(&mode, &fb->modelist);

	cd = vmm_malloc(sizeof(struct vmm_classdev));
	if (!cd) {
		rc = VMM_EFAIL;
		goto free_pixmap;
	}

	INIT_LIST_HEAD(&cd->head);
	vmm_strcpy(cd->name, fb->dev->node->name);
	cd->dev = fb->dev;
	cd->priv = fb;

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
	if (fb->pixmap.flags & FB_PIXMAP_DEFAULT) {
		vmm_free(fb->pixmap.addr);
	}
	return rc;
}

int vmm_fb_unregister(struct vmm_fb *fb)
{
	int rc;
	struct vmm_classdev *cd;

	if (fb == NULL) {
		return VMM_EFAIL;
	}
	if (fb->dev == NULL) {
		return VMM_EFAIL;
	}

	cd = vmm_devdrv_find_classdev(VMM_FB_CLASS_NAME, fb->dev->node->name);
	if (!cd) {
		return VMM_EFAIL;
	}

	rc = vmm_devdrv_unregister_classdev(VMM_FB_CLASS_NAME, cd);
	if (!rc) {
		vmm_free(cd);
	}

	if (fb->pixmap.addr &&
	    (fb->pixmap.flags & FB_PIXMAP_DEFAULT)) {
		vmm_free(fb->pixmap.addr);
	}
	vmm_fb_destroy_modelist(&fb->modelist);

	return rc;
}

struct vmm_fb *vmm_fb_find(const char *name)
{
	struct vmm_classdev *cd;

	cd = vmm_devdrv_find_classdev(VMM_FB_CLASS_NAME, name);

	if (!cd) {
		return NULL;
	}

	return cd->priv;
}

struct vmm_fb *vmm_fb_get(int num)
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
