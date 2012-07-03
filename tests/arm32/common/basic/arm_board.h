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
 * @file arm_board.h
 * @author Jean-Christophe Dubois (jcd@tribudubois.net)
 * @brief ARM Platform misc functions Header
 */
#ifndef _ARM_MISC_H__
#define _ARM_MISC_H__

void arm_board_reset(void);
void arm_board_init(void);
char *arm_board_name(void);
u32 arm_board_flash_addr(void);

#endif
