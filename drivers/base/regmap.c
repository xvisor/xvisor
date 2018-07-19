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
 * Register map access API
 *
 * Copyright 2011 Wolfson Microelectronics plc
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devdrv.h>
#include <vmm_devres.h>
#include <vmm_stdio.h>
#include <vmm_host_io.h>
#include <libs/log2.h>
#include <libs/mathlib.h>

#include "regmap_internal.h"

static int _regmap_update_bits(struct regmap *map, unsigned int reg,
			       unsigned int mask, unsigned int val,
			       bool *change, bool force_write);

static int _regmap_bus_reg_read(void *context, unsigned int reg,
				unsigned int *val);
static int _regmap_bus_read(void *context, unsigned int reg,
			    unsigned int *val);
static int _regmap_bus_formatted_write(void *context, unsigned int reg,
				       unsigned int val);
static int _regmap_bus_reg_write(void *context, unsigned int reg,
				 unsigned int val);
static int _regmap_bus_raw_write(void *context, unsigned int reg,
				 unsigned int val);

bool regmap_reg_in_ranges(unsigned int reg,
			  const struct regmap_range *ranges,
			  unsigned int nranges)
{
	const struct regmap_range *r;
	int i;

	for (i = 0, r = ranges; i < nranges; i++, r++)
		if (regmap_reg_in_range(reg, r))
			return true;
	return false;
}
VMM_EXPORT_SYMBOL(regmap_reg_in_ranges);

bool regmap_check_range_table(struct regmap *map, unsigned int reg,
			      const struct regmap_access_table *table)
{
	/* Check "no ranges" first */
	if (regmap_reg_in_ranges(reg, table->no_ranges, table->n_no_ranges))
		return false;

	/* In case zero "yes ranges" are supplied, any reg is OK */
	if (!table->n_yes_ranges)
		return true;

	return regmap_reg_in_ranges(reg, table->yes_ranges,
				    table->n_yes_ranges);
}
VMM_EXPORT_SYMBOL(regmap_check_range_table);

bool regmap_writeable(struct regmap *map, unsigned int reg)
{
	if (map->max_register && reg > map->max_register)
		return false;

	if (map->writeable_reg)
		return map->writeable_reg(map->dev, reg);

	if (map->wr_table)
		return regmap_check_range_table(map, reg, map->wr_table);

	return true;
}

bool regmap_readable(struct regmap *map, unsigned int reg)
{
	if (!map->reg_read)
		return false;

	if (map->max_register && reg > map->max_register)
		return false;

	if (map->format.format_write)
		return false;

	if (map->readable_reg)
		return map->readable_reg(map->dev, reg);

	if (map->rd_table)
		return regmap_check_range_table(map, reg, map->rd_table);

	return true;
}

bool regmap_volatile(struct regmap *map, unsigned int reg)
{
	if (!map->format.format_write && !regmap_readable(map, reg))
		return false;

	if (map->volatile_reg)
		return map->volatile_reg(map->dev, reg);

	if (map->volatile_table)
		return regmap_check_range_table(map, reg, map->volatile_table);

	return true;
}

bool regmap_precious(struct regmap *map, unsigned int reg)
{
	if (!regmap_readable(map, reg))
		return false;

	if (map->precious_reg)
		return map->precious_reg(map->dev, reg);

	if (map->precious_table)
		return regmap_check_range_table(map, reg, map->precious_table);

	return false;
}

static bool regmap_volatile_range(struct regmap *map, unsigned int reg,
	size_t num)
{
	unsigned int i;

	for (i = 0; i < num; i++)
		if (!regmap_volatile(map, reg + i))
			return false;

	return true;
}

static void regmap_format_2_6_write(struct regmap *map,
				     unsigned int reg, unsigned int val)
{
	u8 *out = map->work_buf;

	*out = (reg << 6) | val;
}

static void regmap_format_4_12_write(struct regmap *map,
				     unsigned int reg, unsigned int val)
{
	u16 *out = map->work_buf;
	*out = vmm_cpu_to_be16((reg << 12) | val);
}

static void regmap_format_7_9_write(struct regmap *map,
				    unsigned int reg, unsigned int val)
{
	u16 *out = map->work_buf;
	*out = vmm_cpu_to_be16((reg << 9) | val);
}

static void regmap_format_10_14_write(struct regmap *map,
				    unsigned int reg, unsigned int val)
{
	u8 *out = map->work_buf;

	out[2] = val;
	out[1] = (val >> 8) | (reg << 6);
	out[0] = reg >> 2;
}

static void regmap_format_8(void *buf, unsigned int val, unsigned int shift)
{
	u8 *b = buf;

	b[0] = val << shift;
}

static void regmap_format_16_be(void *buf, unsigned int val, unsigned int shift)
{
	u16 *b = buf;

	b[0] = vmm_cpu_to_be16(val << shift);
}

static void regmap_format_16_le(void *buf, unsigned int val, unsigned int shift)
{
	u16 *b = buf;

	b[0] = vmm_cpu_to_le16(val << shift);
}

static void regmap_format_16_native(void *buf, unsigned int val,
				    unsigned int shift)
{
	*(u16 *)buf = val << shift;
}

static void regmap_format_24(void *buf, unsigned int val, unsigned int shift)
{
	u8 *b = buf;

	val <<= shift;

	b[0] = val >> 16;
	b[1] = val >> 8;
	b[2] = val;
}

static void regmap_format_32_be(void *buf, unsigned int val, unsigned int shift)
{
	u32 *b = buf;

	b[0] = vmm_cpu_to_be32(val << shift);
}

static void regmap_format_32_le(void *buf, unsigned int val, unsigned int shift)
{
	u32 *b = buf;

	b[0] = vmm_cpu_to_le32(val << shift);
}

static void regmap_format_32_native(void *buf, unsigned int val,
				    unsigned int shift)
{
	*(u32 *)buf = val << shift;
}

