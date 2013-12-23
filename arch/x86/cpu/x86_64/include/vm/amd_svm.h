/**
 * Copyright (c) 2013 Himanshu Chauhan.
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * @file amd_svm.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Header file for SVM's definition.
 */

#ifndef _AMD_SVM_H__
#define _AMD_SVM_H__

#include <vmm_types.h>
#include <cpu_features.h>
#include <cpu_vm.h>
#include <vm/amd_vmcb.h>

#ifndef __ASSEMBLY__

extern int amd_setup_vm_control(struct vcpu_hw_context *context);
extern int init_amd(struct cpuinfo_x86 *cpuinfo);
extern void svm_launch(void);

#endif

#endif /* _AMD_SVM_H__ */
