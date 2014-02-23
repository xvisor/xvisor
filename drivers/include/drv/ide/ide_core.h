/**
 * Copyright (c) 2014 Himanshu Chauhan.
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
 * @file ide_core.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief IDE Core Framework related defines.
 */
#ifndef __IDE_CORE_H_
#define __IDE_CORE_H_

#include <vmm_compiler.h>
#include <vmm_types.h>
#include <vmm_mutex.h>
#include <vmm_completion.h>
#include <vmm_threads.h>
#include <block/vmm_blockdev.h>
#include <libs/list.h>
#include <drv/ide/ide.h>
#include <drv/ide/ata.h>

#define IDE_CORE_IPRIORITY		(VMM_BLOCKDEV_CLASS_IPRIORITY + 1)

struct ide_drive_io {
	struct dlist head;
	struct vmm_request *r;
	struct vmm_request_queue *rq;
};

#endif /* __IDE_CORE_H_ */
