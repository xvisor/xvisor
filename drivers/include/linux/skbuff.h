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
 * @file skbuff.h
 * @author Sukanto Ghosh <sukantoghosh@gmail.com>
 * @brief Linux skbuff compatibility APIs 
 *
 * Portions of this file has been adapted from following linux files-
 * 	include/linux/skbuff.h
 * 	net/core/skbuff.c
 *
 * Their authors are
 *	Alan Cox, <gw4pts@gw4pts.ampr.org>
 *	Florian La Roche, <rzsfl@rz.uni-sb.de>
 *
 * The original code is licensed under the GPL.
 */

#ifndef __LINUX_SKBUFF_H_
#define __LINUX_SKBUFF_H_

#include <net/vmm_mbuf.h>
#include <vmm_macros.h>
#include <linux/printk.h>
#include <linux/types.h>

/**
 * Our goal is not to be 100% compatibilty with sk_buff (as much as 
 * possible by not defining data structure other than vmm_mbuf)
 *
 * In general the 4 primary skb pointers are emulated as follows:
 * skb->head = mbuf->m_extbuf
 * skb->data = mbuf->m_data
 * skb->tail = (mbuf->m_data + mbuf->m_len)
 * skb->end  = (mbuf->m_extbuf + mbuf->m_extlen)
 */
#define sk_buff	vmm_mbuf

#define skb_head(skb)	(skb->m_extbuf)
#define skb_data(skb)	(skb->m_data)
#define skb_len(skb)	(skb->m_len)

static inline struct sk_buff *alloc_skb(unsigned int size,
					u8 dummy_priority)
{
	struct vmm_mbuf *m;
	MGETHDR(m, 0, 0);
	if(m == NULL) {
		return NULL;
	}
	MEXTMALLOC(m, size, 0);
	if(m->m_extbuf == NULL) {
		m_freem(m);
		return NULL;
	}
	return m;
}

static inline void skb_reserve(struct sk_buff *skb, int len)
{
	skb_data(skb) += len;
}

static inline unsigned char *skb_tail_pointer(const struct sk_buff *skb)
{
	return ((unsigned char *)skb_data(skb) + skb_len(skb));
}

static inline void skb_reset_tail_pointer(struct sk_buff *skb)
{
	skb_len(skb) = 0;
}

static inline void skb_set_tail_pointer(struct sk_buff *skb, const int offset)
{
	skb_len(skb) = offset;
}

static inline int skb_is_nonlinear(const struct sk_buff *skb)
{
	return (skb->m_next != NULL);
}

#define SKB_LINEAR_ASSERT(skb)  	BUG_ON(skb_is_nonlinear(skb))

static inline unsigned char *__skb_put(struct sk_buff *skb, unsigned int len)
{
	unsigned char *tmp = skb_tail_pointer(skb);
	SKB_LINEAR_ASSERT(skb);
	skb_len(skb) += len;
	skb->m_pktlen += len;
	return tmp;
}

/**
 * skb_put - add data to a buffer
 * @skb: buffer to use
 * @len: amount of data to add
 * 
 * This function extends the used data area of the buffer. If this would
 * exceed the total buffer size the kernel will panic. A pointer to the
 * first byte of the extra data is returned.
 */
static inline unsigned char *skb_put(struct sk_buff *skb, unsigned int len)
{
	unsigned char *tmp = __skb_put(skb, len);
	if(unlikely((skb_data(skb) + skb_len(skb)) > 
				(skb_head(skb) + skb->m_extlen))) {
		vmm_panic("%s: skb->tail crossing skb->end\n", __func__);
	}
	return tmp;
}

/**
 * skb_push - add data to the start of a buffer
 * @skb: buffer to use
 * @len: amount of data to add
 * 
 * This function extends the used data area of the buffer at the buffer
 * start. If this would exceed the total buffer headroom the kernel will
 * panic. A pointer to the first byte of the extra data is returned.
 */
static inline unsigned char *skb_push(struct sk_buff *skb, unsigned int len)
{
	skb_data(skb) -= len;
	skb_len(skb)  += len;
	skb->m_pktlen += len;
	if (unlikely(skb_data(skb) < skb_head(skb))) {
		vmm_panic("%s: skb->data crossing skb->head\n", __func__);
	}
	return (unsigned char *)(skb_data(skb));
}

#ifndef NET_IP_ALIGN
#define NET_IP_ALIGN	2
#endif

#ifndef NET_SKB_PAD
#define NET_SKB_PAD	32
#endif

/**
 * dev_alloc_skb - allocate an skbuff for receiving
 * @length: length to allocate
 *
 * Allocate a new &sk_buff and assign it a usage count of one. The
 * buffer has unspecified headroom built in. Users should allocate
 * the headroom they think they need without accounting for the
 * built in space. The built in space is used for optimisations.
 *
 * NULL is returned if there is no free memory.
 */
static inline struct sk_buff *dev_alloc_skb(unsigned int length)
{
	struct sk_buff *skb;
	skb = alloc_skb(length + NET_SKB_PAD, 0);
	if(likely(skb != NULL))
		skb_reserve(skb, NET_SKB_PAD);
	return skb;
}

#define dev_kfree_skb(skb)		m_freem((struct vmm_mbuf *) skb)

#define netdev_alloc_skb(dev, length)	dev_alloc_skb(length)

#define skb_checksum_none_assert(skb)

static inline void skb_copy_from_linear_data(const struct sk_buff *skb,
					     void *to,
                                             const unsigned int len)
{
        memcpy(to, skb_data(skb), len);
}

static inline void skb_copy_and_csum_dev(const struct sk_buff *skb, void *to)
{
	/* FIXME: Add csum */
	skb_copy_from_linear_data(skb, to, skb_len(skb));
}

#endif /* __LINUX_SKBUFF_H_ */
