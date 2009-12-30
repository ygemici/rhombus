
#include "libc.h"
#include <signal.h>
#include <khaos.h>

static void __exit(uint32_t source, void *grant) {
	_exit(1);
}

void _cini() {
	/* Heap */
	_heap_init();

	/* Fault handling */
	siginit();
	sigregister(0, __exit);
	sigregister(2, __exit);
	sigregister(5, __exit);
}
