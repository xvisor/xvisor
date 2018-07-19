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
 * @file regmap.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Regmap framework header.
 *
 * The source has been largely adapted from Linux
 * include/linux/regmap.h
 *
 * Register map access API
 *
 * Copyright 2011 Wolfson Microelectronics plc
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * The original code is licensed under the GPL.
 */

#ifndef __REGMAP_H_
#define __REGMAP_H_

#include <vmm_error.h>
#include <vmm_types.h>

struct vmm_device;
struct vmm_devtree_node;
struct regmap;

#define	regmap_update_bits(map, reg, mask, val) \
	regmap_update_bits_base(map, reg, mask, val, NULL, false, false)

#define	regmap_write_bits(map, reg, mask, val) \
	regmap_update_bits_base(map, reg, mask, val, NULL, false, true)

#ifdef CONFIG_REGMAP

enum regmap_endian {
	/* Unspecified -> 0 -> Backwards compatible default */
	REGMAP_ENDIAN_DEFAULT = 0,
	REGMAP_ENDIAN_BIG,
	REGMAP_ENDIAN_LITTLE,
	REGMAP_ENDIAN_NATIVE,
};

/**
 * struct regmap_range - A register range, used for access related checks
 *                       (readable/writeable/volatile/precious checks)
 *
 * @range_min: address of first register
 * @range_max: address of last register
 */
struct regmap_range {
	unsigned int range_min;
	unsigned int range_max;
};

#define regmap_reg_range(low, high) { .range_min = low, .range_max = high, }

/**
 * struct regmap_access_table - A table of register ranges for access checks
 *
 * @yes_ranges : pointer to an array of regmap ranges used as "yes ranges"
 * @n_yes_ranges: size of the above array
 * @no_ranges: pointer to an array of regmap ranges used as "no ranges"
 * @n_no_ranges: size of the above array
 *
 * A table of ranges including some yes ranges and some no ranges.
 * If a register belongs to a no_range, the corresponding check function
 * will return false. If a register belongs to a yes range, the corresponding
 * check function will return true. "no_ranges" are searched first.
 */
struct regmap_access_table {
	const struct regmap_range *yes_ranges;
	unsigned int n_yes_ranges;
	const struct regmap_range *no_ranges;
	unsigned int n_no_ranges;
};

typedef void (*regmap_lock)(void *);
typedef void (*regmap_unlock)(void *);

/**
 * struct regmap_config - Configuration for the register map of a device.
 *
 * @name: Optional name of the regmap. Useful when a device has multiple
 *        register regions.
 *
 * @reg_bits: Number of bits in a register address, mandatory.
 * @reg_stride: The register address stride. Valid register addresses are a
 *              multiple of this value. If set to 0, a value of 1 will be
 *              used.
 * @pad_bits: Number of bits of padding between register and value.
 * @val_bits: Number of bits in a register value, mandatory.
 *
 * @lock:	  Optional lock callback (overrides regmap's default lock
 *		  function, based on spinlock or mutex).
 * @unlock:	  As above for unlocking.
 * @lock_arg:	  this field is passed as the only argument of lock/unlock
 *		  functions (ignored in case regular lock/unlock functions
 *		  are not overridden).
 * @reg_read:	  Optional callback that if filled will be used to perform
 *           	  all the reads from the registers. Should only be provided for
 *		  devices whose read operation cannot be represented as a simple
 *		  read operation on a bus such as SPI, I2C, etc. Most of the
 *		  devices do not need this.
 * @reg_write:	  Same as above for writing.
 * @fast_io:	  Register IO is fast. Use a spinlock instead of a mutex
 *	     	  to perform locking. This field is ignored if custom lock/unlock
 *	     	  functions are used (see fields lock/unlock of struct regmap_config).
 *		  This field is a duplicate of a similar file in
 *		  'struct regmap_bus' and serves exact same purpose.
 *		   Use it only for "no-bus" cases.
 * @max_register: Optional, specifies the maximum valid register address.
 */
