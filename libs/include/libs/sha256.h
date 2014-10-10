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
 * @file sha256.h
 * @author Himanshu Chauhan (hschauhan@nulltrace.org)
 * @brief SHA-256 related declarations/definitions.
 */
#ifndef __SHA_256_H_
#define __SHA_256_H_

typedef struct sha256_context {
	u8 data[64];
	u32 datalen;
	u32 bitlen[2];
	u32 state[8];
} sha256_context_t;

#define SHA256_DIGEST_LEN	32
typedef u8 sha256_digest_t[SHA256_DIGEST_LEN];

void sha256_init(struct sha256_context *ctx);
void sha256_update(struct sha256_context *ctx, u8 data[], u32 len);
void sha256_final(sha256_digest_t digest, struct sha256_context *ctx);

#endif /* __SHA_256_H_ */
