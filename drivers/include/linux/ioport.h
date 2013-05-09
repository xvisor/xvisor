/*
 * ioport.h	Definitions of routines for detecting, reserving and
 *		allocating system resources.
 *
 * Authors:	Linus Torvalds
 */

#ifndef _LINUX_IOPORT_H
#define _LINUX_IOPORT_H

#include <vmm_host_ram.h>

#define request_region(start,n,name)		1

#define request_mem_region(start,n,name)	\
				(vmm_host_ram_reserve((start), (n)) ? 0 : 1)

#define release_mem_region(start,n)		\
				(vmm_host_ram_free((start), (n)))

#endif
