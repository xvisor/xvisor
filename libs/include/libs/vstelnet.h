/**
 * Copyright (c) 2013 Anup Patel.
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
 * @file vstelnet.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief vserial telnet library interface
 */

#ifndef __VSTELNET_H_
#define __VSTELNET_H_

#include <vmm_types.h>
#include <vmm_threads.h>
#include <vmm_vserial.h>
#include <libs/list.h>
#include <libs/netstack.h>

#define VSTELNET_IPRIORITY			(NETSTACK_IPRIORITY + 1)
#define VSTELNET_TXBUF_SIZE			4096
#define VSTELNET_RXTIMEOUT_MS			400

struct vstelnet {
	/* tcp port number */
	u32 port;

	/* socket pointers */
	struct netstack_socket *sk;

	/* active connection */
	struct netstack_socket *active_sk;

	/* tx buffer */
	u8 tx_buf[VSTELNET_TXBUF_SIZE];
	u32 tx_buf_head;
	u32 tx_buf_tail;
	u32 tx_buf_count;
	vmm_spinlock_t tx_buf_lock;

	/* vserial port */
	struct vmm_vserial *vser;

	/* underlying thread */
	struct vmm_thread *thread;

	/* list head */
	struct dlist head;
};

/** Valid vstelnet port numbers */
static inline bool vstelnet_valid_port(u32 port)
{
	if (port < 1024) {
		/* Well-known port number not allowed */
		return FALSE;
	}

	if (65535 < port) {
		/* Maximum port number is 65535 */
		return FALSE;
	}

	return TRUE;
}

/** Create vstelnet instance */
struct vstelnet *vstelnet_create(u32 port, const char *vser_name);

/** Destroy vstelnet instance */
int vstelnet_destroy(struct vstelnet *vst);

/** Find vstelnet based on port number */
struct vstelnet *vstelnet_find(u32 port);

/** Get vstelnet based on index */
struct vstelnet *vstelnet_get(int index);

/** Count vstelnet instances */
u32 vstelnet_count(void);

#endif /* __VSTELNET_H_ */
