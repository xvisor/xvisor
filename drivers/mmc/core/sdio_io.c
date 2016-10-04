/**
 * Copyright (c) 2016 Pramod Kanni.
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
 * @file sdio_io.c
 * @author Pramod Kanni (kanni.pramod@gmail.com)
 * @brief SDIO interface input-output operations
 *
 * The source has been largely adapted from linux:
 * drivers/mmc/core/sdio_io.c & drivers/mmc/core/sdio_ops.c
 *
 * Copyright 2006-2007, 2007-2008 Pierre Ossman
 *
 * This linux code was extracted from:
 * git://github.com/raspberrypi/linux.git master
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <vmm_delay.h>
#include <vmm_timer.h>
#include <vmm_host_io.h>
#include <vmm_modules.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <linux/jiffies.h>
#include <linux/printk.h>

#include <drv/mmc/mmc_core.h>
#include <drv/mmc/sdio.h>
#include <drv/mmc/sdio_func.h>
#include <drv/mmc/sdio_ids.h>

#include "core.h"
#include "sdio_io.h"

/*
 *  Debugging related defines
 */
#undef CONFIG_MMC_TRACE

#undef DEBUG

#ifdef DEBUG
#define DPRINTF(msg...)			vmm_printf(msg)
#else
#define DPRINTF(msg...)
#endif

static int mmc_io_rw_direct_host(struct mmc_host *host, int write, unsigned fn,
	unsigned addr, u8 in, u8 *out)
{
	struct mmc_cmd cmd = {0};
	int err;

	BUG_ON(!host);
	BUG_ON(fn > 7);

#ifdef CONFIG_MMC_TRACE
	DPRINTF("%s : write=%d fn=%d addr=%x in=0x%x\n", __func__, write, fn, addr, in);
#endif

	/* sanity check */
	if (addr & ~0x1FFFF) {
		return VMM_EINVALID;
	}

	cmd.cmdidx = SD_IO_RW_DIRECT;
	cmd.cmdarg = write ? 0x80000000 : 0x00000000;
	cmd.cmdarg |= fn << 28;
	cmd.cmdarg |= (write && out) ? 0x08000000 : 0x00000000;
	cmd.cmdarg |= addr << 9;
	cmd.cmdarg |= in;
	cmd.resp_type = MMC_RSP_R1;

	err = mmc_send_cmd(host, &cmd, NULL);
	if (err) {
		return err;
	}

	if (mmc_host_is_spi(host)) {
		/* host driver already reported errors */
	} else {
		if (cmd.response[0] & R5_ERROR) {
			DPRINTF("%s : error R5_ERROR\n", __func__);
			err = VMM_EIO;
			goto finish;
		}
		if (cmd.response[0] & R5_FUNCTION_NUMBER) {
			DPRINTF("%s : error R5_FUNCTION_NUMBER\n", __func__);
			err = VMM_EINVALID;
			goto finish;
		}
		if (cmd.response[0] & R5_OUT_OF_RANGE) {
			DPRINTF("%s : error R5_OUT_OF_RANGE\n", __func__);
			err = VMM_ERANGE;
			goto finish;
		}
	}

	if (out) {
		if (mmc_host_is_spi(host))
			*out = (cmd.response[0] >> 8) & 0xFF;
		else
			*out = cmd.response[0] & 0xFF;
	}

finish:
	return err;
}

int mmc_io_rw_direct(struct mmc_card *card, int write, unsigned fn,
	unsigned addr, u8 in, u8 *out)
{
	BUG_ON(!card);
	return mmc_io_rw_direct_host(card->host, write, fn, addr, in, out);
}

