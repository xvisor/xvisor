/**
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
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
 * @file cpu_guest_serial.h
 * @author Atish Patra(atish.patra@wdc.com)
 * @brief RISC-V guest serial header
 */

#ifndef _CPU_GUEST_SERIAL_H__
#define _CPU_GUEST_SERIAL_H__

#include <vio/vmm_vserial.h>
#include <vmm_notifier.h>

struct riscv_guest_serial {
	char name[VMM_FIELD_NAME_SIZE];
	struct vmm_notifier_block vser_client;
	struct vmm_vserial *vserial;
};

#endif
