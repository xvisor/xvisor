/**
 * Copyright (c) 2012 Sukanto Ghosh.
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
 * @file vmm_net.h
 * @author Sukanto Ghosh <sukantoghosh@gmail.com>
 * @brief Network Layer interface layer API.
 */
#ifndef __VMM_NET_H_
#define __VMM_NET_H_

#include <vmm_types.h>

#define	VMM_NET_CLASS_IPRIORITY		1 

extern int vmm_mbufpool_init(void);
extern void vmm_mbufpool_exit(void);
extern int vmm_netswitch_init(void);
extern void vmm_netswitch_exit(void);
extern int vmm_netbridge_init(void);
extern void vmm_netbridge_exit(void);
extern int vmm_netport_init(void);
extern void vmm_netport_exit(void);
extern int vmm_netdev_init(void);
extern void vmm_netdev_exit(void);

#endif


