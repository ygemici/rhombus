// Copyright 2009 Nick Johnson

#include <lib.h>
#include <mem.h>
#include <trap.h>
#include <task.h>
#include <init.h>

#define KHAOS_VERSION_MAJOR 0
#define KHAOS_VERSION_MINOR 1
#define KHAOS_KERNEL_REVISN 1

__attribute__ ((section(".tdata"))) 
char *stamp = "\
Khaos Operating System v%d.%d\n\
Copyright 2009 Nick Johnson\n\
===========================\n\n";

typedef void (*init_t)(void);
__attribute__ ((section(".tdata")))
init_t init_list[] = {
init_detect,
init_kload,
init_mem,
init_int,
init_task,
init_pit,
init_initrd_rmap,
init_free,
NULL
};

#ifdef TEST
init_t test_list[] = {
test_mem,
NULL
};
#endif

struct multiboot *mboot;

typedef void (*entry_t)();

void init(void *mboot_ptr, uint32_t mboot_magic) {
	uint32_t i;

	cleark();
	printk(stamp, KHAOS_VERSION_MAJOR, KHAOS_VERSION_MINOR);

	if (mboot_magic != 0x2BADB002) panic("Bootloader is not multiboot compliant");
	else mboot = mboot_ptr;

	for (i = 0; init_list[i]; i++) init_list[i]();

	#ifdef TEST
	for (i = 0; test_list[i]; i++) test_list[i]();	
	for(;;);
	#endif

	printk("  Kernel: dropping to usermode");
	cursek(74, -1);
	printk("[done]");

	cleark();
	cursek(0, 15);
}
