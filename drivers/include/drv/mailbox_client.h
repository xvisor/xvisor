/**
 * Copyright (c) 2016 Anup Patel.
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
 * @file mailbox_client.h
 * @author Anup Patel (anup@brainfault.org)
 * @brief Mailbox client API interface.
 *
 * The source has been largely adapted from Linux
 * include/linux/mailbox_client.h
 *
 * Copyright (C) 2013-2014 Linaro Ltd.
 * Author: Jassi Brar <jassisinghbrar@gmail.com>
 *
 * The original code is licensed under the GPL.
 */

#ifndef __MAILBOX_CLIENT_H__
#define __MAILBOX_CLIENT_H__

#include <vmm_devtree.h>
#include <vmm_devdrv.h>

struct mbox_chan;

/**
 * struct mbox_client - User of a mailbox
 * @dev:		The client device
 * @tx_block:		If the mbox_send_message should block until data is
 *			transmitted.
 * @tx_tout:		Max block period in ms before TX is assumed failure
 * @knows_txdone:	If the client could run the TX state machine. Usually
 *			if the client receives some ACK packet for transmission.
 *			Unused if the controller already has TX_Done/RTR IRQ.
 * @rx_callback:	Atomic callback to provide client the data received
 * @tx_prepare: 	Atomic callback to ask client to prepare the payload
 *			before initiating the transmission if required.
 * @tx_done:		Atomic callback to tell client of data transmission
 */
struct mbox_client {
	struct vmm_device *dev;
	bool tx_block;
	unsigned long tx_tout;
	bool knows_txdone;

	void (*rx_callback)(struct mbox_client *cl, void *mssg);
	void (*tx_prepare)(struct mbox_client *cl, void *mssg);
	void (*tx_done)(struct mbox_client *cl, void *mssg, int r);
};

struct mbox_chan *mbox_request_channel_byname(struct mbox_client *cl,
					      const char *name);
struct mbox_chan *mbox_request_channel(struct mbox_client *cl, int index);
int mbox_send_message(struct mbox_chan *chan, void *mssg);
void mbox_client_txdone(struct mbox_chan *chan, int r); /* atomic */
bool mbox_client_peek_data(struct mbox_chan *chan); /* atomic */
void mbox_free_channel(struct mbox_chan *chan); /* may sleep */

#endif /* __MAILBOX_CLIENT_H__ */
