/*
 * Copyright (C) 2009-2011 Nick Johnson <nickbjohnson4224 at gmail.com>
 * 
 * Permission to use, copy, modify, and distribute this software for any
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

#include <stdlib.h>
#include <string.h>
#include <mutex.h>
#include <natio.h>
#include <proc.h>
#include <vfs.h>

/*****************************************************************************
 * __type_rcall_wrapper
 */

char *__type_rcall_wrapper(uint64_t source, uint32_t index, int argc, char **argv) {
	struct resource *r;
	char *reply;

	r = index_get(index);

	if (!r) {
		return strdup("! nfound");
	}

	reply = malloc(sizeof(char) * 2);
	reply[1] = '\0';
	reply[0] = typechar(r->type);

	return reply;
}

/*****************************************************************************
 * __type_wrapper
 *
 * Performs the requested actions of a FS_TYPE_ command.
 *
 * protocol:
 *   port: PORT_TYPE
 *
 *   request:
 *
 *   reply:
 *     uint8_t type
 */

void __type_wrapper(struct msg *msg) {
	struct resource *file;

	/* find file node */
	file = index_get(RP_INDEX(msg->target));

	if (!file) {
		merror(msg);
		return;
	}

	msg->length = 1;
	msg->data[0] = file->type;
	mreply(msg);
}