int mmc_io_rw_extended(struct mmc_card *card, int write, unsigned fn,
	unsigned addr, int incr_addr, u8 *buf, unsigned blocks, unsigned blksz)
{
	struct mmc_cmd cmd = {0};
	struct mmc_data data = {{0}};
	int timeout = 1000;
	int err;

	BUG_ON(!card);
	BUG_ON(fn > 7);
	WARN_ON(blksz == 0);

	/* sanity check */
	if (addr & ~0x1FFFF) {
		return VMM_EINVALID;
	}

#ifdef CONFIG_MMC_TRACE
	DPRINTF("%s: write=%d fn=%d start=0x%x blocks=%d blocksize=%d\n",
				__func__, write, fn, addr, blocks, blksz);
#endif

	cmd.cmdidx = SD_IO_RW_EXTENDED;
	cmd.cmdarg = write ? 0x80000000 : 0x00000000;
	cmd.cmdarg |= fn << 28;
	cmd.cmdarg |= incr_addr ? 0x04000000 : 0x00000000;
	cmd.cmdarg |= addr << 9;
	if (blocks == 0)
		cmd.cmdarg |= (blksz == 512) ? 0 : blksz;	/* byte mode */
	else
		cmd.cmdarg |= 0x08000000 | blocks;		/* block mode */
	cmd.resp_type = MMC_RSP_R1;

	if (write) {
		data.src = buf;
	} else {
		data.dest = buf;
	}
	data.blocksize = blksz;
	/* Code in host drivers/fwk assumes that "blocks" always is >=1 */
	data.blocks = blocks ? blocks : 1;
	data.flags = write ? MMC_DATA_WRITE : MMC_DATA_READ;

	err = mmc_send_cmd(card->host, &cmd, &data);
	if (err) {
		DPRINTF("%s : flags=%s blocks=%d blksize=%d err=%d\n",
					__func__, write ? "WRITE" : "READ",
					blocks, blksz, err);
	}

	if (mmc_host_is_spi(card->host)) {
		/* host driver already reported errors */
	} else {
		if (cmd.response[0] & R5_ERROR) {
			DPRINTF("%s : error=R5_ERROR\n", __func__);
			err = VMM_EIO;
		}
		if (cmd.response[0] & R5_FUNCTION_NUMBER) {
			DPRINTF("%s : error=R5_FUNCTION_NUMBER\n", __func__);
			err = VMM_EINVALID;
		}
		if (cmd.response[0] & R5_OUT_OF_RANGE) {
			DPRINTF("%s : error=R5_OUT_OF_RANGE\n", __func__);
			err = VMM_ERANGE;
		}
	}

	/* SPI multiblock writes terminate using a special
	 * token, not a STOP_TRANSMISSION request.
	 */
	if (!(card->host->caps2 & MMC_CAP2_AUTO_CMD12) && !mmc_host_is_spi(card->host) &&
	    blocks > 1) {
		cmd.cmdidx = MMC_CMD_STOP_TRANSMISSION;
		cmd.cmdarg = 0;
		cmd.resp_type = MMC_RSP_R1b;
		err |= mmc_send_cmd(card->host, &cmd, NULL);
		if (err) {
			DPRINTF("%s : CMD12 flags=%s" \
					" blocks=%d blocksize=%d err=%d\n", __func__,
					write ? "WRITE" : "READ", blocks, blksz, err);
		}
		else {
			goto finish;
		}
	}

	/* Waiting for the ready status */
	err |= mmc_send_status(card->host, card, timeout);
	if (err) {
		DPRINTF("%s : mmc_send_status : flags=%s" \
				" blocks=%d blocksize=%d err=%d\n", __func__,
				write ? "WRITE" : "READ", blocks, blksz, err);
	}

finish:
	return err;
}

int sdio_reset(struct mmc_host *host)
{
	int ret;
	u8 abort;

	/* SDIO Simplified Specification V2.0, 4.4 Reset for SDIO */

	ret = mmc_io_rw_direct_host(host, 0, 0, SDIO_CCCR_ABORT, 0, &abort);
	if (ret)
		abort = 0x08;
	else
		abort |= 0x08;

	ret = mmc_io_rw_direct_host(host, 1, 0, SDIO_CCCR_ABORT, abort, NULL);
	return ret;
}

/**
 *	sdio_enable_func - enables a SDIO function for usage
 *	@func: SDIO function to enable
 *
 *	Powers up and activates a SDIO function so that register
 *	access is possible.
 */
