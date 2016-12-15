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
 * @file mailbox.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief Common code for Mailbox controllers and users.
 *
 * The source has been largely adapted from Linux
 * drivers/mailbox/mailbox.c
 *
 * Mailbox: Common code for Mailbox controllers and users
 *
 * Copyright (C) 2013-2014 Linaro Ltd.
 * Author: Jassi Brar <jassisinghbrar@gmail.com>
 *
 * The original code is licensed under the GPL.
 */

#include <vmm_error.h>
#include <vmm_delay.h>
#include <vmm_heap.h>
#include <vmm_mutex.h>
#include <vmm_modules.h>
#include <libs/list.h>
#include <libs/bitops.h>
#include <drv/mailbox_client.h>
#include <drv/mailbox_controller.h>

#include "mailbox.h"

#define MODULE_DESC			"Mailbox Framework"
#define MODULE_AUTHOR			"Anup Patel"
#define MODULE_LICENSE			"GPL"
#define MODULE_IPRIORITY		(0)
#define	MODULE_INIT			NULL
#define	MODULE_EXIT			NULL

static LIST_HEAD(mbox_cons);
static DEFINE_MUTEX(con_mutex);

static int add_to_rbuf(struct mbox_chan *chan, void *mssg)
{
	int idx;
	irq_flags_t flags;

	vmm_spin_lock_irqsave(&chan->lock, flags);

	/* See if there is any space left */
	if (chan->msg_count == MBOX_TX_QUEUE_LEN) {
		vmm_spin_unlock_irqrestore(&chan->lock, flags);
		return VMM_ENOSPC;
	}

	idx = chan->msg_free;
	chan->msg_data[idx] = mssg;
	chan->msg_count++;

	if (idx == MBOX_TX_QUEUE_LEN - 1)
		chan->msg_free = 0;
	else
		chan->msg_free++;

	vmm_spin_unlock_irqrestore(&chan->lock, flags);

	return idx;
}

static void txdone_hrtimer(struct vmm_timer_event *hrtimer);

static void msg_submit(struct mbox_chan *chan)
{
	unsigned count, idx;
	irq_flags_t flags;
	void *data;
	int err = VMM_EBUSY;

	vmm_spin_lock_irqsave(&chan->lock, flags);

	if (!chan->msg_count || chan->active_req)
		goto exit;

	count = chan->msg_count;
	idx = chan->msg_free;
	if (idx >= count)
		idx -= count;
	else
		idx += MBOX_TX_QUEUE_LEN - count;

	data = chan->msg_data[idx];

	if (chan->cl->tx_prepare)
		chan->cl->tx_prepare(chan->cl, data);
	/* Try to submit a message to the MBOX controller */
	err = chan->mbox->ops->send_data(chan, data);
	if (!err) {
		chan->active_req = data;
		chan->msg_count--;
	}
exit:
	vmm_spin_unlock_irqrestore(&chan->lock, flags);

	if (!err && (chan->txdone_method & TXDONE_BY_POLL)) {
		/* kick start the timer immediately to avoid delays */
		vmm_timer_event_stop(&chan->mbox->poll_hrt);
		txdone_hrtimer(&chan->mbox->poll_hrt);
	}
}

static void tx_tick(struct mbox_chan *chan, int r)
{
	irq_flags_t flags;
	void *mssg;

	vmm_spin_lock_irqsave(&chan->lock, flags);
	mssg = chan->active_req;
	chan->active_req = NULL;
	vmm_spin_unlock_irqrestore(&chan->lock, flags);

	/* Submit next message */
	msg_submit(chan);

	/* Notify the client */
	if (mssg && chan->cl->tx_done)
		chan->cl->tx_done(chan->cl, mssg, r);

	if (chan->cl->tx_block)
		vmm_completion_complete(&chan->tx_complete);
}

static void txdone_hrtimer(struct vmm_timer_event *hrtimer)
{
	struct mbox_controller *mbox =
		container_of(hrtimer, struct mbox_controller, poll_hrt);
	bool txdone, resched = false;
	int i;

	for (i = 0; i < mbox->num_chans; i++) {
		struct mbox_chan *chan = &mbox->chans[i];

		if (chan->active_req && chan->cl) {
			txdone = chan->mbox->ops->last_tx_done(chan);
			if (txdone)
				tx_tick(chan, 0);
			else
				resched = true;
		}
	}

	if (resched) {
		vmm_timer_event_start(hrtimer,
				(u64)mbox->txpoll_period * 1000000ULL);
	}
}

