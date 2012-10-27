#/**
# Copyright (c) 2012 Sukanto Ghosh.
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
# @author Sukanto Ghosh (sukantoghosh@gmail.com)
# @brief list of network interface controller emulator objects
# */

emulators-objs-$(CONFIG_EMU_NET_LAN9118)+= net/lan9118.o
emulators-objs-$(CONFIG_EMU_NET_SMC91C111)+= net/smc91c111.o


