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

#include <vmm_macros.h>
#include <vmm_types.h>
#include <vmm_stdio.h>
#include <vmm_string.h>
#include <vmm_heap.h>
#include <net/vmm_mbuf.h>

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

	if (off < 0 || len < 0)
		vmm_panic("m_copydata: off %d, len %d", off, len);
	while (off > 0) {
		if (m == NULL)
			vmm_panic("m_copydata: m == NULL, off %d", off);
		if (off < m->m_len)
			break;
		off -= m->m_len;
		m = m->m_next;
	}
	while (len > 0) {
		if (m == NULL)
			vmm_panic("m_copydata: m == NULL, len %d", len);
		count = min(m->m_len - off, len);
		vmm_memcpy(cp, mtod(m, char *) + off, count);
		len -= count;
		cp = (char *)cp + count;
		off = 0;
		m = m->m_next;
	}
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

	m = vmm_malloc(sizeof(struct vmm_mbuf));
	if (m == NULL)
		return NULL;

	m->m_next = NULL;
	m->m_data = NULL;
	m->m_len = 0;
	m->m_flags = flags;
	if(flags & M_PKTHDR)
		m->m_pktlen = 0;
	m->m_ref = 1;

	return m;
}

/*
 * m_ext_free: release a reference to the mbuf external storage.
 * free the mbuf itself as well.
 */

void m_ext_free(struct vmm_mbuf *m)
{
	if (!(--(m->m_extref))) {
		/* dropping the last reference */
		if (m->m_extfree) {
			(*m->m_extfree)(m, m->m_extbuf, m->m_extlen, m->m_extarg);
		} else {
			vmm_free(m->m_extbuf);
		}
	}
	if (!(--(m->m_ref))) {
		vmm_free(m);
	}
}

struct vmm_mbuf *m_free(struct vmm_mbuf *m)
{
	struct vmm_mbuf *n;

	MFREE(m, n);
	return (n);
}

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