int sdio_enable_func(struct sdio_func *func)
{
	int ret;
	unsigned char reg;
	unsigned long timeout;

	BUG_ON(!func);
	BUG_ON(!func->card);

	DPRINTF("SDIO: Enabling device %s...\n", sdio_func_id(func));

	ret = mmc_io_rw_direct(func->card, 0, 0, SDIO_CCCR_IOEx, 0, &reg);
	if (ret)
		goto err;

	reg |= 1 << func->num;

	ret = mmc_io_rw_direct(func->card, 1, 0, SDIO_CCCR_IOEx, reg, NULL);
	if (ret)
		goto err;

	timeout = jiffies + msecs_to_jiffies(func->enable_timeout);

	while (1) {
		ret = mmc_io_rw_direct(func->card, 0, 0, SDIO_CCCR_IORx, 0, &reg);
		if (ret)
			goto err;
		if (reg & (1 << func->num))
			break;
		ret = VMM_ETIME;
		if (time_after(jiffies, timeout))
			goto err;
	}

	DPRINTF("SDIO: Enabled device %s\n", sdio_func_id(func));

	return 0;

err:
	DPRINTF("SDIO: Failed to enable device %s\n", sdio_func_id(func));
	return ret;
}
VMM_EXPORT_SYMBOL(sdio_enable_func);

/**
 *	sdio_disable_func - disable a SDIO function
 *	@func: SDIO function to disable
 *
 *	Powers down and deactivates a SDIO function. Register access
 *	to this function will fail until the function is reenabled.
 */
int sdio_disable_func(struct sdio_func *func)
{
	int ret;
	unsigned char reg;

	BUG_ON(!func);
	BUG_ON(!func->card);

	pr_debug("SDIO: Disabling device %s...\n", sdio_func_id(func));

	ret = mmc_io_rw_direct(func->card, 0, 0, SDIO_CCCR_IOEx, 0, &reg);
	if (ret)
		goto err;

	reg &= ~(1 << func->num);

	ret = mmc_io_rw_direct(func->card, 1, 0, SDIO_CCCR_IOEx, reg, NULL);
	if (ret)
		goto err;

	pr_debug("SDIO: Disabled device %s\n", sdio_func_id(func));

	return 0;

err:
	pr_debug("SDIO: Failed to disable device %s\n", sdio_func_id(func));
	return VMM_EIO;
}
VMM_EXPORT_SYMBOL(sdio_disable_func);

/**
 *	sdio_set_block_size - set the block size of an SDIO function
 *	@func: SDIO function to change
 *	@blksz: new block size or 0 to use the default.
 *
 *	The default block size is the largest supported by both the function
 *	and the host, with a maximum of 512 to ensure that arbitrarily sized
 *	data transfer use the optimal (least) number of commands.
 *
 *	A driver may call this to override the default block size set by the
 *	core. This can be used to set a block size greater than the maximum
 *	that reported by the card; it is the driver's responsibility to ensure
 *	it uses a value that the card supports.
 *
 *	Returns 0 on success, -EINVAL if the host does not support the
 *	requested block size, or -EIO (etc.) if one of the resultant FBR block
 *	size register writes failed.
 *
 */
int sdio_set_block_size(struct sdio_func *func, unsigned blksz)
{
	int ret;

	if (blksz > func->card->host->max_blk_size)
		return VMM_EINVALID;

	if (blksz == 0) {
		blksz = min(func->max_blksize, func->card->host->max_blk_size);
		blksz = min(blksz, 512u);
	}

	ret = mmc_io_rw_direct(func->card, 1, 0,
		SDIO_FBR_BASE(func->num) + SDIO_FBR_BLKSIZE,
		blksz & 0xff, NULL);
	if (ret)
		return ret;
	ret = mmc_io_rw_direct(func->card, 1, 0,
		SDIO_FBR_BASE(func->num) + SDIO_FBR_BLKSIZE + 1,
		(blksz >> 8) & 0xff, NULL);
	if (ret)
		return ret;
	func->cur_blksize = blksz;
	return 0;
}
VMM_EXPORT_SYMBOL(sdio_set_block_size);

