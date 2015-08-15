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
 * @file vmm_mbuf.h
 * @author Sukanto Ghosh <sukantoghosh@gmail.com>
 * @brief Network Buffer Handling
 *
 * The code has been adapted from NetBSD 5.1.2 src/sys/sys/mbuff.c
 */

/*
 * Copyright (c) 1996, 1997, 1999, 2001, 2007 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center and Matt Thomas of 3am Software Foundry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982, 1986, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)mbuf.h	8.5 (Berkeley) 2/19/95
 */

#ifndef __VMM_MBUF_H_
#define __VMM_MBUF_H_

#include <vmm_types.h>
#include <vmm_macros.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <libs/list.h>

struct vmm_mbuf;

/* header at beginning of each mbuf: */
struct m_hdr {
	u32 mh_refcnt;
	struct vmm_mbuf *mh_next;	/* next buffer in chain */
	char *mh_data;			/* location of data */
	void (*mh_freefn)(struct vmm_mbuf *);
					/* free routine */
	int mh_len;			/* amount of data in this mbuf */
	int mh_flags;			/* flags; see below */
};

/*
 * record/packet header in first mbuf of chain; valid if M_PKTHDR set
 */
struct m_pkthdr {
	int	len;			/* total packet length */
};

struct m_ext {
	u32 ext_refcnt;			/* reference count */
	char *ext_buf;			/* start of buffer */
	u32 ext_size;			/* size of buffer, for ext_free */
	void (*ext_free)		/* free routine if not the usual */
		(struct vmm_mbuf *, void *, u32, void *);
	void *ext_arg;			/* argument for ext_free */
};

struct vmm_mbuf {
	struct dlist m_list;		/* for list of mbufs */
	struct m_hdr m_hdr;
	struct m_pkthdr m_pkthdr;
	struct m_ext m_ext;
};

#define	m_next		m_hdr.mh_next
#define	m_ref		m_hdr.mh_refcnt
#define	m_data		m_hdr.mh_data
#define	m_len		m_hdr.mh_len
#define	m_flags		m_hdr.mh_flags
#define m_pktlen	m_pkthdr.len
#define m_extbuf	m_ext.ext_buf
#define m_extlen	m_ext.ext_size
#define m_extref	m_ext.ext_refcnt
#define m_extfree	m_ext.ext_free
#define m_extarg	m_ext.ext_arg
#define m_freefn	m_hdr.mh_freefn

#define m_list_entry(l)	list_entry(l, struct vmm_mbuf, m_list)
/*
 * Macros for type conversion
 * mtod(m,t) -	convert mbuf pointer to data pointer of correct type
 */
#define	mtod(m, t)	((t)((m)->m_data))

/* mbuf flags */
#define	M_PKTHDR	0x00001	/* start of record */

/* additional flags for M_EXT mbufs */
#define	M_EXT_FLAGS	0xff000000
#define	M_EXT_RW	0x01000000	/* ext storage is writable */
#define	M_EXT_ROMAP	0x02000000	/* ext mapping is r-o at MMU */
#define	M_EXT_DONTFREE	0x04000000	/* extfree not required */
#define	M_EXT_POOL	0x08000000	/* ext storage is pool alloced */
#define	M_EXT_HEAP	0x10000000	/* ext storage is normal heap alloced */
#define	M_EXT_DMA	0x20000000	/* ext storage is dma heap alloced */

/* flags copied when copying m_pkthdr */
#define	M_COPYFLAGS	(M_PKTHDR)

/* flag copied when shallow-copying external storage */
#define	M_EXTCOPYFLAGS	(M_EXT_FLAGS)

#define MCLBYTES	2048

/*
 * mbuf allocation/deallocation macros:
 *
 *	MGET(struct vmm_mbuf *m, int how, int type)
 * allocates an mbuf and initializes it to contain no data
 */
#define	MGET(m, how, flags)	m = m_get((how), (flags))
#define	MGETHDR(m, how, flags)	m = m_get((how), (flags | M_PKTHDR))

#define	MCLINITREFERENCE(m)	m->m_ext.ext_refcnt = 1

