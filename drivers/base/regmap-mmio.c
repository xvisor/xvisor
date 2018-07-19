/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file regmap.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Regmap framework implementation.
 *
 * The source has been largely adapted from Linux
 * drivers/base/regmap.c
 *
 * Register map access API - MMIO support
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devdrv.h>
#include <vmm_devres.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <drv/clk.h>
#include <drv/regmap.h>

#include "regmap_internal.h"

struct regmap_mmio_context {
	void *regs;
	unsigned val_bytes;
	struct clk *clk;

	void (*reg_write)(struct regmap_mmio_context *ctx,
			  unsigned int reg, unsigned int val);
	unsigned int (*reg_read)(struct regmap_mmio_context *ctx,
			         unsigned int reg);
};

static int regmap_mmio_regbits_check(size_t reg_bits)
{
	switch (reg_bits) {
	case 8:
	case 16:
	case 32:
#ifdef CONFIG_64BIT
	case 64:
#endif
		return 0;
	default:
		return VMM_EINVALID;
	}
}

static int regmap_mmio_get_min_stride(size_t val_bits)
{
	int min_stride;

	switch (val_bits) {
	case 8:
		/* The core treats 0 as 1 */
		min_stride = 0;
		return 0;
	case 16:
		min_stride = 2;
		break;
	case 32:
		min_stride = 4;
		break;
#ifdef CONFIG_64BIT
	case 64:
		min_stride = 8;
		break;
#endif
	default:
		return VMM_EINVALID;
	}

	return min_stride;
}

static void regmap_mmio_write8(struct regmap_mmio_context *ctx,
				unsigned int reg,
				unsigned int val)
{
	vmm_writeb(val, ctx->regs + reg);
}

static void regmap_mmio_write16le(struct regmap_mmio_context *ctx,
				  unsigned int reg,
				  unsigned int val)
{
	vmm_writew(val, ctx->regs + reg);
}

static void regmap_mmio_write16be(struct regmap_mmio_context *ctx,
				  unsigned int reg,
				  unsigned int val)
{
	vmm_out_be16(ctx->regs + reg, val);
}

static void regmap_mmio_write32le(struct regmap_mmio_context *ctx,
				  unsigned int reg,
				  unsigned int val)
{
	vmm_writel(val, ctx->regs + reg);
}

static void regmap_mmio_write32be(struct regmap_mmio_context *ctx,
				  unsigned int reg,
				  unsigned int val)
{
	vmm_out_be32(ctx->regs + reg, val);
}

#ifdef CONFIG_64BIT
static void regmap_mmio_write64le(struct regmap_mmio_context *ctx,
				  unsigned int reg,
				  unsigned int val)
{
	vmm_writeq(val, ctx->regs + reg);
}
#endif

static int regmap_mmio_write(void *context, unsigned int reg, unsigned int val)
{
	struct regmap_mmio_context *ctx = context;
	int ret;

	if (!VMM_IS_ERR(ctx->clk)) {
		ret = clk_enable(ctx->clk);
		if (ret < 0)
			return ret;
	}

	ctx->reg_write(ctx, reg, val);

	if (!VMM_IS_ERR(ctx->clk))
		clk_disable(ctx->clk);

	return 0;
}

static unsigned int regmap_mmio_read8(struct regmap_mmio_context *ctx,
				      unsigned int reg)
{
	return vmm_readb(ctx->regs + reg);
}

static unsigned int regmap_mmio_read16le(struct regmap_mmio_context *ctx,
				         unsigned int reg)
{
	return vmm_readw(ctx->regs + reg);
}

static unsigned int regmap_mmio_read16be(struct regmap_mmio_context *ctx,
				         unsigned int reg)
{
	return vmm_in_be16(ctx->regs + reg);
}

static unsigned int regmap_mmio_read32le(struct regmap_mmio_context *ctx,
				         unsigned int reg)
{
	return vmm_readl(ctx->regs + reg);
}

static unsigned int regmap_mmio_read32be(struct regmap_mmio_context *ctx,
				         unsigned int reg)
{
	return vmm_in_be32(ctx->regs + reg);
}

#ifdef CONFIG_64BIT
static unsigned int regmap_mmio_read64le(struct regmap_mmio_context *ctx,
				         unsigned int reg)
{
	return vmm_readq(ctx->regs + reg);
}
#endif

static int regmap_mmio_read(void *context, unsigned int reg, unsigned int *val)
{
	struct regmap_mmio_context *ctx = context;
	int ret;

	if (!VMM_IS_ERR(ctx->clk)) {
		ret = clk_enable(ctx->clk);
		if (ret < 0)
			return ret;
	}

	*val = ctx->reg_read(ctx, reg);

	if (!VMM_IS_ERR(ctx->clk))
		clk_disable(ctx->clk);

	return 0;
}

