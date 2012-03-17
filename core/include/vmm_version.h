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
 * @author Anup Patel (anup@brainfault.org)
 * @brief header file for VMM version number
 */
#ifndef _VMM_VERSION_H__
#define _VMM_VERSION_H__

#define VMM_NAME			"Xvisor"
#define VMM_VERSION_MAJOR		CONFIG_MAJOR
#define VMM_VERSION_MINOR		CONFIG_MINOR
#define VMM_VERSION_RELEASE		CONFIG_RELEASE

#if defined(CONFIG_BANNER_BANNER4)
/** Banner string in ASCII Banner4 font */
#define VMM_BANNER_STRING	"\n" \
".##.....##.##.....##.####..######...#######..########.\n" \
"..##...##..##.....##..##..##....##.##.....##.##.....##\n" \
"...##.##...##.....##..##..##.......##.....##.##.....##\n" \
"....###....##.....##..##...######..##.....##.########.\n" \
"...##.##....##...##...##........##.##.....##.##...##..\n" \
"..##...##....##.##....##..##....##.##.....##.##....##.\n" \
".##.....##....###....####..######...#######..##.....##\n" \
"\n"
#elif defined(CONFIG_BANNER_COLOSSAL)
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
#elif defined(CONFIG_BANNER_O8)
/** Banner string in ASCII O8 font */
#define VMM_BANNER_STRING	"\n" \
"ooooo  oooo ooooo  oooo ooooo  oooooooo8    ooooooo  oooooooooo  \n" \
"  888  88    888    88   888  888         o888   888o 888    888 \n" \
"    888       888  88    888   888oooooo  888     888 888oooo88  \n" \
"   88 888      88888     888          888 888o   o888 888  88o   \n" \
"o88o  o888o     888     o888o o88oooo888    88ooo88  o888o  88o8 \n" \
"\n"
#elif defined(CONFIG_BANNER_ROMAN)
/** Banner string in ASCII Roman font */
#define VMM_BANNER_STRING	"\n" \
"ooooooo  ooooo oooooo     oooo ooooo  .oooooo..o   .oooooo.   ooooooooo.  \n" \
" `8888    d8'   `888.     .8'  `888' d8P'    `Y8  d8P'  `Y8b  `888   `Y88.\n" \
"   Y888..8P      `888.   .8'    888  Y88bo.      888      888  888   .d88'\n" \
"    `8888'        `888. .8'     888    `Y8888o.  888      888  888ooo88P' \n" \
"   .8PY888.        `888.8'      888        `Y88b 888      888  888`88b.   \n" \
"  d8'  `888b        `888'       888  oo     .d8P `88b    d88'  888  `88b. \n" \
"o888o  o88888o       `8'       o888o 8''88888P'   `Y8bood8P'  o888o  o888o\n" \
"\n"
#else
#define VMM_BANNER_STRING	"\n"
#endif

#endif
