#ifndef _LINUX_FB_H
#define _LINUX_FB_H

#include <drv/fb.h>

#define	framebuffer_alloc		fb_alloc
#define framebuffer_release		fb_release
#define remove_conflicting_framebuffers	fb_remove_conflicting_framebuffers
#define register_framebuffer		fb_register
#define unregister_framebuffer		fb_unregister

#endif /* _LINUX_FB_H */
