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
 * @file lifo.c
 * @author Anup Patel (anup@brainfault.org)
 * @brief source file for generic last-in-first-out queue.
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <libs/stringlib.h>
#include <libs/lifo.h>

struct lifo *lifo_alloc(u32 element_size, u32 element_count)
{
	struct lifo *l;

	if (!element_size || !element_count) {
		return NULL;
	}

	l = vmm_zalloc(sizeof(struct lifo));
	if (!l) {
		return NULL;
	}

	l->elements = vmm_zalloc(element_size * element_count);
	if (!l->elements) {
		vmm_free(l);
		return NULL;
	}
	l->element_size = element_size;
	l->element_count = element_count;

	INIT_SPIN_LOCK(&l->lock);
	l->head_pos = 0;
	l->avail_count = 0;

	return l;
}

int lifo_free(struct lifo *l)
{
	vmm_free(l->elements);
	vmm_free(l);

	return VMM_OK;
}

#define __lifo_isempty(l)	(((l)->avail_count) ? FALSE : TRUE)

bool lifo_isempty(struct lifo *l)
{
	bool ret;
	irq_flags_t flags;

	if (!l) {
		return TRUE;
	}

	vmm_spin_lock_irqsave(&l->lock, flags);
	ret = __lifo_isempty(l);
	vmm_spin_unlock_irqrestore(&l->lock, flags);

	return ret;
}

#define __lifo_isfull(l)	(((l)->avail_count >= (l)->element_count) ? \
							TRUE : FALSE)

bool lifo_isfull(struct lifo *l)
{
	bool ret;
	irq_flags_t flags;

	if (!l) {
		return FALSE;
	}

	vmm_spin_lock_irqsave(&l->lock, flags);
	ret = __lifo_isfull(l);
	vmm_spin_unlock_irqrestore(&l->lock, flags);

	return ret;
}

bool lifo_enqueue(struct lifo *l, void *src, bool overwrite)
{
	bool ret = FALSE;
	irq_flags_t flags;

	if (!l || !src) {
		return FALSE;
	}

	vmm_spin_lock_irqsave(&l->lock, flags);

	if (overwrite && __lifo_isfull(l)) {
		l->avail_count--;
	}

	if (!__lifo_isfull(l)) {
		switch(l->element_size) {
		case 1:
			*((u8 *)(l->elements + l->head_pos))
			= *((u8 *)src);
			break;
		case 2:
			*((u16 *)(l->elements + (l->head_pos * 2)))
			= *((u16 *)src);
			break;
		case 4:
			*((u32 *)(l->elements + (l->head_pos * 4)))
			= *((u32 *)src);
			break;
		case 8:
			*((u64 *)(l->elements + (l->head_pos * 8)))
			= *((u64 *)src);
			break;
		default:
			memcpy(l->elements + (l->head_pos * l->element_size), 
				src, l->element_size);
			break;
		};
		l->head_pos++;
		if (l->element_count <= l->head_pos) {
			l->head_pos = 0;
		}
		l->avail_count++;
		ret = TRUE;
	}

	vmm_spin_unlock_irqrestore(&l->lock, flags);

	return ret;
}

bool lifo_dequeue(struct lifo *l, void *dst)
{
	bool ret = FALSE;
	irq_flags_t flags;

	if (!l || !dst) {
		return FALSE;
	}

	vmm_spin_lock_irqsave(&l->lock, flags);

	if (!__lifo_isempty(l)) {
		if (l->head_pos == 0) {
			l->head_pos = l->element_count - 1;
		} else {
			l->head_pos--;
		}
		switch (l->element_size) {
		case 1:
			*((u8 *)dst) = *((u8 *)(l->elements + l->head_pos));
			break;
		case 2:
			*((u16 *)dst) =
				*((u16 *)(l->elements + (l->head_pos * 2)));
			break;
		case 4:
			*((u32 *)dst) = 
				*((u32 *)(l->elements + (l->head_pos * 4)));
			break;
		case 8:
			*((u64 *)dst) = 
				*((u64 *)(l->elements + (l->head_pos * 8)));
			break;
		default:
			memcpy(dst, 
				l->elements + (l->head_pos * l->element_size),
				l->element_size);
			break;
		};
		l->avail_count--;
		ret = TRUE;
	}

	vmm_spin_unlock_irqrestore(&l->lock, flags);

	return ret;
}

bool lifo_getelement(struct lifo *l, u32 index, void *dst)
{
	u32 first_pos;
	irq_flags_t flags;

	if (!l || !dst) {
		return FALSE;
	}

	if (l->element_count <= index) {
		return FALSE;
	}
	
	vmm_spin_lock_irqsave(&l->lock, flags);

	first_pos = l->head_pos + (l->element_count - l->avail_count);
	if (l->element_count <= first_pos) {
		first_pos -= l->element_count;
	}
	index = first_pos + index;
	if (l->element_count <= first_pos) {
		index -= l->element_count;
	}

	switch(l->element_size) {
	case 1:
		*((u8 *)dst) = *((u8 *)(l->elements + index));
		break;
	case 2:
		*((u16 *)dst) = *((u16 *)(l->elements + (index * 2)));
		break;
	case 4:
		*((u32 *)dst) = *((u32 *)(l->elements + (index * 4)));
		break;
	case 8:
		*((u64 *)dst) = *((u64 *)(l->elements + (index * 8)));
		break;
	default:
		memcpy(dst, 
			l->elements + (index * l->element_size),
			l->element_size);
		break;
	};

	vmm_spin_unlock_irqrestore(&l->lock, flags);

	return TRUE;
}

u32 lifo_avail(struct lifo *l)
{
	u32 ret;
	irq_flags_t flags;

	if (!l) {
		return 0;
	}

	vmm_spin_lock_irqsave(&l->lock, flags);
	ret = l->avail_count;
	vmm_spin_unlock_irqrestore(&l->lock, flags);

	return ret;
}