/*
 * Calculate the maximum byte mode transfer size
 */
static inline unsigned int sdio_max_byte_size(struct sdio_func *func)
{
	unsigned mval =	func->card->host->max_blk_size;

	if (func->card->quirks & MMC_QUIRK_BLKSZ_FOR_BYTE_MODE)
		mval = min(mval, func->cur_blksize);
	else
		mval = min(mval, func->max_blksize);

	if (func->card->quirks & MMC_QUIRK_BROKEN_BYTE_MODE_512)
		return min(mval, 511u);

	return min(mval, 512u); /* maximum size for byte mode */
}

/**
 *	sdio_align_size - pads a transfer size to a more optimal value
 *	@func: SDIO function
 *	@sz: original transfer size
 *
 *	Pads the original data size with a number of extra bytes in
 *	order to avoid controller bugs and/or performance hits
 *	(e.g. some controllers revert to PIO for certain sizes).
 *
 *	If possible, it will also adjust the size so that it can be
 *	handled in just a single request.
 *
 *	Returns the improved size, which might be unmodified.
 */
unsigned int sdio_align_size(struct sdio_func *func, unsigned int sz)
{
	unsigned int orig_sz;
	unsigned int blk_sz, byte_sz;
	unsigned chunk_sz;

	orig_sz = sz;

	/*
	 * Do a first check with the controller, in case it
	 * wants to increase the size up to a point where it
	 * might need more than one block.
	 */
	sz = mmc_align_data_size(func->card, sz);

	/*
	 * If we can still do this with just a byte transfer, then
	 * we're done.
	 */
	if (sz <= sdio_max_byte_size(func))
		return sz;

	if (func->card->cccr.multi_block) {
		/*
		 * Check if the transfer is already block aligned
		 */
		if (umod32(sz, func->cur_blksize) == 0)
			return sz;

		/*
		 * Realign it so that it can be done with one request,
		 * and recheck if the controller still likes it.
		 */
		blk_sz = udiv32((sz + func->cur_blksize - 1),
			func->cur_blksize) * func->cur_blksize;
		blk_sz = mmc_align_data_size(func->card, blk_sz);

		/*
		 * This value is only good if it is still just
		 * one request.
		 */
		if (umod32(blk_sz, func->cur_blksize) == 0)
			return blk_sz;

		/*
		 * We failed to do one request, but at least try to
		 * pad the remainder properly.
		 */
		byte_sz = mmc_align_data_size(func->card,
				umod32(sz, func->cur_blksize));
		if (byte_sz <= sdio_max_byte_size(func)) {
			blk_sz = udiv32(sz, func->cur_blksize);
			return blk_sz * func->cur_blksize + byte_sz;
		}
	} else {
		/*
		 * We need multiple requests, so first check that the
		 * controller can handle the chunk size;
		 */
		chunk_sz = mmc_align_data_size(func->card,
				sdio_max_byte_size(func));
		if (chunk_sz == sdio_max_byte_size(func)) {
			/*
			 * Fix up the size of the remainder (if any)
			 */
			byte_sz = umod32(orig_sz, chunk_sz);
			if (byte_sz) {
				byte_sz = mmc_align_data_size(func->card,
						byte_sz);
			}

			return udiv32(orig_sz, chunk_sz) * chunk_sz + byte_sz;
		}
	}

	/*
	 * The controller is simply incapable of transferring the size
	 * we want in decent manner, so just return the original size.
	 */
	return orig_sz;
}
VMM_EXPORT_SYMBOL(sdio_align_size);

/* Split an arbitrarily sized data transfer into several
 * IO_RW_EXTENDED commands. */