#ifdef CONFIG_64BIT
static void regmap_format_64_be(void *buf, unsigned int val, unsigned int shift)
{
	u64 *b = buf;

	b[0] = vmm_cpu_to_be64((u64)val << shift);
}

static void regmap_format_64_le(void *buf, unsigned int val, unsigned int shift)
{
	u64 *b = buf;

	b[0] = vmm_cpu_to_le64((u64)val << shift);
}

static void regmap_format_64_native(void *buf, unsigned int val,
				    unsigned int shift)
{
	*(u64 *)buf = (u64)val << shift;
}
#endif

static void regmap_parse_inplace_noop(void *buf)
{
}

static unsigned int regmap_parse_8(const void *buf)
{
	const u8 *b = buf;

	return b[0];
}

static unsigned int regmap_parse_16_be(const void *buf)
{
	const u16 *b = buf;

	return vmm_be16_to_cpu(b[0]);
}

static unsigned int regmap_parse_16_le(const void *buf)
{
	const u16 *b = buf;

	return vmm_le16_to_cpu(b[0]);
}

static void regmap_parse_16_be_inplace(void *buf)
{
	u16 *b = buf;

	b[0] = vmm_be16_to_cpu(b[0]);
}

static void regmap_parse_16_le_inplace(void *buf)
{
	u16 *b = buf;

	b[0] = vmm_le16_to_cpu(b[0]);
}

static unsigned int regmap_parse_16_native(const void *buf)
{
	return *(u16 *)buf;
}

static unsigned int regmap_parse_24(const void *buf)
{
	const u8 *b = buf;
	unsigned int ret = b[2];
	ret |= ((unsigned int)b[1]) << 8;
	ret |= ((unsigned int)b[0]) << 16;

	return ret;
}

static unsigned int regmap_parse_32_be(const void *buf)
{
	const u32 *b = buf;

	return vmm_be32_to_cpu(b[0]);
}

static unsigned int regmap_parse_32_le(const void *buf)
{
	const u32 *b = buf;

	return vmm_le32_to_cpu(b[0]);
}

static void regmap_parse_32_be_inplace(void *buf)
{
	u32 *b = buf;

	b[0] = vmm_be32_to_cpu(b[0]);
}

static void regmap_parse_32_le_inplace(void *buf)
{
	u32 *b = buf;

	b[0] = vmm_le32_to_cpu(b[0]);
}

static unsigned int regmap_parse_32_native(const void *buf)
{
	return *(u32 *)buf;
}

#ifdef CONFIG_64BIT
static unsigned int regmap_parse_64_be(const void *buf)
{
	const u64 *b = buf;

	return vmm_be64_to_cpu(b[0]);
}

static unsigned int regmap_parse_64_le(const void *buf)
{
	const u64 *b = buf;

	return vmm_le64_to_cpu(b[0]);
}

static void regmap_parse_64_be_inplace(void *buf)
{
	u64 *b = buf;

	b[0] = vmm_be64_to_cpu(b[0]);
}

static void regmap_parse_64_le_inplace(void *buf)
{
	u64 *b = buf;

	b[0] = vmm_le64_to_cpu(b[0]);
}

static unsigned int regmap_parse_64_native(const void *buf)
{
	return *(u64 *)buf;
}
#endif

static void regmap_lock_mutex(void *__map)
{
	struct regmap *map = __map;
	vmm_mutex_lock(&map->mutex);
}

static void regmap_unlock_mutex(void *__map)
{
	struct regmap *map = __map;
	vmm_mutex_unlock(&map->mutex);
}

static void regmap_lock_spinlock(void *__map)
{
	struct regmap *map = __map;
	unsigned long flags;

	vmm_spin_lock_irqsave(&map->spinlock, flags);
	map->spinlock_flags = flags;
}

static void regmap_unlock_spinlock(void *__map)
{
	struct regmap *map = __map;
	vmm_spin_unlock_irqrestore(&map->spinlock, map->spinlock_flags);
}

static void dev_get_regmap_release(struct vmm_device *dev, void *res)
{
	/*
	 * We don't actually have anything to do here; the goal here
	 * is not to manage the regmap but to provide a simple way to
	 * get the regmap back given a struct device.
	 */
}

int regmap_attach_dev(struct vmm_device *dev, struct regmap *map,
		      const struct regmap_config *config)
{
	struct regmap **m;

	map->dev = dev;

	/* Add a devres resource for dev_get_regmap() */
	m = vmm_devres_alloc(dev_get_regmap_release, sizeof(*m));
	if (!m) {
		return VMM_ENOMEM;
	}
	*m = map;
	vmm_devres_add(dev, m);

	return 0;
}
VMM_EXPORT_SYMBOL(regmap_attach_dev);

static enum regmap_endian regmap_get_reg_endian(const struct regmap_bus *bus,
					const struct regmap_config *config)
{
	enum regmap_endian endian;

	/* Retrieve the endianness specification from the regmap config */
	endian = config->reg_format_endian;

	/* If the regmap config specified a non-default value, use that */
	if (endian != REGMAP_ENDIAN_DEFAULT)
		return endian;

	/* Retrieve the endianness specification from the bus config */
	if (bus && bus->reg_format_endian_default)
		endian = bus->reg_format_endian_default;

	/* If the bus specified a non-default value, use that */
	if (endian != REGMAP_ENDIAN_DEFAULT)
		return endian;

	/* Use this if no other value was found */
	return REGMAP_ENDIAN_BIG;
}

