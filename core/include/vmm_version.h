/**
 * Copyright (c) 2010 Anup Patel.
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
 * @file vmm_version.h
 * @version 1.0
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for VMM version number
 */
#ifndef _VMM_VERSION_H__
#define _VMM_VERSION_H__

#define VMM_NAME			"Xvisor"
#define VMM_VERSION_MAJOR		CONFIG_MAJOR
#define VMM_VERSION_MINOR		CONFIG_MINOR
#define VMM_VERSION_RELEASE		CONFIG_RELEASE

#if defined(CONFIG_BANNER_DEFAULT)
/** Banner string in ASCII colossal font */
#define VMM_BANNER_STRING	"\n" \
"Y88b   d88P 888     888 8888888 .d8888b.   .d88888b.  8888888b.  \n" \
" Y88b d88P  888     888   888  d88P  Y88b d88P   Y88b 888   Y88b \n" \
"  Y88o88P   888     888   888  Y88b.      888     888 888    888 \n" \
"   Y888P    Y88b   d88P   888    Y888b.   888     888 888   d88P \n" \
"   d888b     Y88b d88P    888       Y88b. 888     888 8888888P   \n" \
"  d88888b     Y88o88P     888         888 888     888 888 T88b   \n" \
" d88P Y88b     Y888P      888  Y88b  d88P Y88b. .d88P 888  T88b  \n" \
"d88P   Y88b     Y8P     8888888  Y8888P     Y88888P   888   T88b \n" \
"\n"
#else
#define VMM_BANNER_STRING	"\n"
#endif

#endif