static int sdio_io_rw_ext_helper(struct sdio_func *func, int write,
	unsigned addr, int incr_addr, u8 *buf, unsigned size)
{
	unsigned remainder = size;
	unsigned max_blocks;
	int ret;

	/* Do the bulk of the transfer using block mode (if supported). */
	if (func->card->cccr.multi_block && (size > sdio_max_byte_size(func))) {
		/* Blocks per command is limited by host count, host transfer
		 * size and the maximum for IO_RW_EXTENDED of 511 blocks. */
		max_blocks = min(func->card->host->max_blk_count, 511u);

		while (remainder >= func->cur_blksize) {
			unsigned blocks;

			blocks = udiv32(remainder, func->cur_blksize);
			if (blocks > max_blocks)
				blocks = max_blocks;
			size = blocks * func->cur_blksize;

			ret = mmc_io_rw_extended(func->card, write,
				func->num, addr, incr_addr, buf,
				blocks, func->cur_blksize);
			if (ret)
				return ret;

			remainder -= size;
			buf += size;
			if (incr_addr)
				addr += size;
		}
	}

	/* Write the remainder using byte mode. */
	while (remainder > 0) {
		size = min(remainder, sdio_max_byte_size(func));

		/* Indicate byte mode by setting "blocks" = 0 */
		ret = mmc_io_rw_extended(func->card, write, func->num, addr,
			 incr_addr, buf, 0, size);
		if (ret)
			return ret;

		remainder -= size;
		buf += size;
		if (incr_addr)
			addr += size;
	}
	return 0;
}

/**
 *	sdio_readb - read a single byte from a SDIO function
 *	@func: SDIO function to access
 *	@addr: address to read
 *	@err_ret: optional status value from transfer
 *
 *	Reads a single byte from the address space of a given SDIO
 *	function. If there is a problem reading the address, 0xff
 *	is returned and @err_ret will contain the error code.
 */
u8 sdio_readb(struct sdio_func *func, unsigned int addr, int *err_ret)
{
	int ret;
	u8 val;

	BUG_ON(!func);

	if (err_ret)
		*err_ret = 0;

	ret = mmc_io_rw_direct(func->card, 0, func->num, addr, 0, &val);
	if (ret) {
		if (err_ret)
			*err_ret = ret;
		return 0xFF;
	}

	return val;
}
VMM_EXPORT_SYMBOL(sdio_readb);

/**
 *	sdio_writeb - write a single byte to a SDIO function
 *	@func: SDIO function to access
 *	@b: byte to write
 *	@addr: address to write to
 *	@err_ret: optional status value from transfer
 *
 *	Writes a single byte to the address space of a given SDIO
 *	function. @err_ret will contain the status of the actual
 *	transfer.
 */
void sdio_writeb(struct sdio_func *func, u8 b, unsigned int addr, int *err_ret)
{
	int ret;

	BUG_ON(!func);

	ret = mmc_io_rw_direct(func->card, 1, func->num, addr, b, NULL);
	if (err_ret)
		*err_ret = ret;
}
VMM_EXPORT_SYMBOL(sdio_writeb);

/**
 *	sdio_writeb_readb - write and read a byte from SDIO function
 *	@func: SDIO function to access
 *	@write_byte: byte to write
 *	@addr: address to write to
 *	@err_ret: optional status value from transfer
 *
 *	Performs a RAW (Read after Write) operation as defined by SDIO spec -
 *	single byte is written to address space of a given SDIO function and
 *	response is read back from the same address, both using single request.
 *	If there is a problem with the operation, 0xff is returned and
 *	@err_ret will contain the error code.
 */
u8 sdio_writeb_readb(struct sdio_func *func, u8 write_byte,
	unsigned int addr, int *err_ret)
{
	int ret;
	u8 val;

	ret = mmc_io_rw_direct(func->card, 1, func->num, addr,
			write_byte, &val);
	if (err_ret)
		*err_ret = ret;
	if (ret)
		val = 0xff;

	return val;
}
VMM_EXPORT_SYMBOL(sdio_writeb_readb);

