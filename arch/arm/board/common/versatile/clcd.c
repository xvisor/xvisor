
#include <vmm_error.h>
#include <vmm_string.h>
#include <vmm_host_aspace.h>
#include <linux/amba/bus.h>
#include <linux/amba/clcd.h>
#include <versatile/clcd.h>

static struct clcd_panel vga = {
	.mode		= {
		.name		= "VGA",
		.refresh	= 60,
		.xres		= 640,
		.yres		= 480,
		.pixclock	= 39721,
		.left_margin	= 40,
		.right_margin	= 24,
		.upper_margin	= 32,
		.lower_margin	= 11,
		.hsync_len	= 96,
		.vsync_len	= 2,
		.sync		= 0,
		.vmode		= FB_VMODE_NONINTERLACED,
	},
	.width		= -1,
	.height		= -1,
	.tim2		= TIM2_BCD | TIM2_IPC,
	.cntl		= CNTL_LCDTFT | CNTL_BGR | CNTL_LCDVCOMP(1),
	.caps		= CLCD_CAP_5551 | CLCD_CAP_565 | CLCD_CAP_888,
	.bpp		= 16,
};

static struct clcd_panel xvga = {
	.mode		= {
		.name		= "XVGA",
		.refresh	= 60,
		.xres		= 1024,
		.yres		= 768,
		.pixclock	= 15748,
		.left_margin	= 152,
		.right_margin	= 48,
		.upper_margin	= 23,
		.lower_margin	= 3,
		.hsync_len	= 104,
		.vsync_len	= 4,
		.sync		= 0,
		.vmode		= FB_VMODE_NONINTERLACED,
	},
	.width		= -1,
	.height		= -1,
	.tim2		= TIM2_BCD | TIM2_IPC,
	.cntl		= CNTL_LCDTFT | CNTL_BGR | CNTL_LCDVCOMP(1),
	.caps		= CLCD_CAP_5551 | CLCD_CAP_565 | CLCD_CAP_888,
	.bpp		= 16,
};

/* Sanyo TM38QV67A02A - 3.8 inch QVGA (320x240) Color TFT */
static struct clcd_panel sanyo_tm38qv67a02a = {
	.mode		= {
		.name		= "Sanyo TM38QV67A02A",
		.refresh	= 116,
		.xres		= 320,
		.yres		= 240,
		.pixclock	= 100000,
		.left_margin	= 6,
		.right_margin	= 6,
		.upper_margin	= 5,
		.lower_margin	= 5,
		.hsync_len	= 6,
		.vsync_len	= 6,
		.sync		= 0,
		.vmode		= FB_VMODE_NONINTERLACED,
	},
	.width		= -1,
	.height		= -1,
	.tim2		= TIM2_BCD,
	.cntl		= CNTL_LCDTFT | CNTL_BGR | CNTL_LCDVCOMP(1),
	.caps		= CLCD_CAP_5551,
	.bpp		= 16,
};

static struct clcd_panel sanyo_2_5_in = {
	.mode		= {
		.name		= "Sanyo QVGA Portrait",
		.refresh	= 116,
		.xres		= 240,
		.yres		= 320,
		.pixclock	= 100000,
		.left_margin	= 20,
		.right_margin	= 10,
		.upper_margin	= 2,
		.lower_margin	= 2,
		.hsync_len	= 10,
		.vsync_len	= 2,
		.sync		= FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		.vmode		= FB_VMODE_NONINTERLACED,
	},
	.width		= -1,
	.height		= -1,
	.tim2		= TIM2_IVS | TIM2_IHS | TIM2_IPC,
	.cntl		= CNTL_LCDTFT | CNTL_BGR | CNTL_LCDVCOMP(1),
	.caps		= CLCD_CAP_5551,
	.bpp		= 16,
};

/* Epson L2F50113T00 - 2.2 inch 176x220 Color TFT */
static struct clcd_panel epson_l2f50113t00 = {
	.mode		= {
		.name		= "Epson L2F50113T00",
		.refresh	= 390,
		.xres		= 176,
		.yres		= 220,
		.pixclock	= 62500,
		.left_margin	= 3,
		.right_margin	= 2,
		.upper_margin	= 1,
		.lower_margin	= 0,
		.hsync_len	= 3,
		.vsync_len	= 2,
		.sync		= 0,
		.vmode		= FB_VMODE_NONINTERLACED,
	},
	.width		= -1,
	.height		= -1,
	.tim2		= TIM2_BCD | TIM2_IPC,
	.cntl		= CNTL_LCDTFT | CNTL_BGR | CNTL_LCDVCOMP(1),
	.caps		= CLCD_CAP_5551,
	.bpp		= 16,
};

static struct clcd_panel *panels[] = {
	&vga,
	&xvga,
	&sanyo_tm38qv67a02a,
	&sanyo_2_5_in,
	&epson_l2f50113t00,
};

struct clcd_panel *versatile_clcd_get_panel(const char *name)
{
	int i;

	for (i = 0; i < array_size(panels); i++)
		if (vmm_strcmp(panels[i]->mode.name, name) == 0)
			break;

	if (i < array_size(panels))
		return panels[i];

	vmm_printf("CLCD: couldn't get parameters for panel %s\n", name);

	return NULL;
}

int versatile_clcd_setup(struct clcd_fb *fb, unsigned long framesize)
{
	int rc;
	physical_addr_t smem_pa;

	fb->fb.screen_base = (void *)vmm_host_alloc_pages(
						  VMM_SIZE_TO_PAGE(framesize),
						  VMM_MEMORY_READABLE | 
						  VMM_MEMORY_WRITEABLE);

	if (!fb->fb.screen_base) {
		vmm_printf("CLCD: unable to map framebuffer\n");
		return VMM_ENOMEM;
	}

	rc = vmm_host_page_va2pa((virtual_addr_t)fb->fb.screen_base, &smem_pa);
	if (rc) {
		return rc;
	}

	fb->fb.fix.smem_start	= smem_pa;
	fb->fb.fix.smem_len	= framesize;

	return 0;
}

void versatile_clcd_remove(struct clcd_fb *fb)
{
	vmm_host_free_pages((virtual_addr_t)fb->fb.screen_base,
				VMM_SIZE_TO_PAGE(fb->fb.fix.smem_len));
}
