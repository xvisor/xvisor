/**
 * Copyright (c) 2016 Open Wide
 *               2016 Institut de Recherche Technologique SystemX
 *
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
 * @file vmm_devemu_debug.h
 * @author Jean Guyomarc'h (jean.guyomarch@openwide.fr)
 * @brief header file for device emulation debug framework
 */
#ifndef _VMM_DEVEMU_DEBUG_H__
#define _VMM_DEVEMU_DEBUG_H__

#include <vmm_devemu.h>

/**
 * Debugging flags that can be set in the Device Tree describing the
 * interface hypervisor-guest.
 * All flags but @c VMM_DEVEMU_DEBUG_IRQ are automatically handled
 * by Xvisor. @c VMM_DEVEMU_DEBUG_IRQ is implementation defined: it
 * can be used within the implementation of an emulator to provide
 * a better debugging interface, but there is no guarantee all emulators
 * do implement it.
 *
 * Emulators can use bits in range [31;16] as specific debug information.
 * Bits [15;0] are reserved for Xvisor (currently only 5 are used).
 *
 *
 * Example:
 * @code
 * node {
 *	debug = <0x7>; // PROBE | RESET | REMOVE
 * };
 * @endcode
 */
enum vmm_devemu_debug {
	VMM_DEVEMU_DEBUG_NONE	= 0,        /**< No debug */
	VMM_DEVEMU_DEBUG_PROBE	= (1 << 0), /**< Debug when probed */
	VMM_DEVEMU_DEBUG_RESET	= (1 << 1), /**< Debug when reset */
	VMM_DEVEMU_DEBUG_REMOVE	= (1 << 2), /**< Debug when removed */
	VMM_DEVEMU_DEBUG_READ	= (1 << 3), /**< Debug when read */
	VMM_DEVEMU_DEBUG_WRITE	= (1 << 4), /**< Debug when wrote to */
	VMM_DEVEMU_DEBUG_IRQ	= (1 << 5), /**< Debug when an IRQ is emulated */
	VMM_DEVEMU_DEBUG_PARSE  = (1 << 6), /**< Debug parameters manually parsed */
	/* (1 << 7)  is available */
	/* (1 << 8)  is available */
	/* (1 << 9)  is available */
	/* (1 << 10) is available */
	/* (1 << 11) is available */
	/* (1 << 12) is available */
	/* (1 << 13) is available */
	/* (1 << 14) is available */
	/* (1 << 15) is available */
	/* No more debug bits available for Xvisor core */
};



/*
 * When CONFIG_DEVEMU_DEBUG is enabled, a field in the vmm_emudev structure
 * is added to hold debug flags.
 * The functions below are the only proper way to manipulate this field
 * properly, since it is absent when CONFIG_DEVEMU_DEBUG is unset.
 *
 * Since those functions are inlined, branches that use the function
 * below can be optimized out, leading to zero overhead when CONFIG_DEVEMU_DEBUG
 * is disabled.
 */

/**
 * Gets the debug information flags about a device emulator
 * @param[in] edev Device emulator to query
 * @return Debug information flags
 *
 * @note Use this function only to query debug flags, since direct memory
 * access is configuration-dependant.
 * The last 16bits can be used freely by any emulator to provide its own
 * debug through the device tree.
 */
static inline u32 vmm_devemu_get_debug_info(const struct vmm_emudev *edev);

#ifdef CONFIG_DEVEMU_DEBUG
static inline u32 vmm_devemu_get_debug_info(const struct vmm_emudev *edev)
{
	return edev->debug_info;
}
#else /* ! CONFIG_DEVEMU_DEBUG */
static inline u32 vmm_devemu_get_debug_info(const struct vmm_emudev *edev)
{
	return 0;
}
#endif /* CONFIG_DEVEMU_DEBUG */


/** @return TRUE if debug is enabled on probing, FALSE otherwise */
static inline bool vmm_devemu_debug_probe(const struct vmm_emudev *edev)
{
	return (vmm_devemu_get_debug_info(edev) & VMM_DEVEMU_DEBUG_PROBE);
}

/** @return TRUE if debug is enabled on reset, FALSE otherwise */
static inline bool vmm_devemu_debug_reset(const struct vmm_emudev *edev)
{
	return (vmm_devemu_get_debug_info(edev) & VMM_DEVEMU_DEBUG_RESET);
}

/** @return TRUE if debug is enabled on removal, FALSE otherwise */
static inline bool vmm_devemu_debug_remove(const struct vmm_emudev *edev)
{
	return (vmm_devemu_get_debug_info(edev) & VMM_DEVEMU_DEBUG_REMOVE);
}

/** @return TRUE if debug is enabled on read, FALSE otherwise */
static inline bool vmm_devemu_debug_read(const struct vmm_emudev *edev)
{
	return (vmm_devemu_get_debug_info(edev) & VMM_DEVEMU_DEBUG_READ);
}

/** @return TRUE if debug is enabled on write, FALSE otherwise */
static inline bool vmm_devemu_debug_write(const struct vmm_emudev *edev)
{
	return (vmm_devemu_get_debug_info(edev) & VMM_DEVEMU_DEBUG_WRITE);
}

/**
 * @return TRUE if debug is enabled when an IRQ is emulated, FALSE otherwise
 * @note This function must be explicitely used in emulators implementation
 * to provide debug information.
 */
static inline bool vmm_devemu_debug_irq(const struct vmm_emudev *edev)
{
	return (vmm_devemu_get_debug_info(edev) & VMM_DEVEMU_DEBUG_IRQ);
}

/**
 * @return TRUE if debug is enabled when a device tree parameter has been
 * parsed manually, FALSE otherwise.
 * @note This function must be explicitely used in emulators implementation
 * to provide debug information.
 */
static inline bool vmm_devemu_debug_parsed_params(const struct vmm_emudev *edev)
{
	return (vmm_devemu_get_debug_info(edev) & VMM_DEVEMU_DEBUG_PARSE);
}

#endif /* ! _VMM_DEVEMU_DEBUG_H__ */
