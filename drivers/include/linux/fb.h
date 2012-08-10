#ifndef _LINUX_FB_H
#define _LINUX_FB_H

#include <fb/vmm_fb.h>

#define fb_info				vmm_fb

#define register_framebuffer		vmm_fb_register
#define unregister_framebuffer		vmm_fb_unregister

#endif /* _LINUX_FB_H */