/**
 *	sdio_memcpy_fromio - read a chunk of memory from a SDIO function
 *	@func: SDIO function to access
 *	@dst: buffer to store the data
 *	@addr: address to begin reading from
 *	@count: number of bytes to read
 *
 *	Reads from the address space of a given SDIO function. Return
 *	value indicates if the transfer succeeded or not.
 */
int sdio_memcpy_fromio(struct sdio_func *func, void *dst,
	unsigned int addr, int count)
{
	return sdio_io_rw_ext_helper(func, 0, addr, 1, dst, count);
}
VMM_EXPORT_SYMBOL(sdio_memcpy_fromio);

/**
 *	sdio_memcpy_toio - write a chunk of memory to a SDIO function
 *	@func: SDIO function to access
 *	@addr: address to start writing to
 *	@src: buffer that contains the data to write
 *	@count: number of bytes to write
 *
 *	Writes to the address space of a given SDIO function. Return
 *	value indicates if the transfer succeeded or not.
 */
int sdio_memcpy_toio(struct sdio_func *func, unsigned int addr,
	void *src, int count)
{
	return sdio_io_rw_ext_helper(func, 1, addr, 1, src, count);
}
VMM_EXPORT_SYMBOL(sdio_memcpy_toio);

/**
 *	sdio_readsb - read from a FIFO on a SDIO function
 *	@func: SDIO function to access
 *	@dst: buffer to store the data
 *	@addr: address of (single byte) FIFO
 *	@count: number of bytes to read
 *
 *	Reads from the specified FIFO of a given SDIO function. Return
 *	value indicates if the transfer succeeded or not.
 */
int sdio_readsb(struct sdio_func *func, void *dst, unsigned int addr,
	int count)
{
	return sdio_io_rw_ext_helper(func, 0, addr, 0, dst, count);
}
VMM_EXPORT_SYMBOL(sdio_readsb);

/**
 *	sdio_writesb - write to a FIFO of a SDIO function
 *	@func: SDIO function to access
 *	@addr: address of (single byte) FIFO
 *	@src: buffer that contains the data to write
 *	@count: number of bytes to write
 *
 *	Writes to the specified FIFO of a given SDIO function. Return
 *	value indicates if the transfer succeeded or not.
 */
int sdio_writesb(struct sdio_func *func, unsigned int addr, void *src,
	int count)
{
	return sdio_io_rw_ext_helper(func, 1, addr, 0, src, count);
}
VMM_EXPORT_SYMBOL(sdio_writesb);

/**
 *	sdio_readw - read a 16 bit integer from a SDIO function
 *	@func: SDIO function to access
 *	@addr: address to read
 *	@err_ret: optional status value from transfer
 *
 *	Reads a 16 bit integer from the address space of a given SDIO
 *	function. If there is a problem reading the address, 0xffff
 *	is returned and @err_ret will contain the error code.
 */
u16 sdio_readw(struct sdio_func *func, unsigned int addr, int *err_ret)
{
	int ret;
	u16 val;

	if (err_ret)
		*err_ret = 0;

	ret = sdio_memcpy_fromio(func, func->tmpbuf, addr, 2);
	if (ret) {
		if (err_ret)
			*err_ret = ret;
		return 0xFFFF;
	}

	// TODO : print val
	//return vmm_le16_to_cpu((__le16 *)func->tmpbuf);
	strncpy((char *)&val, (char *)func->tmpbuf, 2);
	return val;
}
VMM_EXPORT_SYMBOL(sdio_readw);

/**
 *	sdio_writew - write a 16 bit integer to a SDIO function
 *	@func: SDIO function to access
 *	@b: integer to write
 *	@addr: address to write to
 *	@err_ret: optional status value from transfer
 *
 *	Writes a 16 bit integer to the address space of a given SDIO
 *	function. @err_ret will contain the status of the actual
 *	transfer.
 */
void sdio_writew(struct sdio_func *func, u16 b, unsigned int addr, int *err_ret)
{
	int ret;

	*(__le16 *)func->tmpbuf = vmm_cpu_to_le16(b);

	ret = sdio_memcpy_toio(func, addr, func->tmpbuf, 2);
	if (err_ret)
		*err_ret = ret;
}
VMM_EXPORT_SYMBOL(sdio_writew);

