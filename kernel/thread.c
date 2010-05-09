/*
 * Copyright 2010 Nick Johnson
 * ISC Licensed, see LICENSE for details
 */

#include <util.h>
#include <time.h>
#include <space.h>
#include <init.h>

/****************************************************************************
 * thread_alloc
 *
 * Returns a pointer to a thread that was not previously allocated. Returns
 * null on error. The thread structure is page aligned.
 */

thread_t *thread_alloc(void) {
	return heap_alloc(sizeof(thread_t));
}

/****************************************************************************
 * thread_drop
 *
 * Kills the given thread and switches to another runnable thread. If legacy
 * mode is enabled, the other runnable thread is the next thread on the 
 * thread image stack of the current process.
 */

thread_t *thread_drop(thread_t *image) {
	thread_t *old_image;

	old_image = image;
	schedule_remove(old_image);

	image = thread_switch(image, schedule_next());

	thread_free(old_image);

	return image;
}

/****************************************************************************
 * thread_fire
 *
 * Sends a signal to the process with pid targ of the type sig with the
 * granted page at current virtual address grant. If the target process cannot
 * accept it or has value SIG_POLICY_QUEUE at the signals' offset in 
 * signal_policy, the signal is added to that process' mailbox. Otherwise, a 
 * new thread is created in the target process to handle the incoming signal, 
 * and that thread is made active.
 *
 * The granted page in the current process is replaced with a page with
 * undefined contents and the same permissions as the granted page.
 *
 * Returns a runnable and active thread that may or may not be the thread
 * passed as image.
 */

thread_t *thread_fire(thread_t *image, uint16_t targ, uint16_t sig, uintptr_t grant){
	process_t *p_targ;
	thread_t *new_image;
	uintptr_t addr;
	struct signal_queue *sq;

	p_targ = process_get(targ);

	if (grant) {
		addr   = grant;
		grant  = page_get(grant);
		page_set(addr, page_fmt(frame_new(), (grant & 0xFFF) | PF_PRES));
		grant &= ~0xFFF;
	}

	switch (p_targ->signal_policy[sig]) {
	case SIG_POLICY_EVENT:
		if (p_targ->signal_handle) {
			new_image = thread_alloc();
			thread_bind(new_image, p_targ);
	
			new_image->ds      = 0x23;
			new_image->cs      = 0x1B;
			new_image->ss      = 0x23;
			new_image->eflags  = p_targ->thread[0]->eflags | 0x3000;
			new_image->useresp = new_image->stack + SEGSZ;
			new_image->proc    = p_targ;
			new_image->grant   = grant;
			new_image->ebx     = grant;
			new_image->source  = image->proc->pid;
			new_image->esi     = image->proc->pid;
			new_image->signal  = sig;
			new_image->edi     = sig;
			new_image->eip     = p_targ->signal_handle;
			new_image->fxdata  = NULL;
			schedule_insert(new_image);

			return new_image;
		}
	case SIG_POLICY_QUEUE:
		sq = heap_alloc(sizeof(struct signal_queue));
		sq->signal = sig;
		sq->grant  = grant;
		sq->source = image->proc->pid;
		sq->next   = NULL;

		if (p_targ->mailbox_out[sig]) {
			p_targ->mailbox_in[sig]->next = sq;
			p_targ->mailbox_in[sig] = sq;
		}
		else {
			p_targ->mailbox_out[sig] = sq;
			p_targ->mailbox_in[sig]  = sq;
		}

		return image;
	default :
	case SIG_POLICY_ABORT:
		return syscall_exit(image);
	}
}

/****************************************************************************
 * thread_free
 *
 * Frees a thread and its associated memory.
 */

void thread_free(thread_t *thread) {
	uintptr_t i;

	if (thread->stack) {
		i = (thread->stack - (KSPACE - (SEGSZ * 128))) / SEGSZ;

		thread->proc->thread[i] = NULL;

		space_exmap(thread->proc->space);
	
		for (i = thread->stack; i < thread->stack + SEGSZ; i += PAGESZ) {
			if ((page_exget(i) & PF_PRES) != 0) {
				frame_free(page_ufmt(page_exget(i)));
				page_exset(i, 0);
			}
		}
	}

	heap_free(thread, sizeof(thread_t));
}

