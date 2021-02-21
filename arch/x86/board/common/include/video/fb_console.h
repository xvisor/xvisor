/**
 * Copyright (c) 2021 Himanshu Chauhan.
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
 * @file fb_console.h
 * @author Himanshu Chauhan (hchauhan@xvisor-x86.org)
 * @brief Framebuffer console header file.
 */

#ifndef __FRAMEBUFFER_CONSOLE_H
#define __FRAMEBUFFER_CONSOLE_H

#define CHAR_HEIGHT 16
#define CHAR_WIDTH 8

void fb_console_set_font(void* reg, void* bold);

struct defterm_ops *get_fb_defterm_ops(void *data);

#endif /* __FRAMEBUFFER_CONSOLE_H */