enum regmap_endian regmap_get_val_endian(struct vmm_device *dev,
					 const struct regmap_bus *bus,
					 const struct regmap_config *config)
{
	struct vmm_devtree_node *np;
	enum regmap_endian endian;

	/* Retrieve the endianness specification from the regmap config */
	endian = config->val_format_endian;

	/* If the regmap config specified a non-default value, use that */
	if (endian != REGMAP_ENDIAN_DEFAULT)
		return endian;

	/* If the dev and dev->of_node exist try to get endianness from DT */
	if (dev && dev->of_node) {
		np = dev->of_node;

		/* Parse the device's DT node for an endianness specification */
		if (vmm_devtree_getattr(np, "big-endian"))
			endian = REGMAP_ENDIAN_BIG;
		else if (vmm_devtree_getattr(np, "little-endian"))
			endian = REGMAP_ENDIAN_LITTLE;
		else if (vmm_devtree_getattr(np, "native-endian"))
			endian = REGMAP_ENDIAN_NATIVE;

		/* If the endianness was specified in DT, use that */
		if (endian != REGMAP_ENDIAN_DEFAULT)
			return endian;
	}

	/* Retrieve the endianness specification from the bus config */
	if (bus && bus->val_format_endian_default)
		endian = bus->val_format_endian_default;

	/* If the bus specified a non-default value, use that */
	if (endian != REGMAP_ENDIAN_DEFAULT)
		return endian;

	/* Use this if no other value was found */
	return REGMAP_ENDIAN_BIG;
}
VMM_EXPORT_SYMBOL(regmap_get_val_endian);

