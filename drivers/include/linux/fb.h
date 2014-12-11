#ifndef _LINUX_FB_H
#define _LINUX_FB_H

#include <drv/fb.h>

#include <uapi/asm-generic/ioctl.h>

#define	framebuffer_alloc		fb_alloc
#define framebuffer_release		fb_release
#define remove_conflicting_framebuffers	fb_remove_conflicting_framebuffers
#define register_framebuffer		fb_register
#define unregister_framebuffer		fb_unregister

/* ioctls
   0x46 is 'F'								*/
#define FBIOGET_VSCREENINFO	0x4600
#define FBIOPUT_VSCREENINFO	0x4601
#define FBIOGET_FSCREENINFO	0x4602
#define FBIOGETCMAP		0x4604
#define FBIOPUTCMAP		0x4605
#define FBIOPAN_DISPLAY		0x4606
#ifndef __KERNEL__
#define FBIO_CURSOR            _IOWR('F', 0x08, struct fb_cursor)
#endif
/* 0x4607-0x460B are defined below */
/* #define FBIOGET_MONITORSPEC	0x460C */
/* #define FBIOPUT_MONITORSPEC	0x460D */
/* #define FBIOSWITCH_MONIBIT	0x460E */
#define FBIOGET_CON2FBMAP	0x460F
#define FBIOPUT_CON2FBMAP	0x4610
#define FBIOBLANK		0x4611		/* arg: 0 or vesa level + 1 */
#define FBIOGET_VBLANK		_IOR('F', 0x12, struct fb_vblank)
#define FBIO_ALLOC              0x4613
#define FBIO_FREE               0x4614
#define FBIOGET_GLYPH           0x4615
#define FBIOGET_HWCINFO         0x4616
#define FBIOPUT_MODEINFO        0x4617
#define FBIOGET_DISPINFO        0x4618
#define FBIO_WAITFORVSYNC	_IOW('F', 0x20, __u32)

#endif /* _LINUX_FB_H */