struct regmap_config {
	const char *name;

	int reg_bits;
	int reg_stride;
	int pad_bits;
	int val_bits;

	bool (*writeable_reg)(struct vmm_device *dev, unsigned int reg);
	bool (*readable_reg)(struct vmm_device *dev, unsigned int reg);
	bool (*volatile_reg)(struct vmm_device *dev, unsigned int reg);
	bool (*precious_reg)(struct vmm_device *dev, unsigned int reg);
	regmap_lock lock;
	regmap_unlock unlock;
	void *lock_arg;

	int (*reg_read)(void *context, unsigned int reg, unsigned int *val);
	int (*reg_write)(void *context, unsigned int reg, unsigned int val);

	bool fast_io;

	unsigned int max_register;
	const struct regmap_access_table *wr_table;
	const struct regmap_access_table *rd_table;
	const struct regmap_access_table *volatile_table;
	const struct regmap_access_table *precious_table;

	unsigned long read_flag_mask;
	unsigned long write_flag_mask;

	bool use_single_rw;
	bool can_multi_write;

	enum regmap_endian reg_format_endian;
	enum regmap_endian val_format_endian;
};

typedef int (*regmap_hw_write)(void *context, const void *data,
			       size_t count);
typedef int (*regmap_hw_gather_write)(void *context,
				      const void *reg, size_t reg_len,
				      const void *val, size_t val_len);
typedef int (*regmap_hw_read)(void *context,
			      const void *reg_buf, size_t reg_size,
			      void *val_buf, size_t val_size);
typedef int (*regmap_hw_reg_read)(void *context, unsigned int reg,
				  unsigned int *val);
typedef int (*regmap_hw_reg_write)(void *context, unsigned int reg,
				   unsigned int val);
typedef int (*regmap_hw_reg_update_bits)(void *context, unsigned int reg,
					 unsigned int mask, unsigned int val);
typedef void (*regmap_hw_free_context)(void *context);

/**
 * struct regmap_bus - Description of a hardware bus for the register map
 *                     infrastructure.
 *
 * @fast_io: Register IO is fast. Use a spinlock instead of a mutex
 *	     to perform locking. This field is ignored if custom lock/unlock
 *	     functions are used (see fields lock/unlock of
 *	     struct regmap_config).
 * @write: Write operation.
 * @gather_write: Write operation with split register/value, return VMM_ENOTSUPP
 *                if not implemented  on a given device.
 * @read: Read operation.  Data is returned in the buffer used to transmit
 *         data.
 * @reg_write: Write a single register value to the given register address. This
 *             write operation has to complete when returning from the function.
 * @reg_update_bits: Update bits operation to be used against volatile
 *                   registers, intended for devices supporting some mechanism
 *                   for setting clearing bits without having to
 *                   read/modify/write.
 * @reg_read: Read a single register value from a given register address.
 * @free_context: Free context.
 * @reg_format_endian_default: Default endianness for formatted register
 *     addresses. Used when the regmap_config specifies DEFAULT. If this is
 *     DEFAULT, BIG is assumed.
 * @val_format_endian_default: Default endianness for formatted register
 *     values. Used when the regmap_config specifies DEFAULT. If this is
 *     DEFAULT, BIG is assumed.
 * @max_raw_read: Max raw read size that can be used on the bus.
 * @max_raw_write: Max raw write size that can be used on the bus.
 */
struct regmap_bus {
	bool fast_io;
	regmap_hw_write write;
	regmap_hw_gather_write gather_write;
	regmap_hw_read read;
	regmap_hw_reg_write reg_write;
	regmap_hw_reg_read reg_read;
	regmap_hw_reg_update_bits reg_update_bits;
	regmap_hw_free_context free_context;
	u8 read_flag_mask;
	enum regmap_endian reg_format_endian_default;
	enum regmap_endian val_format_endian_default;
	size_t max_raw_read;
	size_t max_raw_write;
};

