/**
 *
 * Copyright (c) 2014 Himanshu Chauhan
 * All rights reserved.
 *
 * QEMU Firmware configuration device emulation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * Original Copyright:
 * Copyright (c) 2008 Gleb Natapov
 *
 * Adapted for Xvisor from qemu/hw/nvram/fw_cfg.c
 *
 * @file fw_cfg.c
 * @author Himanshu Chauhan <hschauhan@nulltrace.org>
 * @brief Qemu like firmware configuration emulator
 */

#include <vmm_error.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_modules.h>
#include <vmm_devemu.h>
#include <vmm_host_io.h>
#include <emu/fw_cfg.h>

#define FW_CFG_SIZE		2
#define FW_CFG_DATA_SIZE	1
#define FW_CFG_NAME		"fw_cfg"
#define FW_CFG_PATH		"/machine/" FW_CFG_NAME

#define MODULE_DESC		"Firmware Configuration Emulator"
#define MODULE_AUTHOR		"Himanshu Chauhan"
#define MODULE_LICENSE		"GPL"
#define MODULE_IPRIORITY	(0)
#define	MODULE_INIT		fwcfg_emulator_init
#define	MODULE_EXIT		fwcfg_emulator_exit

typedef struct fw_cfg_entry {
	u32 len;
	u8 *data;
	void *callback_opaque;
	FWCfgCallback callback;
	FWCfgReadCallback read_callback;
} fw_cfg_entry_t;

struct fw_cfg_state {
	u32 ctl_iobase, data_iobase;
	fw_cfg_entry_t entries[2][FW_CFG_MAX_ENTRY];
	fw_cfg_files_t *files;
	u16 cur_entry;
	u32 cur_offset;
};

static void fw_cfg_reboot(fw_cfg_state_t *s)
{
	/* Guest rebots in 5 seconds */
	u32 reboot_time = 5;
	fw_cfg_add_file(s, "etc/boot-fail-wait", &reboot_time, 4);
}

static void fw_cfg_write(fw_cfg_state_t *s, u8 value)
{
	int arch = !!(s->cur_entry & FW_CFG_ARCH_LOCAL);
	fw_cfg_entry_t *e = &s->entries[arch][s->cur_entry & FW_CFG_ENTRY_MASK];

	if (s->cur_entry & FW_CFG_WRITE_CHANNEL && e->callback &&
	    s->cur_offset < e->len) {
		e->data[s->cur_offset++] = value;
		if (s->cur_offset == e->len) {
			e->callback(e->callback_opaque, e->data);
			s->cur_offset = 0;
		}
	}
}

static int fw_cfg_select(fw_cfg_state_t *s, u16 key)
{
	int ret;

	s->cur_offset = 0;
	if ((key & FW_CFG_ENTRY_MASK) >= FW_CFG_MAX_ENTRY) {
		s->cur_entry = FW_CFG_INVALID;
		ret = 0;
	} else {
		s->cur_entry = key;
		ret = 1;
	}

	return ret;
}

static u8 fw_cfg_read(fw_cfg_state_t *s)
{
	int arch = !!(s->cur_entry & FW_CFG_ARCH_LOCAL);
	fw_cfg_entry_t *e = &s->entries[arch][s->cur_entry & FW_CFG_ENTRY_MASK];
	u8 ret;

	if (s->cur_entry == FW_CFG_INVALID || !e->data || s->cur_offset >= e->len)
		ret = 0;
	else {
		if (e->read_callback) {
			e->read_callback(e->callback_opaque, s->cur_offset);
		}
		ret = e->data[s->cur_offset++];
	}

	return ret;
}

static u64 fw_cfg_data_mem_read(void *opaque, physical_addr_t addr)
{
	return fw_cfg_read(opaque);
}

static void fw_cfg_data_mem_write(void *opaque, u64 value)
{
	fw_cfg_write(opaque, (u8)value);
}

static void fw_cfg_ctl_mem_write(void *opaque, u64 value)
{
	fw_cfg_select(opaque, (u16)value);
}

