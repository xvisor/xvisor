#ifndef __DRV_SUNXI_CLK_FACTORS_H
#define __DRV_SUNXI_CLK_FACTORS_H

#include <drv/clk-provider.h>
#include <drv/clkdev.h>

#define SUNXI_FACTORS_NOT_APPLICABLE	(0)

struct clk_factors_config {
	u8 nshift;
	u8 nwidth;
	u8 kshift;
	u8 kwidth;
	u8 mshift;
	u8 mwidth;
	u8 pshift;
	u8 pwidth;
};

struct clk *clk_register_factors(struct vmm_device *dev, const char *name,
				 const char *parent_name,
				 unsigned long flags, void *reg,
				 struct clk_factors_config *config,
				 void (*get_factors) (u32 *rate, u32 parent_rate,
						      u8 *n, u8 *k, u8 *m, u8 *p),
				 vmm_spinlock_t *lock);
#endif
