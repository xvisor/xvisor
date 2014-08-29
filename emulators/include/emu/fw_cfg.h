/**
 * Copyright (c) 2014 Himanshu Chauhan
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
 * @file fw_cfg.h
 * @author Himanshu Chauhan <hschauhan@nulltrace.org>
 * @brief Definitions related to firmware configuration emulator
 *
 * This file has been largely adapted from qemu/include/hw/nvram/fw_cfg.h
 * Name of original author isn't mentioned but probably its the same
 * Author who wrote qemu/hw/nvram/fw_cfg.c
 */

#ifndef FW_CFG_H
#define FW_CFG_H

#define FW_CFG_SIGNATURE        0x00
#define FW_CFG_ID               0x01
#define FW_CFG_UUID             0x02
#define FW_CFG_RAM_SIZE         0x03
#define FW_CFG_NOGRAPHIC        0x04
#define FW_CFG_NB_CPUS          0x05
#define FW_CFG_MACHINE_ID       0x06
#define FW_CFG_KERNEL_ADDR      0x07
#define FW_CFG_KERNEL_SIZE      0x08
#define FW_CFG_KERNEL_CMDLINE   0x09
#define FW_CFG_INITRD_ADDR      0x0a
#define FW_CFG_INITRD_SIZE      0x0b
#define FW_CFG_BOOT_DEVICE      0x0c
#define FW_CFG_NUMA             0x0d
#define FW_CFG_BOOT_MENU        0x0e
#define FW_CFG_MAX_CPUS         0x0f
#define FW_CFG_KERNEL_ENTRY     0x10
#define FW_CFG_KERNEL_DATA      0x11
#define FW_CFG_INITRD_DATA      0x12
#define FW_CFG_CMDLINE_ADDR     0x13
#define FW_CFG_CMDLINE_SIZE     0x14
#define FW_CFG_CMDLINE_DATA     0x15
#define FW_CFG_SETUP_ADDR       0x16
#define FW_CFG_SETUP_SIZE       0x17
#define FW_CFG_SETUP_DATA       0x18
#define FW_CFG_FILE_DIR         0x19

#define FW_CFG_FILE_FIRST       0x20
#define FW_CFG_FILE_SLOTS       0x10
#define FW_CFG_MAX_ENTRY        (FW_CFG_FILE_FIRST+FW_CFG_FILE_SLOTS)

#define FW_CFG_WRITE_CHANNEL    0x4000
#define FW_CFG_ARCH_LOCAL       0x8000
#define FW_CFG_ENTRY_MASK       ~(FW_CFG_WRITE_CHANNEL | FW_CFG_ARCH_LOCAL)

#define FW_CFG_INVALID          0xffff

#define FW_CFG_MAX_FILE_PATH    56

typedef struct fw_cfg_file {
	u32  size;        /* file size */
	u16  select;      /* write this to 0x510 to read it */
	u16  reserved;
	char name[FW_CFG_MAX_FILE_PATH];
} fw_cfg_file_t;

typedef struct fw_cfg_files {
	u32  count;
	fw_cfg_file_t f[];
} fw_cfg_files_t;

typedef void (*FWCfgCallback)(void *opaque, u8 *data);
typedef void (*FWCfgReadCallback)(void *opaque, u32 offset);

/* Forward declarations */
struct fw_cfg_state;
typedef struct fw_cfg_state fw_cfg_state_t;

int fw_cfg_add_bytes(fw_cfg_state_t *s, u16 key, void *data, size_t len);
int fw_cfg_add_string(fw_cfg_state_t *s, u16 key, const char *value);
int fw_cfg_add_i16(fw_cfg_state_t *s, u16 key, u16 value);
int fw_cfg_add_i32(fw_cfg_state_t *s, u16 key, u32 value);
int fw_cfg_add_i64(fw_cfg_state_t *s, u16 key, u64 value);
int fw_cfg_add_callback(fw_cfg_state_t *s, u16 key, FWCfgCallback callback,
			void *callback_opaque, void *data, size_t len);
int fw_cfg_add_file(fw_cfg_state_t *s, const char *filename, void *data,
                     size_t len);
int fw_cfg_add_file_callback(fw_cfg_state_t *s, const char *filename,
                              FWCfgReadCallback callback, void *callback_opaque,
                              void *data, size_t len);
#endif
