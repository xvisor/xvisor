/**
 * Copyright (c) 2010 Himanshu Chauhan.
 * Copyright (c) 2012 Anup Patel.
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
 * @file vmm_heap.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @author Anup Patel (anup@brainfault.org)
 * @author Ankit Jindal (thatsjindal@gmail.com)
 * @brief header file for heap managment interface
 */
#ifndef _VMM_HEAP_H__
#define _VMM_HEAP_H__

#include <vmm_types.h>

struct vmm_chardev;

/** Allocate Normal memory */
void *vmm_malloc(virtual_size_t size);

/** Allocate Normal memory and zero set */
void *vmm_zalloc(virtual_size_t size);

/** Retrieve allocation size from Normal heap */
virtual_size_t vmm_alloc_size(const void *ptr);

/** Free Normal memory */
void vmm_free(void *ptr);

/* Translate a normal address to its virtual address */
void *vmm_normal_pa2va(physical_addr_t pa);

/* Translate a normal virtual address to its physical address */
void *vmm_normal_va2pa(virtual_addr_t va);

/** Starting virtual address of Normal heap */
virtual_addr_t vmm_normal_heap_start_va(void);

/** Total size of Normal heap (house-keeping + allocation) */
virtual_size_t vmm_normal_heap_size(void);

/** Size of Normal heap house-keeping */
virtual_size_t vmm_normal_heap_hksize(void);

/** Size of Normal heap free space */
virtual_size_t vmm_normal_heap_free_size(void);

/** Print Normal heap state */
int vmm_normal_heap_print_state(struct vmm_chardev *cdev);

enum dma_data_direction {
	DMA_BIDIRECTIONAL = 0,
	DMA_TO_DEVICE = 1,
	DMA_FROM_DEVICE = 2,
	DMA_NONE = 3,
};

/** Allocate DMA memory */
void *vmm_dma_malloc(virtual_size_t size);

/** Allocate DMA memory and zero set */
void *vmm_dma_zalloc(virtual_size_t size);

/** Allocate DMA memory and get its physical address */
void *vmm_dma_zalloc_phy(virtual_size_t size, physical_addr_t *paddr);

/* Set the buffer to be "owned" by the device */
void vmm_dma_cpu_to_dev(virtual_addr_t start, virtual_addr_t end,
			enum dma_data_direction dir);

/* Set the buffer to be "owned" by the CPU */
void vmm_dma_dev_to_cpu(virtual_addr_t start, virtual_addr_t end,
			enum dma_data_direction dir);

/* Map a DMA buffer for the device */
dma_addr_t vmm_dma_map(virtual_addr_t *pvaddr, virtual_size_t size,
		       enum dma_data_direction dir);

/* Unmap the DMA buffer, which can be then read by the CPU */
void vmm_dma_unmap(dma_addr_t* daddr, physical_size_t size,
		   enum dma_data_direction dir);

/** Retrieve allocation size from DMA heap */
virtual_size_t vmm_dma_alloc_size(const void *ptr);

/** Free DMA memory */
void vmm_dma_free(void *ptr);

/* Translate a DMA physical address to its virtual address */
void *vmm_dma_pa2va(dma_addr_t pa);

/* Translate a DMA virtual address to its physical address */
void *vmm_dma_va2pa(virtual_addr_t va);

/** Starting virtual address of DMA heap */
virtual_addr_t vmm_dma_heap_start_va(void);

/** Total size of DMA heap (house-keeping + allocation) */
virtual_size_t vmm_dma_heap_size(void);

/** Size of DMA heap house-keeping */
virtual_size_t vmm_dma_heap_hksize(void);

/** Size of DMA heap free space */
virtual_size_t vmm_dma_heap_free_size(void);

/** Print DMA heap state */
int vmm_dma_heap_print_state(struct vmm_chardev *cdev);

/** Initialization function for head managment */
int vmm_heap_init(void);

#endif
