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
 * @file vmm_mbuf.c
 * @author Sukanto Ghosh <sukantoghosh@gmail.com>
 * @brief Network Buffer Handling
 *
 * The code has been adapted from NetBSD 5.1.2 src/sys/kern/uipc_mbuf.c
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

#include <vmm_error.h>
#include <vmm_macros.h>
#include <vmm_types.h>
#include <vmm_stdio.h>
#include <vmm_heap.h>
#include <vmm_host_aspace.h>
#include <vmm_modules.h>
#include <net/vmm_mbuf.h>
#include <libs/list.h>
#include <libs/stringlib.h>
#include <libs/mathlib.h>
#include <libs/mempool.h>


/*
 * Mbuffer pool.
 */

#define EPOOL_SLAB_COUNT		4

struct vmm_mbufpool_ctrl {
	struct mempool *mpool;
	struct mempool *epool_slabs[EPOOL_SLAB_COUNT];
};

static struct vmm_mbufpool_ctrl mbpctrl;

static u32 epool_slab_buf_size(u32 slab)
{
	switch (slab) {
	case 0:
		return 512;
	case 1:
		return 1024;
	case 2:
		return 1536;
	case 3:
		return 2048;
	default:
		break;
	};

	return 0;
}

static u32 epool_slab_buf_count(u32 pool_sz, u32 slab)
{
	u32 slab_size, buf_size, weight, total_weight;

	switch (slab) {
	case 0:
		weight = 1;
		break;
	case 1:
		weight = 1;
		break;
	case 2:
		weight = 4;
		break;
	case 3:
		weight = 2;
		break;
	default:
		return 0;
	};
	total_weight = 8;

	buf_size = epool_slab_buf_size(slab);
	if (!buf_size) {
		return 0;
	}

	slab_size = udiv32(pool_sz, total_weight) * weight;
	if (!slab_size) {
		return 0;
	}

	return udiv32(slab_size, buf_size);
}

int __init vmm_mbufpool_init(void)
{
	u32 slab, b_size, b_count, epool_sz;

	memset(&mbpctrl, 0, sizeof(mbpctrl));

	/* Create mbuf pool */
	b_size = sizeof(struct vmm_mbuf);
	b_count = CONFIG_NET_MBUF_POOL_SIZE;
	mbpctrl.mpool = mempool_ram_create(b_size,
					VMM_SIZE_TO_PAGE(b_size * b_count),
					VMM_MEMORY_FLAGS_NORMAL);
	if (!mbpctrl.mpool) {
		return VMM_ENOMEM;
	}

	/* Create ext slab pools */
	epool_sz = (CONFIG_NET_MBUF_EXT_POOL_SIZE_KB * 1024);
	for (slab = 0; slab < EPOOL_SLAB_COUNT; slab++) {
		b_size = epool_slab_buf_size(slab);
		b_count = epool_slab_buf_count(epool_sz, slab);
		if (b_count && b_size) {
			mbpctrl.epool_slabs[slab] = 
				mempool_ram_create(b_size,
					VMM_SIZE_TO_PAGE(b_size * b_count),
					VMM_MEMORY_FLAGS_NORMAL);
		} else {
			mbpctrl.epool_slabs[slab] = NULL;
		}
	}

	return VMM_OK;
}

void __exit vmm_mbufpool_exit(void)
{
	u32 slab;

	/* Destroy mbuf pool */
	if (mbpctrl.mpool) {
		mempool_destroy(mbpctrl.mpool);
	}

	/* Destroy ext slab pools */
	for (slab = 0; slab < EPOOL_SLAB_COUNT; slab++) {
		if (mbpctrl.epool_slabs[slab]) {
			mempool_destroy(mbpctrl.epool_slabs[slab]);
		}
	}
}

/*
 * Mbuffer utility routines.
 */

/*
 * Copy data from an mbuf chain starting "off" bytes from the beginning,
 * continuing for "len" bytes, into the indicated buffer.
 */