struct regmap *__regmap_init(struct vmm_device *dev,
			     const struct regmap_bus *bus,
			     void *bus_context,
			     const struct regmap_config *config)
{
	struct regmap *map;
	int ret = VMM_EINVALID;
	enum regmap_endian reg_endian, val_endian;

	if (!config)
		goto err;

	map = vmm_zalloc(sizeof(*map));
	if (map == NULL) {
		ret = VMM_ENOMEM;
		goto err;
	}

	if (config->lock && config->unlock) {
		map->lock = config->lock;
		map->unlock = config->unlock;
		map->lock_arg = config->lock_arg;
	} else {
		if ((bus && bus->fast_io) ||
		    config->fast_io) {
			INIT_SPIN_LOCK(&map->spinlock);
			map->lock = regmap_lock_spinlock;
			map->unlock = regmap_unlock_spinlock;
		} else {
			INIT_MUTEX(&map->mutex);
			map->lock = regmap_lock_mutex;
			map->unlock = regmap_unlock_mutex;
		}
		map->lock_arg = map;
	}

	map->format.reg_bytes = DIV_ROUND_UP(config->reg_bits, 8);
	map->format.pad_bytes = config->pad_bits / 8;
	map->format.val_bytes = DIV_ROUND_UP(config->val_bits, 8);
	map->format.buf_size = DIV_ROUND_UP(config->reg_bits +
			config->val_bits + config->pad_bits, 8);
	map->reg_shift = config->pad_bits % 8;
	if (config->reg_stride)
		map->reg_stride = config->reg_stride;
	else
		map->reg_stride = 1;
	if (is_power_of_2(map->reg_stride))
		map->reg_stride_order = ilog2(map->reg_stride);
	else
		map->reg_stride_order = -1;
	map->use_single_read = config->use_single_rw || !bus || !bus->read;
	map->use_single_write = config->use_single_rw || !bus || !bus->write;
	map->can_multi_write = config->can_multi_write && bus && bus->write;
	if (bus) {
		map->max_raw_read = bus->max_raw_read;
		map->max_raw_write = bus->max_raw_write;
	}
	map->dev = dev;
	map->bus = bus;
	map->bus_context = bus_context;
	map->max_register = config->max_register;
	map->wr_table = config->wr_table;
	map->rd_table = config->rd_table;
	map->volatile_table = config->volatile_table;
	map->precious_table = config->precious_table;
	map->writeable_reg = config->writeable_reg;
	map->readable_reg = config->readable_reg;
	map->volatile_reg = config->volatile_reg;
	map->precious_reg = config->precious_reg;
	map->name = config->name;

	if (config->read_flag_mask || config->write_flag_mask) {
		map->read_flag_mask = config->read_flag_mask;
		map->write_flag_mask = config->write_flag_mask;
	} else if (bus) {
		map->read_flag_mask = bus->read_flag_mask;
	}

	if (!bus) {
		map->reg_read  = config->reg_read;
		map->reg_write = config->reg_write;
		goto skip_format_initialization;
	} else if (!bus->read || !bus->write) {
		map->reg_read = _regmap_bus_reg_read;
		map->reg_write = _regmap_bus_reg_write;
		goto skip_format_initialization;
	} else {
		map->reg_read  = _regmap_bus_read;
		map->reg_update_bits = bus->reg_update_bits;
	}

	reg_endian = regmap_get_reg_endian(bus, config);
	val_endian = regmap_get_val_endian(dev, bus, config);

	switch (config->reg_bits + map->reg_shift) {
	case 2:
		switch (config->val_bits) {
		case 6:
			map->format.format_write = regmap_format_2_6_write;
			break;
		default:
			goto err_map;
		}
		break;

	case 4:
		switch (config->val_bits) {
		case 12:
			map->format.format_write = regmap_format_4_12_write;
			break;
		default:
			goto err_map;
		}
		break;

	case 7:
		switch (config->val_bits) {
		case 9:
			map->format.format_write = regmap_format_7_9_write;
			break;
		default:
			goto err_map;
		}
		break;

	case 10:
		switch (config->val_bits) {
		case 14:
			map->format.format_write = regmap_format_10_14_write;
			break;
		default:
			goto err_map;
		}
		break;

	case 8:
		map->format.format_reg = regmap_format_8;
		break;

	case 16:
		switch (reg_endian) {
		case REGMAP_ENDIAN_BIG:
			map->format.format_reg = regmap_format_16_be;
			break;
		case REGMAP_ENDIAN_LITTLE:
			map->format.format_reg = regmap_format_16_le;
			break;
		case REGMAP_ENDIAN_NATIVE:
			map->format.format_reg = regmap_format_16_native;
			break;
		default:
			goto err_map;
		}
		break;

	case 24:
		if (reg_endian != REGMAP_ENDIAN_BIG)
			goto err_map;
		map->format.format_reg = regmap_format_24;
		break;

	case 32:
		switch (reg_endian) {
		case REGMAP_ENDIAN_BIG:
			map->format.format_reg = regmap_format_32_be;
			break;
		case REGMAP_ENDIAN_LITTLE:
			map->format.format_reg = regmap_format_32_le;
			break;
		case REGMAP_ENDIAN_NATIVE:
			map->format.format_reg = regmap_format_32_native;
			break;
		default:
			goto err_map;
		}
		break;

#ifdef CONFIG_64BIT
	case 64:
		switch (reg_endian) {
		case REGMAP_ENDIAN_BIG:
			map->format.format_reg = regmap_format_64_be;
			break;
		case REGMAP_ENDIAN_LITTLE:
			map->format.format_reg = regmap_format_64_le;
			break;
		case REGMAP_ENDIAN_NATIVE:
			map->format.format_reg = regmap_format_64_native;
			break;
		default:
			goto err_map;
		}
		break;
#endif

	default:
		goto err_map;
	}

	if (val_endian == REGMAP_ENDIAN_NATIVE)
		map->format.parse_inplace = regmap_parse_inplace_noop;

	switch (config->val_bits) {
	case 8:
		map->format.format_val = regmap_format_8;
		map->format.parse_val = regmap_parse_8;
		map->format.parse_inplace = regmap_parse_inplace_noop;
		break;
	case 16:
		switch (val_endian) {
		case REGMAP_ENDIAN_BIG:
			map->format.format_val = regmap_format_16_be;
			map->format.parse_val = regmap_parse_16_be;
			map->format.parse_inplace = regmap_parse_16_be_inplace;
			break;
		case REGMAP_ENDIAN_LITTLE:
			map->format.format_val = regmap_format_16_le;
			map->format.parse_val = regmap_parse_16_le;
			map->format.parse_inplace = regmap_parse_16_le_inplace;
			break;
		case REGMAP_ENDIAN_NATIVE:
			map->format.format_val = regmap_format_16_native;
			map->format.parse_val = regmap_parse_16_native;
			break;
		default:
			goto err_map;
		}
		break;
	case 24:
		if (val_endian != REGMAP_ENDIAN_BIG)
			goto err_map;
		map->format.format_val = regmap_format_24;
		map->format.parse_val = regmap_parse_24;
		break;
	case 32:
		switch (val_endian) {
		case REGMAP_ENDIAN_BIG:
			map->format.format_val = regmap_format_32_be;
			map->format.parse_val = regmap_parse_32_be;
			map->format.parse_inplace = regmap_parse_32_be_inplace;
			break;
		case REGMAP_ENDIAN_LITTLE:
			map->format.format_val = regmap_format_32_le;
			map->format.parse_val = regmap_parse_32_le;
			map->format.parse_inplace = regmap_parse_32_le_inplace;
			break;
		case REGMAP_ENDIAN_NATIVE:
			map->format.format_val = regmap_format_32_native;
			map->format.parse_val = regmap_parse_32_native;
			break;
		default:
			goto err_map;
		}
		break;
#ifdef CONFIG_64BIT
	case 64:
		switch (val_endian) {
		case REGMAP_ENDIAN_BIG:
			map->format.format_val = regmap_format_64_be;
			map->format.parse_val = regmap_parse_64_be;
			map->format.parse_inplace = regmap_parse_64_be_inplace;
			break;
		case REGMAP_ENDIAN_LITTLE:
			map->format.format_val = regmap_format_64_le;
			map->format.parse_val = regmap_parse_64_le;
			map->format.parse_inplace = regmap_parse_64_le_inplace;
			break;
		case REGMAP_ENDIAN_NATIVE:
			map->format.format_val = regmap_format_64_native;
			map->format.parse_val = regmap_parse_64_native;
			break;
		default:
			goto err_map;
		}
		break;
#endif
	}

	if (map->format.format_write) {
		if ((reg_endian != REGMAP_ENDIAN_BIG) ||
		    (val_endian != REGMAP_ENDIAN_BIG))
			goto err_map;
		map->use_single_write = TRUE;
	}

	if (!map->format.format_write &&
	    !(map->format.format_reg && map->format.format_val))
		goto err;

	map->work_buf = vmm_zalloc(map->format.buf_size);
	if (map->work_buf == NULL) {
		ret = VMM_ENOMEM;
		goto err_map;
	}

	if (map->format.format_write) {
		map->reg_write = _regmap_bus_formatted_write;
	} else if (map->format.format_val) {
		map->reg_write = _regmap_bus_raw_write;
	}

skip_format_initialization:

	if (dev) {
		ret = regmap_attach_dev(dev, map, config);
		if (ret != 0)
			goto err_map;
	}

	return map;

err_map:
	vmm_free(map);
err:
	return VMM_ERR_PTR(ret);
}
VMM_EXPORT_SYMBOL(__regmap_init);

static void devm_regmap_release(struct vmm_device *dev, void *res)
{
	regmap_exit(*(struct regmap **)res);
}

struct regmap *__devm_regmap_init(struct vmm_device *dev,
				  const struct regmap_bus *bus,
				  void *bus_context,
				  const struct regmap_config *config)
{
	struct regmap **ptr, *regmap;

	ptr = vmm_devres_alloc(devm_regmap_release, sizeof(*ptr));
	if (!ptr)
		return VMM_ERR_PTR(VMM_ENOMEM);

	regmap = __regmap_init(dev, bus, bus_context, config);
	if (!VMM_IS_ERR(regmap)) {
		*ptr = regmap;
		vmm_devres_add(dev, ptr);
	} else {
		vmm_devres_free(ptr);
	}

	return regmap;
}
VMM_EXPORT_SYMBOL(__devm_regmap_init);

