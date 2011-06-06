#/**
# Copyright (c) 2010 Himanshu Chauhan.
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
# @file vmm_buddy_alloc.c
# @version 0.01
# @author Himanshu Chauhan (hschauhan@nulltrace.org)
# @brief source file for buddy allocator in VMM
# */

APP=buddy
OBJS=vmm_buddy_alloc.o buddy_driver.o
CC=gcc
CFLAGS=-I../core/include -DTEST_BUDDY_SYSTEM=1

$(APP): $(OBJS)
	@echo "[LINK]    $@"
	@$(CC) $(CFLAGS) -o $@ $(OBJS)

vmm_buddy_alloc.o: ../core/mm/vmm_buddy_alloc.c ../core/include/vmm_list.h ../core/include/mm/vmm_buddy_alloc.h
	@echo "[CC]      $@"
	@$(CC) $(CFLAGS) -c $<

buddy_driver.o: buddy_driver.c
	@echo "[CC]      $@"
	@$(CC) $(CFLAGS) -c $<

.PHONY: clean
clean:
	@echo "[RM] *.0 *~ $(APP)"
	@rm -rf *.o *~ $(APP)