/**
 * mbox_chan_received_data - A way for controller driver to push data
 *				received from remote to the upper layer.
 * @chan: Pointer to the mailbox channel on which RX happened.
 * @mssg: Client specific message typecasted as void *
 *
 * After startup and before shutdown any data received on the chan
 * is passed on to the API via atomic mbox_chan_received_data().
 * The controller should ACK the RX only after this call returns.
 */
void mbox_chan_received_data(struct mbox_chan *chan, void *mssg)
{
	/* No buffering the received data */
	if (chan->cl->rx_callback)
		chan->cl->rx_callback(chan->cl, mssg);
}
VMM_EXPORT_SYMBOL_GPL(mbox_chan_received_data);

/**
 * mbox_chan_txdone - A way for controller driver to notify the
 *			framework that the last TX has completed.
 * @chan: Pointer to the mailbox chan on which TX happened.
 * @r: Status of last TX - OK or ERROR
 *
 * The controller that has IRQ for TX ACK calls this atomic API
 * to tick the TX state machine. It works only if txdone_irq
 * is set by the controller.
 */
void mbox_chan_txdone(struct mbox_chan *chan, int r)
{
	if (unlikely(!(chan->txdone_method & TXDONE_BY_IRQ))) {
		vmm_lerror(chan->mbox->dev->name,
		       "Controller can't run the TX ticker\n");
		return;
	}

	tx_tick(chan, r);
}
VMM_EXPORT_SYMBOL_GPL(mbox_chan_txdone);

/**
 * mbox_client_txdone - The way for a client to run the TX state machine.
 * @chan: Mailbox channel assigned to this client.
 * @r: Success status of last transmission.
 *
 * The client/protocol had received some 'ACK' packet and it notifies
 * the API that the last packet was sent successfully. This only works
 * if the controller can't sense TX-Done.
 */
void mbox_client_txdone(struct mbox_chan *chan, int r)
{
	if (unlikely(!(chan->txdone_method & TXDONE_BY_ACK))) {
		vmm_lerror(chan->mbox->dev->name,
			   "Client can't run the TX ticker\n");
		return;
	}

	tx_tick(chan, r);
}
VMM_EXPORT_SYMBOL_GPL(mbox_client_txdone);

/**
 * mbox_client_peek_data - A way for client driver to pull data
 *			received from remote by the controller.
 * @chan: Mailbox channel assigned to this client.
 *
 * A poke to controller driver for any received data.
 * The data is actually passed onto client via the
 * mbox_chan_received_data()
 * The call can be made from atomic context, so the controller's
 * implementation of peek_data() must not sleep.
 *
 * Return: True, if controller has, and is going to push after this,
 *          some data.
 *         False, if controller doesn't have any data to be read.
 */
bool mbox_client_peek_data(struct mbox_chan *chan)
{
	if (chan->mbox->ops->peek_data)
		return chan->mbox->ops->peek_data(chan);

	return FALSE;
}
VMM_EXPORT_SYMBOL_GPL(mbox_client_peek_data);

/**
 * mbox_send_message -	For client to submit a message to be
 *				sent to the remote.
 * @chan: Mailbox channel assigned to this client.
 * @mssg: Client specific message typecasted.
 *
 * For client to submit data to the controller destined for a remote
 * processor. If the client had set 'tx_block', the call will return
 * either when the remote receives the data or when 'tx_tout' millisecs
 * run out.
 *  In non-blocking mode, the requests are buffered by the API and a
 * non-negative token is returned for each queued request. If the request
 * is not queued, a negative token is returned. Upon failure or successful
 * TX, the API calls 'tx_done' from atomic context, from which the client
 * could submit yet another request.
 * The pointer to message should be preserved until it is sent
 * over the chan, i.e, tx_done() is made.
 * This function could be called from atomic context as it simply
 * queues the data and returns a token against the request.
 *
 * Return: Non-negative integer for successful submission (non-blocking mode)
 *	or transmission over chan (blocking mode).
 *	Negative value denotes failure.
 */