void regmap_exit(struct regmap *map)
{
	if (map->bus && map->bus->free_context)
		map->bus->free_context(map->bus_context);
	vmm_free(map->work_buf);
	vmm_free(map);
}
VMM_EXPORT_SYMBOL(regmap_exit);

static int dev_get_regmap_match(struct vmm_device *dev, void *res, void *data)
{
	struct regmap **r = res;
	if (!r || !*r) {
		WARN_ON(!r || !*r);
		return 0;
	}

	/* If the user didn't specify a name match any */
	if (data)
		return (*r)->name == data;
	else
		return 1;
}

/**
 * dev_get_regmap() - Obtain the regmap (if any) for a device
 *
 * @dev: Device to retrieve the map for
 * @name: Optional name for the register map, usually NULL.
 *
 * Returns the regmap for the device if one is present, or NULL.  If
 * name is specified then it must match the name specified when
 * registering the device, if it is NULL then the first regmap found
 * will be used.  Devices with multiple register maps are very rare,
 * generic code should normally not need to specify a name.
 */
struct regmap *dev_get_regmap(struct vmm_device *dev, const char *name)
{
	struct regmap **r = vmm_devres_find(dev, dev_get_regmap_release,
					dev_get_regmap_match, (void *)name);

	if (!r)
		return NULL;
	return *r;
}
VMM_EXPORT_SYMBOL(dev_get_regmap);

/**
 * regmap_get_device() - Obtain the device from a regmap
 *
 * @map: Register map to operate on.
 *
 * Returns the underlying device that the regmap has been created for.
 */
struct vmm_device *regmap_get_device(struct regmap *map)
{
	return map->dev;
}
VMM_EXPORT_SYMBOL(regmap_get_device);

static void regmap_set_work_buf_flag_mask(struct regmap *map, int max_bytes,
					  unsigned long mask)
{
	u8 *buf;
	int i;

	if (!mask || !map->work_buf)
		return;

	buf = map->work_buf;

	for (i = 0; i < max_bytes; i++)
		buf[i] |= (mask >> (8 * i)) & 0xff;
}

int _regmap_raw_write(struct regmap *map, unsigned int reg,
		      const void *val, size_t val_len)
{
	void *work_val = map->work_buf + map->format.reg_bytes +
		map->format.pad_bytes;
	void *buf;
	int ret = VMM_ENOTSUPP;
	size_t len;
	int i;

	WARN_ON(!map->bus);

	/* Check for unwritable registers before we start */
	if (map->writeable_reg)
		for (i = 0; i < udiv64(val_len, map->format.val_bytes); i++)
			if (!map->writeable_reg(map->dev,
					       reg + regmap_get_offset(map, i)))
				return VMM_EINVALID;

	map->format.format_reg(map->work_buf, reg, map->reg_shift);
	regmap_set_work_buf_flag_mask(map, map->format.reg_bytes,
				      map->write_flag_mask);

	/*
	 * Essentially all I/O mechanisms will be faster with a single
	 * buffer to write.  Since register syncs often generate raw
	 * writes of single registers optimise that case.
	 */
	if (val != work_val && val_len == map->format.val_bytes) {
		memcpy(work_val, val, map->format.val_bytes);
		val = work_val;
	}

	/* If we're doing a single register write we can probably just
	 * send the work_buf directly, otherwise try to do a gather
	 * write.
	 */
	if (val == work_val)
		ret = map->bus->write(map->bus_context, map->work_buf,
				      map->format.reg_bytes +
				      map->format.pad_bytes +
				      val_len);
	else if (map->bus->gather_write)
		ret = map->bus->gather_write(map->bus_context, map->work_buf,
					     map->format.reg_bytes +
					     map->format.pad_bytes,
					     val, val_len);

	/* If that didn't work fall back on linearising by hand. */
	if (ret == VMM_ENOTSUPP) {
		len = map->format.reg_bytes + map->format.pad_bytes + val_len;
		buf = vmm_zalloc(len);
		if (!buf)
			return VMM_ENOMEM;

		memcpy(buf, map->work_buf, map->format.reg_bytes);
		memcpy(buf + map->format.reg_bytes + map->format.pad_bytes,
		       val, val_len);
		ret = map->bus->write(map->bus_context, buf, len);

		vmm_free(buf);
	}

	return ret;
}

/**
 * regmap_can_raw_write - Test if regmap_raw_write() is supported
 *
 * @map: Map to check.
 */
bool regmap_can_raw_write(struct regmap *map)
{
	return map->bus && map->bus->write && map->format.format_val &&
		map->format.format_reg;
}
VMM_EXPORT_SYMBOL(regmap_can_raw_write);

/**
 * regmap_get_raw_read_max - Get the maximum size we can read
 *
 * @map: Map to check.
 */
size_t regmap_get_raw_read_max(struct regmap *map)
{
	return map->max_raw_read;
}
VMM_EXPORT_SYMBOL(regmap_get_raw_read_max);

/**
 * regmap_get_raw_write_max - Get the maximum size we can read
 *
 * @map: Map to check.
 */
size_t regmap_get_raw_write_max(struct regmap *map)
{
	return map->max_raw_write;
}
VMM_EXPORT_SYMBOL(regmap_get_raw_write_max);

static int _regmap_bus_formatted_write(void *context, unsigned int reg,
				       unsigned int val)
{
	struct regmap *map = context;

	WARN_ON(!map->bus || !map->format.format_write);

	map->format.format_write(map, reg, val);

	return map->bus->write(map->bus_context, map->work_buf,
			      map->format.buf_size);
}

static int _regmap_bus_reg_write(void *context, unsigned int reg,
				 unsigned int val)
{
	struct regmap *map = context;

	return map->bus->reg_write(map->bus_context, reg, val);
}

