/**
 * Copyright (c) 2018 Anup Patel.
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
 * @file pinctrl-state.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Standard pin control state definitions
 *
 * Adapted from linux/include/linux/pinctrl/pinctrl-state.h
 *
 * The original source is licensed under GPL.
 */

/**
 * @PINCTRL_STATE_DEFAULT: the state the pinctrl handle shall be put
 *	into as default, usually this means the pins are up and ready to
 *	be used by the device driver. This state is commonly used by
 *	hogs to configure muxing and pins at boot, and also as a state
 *	to go into when returning from sleep and idle in
 *	.pm_runtime_resume() or ordinary .resume() for example.
 * @PINCTRL_STATE_INIT: normally the pinctrl will be set to "default"
 *	before the driver's probe() function is called.  There are some
 *	drivers where that is not appropriate becausing doing so would
 *	glitch the pins.  In those cases you can add an "init" pinctrl
 *	which is the state of the pins before drive probe.  After probe
 *	if the pins are still in "init" state they'll be moved to
 *	"default".
 * @PINCTRL_STATE_IDLE: the state the pinctrl handle shall be put into
 *	when the pins are idle. This is a state where the system is relaxed
 *	but not fully sleeping - some power may be on but clocks gated for
 *	example. Could typically be set from a pm_runtime_suspend() or
 *	pm_runtime_idle() operation.
 * @PINCTRL_STATE_SLEEP: the state the pinctrl handle shall be put into
 *	when the pins are sleeping. This is a state where the system is in
 *	its lowest sleep state. Could typically be set from an
 *	ordinary .suspend() function.
 */
#define PINCTRL_STATE_DEFAULT "default"
#define PINCTRL_STATE_INIT "init"
#define PINCTRL_STATE_IDLE "idle"
#define PINCTRL_STATE_SLEEP "sleep"