int mbox_send_message(struct mbox_chan *chan, void *mssg)
{
	int t;

	if (!chan || !chan->cl)
		return VMM_EINVALID;

	t = add_to_rbuf(chan, mssg);
	if (t < 0) {
		vmm_lerror(chan->mbox->dev->name,
			   "Try increasing MBOX_TX_QUEUE_LEN\n");
		return t;
	}

	msg_submit(chan);

	if (chan->cl->tx_block && chan->active_req) {
		u64 wait;
		int ret;

		if (!chan->cl->tx_tout) /* wait forever */
			wait = 3600000000000ULL;
		else
			wait = (u64)chan->cl->tx_tout * 1000000ULL;

		ret = vmm_completion_wait_timeout(&chan->tx_complete, &wait);
		if (ret == 0) {
			t = VMM_EIO;
			tx_tick(chan, VMM_EIO);
		}
	}

	return t;
}
VMM_EXPORT_SYMBOL_GPL(mbox_send_message);

/**
 * mbox_request_channel - Request a mailbox channel.
 * @cl: Identity of the client requesting the channel.
 * @index: Index of mailbox specifier in 'mboxes' property.
 *
 * The Client specifies its requirements and capabilities while asking for
 * a mailbox channel. It can't be called from atomic context.
 * The channel is exclusively allocated and can't be used by another
 * client before the owner calls mbox_free_channel.
 * After assignment, any packet received on this channel will be
 * handed over to the client via the 'rx_callback'.
 * The framework holds reference to the client, so the mbox_client
 * structure shouldn't be modified until the mbox_free_channel returns.
 *
 * Return: Pointer to the channel assigned to the client if successful.
 *		ERR_PTR for request failure.
 */
struct mbox_chan *mbox_request_channel(struct mbox_client *cl, int index)
{
	struct vmm_device *dev = cl->dev;
	struct mbox_controller *mbox;
	struct vmm_devtree_phandle_args spec;
	struct mbox_chan *chan;
	irq_flags_t flags;
	int ret;

	if (!dev || !dev->of_node) {
		vmm_printf("%s: No owner device node\n", __func__);
		return VMM_ERR_PTR(VMM_ENODEV);
	}

	vmm_mutex_lock(&con_mutex);

	if (vmm_devtree_parse_phandle_with_args(dev->of_node, "mboxes",
				       "#mbox-cells", index, &spec)) {
		vmm_lerror(dev->name, "%s: can't parse \"mboxes\" property\n",
			   __func__);
		vmm_mutex_unlock(&con_mutex);
		return VMM_ERR_PTR(VMM_ENODEV);
	}

	chan = VMM_ERR_PTR(VMM_EPROBE_DEFER);
	list_for_each_entry(mbox, &mbox_cons, node)
		if (mbox->dev->of_node == spec.np) {
			chan = mbox->of_xlate(mbox, &spec);
			break;
		}

	vmm_devtree_dref_node(spec.np);

	if (VMM_IS_ERR(chan)) {
		vmm_mutex_unlock(&con_mutex);
		return chan;
	}

	if (chan->cl) {
		vmm_lerror(dev->name, "%s: mailbox not free\n", __func__);
		vmm_mutex_unlock(&con_mutex);
		return VMM_ERR_PTR(VMM_EBUSY);
	}

	vmm_spin_lock_irqsave(&chan->lock, flags);
	chan->msg_free = 0;
	chan->msg_count = 0;
	chan->active_req = NULL;
	chan->cl = cl;
	INIT_COMPLETION(&chan->tx_complete);

	if (chan->txdone_method	== TXDONE_BY_POLL && cl->knows_txdone)
		chan->txdone_method |= TXDONE_BY_ACK;

	vmm_spin_unlock_irqrestore(&chan->lock, flags);

	ret = chan->mbox->ops->startup(chan);
	if (ret) {
		vmm_lerror(dev->name,
			   "Unable to startup the chan (%d)\n", ret);
		mbox_free_channel(chan);
		chan = VMM_ERR_PTR(ret);
	}

	vmm_mutex_unlock(&con_mutex);
	return chan;
}
VMM_EXPORT_SYMBOL_GPL(mbox_request_channel);

