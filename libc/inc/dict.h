/*
 * Copyright (C) 2009, 2010 Nick Johnson <nickbjohnson4224 at gmail.com>
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef FLUX_DICT_H
#define FLUX_DICT_H

#include <stdint.h>
#include <stdbool.h>

/* dictionary interface ****************************************************/

struct dict {
	struct dict *next[256];
	const uint8_t *value;
};

extern struct dict_info {
	struct dict root;
	bool        mutex;
	uintptr_t   brk;
} *dict_info;

void dict_init(void);

const uint8_t *dict_read
	(const uint8_t *key, size_t keylen);

const uint8_t *dict_readstr
	(const char *key);

const uint8_t *dict_readstrns
	(const char *namespace, const char *key);

void dict_write
	(const uint8_t *key, size_t keylen, 
	const uint8_t *val, size_t vallen);

void dict_writestr
	(const char *key, const uint8_t *val, size_t vallen);

void dict_writestrns
	(const char *namespace, const char *key, 
	const uint8_t *val, size_t vallen);

void dict_setlink
	(const uint8_t *key, size_t keylen, 
	const uint8_t *prefix, size_t prefixlen,
	uint32_t server, uint64_t inode);

/* dictionary heap - garbage collected *************************************/

void *dict_alloc(size_t size, bool data);
void  dict_sweep(void);

#endif/*FLUX_DICT_H*/