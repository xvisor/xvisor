#/**
# Copyright (c) 2013 Anup Patel.
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
# @brief list of driver objects
# */

drivers-objs-$(CONFIG_MOUSE_PS2)+= input/mouse/psmouse.o

psmouse-y += psmouse-base.o synaptics.o
psmouse-$(CONFIG_MOUSE_PS2_ALPS)	+= alps.o
psmouse-$(CONFIG_MOUSE_PS2_ELANTECH)	+= elantech.o
psmouse-$(CONFIG_MOUSE_PS2_LOGIPS2PP)	+= logips2pp.o
psmouse-$(CONFIG_MOUSE_PS2_SENTELIC)	+= sentelic.o
psmouse-$(CONFIG_MOUSE_PS2_TRACKPOINT)	+= trackpoint.o
psmouse-$(CONFIG_MOUSE_PS2_TOUCHKIT)	+= touchkit_ps2.o

%/psmouse.o: $(foreach obj,$(psmouse-y),%/$(obj))
	$(call merge_objs,$@,$^)

%/psmouse.dep: $(foreach dep,$(psmouse-y:.o=.dep),%/$(dep))
	$(call merge_deps,$@,$^)

