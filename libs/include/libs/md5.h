/**
 * Copyright (c) 2014 Himanshu Chauhan.
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
 * @file md5.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief MD5 hash related defines
 *
 * Taken from https://www.fourmilab.ch/md5/md5.tar.gz
 */
#ifndef __MD5_H
#define __MD5_H

typedef struct md5_context {
        u32 buf[4];
        u32 bits[2];
        u8 in[64];
} md5_context_t;

extern void md5_init(struct md5_context *ctxt);
extern void md5_update(struct md5_context *ctx, u8 *buf, u32 len);
extern void md5_final(u8 digest[16], struct md5_context *ctx);
extern void md5_transform(u32 buf[4], u32 in[16]);

#endif /* !__MD5_H */
