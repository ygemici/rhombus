/* 
 * Copyright 2009, 2010 Nick Johnson 
 * ISC Licensed, see LICENSE for details
 */

#include <lib.h>
#include <int.h>
#include <task.h>
#include <mem.h>

/***** IRQ HANDLERS *****/

static uint8_t held_irq[15];
static uint16_t held_count;

/* Handles IRQ 0, and advances a simple counter used as a clock */
/* If an IRQ was held, it is redirected now */
uint32_t tick = 0;
image_t *pit_handler(image_t *image) {
	size_t i;
	task_t *holder;

	if (image->cs | 1) tick++;

	if (held_count) {
		for (i = 0; i < 15; i++) {
			if (held_irq[i]) {
				holder = task_get(irq_holder[i]);
				if (!holder || 
					(holder->flags & CTRL_CLEAR) || 
					(holder->sigflags & CTRL_SIRQ)) {
					continue;
				}
				held_irq[i]--;
				held_count--;
				return signal(irq_holder[i], SSIG_IRQ, NULL, NOERR);
			}
		}
	}

	/* Switch to next scheduled task */
	return task_switch(task_next(0));
}

image_t *irq_redirect(image_t *image) {
	task_t *holder;

	holder = task_get(irq_holder[DEIRQ(image->num)]);
	if ((holder->flags & CTRL_CLEAR) || (holder->flags & CTRL_SIRQ)) {
		held_irq[DEIRQ(image->num)]++;
		held_count++;
		return image;
	}

	/* Send S_IRQ signal to the task registered with the IRQ */
	return signal(irq_holder[DEIRQ(image->num)], SSIG_IRQ, NULL, NOERR);
}

/***** FAULT HANDLERS *****/

/* Generic fault */
image_t *fault_generic(image_t *image) {

	#ifdef PARANOID
	/* If in kernelspace, panic */
	if ((image->cs & 0x3) == 0) {
		printk("EIP:%x NUM:%d ERR:%x\n", image->eip, image->num, image->err);
		panic("unknown exception");
	}
	#endif

	return signal(curr_pid, SSIG_FAULT, NULL, NOERR | EKILL);
}

/* Page fault */
image_t *fault_page(image_t *image) {

	#ifdef PARANOID
	extern uint32_t get_cr2(void);
	uint32_t cr2;

	/* Get faulting address from register CR2 */
	cr2 = get_cr2();

	/* If in kernelspace, panic */
	if ((image->cs & 0x3) == 0) { /* i.e. if it was kernelmode */
		printk("page fault at %x, ip = %x frame %x task %d\n", 
			cr2, image->eip, page_get(cr2), curr_pid);
		panic("page fault exception");
	}
	#endif

	return signal(curr_pid, SSIG_PAGE, NULL, NOERR | EKILL);
}

/* Floating point exception */
image_t *fault_float(image_t *image) {

	#ifdef PARANOID
	/* If in kernelspace, panic */
	if ((image->cs & 0x3) == 0) {
		panic("floating point exception");
	}
	#endif

	return signal(curr_pid, SSIG_FLOAT, NULL, NOERR | EKILL);
}

/* Double fault */
image_t *fault_double(image_t *image) {

	/* Can only come from kernel problems */
	printk("DS:%x CS:%x\n", image->ds, image->cs);
	panic("double fault exception");
	return NULL;

}

/***** System Calls *****/
/* See section I of the Flux manual for details */

image_t *fire(image_t *image) {
	uint32_t targ  = image->eax;
	uint32_t sig   = image->ecx;
	uint32_t grant = image->ebx;
	uint32_t flags = image->edx;
	task_t *dst_t  = task_get(targ);

	if (targ == 0) {
		return task_switch(task_next(0));
	}

	if (!dst_t || !dst_t->shandler || (dst_t->sigflags & (1 << sig))) {
			ret(image, ERROR);
	}

	if (flags & 0x1) {
		image = sret(image);
	}

	return signal(targ, sig, (void*) grant, 0);
}

image_t *drop(image_t *image) {
	return sret(image);
}

image_t *hand(image_t *image) {
	uint32_t old_handler;

	old_handler = curr_task->shandler;
	curr_task->shandler = image->eax;
	ret(image, old_handler);
}

