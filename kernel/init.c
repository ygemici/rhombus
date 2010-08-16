/* 
 * Copyright 2009, 2010 Nick Johnson
 * ISC Licensed, see LICENSE for details 
 */

#include <util.h>
#include <time.h>
#include <space.h>
#include <init.h>

#define FLUX_VERSION_MAJOR 0
#define FLUX_VERSION_MINOR 5
#define FLUX_KERNEL_REVISN 0

const char *stamp = "\
Flux Operating System Kernel v%x.%x\n\
Copyright 2010 Nick Johnson\n\n";

typedef void (*init_t)(void);

init_t init_list[] = {
	mem_init,
	thread_init,
	process_init,
	init_task,
	NULL
};

struct multiboot *mboot;

typedef void (*entry_t)();

void *init(void *mboot_ptr, uint32_t mboot_magic) {
	extern void halt(void);
	extern uint32_t get_cr0(void);
	uint32_t i;
	struct thread *boot_image;

	cleark(); 
	printk(stamp, FLUX_VERSION_MAJOR, FLUX_VERSION_MINOR);

	if (mboot_magic != 0x2BADB002) {
		panic("Bootloader is not multiboot compliant");
	}
	mboot = mboot_ptr;

	for (i = 0; init_list[i]; i++) {
		init_list[i]();
	}

	boot_image = thread_alloc();

	printk("dropping to usermode\n");

	return &boot_image->tss_start;
}