static int _regmap_bus_raw_write(void *context, unsigned int reg,
				 unsigned int val)
{
	struct regmap *map = context;

	WARN_ON(!map->bus || !map->format.format_val);

	map->format.format_val(map->work_buf + map->format.reg_bytes
			       + map->format.pad_bytes, val, 0);
	return _regmap_raw_write(map, reg,
				 map->work_buf +
				 map->format.reg_bytes +
				 map->format.pad_bytes,
				 map->format.val_bytes);
}

static inline void *_regmap_map_get_context(struct regmap *map)
{
	return (map->bus) ? map : map->bus_context;
}

int _regmap_write(struct regmap *map, unsigned int reg,
		  unsigned int val)
{
	void *context = _regmap_map_get_context(map);

	if (!regmap_writeable(map, reg))
		return VMM_EIO;

	return map->reg_write(context, reg, val);
}

/**
 * regmap_write() - Write a value to a single register
 *
 * @map: Register map to write to
 * @reg: Register to write to
 * @val: Value to be written
 *
 * A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int regmap_write(struct regmap *map, unsigned int reg, unsigned int val)
{
	int ret;

	if (!is_aligned(reg, map->reg_stride))
		return VMM_EINVALID;

	map->lock(map->lock_arg);

	ret = _regmap_write(map, reg, val);

	map->unlock(map->lock_arg);

	return ret;
}
VMM_EXPORT_SYMBOL(regmap_write);

/**
 * regmap_raw_write() - Write raw values to one or more registers
 *
 * @map: Register map to write to
 * @reg: Initial register to write to
 * @val: Block of data to be written, laid out for direct transmission to the
 *       device
 * @val_len: Length of data pointed to by val.
 *
 * This function is intended to be used for things like firmware
 * download where a large block of data needs to be transferred to the
 * device.  No formatting will be done on the data provided.
 *
 * A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int regmap_raw_write(struct regmap *map, unsigned int reg,
		     const void *val, size_t val_len)
{
	int ret;

	if (!regmap_can_raw_write(map))
		return VMM_EINVALID;
	if (umod64(val_len, map->format.val_bytes))
		return VMM_EINVALID;
	if (map->max_raw_write && map->max_raw_write > val_len)
		return VMM_EINVALID;

	map->lock(map->lock_arg);

	ret = _regmap_raw_write(map, reg, val, val_len);

	map->unlock(map->lock_arg);

	return ret;
}
VMM_EXPORT_SYMBOL(regmap_raw_write);

/**
 * regmap_bulk_write() - Write multiple registers to the device
 *
 * @map: Register map to write to
 * @reg: First register to be write from
 * @val: Block of data to be written, in native register size for device
 * @val_count: Number of registers to write
 *
 * This function is intended to be used for writing a large block of
 * data to the device either in single transfer or multiple transfer.
 *
 * A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int regmap_bulk_write(struct regmap *map, unsigned int reg, const void *val,
		     size_t val_count)
{
	int ret = 0, i;
	size_t val_bytes = map->format.val_bytes;
	size_t total_size = val_bytes * val_count;

	if (!is_aligned(reg, map->reg_stride))
		return VMM_EINVALID;

	/*
	 * Some devices don't support bulk write, for
	 * them we have a series of single write operations in the first two if
	 * blocks.
	 *
	 * The first if block is used for memory mapped io. It does not allow
	 * val_bytes of 3 for example.
	 * The second one is for busses that do not provide raw I/O.
	 * The third one is used for busses which do not have these limitations
	 * and can write arbitrary value lengths.
	 */
	if (!map->bus) {
		map->lock(map->lock_arg);
		for (i = 0; i < val_count; i++) {
			unsigned int ival;

			switch (val_bytes) {
			case 1:
				ival = *(u8 *)(val + (i * val_bytes));
				break;
			case 2:
				ival = *(u16 *)(val + (i * val_bytes));
				break;
			case 4:
				ival = *(u32 *)(val + (i * val_bytes));
				break;
#ifdef CONFIG_64BIT
			case 8:
				ival = *(u64 *)(val + (i * val_bytes));
				break;
#endif
			default:
				ret = VMM_EINVALID;
				goto out;
			}

			ret = _regmap_write(map,
					    reg + regmap_get_offset(map, i),
					    ival);
			if (ret != 0)
				goto out;
		}
out:
		map->unlock(map->lock_arg);
	} else if (map->bus && !map->format.parse_inplace) {
		const u8 *u8 = val;
		const u16 *u16 = val;
		const u32 *u32 = val;
		unsigned int ival;

		for (i = 0; i < val_count; i++) {
			switch (map->format.val_bytes) {
			case 4:
				ival = u32[i];
				break;
			case 2:
				ival = u16[i];
				break;
			case 1:
				ival = u8[i];
				break;
			default:
				return VMM_EINVALID;
			}

			ret = regmap_write(map, reg + (i * map->reg_stride),
					   ival);
			if (ret)
				return ret;
		}
	} else if (map->use_single_write ||
		   (map->max_raw_write && map->max_raw_write < total_size)) {
		int chunk_stride = map->reg_stride;
		size_t chunk_size = val_bytes;
		size_t chunk_count = val_count;

		if (!map->use_single_write) {
			chunk_size = map->max_raw_write;
			if (umod64(chunk_size, val_bytes))
				chunk_size -= umod64(chunk_size, val_bytes);
			chunk_count = udiv64(total_size, chunk_size);
			chunk_stride *= udiv64(chunk_size, val_bytes);
		}

		map->lock(map->lock_arg);
		/* Write as many bytes as possible with chunk_size */
		for (i = 0; i < chunk_count; i++) {
			ret = _regmap_raw_write(map,
						reg + (i * chunk_stride),
						val + (i * chunk_size),
						chunk_size);
			if (ret)
				break;
		}

		/* Write remaining bytes */
		if (!ret && chunk_size * i < total_size) {
			ret = _regmap_raw_write(map, reg + (i * chunk_stride),
						val + (i * chunk_size),
						total_size - i * chunk_size);
		}
		map->unlock(map->lock_arg);
	} else {
		void *wval;

		if (!val_count)
			return VMM_EINVALID;

		wval = vmm_zalloc(val_count * val_bytes);
		memcpy(wval, val, val_count * val_bytes);
		if (!wval) {
			vmm_lerror(map->dev->name, "Error in memory allocation\n");
			return VMM_ENOMEM;
		}
		for (i = 0; i < val_count * val_bytes; i += val_bytes)
			map->format.parse_inplace(wval + i);

		map->lock(map->lock_arg);
		ret = _regmap_raw_write(map, reg, wval, val_bytes * val_count);
		map->unlock(map->lock_arg);

		vmm_free(wval);
	}
	return ret;
}
VMM_EXPORT_SYMBOL(regmap_bulk_write);

