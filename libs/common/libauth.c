/**
 * Copyright (c) 2014 Himanshu Chauhan
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
 * @file libauth.c
 * @Author Himanshu Chauhan <hschauhan@nulltrace.org>
 * @brief source file for user authentication
 */

#include <vmm_error.h>
#include <vmm_heap.h>
#include <libs/vfs.h>
#include <libs/stringlib.h>

#ifdef CONFIG_LIBAUTH_USE_MD5_PASSWD
#include <libs/md5.h>
typedef u8 hash_digest_t[16];
#define HASH_LEN	16
#endif

#if CONFIG_LIBAUTH_USE_SHA256_PASSWD
#include <libs/sha256.h>
typedef sha256_digest_t hash_digest_t;
#define HASH_LEN	SHA256_DIGEST_LEN
#endif

/* Current user */
#define VFS_LOAD_BUF_SZ 256

static int string_to_digest(char *digest_str, u8 *digest, u32 dlen)
{
	int i, j;
	u8 dval;

	/* Convert to lower and validate */
	for (i = 0; i < strlen(digest_str); i++) {
		if (!isalnum(digest_str[i])) {
			return VMM_EFAIL;
		}

		if (isalpha(digest_str[i])) {
			digest_str[i] = tolower(digest_str[i]);
			if (digest_str[i] < 'a' || digest_str[i] > 'f') {
				return VMM_EFAIL;
			}
		}
	}

	for (i = 0, j = 0; i < strlen(digest_str); j++, i += 2) {
		dval = 0;
		if (isalpha(digest_str[i])) {
			dval = (((digest_str[i] - 'a') + 10) << 4);
		} else
			dval = (digest_str[i] - '0') << 4;

		if (isalpha(digest_str[i + 1])) {
			dval |= ((digest_str[i + 1] - 'a') + 10);
		} else
			dval |= (digest_str[i + 1] - '0');

		if (j < dlen) {
			digest[j] = dval;
		} else {
			/* digest buffer can't be smaller than
			   required: better fail */
			return VMM_EFAIL;
		}
	}

	return VMM_OK;
}

static int process_auth_entry(char *auth_entry, const char *user,
			      u8 *dst_hash, u32 dst_len)
{
	char *auth_token, *auth_save;
	const char *auth_delim = ":";
	u32 centry = 0, found = 0, auth_tok_len;

	for (auth_token = strtok_r(auth_entry, auth_delim,
				   &auth_save); auth_token;
	     auth_token = strtok_r(NULL, auth_delim,
				   &auth_save)) {
		auth_tok_len = strlen(auth_token);
		if (!centry) {
			if (!strncmp(auth_entry, user, auth_tok_len)) {
				found = 1;
			}
		} else {
			if (found) {
				return string_to_digest(auth_token, dst_hash, dst_len);
			}
		}
		centry++;
	}

	return VMM_EFAIL;
}

static int get_user_hash(const char *user, u8 *dst_hash, u32 dst_len)
{
	int fd, rc;
	u32 len;
	size_t buf_rd;
	char buf[VFS_LOAD_BUF_SZ];
	struct stat st;
	u32 tok_len;
	char *token, *save;
	const char *delim = "\n";
	u32 end, cleanup = 0;
	const char *path = CONFIG_LIBAUTH_FILE;

	fd = vfs_open(path, O_RDONLY, 0);
	if (fd < 0) {
		return VMM_EFAIL;
	}

	rc = vfs_fstat(fd, &st);
	if (rc) {
		vfs_close(fd);
		return VMM_EFAIL;
	}

	if (!(st.st_mode & S_IFREG)) {
		vfs_close(fd);
		return VMM_EFAIL;
	}

	len = st.st_size;
	while (len) {
		memset(buf, 0, sizeof(buf));

		buf_rd = (len < VFS_LOAD_BUF_SZ) ? len : VFS_LOAD_BUF_SZ;
		buf_rd = vfs_read(fd, buf, buf_rd);
		if (buf_rd < 1) {
			break;
		}

		end = buf_rd - 1;
		while (buf[end] != '\n') {
			buf[end] = 0;
			end--;
			cleanup++;
		}

		if (cleanup) {
			vfs_lseek(fd, (buf_rd - cleanup), SEEK_SET);
			cleanup = 0;
		}

		for (token = strtok_r(buf, delim, &save); token;
		     token = strtok_r(NULL, delim, &save)) {
			tok_len = strlen(token);
			if (*token != '#' && *token != '\n') {
				if (process_auth_entry(token, user, dst_hash,
						       dst_len) == VMM_OK)
					return VMM_OK;
			}

			len -= (tok_len + 1);
		}
	}

	rc = vfs_close(fd);
	if (rc) {
		return VMM_EFAIL;
	}

	return VMM_EFAIL;
}

#if CONFIG_LIBAUTH_USE_MD5_PASSWD
static void calculate_hash(char *str, hash_digest_t sig)
{
	struct md5_context md5c;

	md5_init(&md5c);
	md5_update(&md5c, (u8 *)str, strlen(str));
	md5_final(sig, &md5c);
}
#elif CONFIG_LIBAUTH_USE_SHA256_PASSWD
static void calculate_hash(char *str, hash_digest_t sig)
{
	struct sha256_context sha256c;

	sha256_init(&sha256c);
	sha256_update(&sha256c, (u8 *)str, strlen(str));
	sha256_final(sig, &sha256c);
}
#else
#error "No hash selected for password hashing."
#endif

int authenticate_user(const char *user, char *passwd)
{
	hash_digest_t passwd_sig, match_against;
	int i;

	calculate_hash(passwd, passwd_sig);

	memset(match_against, 0 , sizeof(match_against));
	if (get_user_hash(user, (u8 *)&match_against, sizeof(match_against)) == VMM_OK) {
		for (i = 0; i < HASH_LEN; i++) {
			if (match_against[i] != passwd_sig[i]) {
				return VMM_EFAIL;
			}
		}

		return VMM_OK;
	}

	return VMM_EFAIL;
}