image_t *ctrl(image_t *image) {
	extern uint32_t can_use_fpu;
	uint32_t flags = image->eax;
	uint32_t mask  = image->edx;
	uint32_t space = image->ecx;
	uint8_t irq;

	switch (space) {
	case CTRL_PSPACE:

		/* Stop the modification of protected flags if not super */
		if ((curr_task->flags & CTRL_SUPER) == 0) {
			mask &= CTRL_SMASK;
		}

		/* Set flags */
		curr_task->flags = (curr_task->flags & ~mask) | (flags & mask);

		/* Unset CTRL_FLOAT if FPU is disabled */
		if ((flags & mask & CTRL_FLOAT) && (can_use_fpu == 0)) {
			curr_task->flags &= ~CTRL_FLOAT;
		}

		/* Update IRQ redirect if CTRL_IRQRD is changed */
		if (mask & CTRL_IRQRD) {
			if (flags & CTRL_IRQRD) {
				/* Set IRQ redirect */
				irq = (flags >> 24) & 0xFF;
				if (irq < 15) {
					irq_holder[irq] = curr_pid;
					register_int(IRQ(irq), irq_redirect);
					pic_mask(1 << irq);
				}
			}
			else {
				/* Unset IRQ redirect */
				irq = (curr_task->flags >> 24) & 0xFF;
				irq_holder[irq] = 0;
				register_int(IRQ(irq), NULL);
				pic_mask(1 << irq);
			}
		}

		ret(image, curr_task->flags);
		break;

	case CTRL_SSPACE:
		
		/* Set flags */
		curr_task->sigflags = (curr_task->sigflags & ~mask) | (flags & mask);

		ret(image, curr_task->sigflags);
		break;

	default:
		ret(image, -1);
		break;
	}
}

image_t *info(image_t *image) {
	uint8_t sel = image->eax;

	switch (sel) {
	case 0: ret(image, curr_pid);
	case 1: ret(image, curr_task->parent);
	case 2: ret(image, tick);
	case 3: ret(image, 0x0003);
	case 4: ret(image, KSPACE);
	case 5: ret(image, 
		(curr_task->flags & CTRL_SUPER) ? CTRL_CMASK : CTRL_CMASK & CTRL_SMASK);
	case 6: ret(image, MMAP_CMASK);
	case 7: ret(image, curr_task->sigflags);
	default: ret(image, -1);
	}
}

image_t *mmap(image_t *image) {
	uintptr_t addr = image->ebx;
	size_t count   = image->ecx;
	uint16_t flags = image->edx & 0xFFF;
	uint32_t frame = image->edx & ~0xFFF;
	uint16_t pflags = 0;

	if (addr & 0xFFF)   ret(image, -1);	/* Reject unaligned requests */
	if (addr >= KSPACE) ret(image, -1);	/* Reject out of bounds requests */
	if (count > 1024)   ret(image, -1);	/* Reject requests > 4MB */

	if (flags & MMAP_PHYS) {
		ret(image, page_ufmt(page_get(addr)));
	}

	if (flags & MMAP_FREE) {
		mem_free(addr, count * PAGESZ);
		ret(image, 0);
	}
	
	pflags |= (PF_USER | PF_PRES);
	if (flags & MMAP_READ) 	pflags |= PF_PRES;
	if (flags & MMAP_WRITE)	pflags |= PF_RW;
	if (flags & MMAP_EXEC) 	pflags |= PF_PRES;

	if (flags & MMAP_FRAME) {
		if ((curr_task->flags & CTRL_SUPER) || curr_task->grant == frame) {
			p_free(addr);
			page_set(addr, page_fmt(frame, pflags));
			if (curr_task->grant == frame) {
				curr_task->grant = 0;
			}
			ret(image, 0);
		}
		else {
			ret(image, -1);
		}
	}

	mem_alloc(addr, count * PAGESZ, pflags);
	ret(image, 0);
}

image_t *fork(image_t *image) {
	pid_t parent;
	task_t *child;

	/* Save current PID - it will become the parent */
	parent = curr_pid;

	/* Create a new task from the parent */
	child = task_new(task_get(parent));
	if (!child) ret(image, 0); /* Fail if child was not created */

	/* (still in parent) Set return value to child's PID */
	image->eax = child->pid;

	/* Switch to child */
	image = task_switch(child);

	/* (now in child) Set return value to negative parent's PID */
	image->eax = (uint32_t) -parent;

	return image;
}

image_t *exit(image_t *image) {
	pid_t catcher;
	uint32_t ret_val;
	task_t *t;

	/* Send death signal to parent */
	catcher = curr_task->parent;

	/* Copy return value because images are cleared with the address space */
	ret_val = image->eax;

	/* If init exits, halt */
	if (curr_pid == 1) {
		panic("init died");
	}

	/* Deallocate current address space and clear metadata */
	map_clean(curr_task->map);	/* Deallocate pages and page tables */
	map_free(curr_task->map);	/* Deallocate page directory itself */
	task_rem(curr_task);		/* Clear metadata and relinquish PID */

	t = task_get(catcher);
	if (!t || !t->shandler || t->flags & CTRL_CLEAR) {
		/* Parent will not accept - reschedule */
		return task_switch(task_next(0));
	}
	else {
		/* Send S_DTH signal to parent with return value */
		return signal(catcher, SSIG_DEATH, NULL, NOERR);
	}
}