static int _regmap_raw_read(struct regmap *map, unsigned int reg, void *val,
			    unsigned int val_len)
{
	WARN_ON(!map->bus);

	if (!map->bus || !map->bus->read)
		return VMM_EINVALID;

	map->format.format_reg(map->work_buf, reg, map->reg_shift);
	regmap_set_work_buf_flag_mask(map, map->format.reg_bytes,
				      map->read_flag_mask);

	return map->bus->read(map->bus_context, map->work_buf,
			     map->format.reg_bytes + map->format.pad_bytes,
			     val, val_len);
}

static int _regmap_bus_reg_read(void *context, unsigned int reg,
				unsigned int *val)
{
	struct regmap *map = context;

	return map->bus->reg_read(map->bus_context, reg, val);
}

static int _regmap_bus_read(void *context, unsigned int reg,
			    unsigned int *val)
{
	int ret;
	struct regmap *map = context;

	if (!map->format.parse_val)
		return VMM_EINVALID;

	ret = _regmap_raw_read(map, reg, map->work_buf, map->format.val_bytes);
	if (ret == 0)
		*val = map->format.parse_val(map->work_buf);

	return ret;
}

static int _regmap_read(struct regmap *map, unsigned int reg,
			unsigned int *val)
{
	void *context = _regmap_map_get_context(map);

	if (!regmap_readable(map, reg))
		return VMM_EIO;

	return map->reg_read(context, reg, val);
}

/**
 * regmap_read() - Read a value from a single register
 *
 * @map: Register map to read from
 * @reg: Register to be read from
 * @val: Pointer to store read value
 *
 * A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int regmap_read(struct regmap *map, unsigned int reg, unsigned int *val)
{
	int ret;

	if (!is_aligned(reg, map->reg_stride))
		return VMM_EINVALID;

	map->lock(map->lock_arg);

	ret = _regmap_read(map, reg, val);

	map->unlock(map->lock_arg);

	return ret;
}
VMM_EXPORT_SYMBOL(regmap_read);

/**
 * regmap_raw_read() - Read raw data from the device
 *
 * @map: Register map to read from
 * @reg: First register to be read from
 * @val: Pointer to store read value
 * @val_len: Size of data to read
 *
 * A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int regmap_raw_read(struct regmap *map, unsigned int reg, void *val,
		    size_t val_len)
{
	size_t val_bytes = map->format.val_bytes;
	size_t val_count = udiv64(val_len, val_bytes);
	unsigned int v;
	int ret, i;

	if (!map->bus)
		return VMM_EINVALID;
	if (umod64(val_len, map->format.val_bytes))
		return VMM_EINVALID;
	if (!is_aligned(reg, map->reg_stride))
		return VMM_EINVALID;
	if (val_count == 0)
		return VMM_EINVALID;

	map->lock(map->lock_arg);

	if (regmap_volatile_range(map, reg, val_count)) {
		if (!map->bus->read) {
			ret = VMM_ENOTSUPP;
			goto out;
		}
		if (map->max_raw_read && map->max_raw_read < val_len) {
			ret = VMM_EINVALID;
			goto out;
		}

		/* Physical block read if there's no cache involved */
		ret = _regmap_raw_read(map, reg, val, val_len);

	} else {
		/* Otherwise go word by word for the cache; should be low
		 * cost as we expect to hit the cache.
		 */
		for (i = 0; i < val_count; i++) {
			ret = _regmap_read(map, reg + regmap_get_offset(map, i),
					   &v);
			if (ret != 0)
				goto out;

			map->format.format_val(val + (i * val_bytes), v, 0);
		}
	}

 out:
	map->unlock(map->lock_arg);

	return ret;
}
VMM_EXPORT_SYMBOL(regmap_raw_read);

/**
 * regmap_bulk_read() - Read multiple registers from the device
 *
 * @map: Register map to read from
 * @reg: First register to be read from
 * @val: Pointer to store read value, in native register size for device
 * @val_count: Number of registers to read
 *
 * A value of zero will be returned on success, a negative errno will
 * be returned in error cases.
 */