static int fw_cfg_add_bytes_read_callback(fw_cfg_state_t *s, u16 key,
					  FWCfgReadCallback callback,
					  void *callback_opaque,
					  void *data, size_t len)
{
	int arch = !!(key & FW_CFG_ARCH_LOCAL);

	key &= FW_CFG_ENTRY_MASK;

	if (key >= FW_CFG_MAX_ENTRY && len > 0xffffffff)
		return VMM_EFAIL;

	s->entries[arch][key].data = data;
	s->entries[arch][key].len = (u32)len;
	s->entries[arch][key].read_callback = callback;
	s->entries[arch][key].callback_opaque = callback_opaque;

	return VMM_OK;
}

int fw_cfg_add_bytes(fw_cfg_state_t *s, u16 key, void *data, size_t len)
{
	return fw_cfg_add_bytes_read_callback(s, key, NULL, NULL, data, len);
}

int fw_cfg_add_string(fw_cfg_state_t *s, u16 key, const char *value)
{
	size_t sz = strlen(value) + 1;
	u8 *data_dup = vmm_zalloc(sz);

	memcpy(data_dup, value, sz);

	return fw_cfg_add_bytes(s, key, data_dup, sz);
}

int fw_cfg_add_i16(fw_cfg_state_t *s, u16 key, u16 value)
{
	u16 *copy;

	copy = vmm_zalloc(sizeof(value));
	*copy = vmm_cpu_to_le16(value);
	return fw_cfg_add_bytes(s, key, copy, sizeof(value));
}

int fw_cfg_add_i32(fw_cfg_state_t*s, u16 key, u32 value)
{
	u32 *copy;

	copy = vmm_zalloc(sizeof(value));
	*copy = vmm_cpu_to_le32(value);
	return fw_cfg_add_bytes(s, key, copy, sizeof(value));
}

int fw_cfg_add_i64(fw_cfg_state_t *s, u16 key, u64 value)
{
	u64 *copy;

	copy = vmm_zalloc(sizeof(value));
	*copy = vmm_cpu_to_le64(value);
	return fw_cfg_add_bytes(s, key, copy, sizeof(value));
}

int  fw_cfg_add_callback(fw_cfg_state_t *s, u16 key, FWCfgCallback callback,
                         void *callback_opaque, void *data, size_t len)
{
	int arch = !!(key & FW_CFG_ARCH_LOCAL);

	if (!(key & FW_CFG_WRITE_CHANNEL))
		return VMM_EFAIL;

	key &= FW_CFG_ENTRY_MASK;

	if (key >= FW_CFG_MAX_ENTRY && len > 0xffffffff)
		return VMM_EFAIL;

	s->entries[arch][key].data = data;
	s->entries[arch][key].len = (u32)len;
	s->entries[arch][key].callback_opaque = callback_opaque;
	s->entries[arch][key].callback = callback;

	return VMM_OK;
}

int fw_cfg_add_file_callback(fw_cfg_state_t *s,  const char *filename,
			     FWCfgReadCallback callback, void *callback_opaque,
			     void *data, size_t len)
{
	int i, index;
	size_t dsize;

	if (!s->files) {
		dsize = sizeof(u32) + sizeof(fw_cfg_file_t) * FW_CFG_FILE_SLOTS;
		s->files = vmm_zalloc(dsize);
		fw_cfg_add_bytes(s, FW_CFG_FILE_DIR, s->files, dsize);
	}

	index = vmm_be32_to_cpu(s->files->count);
	if (index >= FW_CFG_FILE_SLOTS)
		return VMM_EFAIL;

	fw_cfg_add_bytes_read_callback(s, FW_CFG_FILE_FIRST + index,
				       callback, callback_opaque, data, len);

	strcpy(s->files->f[index].name, filename);

	for (i = 0; i < index; i++) {
		if (strcmp(s->files->f[index].name, s->files->f[i].name) == 0) {
			vmm_printf("%s: Duplicate entry\n", __func__);
			return VMM_OK;
		}
	}

	s->files->f[index].size   = vmm_cpu_to_be32(len);
	s->files->f[index].select = vmm_cpu_to_be16(FW_CFG_FILE_FIRST + index);

	s->files->count = vmm_cpu_to_be32(index+1);

	return VMM_OK;
}

int fw_cfg_add_file(fw_cfg_state_t *s, const char *filename,
                     void *data, size_t len)
{
	return fw_cfg_add_file_callback(s, filename, NULL, NULL, data, len);
}

static int fwcfg_emulator_read8(struct vmm_emudev *edev,
				physical_addr_t offset,
				u8 *dst)
{
	/* Always read zero */
	*dst = fw_cfg_data_mem_read(edev->priv, offset);

	return VMM_OK;
}

