/**
 * Copyright (c) 2013 Himanshu Chauhan
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
 * @file ioport.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief header file for x86 IO Port read/write
 */
#ifndef _IO_PORT_H__
#define _IO_PORT_H__

#include <vmm_types.h>

/* Basic port I/O */
static inline void ioport_outb(u8 v, u16 port)
{
    asm volatile("outb %0,%1" : : "a" (v), "dN" (port));
}

static inline u8 ioport_inb(u16 port)
{
    u8 v;
    asm volatile("inb %1,%0" : "=a" (v) : "dN" (port));
    return v;
}

static inline void ioport_outw(u16 v, u16 port)
{
    asm volatile("outw %0,%1" : : "a" (v), "dN" (port));
}
static inline u16 ioport_inw(u16 port)
{   
    u16 v;
    asm volatile("inw %1,%0" : "=a" (v) : "dN" (port));
    return v;
}

static inline void ioport_outl(u32 v, u16 port)
{   
    asm volatile("outl %0,%1" : : "a" (v), "dN" (port));
}
static inline u32 ioport_inl(u32 port)
{
    u32 v;
    asm volatile("inl %1,%0" : "=a" (v) : "dN" (port));
    return v;
}

#endif /* _IO_PORT_H__ */
