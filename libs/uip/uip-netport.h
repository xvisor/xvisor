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
 * @file uip-netport.h
 * @author Sukanto Ghosh (sukantoghosh@gmail.com)
 * @brief header file of netport interface of uIP
 */

#ifndef __UIP_NETPORT_H_
#define __UIP_NETPORT_H_

#include <net/vmm_mbuf.h>

/**
 * Initializes the netport interface for uIP 
 */
int uip_netport_init(void);

/**
 * Reads a data packet from rx_buffer and places it in uip_buf 
 *
 * returns uip_len
 */
int uip_netport_read(void);

/**
 * Transmits a data packet stored in uip_buf  
 */
void uip_netport_send(void);

/**
 * Transmits a data packet stored in the mbuf to itself
 */
void uip_netport_loopback_send(struct vmm_mbuf *mbuf);

#endif  /* __UIP_NETPORT_H_ */