struct mbox_chan *mbox_request_channel_byname(struct mbox_client *cl,
					      const char *name)
{
	struct vmm_devtree_node *np = cl->dev->of_node;
	struct vmm_devtree_attr *attr;
	const char *mbox_name;
	int index = 0;

	if (!np) {
		vmm_lerror(cl->dev->name,
			   "%s() currently only supports DT\n", __func__);
		return VMM_ERR_PTR(VMM_EINVALID);
	}

	if (!vmm_devtree_attrval(np, "mbox-names")) {
		vmm_lerror(cl->dev->name,
		"%s() requires an \"mbox-names\" attribute\n", __func__);
		return VMM_ERR_PTR(VMM_EINVALID);
	}

	vmm_devtree_for_each_string(np, "mbox-names", attr, mbox_name) {
		if (!strncmp(name, mbox_name, strlen(name)))
			break;
		index++;
	}

	return mbox_request_channel(cl, index);
}
VMM_EXPORT_SYMBOL_GPL(mbox_request_channel_byname);

/**
 * mbox_free_channel - The client relinquishes control of a mailbox
 *			channel by this call.
 * @chan: The mailbox channel to be freed.
 */
void mbox_free_channel(struct mbox_chan *chan)
{
	irq_flags_t flags;

	if (!chan || !chan->cl)
		return;

	chan->mbox->ops->shutdown(chan);

	/* The queued TX requests are simply aborted, no callbacks are made */
	vmm_spin_lock_irqsave(&chan->lock, flags);
	chan->cl = NULL;
	chan->active_req = NULL;
	if (chan->txdone_method == (TXDONE_BY_POLL | TXDONE_BY_ACK))
		chan->txdone_method = TXDONE_BY_POLL;

	vmm_spin_unlock_irqrestore(&chan->lock, flags);
}
VMM_EXPORT_SYMBOL_GPL(mbox_free_channel);

static struct mbox_chan *
of_mbox_index_xlate(struct mbox_controller *mbox,
		    const struct vmm_devtree_phandle_args *sp)
{
	int ind = sp->args[0];

	if (ind >= mbox->num_chans)
		return VMM_ERR_PTR(VMM_EINVALID);

	return &mbox->chans[ind];
}

/**
 * mbox_controller_register - Register the mailbox controller
 * @mbox:	Pointer to the mailbox controller.
 *
 * The controller driver registers its communication channels
 */
int mbox_controller_register(struct mbox_controller *mbox)
{
	int i, txdone;

	/* Sanity check */
	if (!mbox || !mbox->dev || !mbox->ops || !mbox->num_chans)
		return VMM_EINVALID;

	if (mbox->txdone_irq)
		txdone = TXDONE_BY_IRQ;
	else if (mbox->txdone_poll)
		txdone = TXDONE_BY_POLL;
	else /* It has to be ACK then */
		txdone = TXDONE_BY_ACK;

	if (txdone == TXDONE_BY_POLL) {
		INIT_TIMER_EVENT(&mbox->poll_hrt, txdone_hrtimer, NULL);
	}

	for (i = 0; i < mbox->num_chans; i++) {
		struct mbox_chan *chan = &mbox->chans[i];

		chan->cl = NULL;
		chan->mbox = mbox;
		chan->txdone_method = txdone;
		INIT_SPIN_LOCK(&chan->lock);
	}

	if (!mbox->of_xlate)
		mbox->of_xlate = of_mbox_index_xlate;

	vmm_mutex_lock(&con_mutex);
	list_add_tail(&mbox->node, &mbox_cons);
	vmm_mutex_unlock(&con_mutex);

	return 0;
}
VMM_EXPORT_SYMBOL_GPL(mbox_controller_register);

/**
 * mbox_controller_unregister - Unregister the mailbox controller
 * @mbox:	Pointer to the mailbox controller.
 */
void mbox_controller_unregister(struct mbox_controller *mbox)
{
	int i;

	if (!mbox)
		return;

	vmm_mutex_lock(&con_mutex);

	list_del(&mbox->node);

	for (i = 0; i < mbox->num_chans; i++)
		mbox_free_channel(&mbox->chans[i]);

	if (mbox->txdone_poll)
		vmm_timer_event_stop(&mbox->poll_hrt);

	vmm_mutex_unlock(&con_mutex);
}
VMM_EXPORT_SYMBOL_GPL(mbox_controller_unregister);

VMM_DECLARE_MODULE(MODULE_DESC,
			MODULE_AUTHOR,
			MODULE_LICENSE,
			MODULE_IPRIORITY,
			MODULE_INIT,
			MODULE_EXIT);