static void regmap_mmio_free_context(void *context)
{
	struct regmap_mmio_context *ctx = context;

	if (!VMM_IS_ERR(ctx->clk)) {
		clk_unprepare(ctx->clk);
		clk_put(ctx->clk);
	}
	vmm_free(context);
}

static const struct regmap_bus regmap_mmio = {
	.fast_io = true,
	.reg_write = regmap_mmio_write,
	.reg_read = regmap_mmio_read,
	.free_context = regmap_mmio_free_context,
	.val_format_endian_default = REGMAP_ENDIAN_LITTLE,
};

static struct regmap_mmio_context *regmap_mmio_gen_context(
					struct vmm_device *dev,
					const char *clk_id,
					void *regs,
					const struct regmap_config *config)
{
	struct regmap_mmio_context *ctx;
	int min_stride;
	int ret;

	ret = regmap_mmio_regbits_check(config->reg_bits);
	if (ret)
		return VMM_ERR_PTR(ret);

	if (config->pad_bits)
		return VMM_ERR_PTR(VMM_EINVALID);

	min_stride = regmap_mmio_get_min_stride(config->val_bits);
	if (min_stride < 0)
		return VMM_ERR_PTR(min_stride);

	if (config->reg_stride < min_stride)
		return VMM_ERR_PTR(VMM_EINVALID);

	ctx = vmm_zalloc(sizeof(*ctx));
	if (!ctx)
		return VMM_ERR_PTR(VMM_ENOMEM);

	ctx->regs = regs;
	ctx->val_bytes = config->val_bits / 8;
	ctx->clk = VMM_ERR_PTR(VMM_ENODEV);

	switch (regmap_get_val_endian(dev, &regmap_mmio, config)) {
	case REGMAP_ENDIAN_DEFAULT:
	case REGMAP_ENDIAN_LITTLE:
#ifdef CONFIG_CPU_LE
	case REGMAP_ENDIAN_NATIVE:
#endif
		switch (config->val_bits) {
		case 8:
			ctx->reg_read = regmap_mmio_read8;
			ctx->reg_write = regmap_mmio_write8;
			break;
		case 16:
			ctx->reg_read = regmap_mmio_read16le;
			ctx->reg_write = regmap_mmio_write16le;
			break;
		case 32:
			ctx->reg_read = regmap_mmio_read32le;
			ctx->reg_write = regmap_mmio_write32le;
			break;
#ifdef CONFIG_64BIT
		case 64:
			ctx->reg_read = regmap_mmio_read64le;
			ctx->reg_write = regmap_mmio_write64le;
			break;
#endif
		default:
			ret = VMM_EINVALID;
			goto err_free;
		}
		break;
	case REGMAP_ENDIAN_BIG:
#ifdef CONFIG_CPU_BE
	case REGMAP_ENDIAN_NATIVE:
#endif
		switch (config->val_bits) {
		case 8:
			ctx->reg_read = regmap_mmio_read8;
			ctx->reg_write = regmap_mmio_write8;
			break;
		case 16:
			ctx->reg_read = regmap_mmio_read16be;
			ctx->reg_write = regmap_mmio_write16be;
			break;
		case 32:
			ctx->reg_read = regmap_mmio_read32be;
			ctx->reg_write = regmap_mmio_write32be;
			break;
		default:
			ret = VMM_EINVALID;
			goto err_free;
		}
		break;
	default:
		ret = VMM_EINVALID;
		goto err_free;
	}

	if (clk_id == NULL)
		return ctx;

	ctx->clk = clk_get(dev, clk_id);
	if (VMM_IS_ERR(ctx->clk)) {
		ret = VMM_PTR_ERR(ctx->clk);
		goto err_free;
	}

	ret = clk_prepare(ctx->clk);
	if (ret < 0) {
		clk_put(ctx->clk);
		goto err_free;
	}

	return ctx;

err_free:
	vmm_free(ctx);

	return VMM_ERR_PTR(ret);
}

struct regmap *__regmap_init_mmio_clk(struct vmm_device *dev, const char *clk_id,
				      void *regs,
				      const struct regmap_config *config)
{
	struct regmap_mmio_context *ctx;

	ctx = regmap_mmio_gen_context(dev, clk_id, regs, config);
	if (VMM_IS_ERR(ctx))
		return VMM_ERR_CAST(ctx);

	return __regmap_init(dev, &regmap_mmio, ctx, config);
}
VMM_EXPORT_SYMBOL(__regmap_init_mmio_clk);

struct regmap *__devm_regmap_init_mmio_clk(struct vmm_device *dev,
					   const char *clk_id,
					   void *regs,
					   const struct regmap_config *config)
{
	struct regmap_mmio_context *ctx;

	ctx = regmap_mmio_gen_context(dev, clk_id, regs, config);
	if (VMM_IS_ERR(ctx))
		return VMM_ERR_CAST(ctx);

	return __devm_regmap_init(dev, &regmap_mmio, ctx, config);
}
VMM_EXPORT_SYMBOL(__devm_regmap_init_mmio_clk);
