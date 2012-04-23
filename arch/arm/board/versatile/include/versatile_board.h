/**
 * Copyright (c) 2012 Jean-Christophe Dubois.
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
 * @file versatile_board.h
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief Versatile board configuration
 */
#ifndef _VERSATILE_BOARD_H__
#define _VERSATILE_BOARD_H__

#include <versatile_plat.h>

#define IRQ_VERSATILE_PL190_START	INT_WDOGINT
#define IRQ_VERSATILE_PL190_END		(IRQ_VERSATILE_PL190_START + 63)
#define NR_IRQS_VERSATILE		(IRQ_VERSATILE_PL190_END + 1)

#endif
