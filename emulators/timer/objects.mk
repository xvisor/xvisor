#/**
# Copyright (c) 2010 Anup Patel.
# All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# @file objects.mk
# @author Anup Patel (anup@brainfault.org)
# @brief list of timer emulator objects
# */

emulators-objs-$(CONFIG_EMU_TIMER_SP804)+= timer/sp804.o
emulators-objs-$(CONFIG_EMU_TIMER_ARM_MPTIMER)+= timer/arm_mptimer.o
emulators-objs-$(CONFIG_EMU_TIMER_INTEL_8254)+= timer/i8254.o
emulators-objs-$(CONFIG_EMU_TIMER_HPET)+= timer/hpet.o
emulators-objs-$(CONFIG_EMU_TIMER_IMX_GPT)+= timer/imx_gpt.o