/**
 *	sdio_readl - read a 32 bit integer from a SDIO function
 *	@func: SDIO function to access
 *	@addr: address to read
 *	@err_ret: optional status value from transfer
 *
 *	Reads a 32 bit integer from the address space of a given SDIO
 *	function. If there is a problem reading the address,
 *	0xffffffff is returned and @err_ret will contain the error
 *	code.
 */
u32 sdio_readl(struct sdio_func *func, unsigned int addr, int *err_ret)
{
	int ret;
	u32 val;

	if (err_ret)
		*err_ret = 0;

	ret = sdio_memcpy_fromio(func, func->tmpbuf, addr, 4);
	if (ret) {
		if (err_ret)
			*err_ret = ret;
		return 0xFFFFFFFF;
	}

	// TODO : print val
	//return (u32)vmm_le32_to_cpu((__le32 *)func->tmpbuf);
	strncpy((char *)&val, (char *)func->tmpbuf, 4);
	return val;
}
VMM_EXPORT_SYMBOL(sdio_readl);

/**
 *	sdio_writel - write a 32 bit integer to a SDIO function
 *	@func: SDIO function to access
 *	@b: integer to write
 *	@addr: address to write to
 *	@err_ret: optional status value from transfer
 *
 *	Writes a 32 bit integer to the address space of a given SDIO
 *	function. @err_ret will contain the status of the actual
 *	transfer.
 */
void sdio_writel(struct sdio_func *func, u32 b, unsigned int addr, int *err_ret)
{
	int ret;

	*(__le32 *)func->tmpbuf = vmm_cpu_to_le32(b);

	ret = sdio_memcpy_toio(func, addr, func->tmpbuf, 4);
	if (err_ret)
		*err_ret = ret;
}
VMM_EXPORT_SYMBOL(sdio_writel);

/**
 *	sdio_f0_readb - read a single byte from SDIO function 0
 *	@func: an SDIO function of the card
 *	@addr: address to read
 *	@err_ret: optional status value from transfer
 *
 *	Reads a single byte from the address space of SDIO function 0.
 *	If there is a problem reading the address, 0xff is returned
 *	and @err_ret will contain the error code.
 */
unsigned char sdio_f0_readb(struct sdio_func *func, unsigned int addr,
	int *err_ret)
{
	int ret;
	unsigned char val;

	BUG_ON(!func);

	if (err_ret)
		*err_ret = 0;

	ret = mmc_io_rw_direct(func->card, 0, 0, addr, 0, &val);
	if (ret) {
		if (err_ret)
			*err_ret = ret;
		return 0xFF;
	}

	return val;
}
VMM_EXPORT_SYMBOL(sdio_f0_readb);

/**
 *	sdio_f0_writeb - write a single byte to SDIO function 0
 *	@func: an SDIO function of the card
 *	@b: byte to write
 *	@addr: address to write to
 *	@err_ret: optional status value from transfer
 *
 *	Writes a single byte to the address space of SDIO function 0.
 *	@err_ret will contain the status of the actual transfer.
 *
 *	Only writes to the vendor specific CCCR registers (0xF0 -
 *	0xFF) are permiited; @err_ret will be set to -EINVAL for *
 *	writes outside this range.
 */
void sdio_f0_writeb(struct sdio_func *func, unsigned char b, unsigned int addr,
	int *err_ret)
{
	int ret;

	BUG_ON(!func);

	if ((addr < 0xF0 || addr > 0xFF) &&
	    (func->card->quirks & MMC_QUIRK_LENIENT_FN0)) {
		if (err_ret)
			*err_ret = VMM_EINVALID;
		return;
	}

	ret = mmc_io_rw_direct(func->card, 1, 0, addr, b, NULL);
	if (err_ret)
		*err_ret = ret;
}
VMM_EXPORT_SYMBOL(sdio_f0_writeb);