/****************************************************************************
 * thread_init
 *
 * Initializes threading subsystem.
 */

void thread_init(void) {
	uint16_t divisor;

	/* set up interrupt descriptor table */
	init_idt();

	/* initialize task state segment */
	init_tss();

	/* set programmable interrupt controller mask */
	pic_mask(0x0001);

	/* register system calls */
	register_int(0x40, syscall_fire);
	register_int(0x41, syscall_drop);
	register_int(0x42, syscall_sctl);
	register_int(0x43, syscall_mail);

	register_int(0x48, syscall_fork);
	register_int(0x49, syscall_exit);
	register_int(0x4A, syscall_pctl);
	register_int(0x4B, syscall_exec);

	register_int(0x50, syscall_mmap);
	register_int(0x51, syscall_mctl);

	/* register fault handlers */
	register_int(0x00, fault_float);
	register_int(0x01, fault_generic);
	register_int(0x02, fault_generic);
	register_int(0x03, fault_generic);
	register_int(0x04, fault_generic);
	register_int(0x05, fault_generic);
	register_int(0x06, fault_generic);
	register_int(0x07, fault_nomath);
	register_int(0x08, fault_double);
	register_int(0x09, fault_float);
	register_int(0x0A, fault_generic);
	register_int(0x0B, fault_generic);
	register_int(0x0C, fault_generic);
	register_int(0x0D, fault_generic);
	register_int(0x0E, fault_page);
	register_int(0x0F, fault_generic);
	register_int(0x10, fault_float);
	register_int(0x11, fault_generic);
	register_int(0x12, fault_generic);
	register_int(0x13, fault_nomath);

	/* initialize programmable interrupt timer at 256Hz */
	register_int(IRQ(0), pit_handler);
	divisor = 1193180 / 256;
	outb(0x43, 0x36);
	outb(0x40, (uint8_t) (divisor & 0xFF));
	outb(0x40, (uint8_t) (divisor >> 8));

	/* initialize FPU/MMX/SSE */
	init_fpu();
}

/****************************************************************************
 * thread_bind
 *
 * Returns a free segment of size SEGSZ with 16 pages allocated at its end.
 * Associates the thread with this stack with the process.
 */

uintptr_t thread_bind(thread_t *thread, process_t *proc) {
	uintptr_t i;
	uintptr_t addr;

	for (addr = 0, i = 0; i < 128; i++) {
		if (!proc->thread[i]) {
			proc->thread[i] = thread;
			addr = KSPACE - (SEGSZ * 128) + (SEGSZ * i);
			break;
		}
	}

	if (!addr) {
		/* out of threads */
		return 0;
	}

	space_exmap(proc->space);

	for (i = addr + (SEGSZ - PAGESZ * 16); i < addr + SEGSZ; i += PAGESZ) {
		page_exset(i, page_fmt(frame_new(), PF_PRES | PF_RW | PF_USER));
	}

	thread->stack = addr;
	thread->proc  = proc;

	return addr;
}

/****************************************************************************
 * thread_switch
 *
 * Switches to the target thread, completely saving the old threads' state
 * and loading the new ones' state. A process switch is performed if and only
 * if the threads are under different processes. A pointer to the switched to
 * thread is returned.
 */

thread_t *thread_switch(thread_t *old, thread_t *new) {
	extern void fpu_save(void *fxdata);
	extern bool tst_ts(void);
	extern void set_ts(void);

	/* save FPU state */
	if (tst_ts()) {
		if (!old->fxdata) {
			old->fxdata = heap_alloc(512);
		}

		fpu_save(old->fxdata);
	}

	/* switch processes */
	if (old->proc != new->proc) {
		process_switch(new->proc);
	}

	/* set task switched flag */
	set_ts();

	return new;
}