/*
 * __regmap_init functions.
 *
 * These functions take a lock key and name parameter, and should not be called
 * directly. Instead, use the regmap_init macros that generate a key and name
 * for each call.
 */
struct regmap *__regmap_init(struct vmm_device *dev,
			     const struct regmap_bus *bus,
			     void *bus_context,
			     const struct regmap_config *config);
struct regmap *__regmap_init_mmio_clk(struct vmm_device *dev,
				      const char *clk_id,
				      void *regs,
				      const struct regmap_config *config);
struct regmap *__devm_regmap_init(struct vmm_device *dev,
				  const struct regmap_bus *bus,
				  void *bus_context,
				  const struct regmap_config *config);
struct regmap *__devm_regmap_init_mmio_clk(struct vmm_device *dev,
					   const char *clk_id,
					   void *regs,
					   const struct regmap_config *config);

/**
 * regmap_init() - Initialise register map
 *
 * @dev: Device that will be interacted with
 * @bus: Bus-specific callbacks to use with device
 * @bus_context: Data passed to bus-specific callbacks
 * @config: Configuration for register map
 *
 * The return value will be an VMM_ERR_PTR() on error or a valid pointer to
 * a struct regmap.  This function should generally not be called
 * directly, it should be called by bus-specific init functions.
 */
#define regmap_init(dev, bus, bus_context, config)			\
	__regmap_init(dev, bus, bus_context, config)
int regmap_attach_dev(struct vmm_device *dev, struct regmap *map,
		      const struct regmap_config *config);

/**
 * regmap_init_mmio_clk() - Initialise register map with register clock
 *
 * @dev: Device that will be interacted with
 * @clk_id: register clock consumer ID
 * @regs: Pointer to memory-mapped IO region
 * @config: Configuration for register map
 *
 * The return value will be an VMM_ERR_PTR() on error or a valid pointer to
 * a struct regmap.
 */
#define regmap_init_mmio_clk(dev, clk_id, regs, config)			\
	__regmap_init_mmio_clk(dev, clk_id, regs, config)

/**
 * regmap_init_mmio() - Initialise register map
 *
 * @dev: Device that will be interacted with
 * @regs: Pointer to memory-mapped IO region
 * @config: Configuration for register map
 *
 * The return value will be an VMM_ERR_PTR() on error or a valid pointer to
 * a struct regmap.
 */
#define regmap_init_mmio(dev, regs, config)		\
	regmap_init_mmio_clk(dev, NULL, regs, config)

/**
 * devm_regmap_init() - Initialise managed register map
 *
 * @dev: Device that will be interacted with
 * @bus: Bus-specific callbacks to use with device
 * @bus_context: Data passed to bus-specific callbacks
 * @config: Configuration for register map
 *
 * The return value will be an VMM_ERR_PTR() on error or a valid pointer
 * to a struct regmap.  This function should generally not be called
 * directly, it should be called by bus-specific init functions.  The
 * map will be automatically freed by the device management code.
 */
#define devm_regmap_init(dev, bus, bus_context, config)			\
	__devm_regmap_init(dev, bus, bus_context, config)

/**
 * devm_regmap_init_mmio_clk() - Initialise managed register map with clock
 *
 * @dev: Device that will be interacted with
 * @clk_id: register clock consumer ID
 * @regs: Pointer to memory-mapped IO region
 * @config: Configuration for register map
 *
 * The return value will be an VMM_ERR_PTR() on error or a valid pointer
 * to a struct regmap.  The regmap will be automatically freed by the
 * device management code.
 */
#define devm_regmap_init_mmio_clk(dev, clk_id, regs, config)		\
	__devm_regmap_init_mmio_clk(dev, clk_id, regs, config)

/**
 * devm_regmap_init_mmio() - Initialise managed register map
 *
 * @dev: Device that will be interacted with
 * @regs: Pointer to memory-mapped IO region
 * @config: Configuration for register map
 *
 * The return value will be an VMM_ERR_PTR() on error or a valid pointer
 * to a struct regmap.  The regmap will be automatically freed by the
 * device management code.
 */
