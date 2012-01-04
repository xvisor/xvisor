/**
 * Copyright (c) 2010 Himanshu Chauhan.
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
 * @file vmm_list.h
 * @version 0.01
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief Abstraction and functions for common list handling.
 */

#ifndef __VMM_LIST_H__
#define __VMM_LIST_H__

#include <vmm_macros.h>

#define LIST_POISON_PREV	0xDEADBEEF
#define LIST_POISON_NEXT	0xFADEBABE

struct dlist {
	struct dlist *next, *prev;
};

#define INIT_HEAD(__lname)	{ &(__lname), &(__lname) }
#define LIST_HEAD(_lname)	struct dlist _lname = INIT_HEAD(_lname)
#define INIT_LIST_HEAD(ptr)  do { \
		(ptr)->next = ptr; (ptr)->prev = ptr;	\
	}while (0);

#define list_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

#define list_for_each(curr, head) \
	for (curr = (head)->next; curr != head; curr = (curr)->next)

static inline void __list_add(struct dlist *prev,
			      struct dlist *next, struct dlist *new)
{
	new->prev = prev;
	new->next = next;
	prev->next = new;
	next->prev = new;
}

/**
 * Adds the new node after the given head.
 * @param head: List head after which the "new" node should be added.
 * @param new: New node that needs to be added to list.
 * @note Please note that new node is added after the head.
 */
static inline void list_add(struct dlist *head, struct dlist *new)
{
	__list_add(head, head->next, new);
}

/**
 * Adds a node at the tail where tnode points to tail node.
 * @param tnode: The current tail node.
 * @param new: The new node to be added before tail.
 * @note: Please note that new node is added before tail node.
 */
static inline void list_add_tail(struct dlist *tnode, struct dlist *new)
{
	__list_add(tnode->prev, tnode, new);
}

static inline void __list_del(struct dlist *node,
			      struct dlist *prev, struct dlist *next)
{
	prev->next = node->next;
	next->prev = node->prev;
	node->next = (void *)LIST_POISON_NEXT;
	node->prev = (void *)LIST_POISON_PREV;
}

/**
 * Deletes a given node from list.
 * @param node: Node to be deleted.
 */
static inline void list_del(struct dlist *node)
{
	__list_del(node, node->prev, node->next);
}

static inline struct dlist *list_pop_tail(struct dlist *head)
{
	struct dlist *dnode = head->prev;
	list_del(head->prev);
	return dnode;
}

static inline struct dlist *list_pop(struct dlist *head)
{
	struct dlist *dnode = head->next;
	list_del(head->next);
	return dnode;
}

static inline struct dlist *list_first(struct dlist *head)
{
	return head->next;
}

static inline int list_empty(struct dlist *head)
{
	return (head->next == head);
}

#endif /* __VMM_LIST_H__ */
