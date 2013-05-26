#ifndef _LINUX_FB_H
#define _LINUX_FB_H

#include <fb/vmm_fb.h>

#define fb_info				vmm_fb_info
#define fb_ops				vmm_fb_ops
#define fb_fix_screeninfo		vmm_fb_fix_screeninfo
#define fb_var_screeninfo		vmm_fb_var_screeninfo
#define fb_videomode			vmm_fb_videomode
#define fb_bitfield			vmm_fb_bitfield
#define apertures_struct		vmm_apertures_struct
#define aperture			vmm_aperture

#define cfb_fillrect			vmm_cfb_fillrect
#define cfb_copyarea			vmm_cfb_copyarea
#define cfb_imageblit			vmm_cfb_imageblit

#define sys_fillrect			vmm_sys_fillrect
#define sys_copyarea			vmm_sys_copyarea
#define sys_imageblit			vmm_sys_imageblit

#define alloc_apertures			vmm_alloc_apertures
#define	framebuffer_alloc		vmm_fb_alloc
#define framebuffer_release		vmm_fb_release
#define remove_conflicting_framebuffers	vmm_fb_remove_conflicting_framebuffers
#define register_framebuffer		vmm_fb_register
#define unregister_framebuffer		vmm_fb_unregister
#define fb_set_var			vmm_fb_set_var
#define fb_alloc_cmap			vmm_fb_alloc_cmap
#define fb_dealloc_cmap			vmm_fb_dealloc_cmap

#endif /* _LINUX_FB_H */
