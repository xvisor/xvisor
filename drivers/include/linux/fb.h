#ifndef _LINUX_FB_H
#define _LINUX_FB_H

#include <fb/vmm_fb.h>

#define fb_info				vmm_fb_info
#define fb_fix_screeninfo		vmm_fb_fix_screeninfo
#define fb_var_screeninfo		vmm_fb_var_screeninfo
#define fb_videomode			vmm_fb_videomode

#define register_framebuffer		vmm_fb_register
#define unregister_framebuffer		vmm_fb_unregister

#endif /* _LINUX_FB_H */