static int fwcfg_emulator_read16(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u16 *dst)
{
	/* Always read zero */
	*dst = (u16)fw_cfg_data_mem_read(edev->priv, offset);

	return VMM_OK;
}

static int fwcfg_emulator_read32(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u32 *dst)
{
	/* Always read zero */
	*dst = (u32)fw_cfg_data_mem_read(edev->priv, offset);
	return VMM_OK;
}

static int fwcfg_emulator_write8(struct vmm_emudev *edev,
				 physical_addr_t offset,
				 u8 src)
{
	switch(offset) {
	case 0:
		fw_cfg_ctl_mem_write(edev->priv, src);
		break;
	case 1:
		fw_cfg_data_mem_write(edev->priv, src);
		break;
	default:
		return VMM_EFAIL;
	}

	/* Ignore it. */
	return VMM_OK;
}

static int fwcfg_emulator_write16(struct vmm_emudev *edev,
				  physical_addr_t offset,
				  u16 src)
{
	switch(offset) {
	case 0:
		fw_cfg_ctl_mem_write(edev->priv, src);
		break;
	case 1:
		fw_cfg_data_mem_write(edev->priv, src);
		break;
	default:
		return VMM_EFAIL;
	}

	/* Ignore it. */
	return VMM_OK;
}

static int fwcfg_emulator_write32(struct vmm_emudev *edev,
				  physical_addr_t offset,
				  u32 src)
{
	switch(offset) {
	case 0:
		fw_cfg_ctl_mem_write(edev->priv, src);
		break;
	case 1:
		fw_cfg_data_mem_write(edev->priv, src);
		break;
	default:
		return VMM_EFAIL;
	}

	/* Ignore it. */
	return VMM_OK;
}

static int fwcfg_emulator_reset(struct vmm_emudev *edev)
{
	fw_cfg_select(edev->priv, 0);

	return VMM_OK;
}

static int fwcfg_emulator_probe(struct vmm_guest *guest,
				struct vmm_emudev *edev,
				const struct vmm_devtree_nodeid *eid)
{
	fw_cfg_state_t *s;

	s = vmm_zalloc(sizeof(fw_cfg_state_t));

	if (!s)
		return VMM_ENOMEM;

	fw_cfg_add_bytes(s, FW_CFG_SIGNATURE, (char *)"QEMU", 4);
	fw_cfg_add_i16(s, FW_CFG_NOGRAPHIC, 1);

	/* SMP FIXME: Change when SMP support is added */
	fw_cfg_add_i16(s, FW_CFG_NB_CPUS, 1);
	/* No boot menu */
	fw_cfg_add_i16(s, FW_CFG_BOOT_MENU, 0);
	fw_cfg_reboot(s);

	edev->priv = s;

	return VMM_OK;
}

static int fwcfg_emulator_remove(struct vmm_emudev *edev)
{
	if (edev->priv) {
		vmm_free(edev->priv);
		edev->priv = NULL;
	}

	return VMM_OK;
}

static struct vmm_devtree_nodeid fwcfg_emuid_table[] = {
	{
		.type = "misc",
		.compatible = "fwcfg",
	},
	{ /* end of list */ },
};

static struct vmm_emulator fwcfg_emulator = {
	.name = "fwcfg",
	.match_table = fwcfg_emuid_table,
	.endian = VMM_DEVEMU_NATIVE_ENDIAN,
	.probe = fwcfg_emulator_probe,
	.read8 = fwcfg_emulator_read8,
	.write8 = fwcfg_emulator_write8,
	.read16 = fwcfg_emulator_read16,
	.write16 = fwcfg_emulator_write16,
	.read32 = fwcfg_emulator_read32,
	.write32 = fwcfg_emulator_write32,
	.reset = fwcfg_emulator_reset,
	.remove = fwcfg_emulator_remove,
};

static int __init fwcfg_emulator_init(void)
{
	return vmm_devemu_register_emulator(&fwcfg_emulator);
}

static void __exit fwcfg_emulator_exit(void)
{
	vmm_devemu_unregister_emulator(&fwcfg_emulator);
}

VMM_DECLARE_MODULE(MODULE_DESC,
		   MODULE_AUTHOR,
		   MODULE_LICENSE,
		   MODULE_IPRIORITY,
		   MODULE_INIT,
		   MODULE_EXIT);
