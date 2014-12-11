/*
 * Copyright 2005-2013 Freescale Semiconductor, Inc.
 * Copyright (C) 2014 Institut de Recherche Technologique SystemX and OpenWide.
 * All rights reserved.
 * Code from the Linux kernel 3.16.
 * Modified by Jimmy Durand Wesolowski <jimmy.durand-wesolowski@openwide.fr>
 * for Xvisor.
 *
 * The code contained herein is licensed under the GNU Lesser General
 * Public License.  You may obtain a copy of the GNU Lesser General
 * Public License Version 2.1 or later at the following locations:
 *
 * http://www.opensource.org/licenses/lgpl-license.html
 * http://www.gnu.org/copyleft/lgpl.html
 *
 * @file ipu.h
 * @author Jimmy Durand Wesolowski (jimmy.durand-wesolowski@openwide.fr)
 * @brief This file contains the IPU driver API declarations.
 *
 * @defgroup IPU MXC Image Processing Unit (IPU) Driver
 * @ingroup IPU
 */

#ifndef __LINUX_IPU_H__
#define __LINUX_IPU_H__

#include <asm/ipu.h>

unsigned int fmt_to_bpp(unsigned int pixelformat);
cs_t colorspaceofpixel(int fmt);
int need_csc(int ifmt, int ofmt);

int ipu_queue_task(struct ipu_task *task);
int ipu_check_task(struct ipu_task *task);

#endif
