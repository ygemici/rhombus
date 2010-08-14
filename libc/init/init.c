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

#include <arch.h>
#include <stdlib.h>
#include <string.h>
#include <proc.h>
#include <ipc.h>
#include <exec.h>
#include <stdio.h>
#include <natio.h>
#include <mmap.h>
#include <dict.h>

/****************************************************************************
 * reject
 *
 * Action to be taken on the reception of an unwanted event.
 */

static void reject(uint32_t caller, struct packet *packet) {

	if (packet) {
		pfree(packet);
	}

	psend(PORT_REPLY, caller, NULL);
}

/****************************************************************************
 * pagefault
 *
 * Action to be taken on a caught page fault event.
 */

static void pagefault(uint32_t caller, struct packet *packet) {

	printf("Page Fault (pid %d: \"%s\" terminated)\n", getpid(), getenv("NAME"));

	exit(EXIT_FAILURE);
}

/****************************************************************************
 * fpufault
 *
 * Action to be taken on a caught floating point exception event.
 */

static void fpufault(uint32_t caller, struct packet *packet) {

	printf("Floating Point Exception (pid %d: \"%s\" terminated)\n", getpid(), getenv("NAME"));

	exit(EXIT_FAILURE);
}

/****************************************************************************
 * _init
 *
 * Function called at beginning of all processes, used to initialize the C
 * library.
 */

void _init(bool is_init) {
	extern int main(int argc, char **argv);
	char **argv;
	int argc;

	/* setup standard streams */
	stdin  = __fload("stdin");
	stdout = __fload("stdout");
	stderr = __fload("stderr");
	stdvfs = __fload("stdvfs");

	when(PORT_FAULT, pagefault);
	when(PORT_FLOAT, fpufault);
	when(PORT_READ,  reject);
	when(PORT_WRITE, reject);
	when(PORT_INFO,  reject);
	when(PORT_CTRL,  reject);
	when(PORT_QUERY, reject);

	when(PORT_DREAD,  _devent_read);
	when(PORT_DWRITE, _devent_write);
	when(PORT_DLINK,  _devent_link);

	if (is_init) {
		argc = 0;
		argv = NULL;
	}
	else {
		argc = argc_unpack();
		argv = argv_unpack();

		if (argc) {
			setenv("NAME", argv[0]);
		}
	}
	
	exit(main(argc, argv));
}