#define devm_regmap_init_mmio(dev, regs, config)		\
	devm_regmap_init_mmio_clk(dev, NULL, regs, config)


void regmap_exit(struct regmap *map);

struct regmap *dev_get_regmap(struct vmm_device *dev, const char *name);

struct vmm_device *regmap_get_device(struct regmap *map);

int regmap_write(struct regmap *map, unsigned int reg, unsigned int val);

int regmap_raw_write(struct regmap *map, unsigned int reg,
		     const void *val, size_t val_len);

int regmap_bulk_write(struct regmap *map, unsigned int reg, const void *val,
			size_t val_count);

int regmap_read(struct regmap *map, unsigned int reg, unsigned int *val);

int regmap_raw_read(struct regmap *map, unsigned int reg,
		    void *val, size_t val_len);

int regmap_bulk_read(struct regmap *map, unsigned int reg, void *val,
		     size_t val_count);

int regmap_update_bits_base(struct regmap *map, unsigned int reg,
			    unsigned int mask, unsigned int val,
			    bool *change, bool async, bool force);

int regmap_get_val_bytes(struct regmap *map);

int regmap_get_max_register(struct regmap *map);

int regmap_get_reg_stride(struct regmap *map);

bool regmap_can_raw_write(struct regmap *map);
size_t regmap_get_raw_read_max(struct regmap *map);
size_t regmap_get_raw_write_max(struct regmap *map);

int regmap_parse_val(struct regmap *map, const void *buf,
				unsigned int *val);

static inline bool regmap_reg_in_range(unsigned int reg,
				       const struct regmap_range *range)
{
	return reg >= range->range_min && reg <= range->range_max;
}

bool regmap_reg_in_ranges(unsigned int reg,
			  const struct regmap_range *ranges,
			  unsigned int nranges);

#else

/*
 * These stubs should only ever be called by generic code which has
 * regmap based facilities, if they ever get called at runtime
 * something is going wrong and something probably needs to select
 * REGMAP.
 */

static inline int regmap_write(struct regmap *map, unsigned int reg,
			       unsigned int val)
{
	WARN_ON(1);
	return VMM_EINVALID;
}

static inline int regmap_raw_write(struct regmap *map, unsigned int reg,
				   const void *val, size_t val_len)
{
	WARN_ON(1);
	return VMM_EINVALID;
}

static inline int regmap_bulk_write(struct regmap *map, unsigned int reg,
				    const void *val, size_t val_count)
{
	WARN_ON(1);
	return VMM_EINVALID;
}

static inline int regmap_read(struct regmap *map, unsigned int reg,
			      unsigned int *val)
{
	WARN_ON(1);
	return VMM_EINVALID;
}

static inline int regmap_raw_read(struct regmap *map, unsigned int reg,
				  void *val, size_t val_len)
{
	WARN_ON(1);
	return VMM_EINVALID;
}

static inline int regmap_bulk_read(struct regmap *map, unsigned int reg,
				   void *val, size_t val_count)
{
	WARN_ON(1);
	return VMM_EINVALID;
}

static inline int regmap_update_bits_base(struct regmap *map, unsigned int reg,
					  unsigned int mask, unsigned int val,
					  bool *change, bool async, bool force)
{
	WARN_ON(1);
	return VMM_EINVALID;
}

static inline int regmap_get_val_bytes(struct regmap *map)
{
	WARN_ON(1);
	return VMM_EINVALID;
}

static inline int regmap_get_max_register(struct regmap *map)
{
	WARN_ON(1);
	return VMM_EINVALID;
}

static inline int regmap_get_reg_stride(struct regmap *map)
{
	WARN_ON(1);
	return VMM_EINVALID;
}

static inline int regmap_parse_val(struct regmap *map, const void *buf,
				unsigned int *val)
{
	WARN_ON(1);
	return VMM_EINVALID;
}

#endif

#endif /* __REGMAP_H_ */