void m_copydata(struct vmm_mbuf *m, int off, int len, void *vp)
{
	unsigned count;
	void *cp = vp;

	if(m == NULL || vp == NULL)
		vmm_panic("%s: either m or vp is NULL\n", __func__);
	if (off < 0 || len < 0)
		vmm_panic("%s: off %d, len %d", __func__, off, len);
	while (off > 0) {
		if (m == NULL)
			vmm_panic("%s: m == NULL, off %d", __func__, off);
		if (off < m->m_len)
			break;
		off -= m->m_len;
		m = m->m_next;
	}
	while (len > 0) {
		count = min(m->m_len - off, len);
		memcpy(cp, mtod(m, char *) + off, count);
		len -= count;
		cp = (char *)cp + count;
		off = 0;
		m = m->m_next;
	}
}
VMM_EXPORT_SYMBOL(m_copydata);

static void mbuf_pool_free(struct vmm_mbuf *m)
{
	mempool_free(mbpctrl.mpool, m);
}

static void mbuf_heap_free(struct vmm_mbuf *m)
{
	vmm_free(m);
}

/*
 * Space allocation routines.
 * These are also available as macros
 * for critical paths.
 */
struct vmm_mbuf *m_get(int nowait, int flags)
{
	struct vmm_mbuf *m;

	/* TODO: implement non-blocking variant */

	m = mempool_zalloc(mbpctrl.mpool);
	if (m) {
		m->m_freefn = mbuf_pool_free;
	} else if (NULL != (m = vmm_malloc(sizeof (struct vmm_mbuf)))) {
		m->m_freefn = mbuf_heap_free;
	} else {
		return NULL;
	}

	INIT_LIST_HEAD(&m->m_list);
	m->m_next = NULL;
	m->m_data = NULL;
	m->m_len = 0;
	m->m_flags = flags;
	if (flags & M_PKTHDR) {
		m->m_pktlen = 0;
	}
	m->m_ref = 1;

	return m;
}
VMM_EXPORT_SYMBOL(m_get);

static void ext_pool_free(struct vmm_mbuf *m, void *ptr, u32 size, void *arg)
{
	struct mempool *mp = arg;

	mempool_free(mp, ptr);
}

void *m_ext_get(struct vmm_mbuf *m, u32 size, int how)
{
	void *buf;
	u32 slab;
	struct mempool *mp;

	mp = NULL;
	for (slab = 0; slab < EPOOL_SLAB_COUNT; slab++) {
		if (size <= epool_slab_buf_size(slab)) {
			mp = mbpctrl.epool_slabs[slab];
			break;
		}
	}

	if (mp && (buf = mempool_malloc(mp))) {
		MEXTADD(m, buf, size, ext_pool_free, mp);
	} else if ((buf = vmm_malloc(size))) {
		MEXTADD(m, buf, size, NULL, NULL);
	}

	return m->m_extbuf;
}
VMM_EXPORT_SYMBOL(m_ext_get);

/*
 * m_ext_free: release a reference to the mbuf external storage.
 * free the mbuf itself as well.
 */

void m_ext_free(struct vmm_mbuf *m)
{
	if (!(--(m->m_extref)) && !(m->m_flags & M_EXT_DONTFREE)) {
		/* dropping the last reference */
		if (m->m_extfree) {
			(*m->m_extfree)(m, m->m_extbuf, m->m_extlen, m->m_extarg);
		} else {
			vmm_free(m->m_extbuf);
		}
	}
	if (!(--(m->m_ref))) {
		m->m_freefn(m);
	}
}
VMM_EXPORT_SYMBOL(m_ext_free);

struct vmm_mbuf *m_free(struct vmm_mbuf *m)
{
	struct vmm_mbuf *n;

	MFREE(m, n);
	return (n);
}
VMM_EXPORT_SYMBOL(m_free);

void m_freem(struct vmm_mbuf *m)
{
	struct vmm_mbuf *n;

	if (m == NULL)
		return;
	do {
		MFREE(m, n);
		m = n;
	} while (m);
}
VMM_EXPORT_SYMBOL(m_freem);