int regmap_bulk_read(struct regmap *map, unsigned int reg, void *val,
		     size_t val_count)
{
	int ret, i;
	size_t val_bytes = map->format.val_bytes;
	bool vol = regmap_volatile_range(map, reg, val_count);

	if (!is_aligned(reg, map->reg_stride))
		return VMM_EINVALID;

	if (map->bus && map->format.parse_inplace && vol) {
		/*
		 * Some devices does not support bulk read, for
		 * them we have a series of single read operations.
		 */
		size_t total_size = val_bytes * val_count;

		if (!map->use_single_read &&
		    (!map->max_raw_read || map->max_raw_read > total_size)) {
			ret = regmap_raw_read(map, reg, val,
					      val_bytes * val_count);
			if (ret != 0)
				return ret;
		} else {
			/*
			 * Some devices do not support bulk read or do not
			 * support large bulk reads, for them we have a series
			 * of read operations.
			 */
			int chunk_stride = map->reg_stride;
			size_t chunk_size = val_bytes;
			size_t chunk_count = val_count;

			if (!map->use_single_read) {
				chunk_size = map->max_raw_read;
				if (umod64(chunk_size, val_bytes))
					chunk_size -= umod64(chunk_size, val_bytes);
				chunk_count = udiv64(total_size, chunk_size);
				chunk_stride *= udiv64(chunk_size, val_bytes);
			}

			/* Read bytes that fit into a multiple of chunk_size */
			for (i = 0; i < chunk_count; i++) {
				ret = regmap_raw_read(map,
						      reg + (i * chunk_stride),
						      val + (i * chunk_size),
						      chunk_size);
				if (ret != 0)
					return ret;
			}

			/* Read remaining bytes */
			if (chunk_size * i < total_size) {
				ret = regmap_raw_read(map,
						      reg + (i * chunk_stride),
						      val + (i * chunk_size),
						      total_size - i * chunk_size);
				if (ret != 0)
					return ret;
			}
		}

		for (i = 0; i < val_count * val_bytes; i += val_bytes)
			map->format.parse_inplace(val + i);
	} else {
		for (i = 0; i < val_count; i++) {
			unsigned int ival;
			ret = regmap_read(map, reg + regmap_get_offset(map, i),
					  &ival);
			if (ret != 0)
				return ret;

			if (map->format.format_val) {
				map->format.format_val(val + (i * val_bytes), ival, 0);
			} else {
				/* Devices providing read and write
				 * operations can use the bulk I/O
				 * functions if they define a val_bytes,
				 * we assume that the values are native
				 * endian.
				 */
#ifdef CONFIG_64BIT
				u64 *u64 = val;
#endif
				u32 *u32 = val;
				u16 *u16 = val;
				u8 *u8 = val;

				switch (map->format.val_bytes) {
#ifdef CONFIG_64BIT
				case 8:
					u64[i] = ival;
					break;
#endif
				case 4:
					u32[i] = ival;
					break;
				case 2:
					u16[i] = ival;
					break;
				case 1:
					u8[i] = ival;
					break;
				default:
					return VMM_EINVALID;
				}
			}
		}
	}

	return 0;
}
VMM_EXPORT_SYMBOL(regmap_bulk_read);

static int _regmap_update_bits(struct regmap *map, unsigned int reg,
			       unsigned int mask, unsigned int val,
			       bool *change, bool force_write)
{
	int ret;
	unsigned int tmp, orig;

	if (change)
		*change = FALSE;

	if (regmap_volatile(map, reg) && map->reg_update_bits) {
		ret = map->reg_update_bits(map->bus_context, reg, mask, val);
		if (ret == 0 && change)
			*change = TRUE;
	} else {
		ret = _regmap_read(map, reg, &orig);
		if (ret != 0)
			return ret;

		tmp = orig & ~mask;
		tmp |= val & mask;

		if (force_write || (tmp != orig)) {
			ret = _regmap_write(map, reg, tmp);
			if (ret == 0 && change)
				*change = TRUE;
		}
	}

	return ret;
}

/**
 * regmap_update_bits_base() - Perform a read/modify/write cycle on a register
 *
 * @map: Register map to update
 * @reg: Register to update
 * @mask: Bitmask to change
 * @val: New value for bitmask
 * @change: Boolean indicating if a write was done
 * @async: Boolean indicating asynchronously
 * @force: Boolean indicating use force update
 *
 * Perform a read/modify/write cycle on a register map with change, async, force
 * options.
 *
 * If async is true:
 *
 * With most buses the read must be done synchronously so this is most useful
 * for devices with a cache which do not need to interact with the hardware to
 * determine the current register value.
 *
 * Returns zero for success, a negative number on error.
 */
int regmap_update_bits_base(struct regmap *map, unsigned int reg,
			    unsigned int mask, unsigned int val,
			    bool *change, bool async, bool force)
{
	int ret;

	map->lock(map->lock_arg);

	ret = _regmap_update_bits(map, reg, mask, val, change, force);

	map->unlock(map->lock_arg);

	return ret;
}
VMM_EXPORT_SYMBOL(regmap_update_bits_base);

/**
 * regmap_get_val_bytes() - Report the size of a register value
 *
 * @map: Register map to operate on.
 *
 * Report the size of a register value, mainly intended to for use by
 * generic infrastructure built on top of regmap.
 */
int regmap_get_val_bytes(struct regmap *map)
{
	if (map->format.format_write)
		return VMM_EINVALID;

	return map->format.val_bytes;
}
VMM_EXPORT_SYMBOL(regmap_get_val_bytes);

/**
 * regmap_get_max_register() - Report the max register value
 *
 * @map: Register map to operate on.
 *
 * Report the max register value, mainly intended to for use by
 * generic infrastructure built on top of regmap.
 */
int regmap_get_max_register(struct regmap *map)
{
	return map->max_register ? map->max_register : VMM_EINVALID;
}
VMM_EXPORT_SYMBOL(regmap_get_max_register);

/**
 * regmap_get_reg_stride() - Report the register address stride
 *
 * @map: Register map to operate on.
 *
 * Report the register address stride, mainly intended to for use by
 * generic infrastructure built on top of regmap.
 */
int regmap_get_reg_stride(struct regmap *map)
{
	return map->reg_stride;
}
VMM_EXPORT_SYMBOL(regmap_get_reg_stride);

int regmap_parse_val(struct regmap *map, const void *buf,
			unsigned int *val)
{
	if (!map->format.parse_val)
		return VMM_EINVALID;

	*val = map->format.parse_val(buf);

	return 0;
}
VMM_EXPORT_SYMBOL(regmap_parse_val);
