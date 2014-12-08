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
# @brief list of command objects to be build
# */

commands-objs-$(CONFIG_CMD_VERSION)+= cmd_version.o
commands-objs-$(CONFIG_CMD_RESET)+= cmd_reset.o
commands-objs-$(CONFIG_CMD_SHUTDOWN)+= cmd_shutdown.o
commands-objs-$(CONFIG_CMD_HOST)+= cmd_host.o
commands-objs-$(CONFIG_CMD_DEVTREE)+= cmd_devtree.o
commands-objs-$(CONFIG_CMD_VCPU)+= cmd_vcpu.o
commands-objs-$(CONFIG_CMD_GUEST)+= cmd_guest.o
commands-objs-$(CONFIG_CMD_MEMORY)+= cmd_memory.o
commands-objs-$(CONFIG_CMD_THREAD)+= cmd_thread.o
commands-objs-$(CONFIG_CMD_CHARDEV)+= cmd_chardev.o
commands-objs-$(CONFIG_CMD_STDIO)+= cmd_stdio.o
commands-objs-$(CONFIG_CMD_HEAP)+= cmd_heap.o
commands-objs-$(CONFIG_CMD_WALLCLOCK)+= cmd_wallclock.o
commands-objs-$(CONFIG_CMD_MODULE)+= cmd_module.o
commands-objs-$(CONFIG_CMD_PROFILE)+= cmd_profile.o

commands-objs-$(CONFIG_CMD_VSERIAL)+= cmd_vserial.o
commands-objs-$(CONFIG_CMD_VDISPLAY)+= cmd_vdisplay.o
commands-objs-$(CONFIG_CMD_VINPUT)+= cmd_vinput.o
commands-objs-$(CONFIG_CMD_VSCREEN)+= cmd_vscreen.o

commands-objs-$(CONFIG_CMD_RTCDEV)+= cmd_rtcdev.o
commands-objs-$(CONFIG_CMD_INPUT)+= cmd_input.o
commands-objs-$(CONFIG_CMD_FB)+= cmd_fb_mod.o
commands-objs-$(CONFIG_CMD_BLOCKDEV)+= cmd_blockdev.o
commands-objs-$(CONFIG_CMD_RBD)+= cmd_rbd.o
commands-objs-$(CONFIG_CMD_FLASH)+= cmd_flash.o

commands-objs-$(CONFIG_CMD_NET)+= cmd_net.o
commands-objs-$(CONFIG_CMD_IPCONFIG)+= cmd_ipconfig.o
commands-objs-$(CONFIG_CMD_PING)+= cmd_ping.o

commands-objs-$(CONFIG_CMD_VSTELNET)+= cmd_vstelnet.o
commands-objs-$(CONFIG_CMD_VFS)+= cmd_vfs.o

cmd_fb_mod-y += cmd_fb.o
cmd_fb_mod-$(CONFIG_CMD_FB_LOGO) += cmd_fb_logo.o

%/cmd_fb_mod.o: $(foreach obj,$(cmd_fb_mod-y),%/$(obj))
	$(call merge_objs,$@,$^)

%/cmd_fb_mod.dep: $(foreach dep,$(cmd_fb_mod-y:.o=.dep),%/$(dep))
	$(call merge_deps,$@,$^)

