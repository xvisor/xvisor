#ifndef _INPUT_H
#define _INPUT_H

/*
 * Copyright (c) 1999-2002 Vojtech Pavlik
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <input/vmm_input.h>

#define input_dev			vmm_input_dev

#define input_allocate_device		vmm_input_alloc_device
#define input_free_device		vmm_input_free_device
#define input_register_device		vmm_input_register_device
#define input_unregister_device		vmm_input_unregister_device

#define input_event			vmm_input_event
#define input_sync			vmm_input_sync
#define input_report_key		vmm_input_report_key
#define input_report_rel		vmm_input_report_rel
#define input_get_drvdata		vmm_input_get_drvdata
#define input_set_drvdata		vmm_input_set_drvdata

#endif /* _INPUT_H */