/*
 * Macros for mbuf external storage.
 *
 * MEXTADD adds pre-allocated external storage to
 * a normal mbuf; the flag M_EXT is set upon success.
 *
 * MEXTMALLOC allocates external storage and adds it to
 * a normal mbuf; the flag M_EXT is set upon success.
 *
 * MCLGET allocates and adds an mbuf cluster to a normal mbuf;
 * the flag M_EXT is set upon success.
 */

#define	MEXTADD(m, buf, size, free, arg)				\
do {									\
	MCLINITREFERENCE(m);						\
	(m)->m_data = (m)->m_extbuf = (void *)(buf);			\
	(m)->m_flags |= M_EXT_RW;					\
	(m)->m_ext.ext_size = (size);					\
	(m)->m_ext.ext_free = (free);					\
	(m)->m_ext.ext_arg = (arg);					\
} while (/* CONSTCOND */ 0)

#define	MEXTMALLOC(m, size, how)	m_ext_get(m, size, how)

#define MCLGET(m, how)			MEXTMALLOC(m, MCLBYTES, how)

/*
 * Reset the data pointer on an mbuf.
 */
#define	MRESETDATA(m)							\
do {									\
	(m)->m_data = (m)->m_extbuf;					\
	(m)->m_len = 0;							\
} while (/* CONSTCOND */ 0)

/*
 * MFREE(struct vmm_mbuf *m, struct vmm_mbuf *n)
 * Free a single mbuf and associated external storage.
 * Place the successor, if any, in n.
 */
#define	MFREE(m, n)							\
do {									\
	(n) = (m)->m_next;						\
	m_ext_free(m);							\
} while (/* CONSTCOND */ 0)

#define MCLADDREFERENCE(o)		o->m_extref++
#define MADDREFERENCE(o)		o->m_ref++

/*
 * Determine if an mbuf's data area is read-only.  This is true
 * if external storage is read-only mapped, or not marked as R/W,
 * or referenced by more than one mbuf.
 */
#define	M_READONLY(m)							\
	  (((m)->m_flags & (M_EXT_ROMAP|M_EXT_RW)) != M_EXT_RW ||	\
	  ((m)->m_extref > 1))

#define	M_UNWRITABLE(__m, __len)					\
	((__m)->m_len < (__len) || M_READONLY((__m)))
/*
 * Determine if an mbuf's data area is read-only at the MMU.
 */
#define	M_ROMAP(m)	(!!((m)->m_flags & (M_EXT_ROMAP)))

/*
 * Compute the amount of space available
 * before the current start of data in an mbuf.
 */
#define	_M_LEADINGSPACE(m)						\
	((m)->m_data - (m)->m_extbuf)

#define	M_LEADINGSPACE(m)						\
	(M_READONLY((m)) ? 0 : _M_LEADINGSPACE((m)))

/*
 * Compute the amount of space available
 * after the end of data in an mbuf.
 */
#define	_M_TRAILINGSPACE(m)						\
	((m)->m_extbuf + (m)->m_extlen - ((m)->m_data + (m)->m_len))

#define	M_TRAILINGSPACE(m)						\
	(M_READONLY((m)) ? 0 : _M_TRAILINGSPACE((m)))

/*
 * Compute the address of an mbuf's data area.
 */
#define	M_BUFADDR(m)	((m)->m_data)

/*
 * mbuf allocation types.
 */
enum vmm_mbuf_alloc_types {
	VMM_MBUF_ALLOC_DEFAULT=0,
	VMM_MBUF_ALLOC_DMA=1
};

/*
 * mbuf APIs.
 */
struct vmm_mbuf *m_free(struct vmm_mbuf *m);
struct vmm_mbuf *m_get(int nowait, int flags);
void *m_ext_get(struct vmm_mbuf *m, u32 size, enum vmm_mbuf_alloc_types how);
void m_copydata(struct vmm_mbuf *m, int off, int len, void *vp);
void m_freem(struct vmm_mbuf *m);
void m_ext_free(struct vmm_mbuf *m);
void m_dump(struct vmm_mbuf *m);

/*
 * mbuf pool initializaton and exit.
 */
int vmm_mbufpool_init(void);
void vmm_mbufpool_exit(void);

#endif /* __VMM_MBUF_H_ */

