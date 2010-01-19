/* Copyright 2009, 2010 Nick Johnson */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <flux.h>
#include <mmap.h>
#include <driver.h>

#include <driver/terminal.h>

#define VMEM 0xB8000
#define WIDTH 80
#define HEIGHT 25
#define TAB 4

static void terminal_init (device_t selector);
static void terminal_halt (void);
static void terminal_write(uint32_t caller, struct request *req);

static uint16_t *vbuf;
static uint16_t c_base = 0;
static uint16_t cursor = 0;
static uint16_t buffer = 0;
static void char_write(char c);

struct driver_interface terminal = {
	terminal_init,
	terminal_halt,

	NULL,
	0,
};

static void terminal_init(device_t selector) {
	size_t i;

	vbuf = malloc(0x1000);
	emap(vbuf, VMEM, PROT_READ | PROT_WRITE);

	for (i = 0; i < WIDTH * HEIGHT; i++) {
		vbuf[i] = 0x0F20;
	}

	sigregister(SIG_WRITE, terminal_write);
}

static void terminal_halt(void) {
	return;
}

static void terminal_write(uint32_t caller, struct request *r) {
	size_t i;

	if (!req_check(r)) {
		tail(caller, SIG_ERROR, NULL);
	}

	for (i = 0; i < r->datasize; i++) {
		char_write(r->reqdata[i]);
	}

	outb(0x3D4, 14);
	outb(0x3D5, cursor >> 8);
	outb(0x3D4, 15);
	outb(0x3D5, cursor & 0xFF);

	r->datasize = i;
	r->format = REQ_READ;
	tail(caller, SIG_REPLY, req_checksum(r));
}

static void char_write(char c) {
	uint16_t i;

	if (cursor >= WIDTH * HEIGHT) {
		for (i = 0; i < WIDTH * HEIGHT - WIDTH; i++) {
			vbuf[i] = vbuf[i + WIDTH];
		}

		for (i = WIDTH * HEIGHT - WIDTH; i < WIDTH * HEIGHT; i++) {
			vbuf[i] = 0x0F20;
		}
		
		c_base -= WIDTH;
		cursor -= WIDTH;
	}

	switch (c) {
		case '\0': 
			break;
		case '\t': 
			cursor = (cursor + TAB) - (cursor % TAB); 
			break;
		case '\n': 
			buffer = c_base;
			vbuf[cursor] = 0x0000;
			c_base = cursor = (cursor - cursor % WIDTH) + 80; 
			break;
		case '\r': 
			cursor = (cursor - cursor % WIDTH); 
			c_base = cursor; 
			break;
		case '\b': 
			if (c_base < cursor) cursor--; 
			vbuf[cursor] = 0x0F00; 
			break;
		default: 
			vbuf[cursor++] = (uint16_t) (c | 0x0F00); 
			break;
	}

	if (cursor >= WIDTH * HEIGHT) {
		for (i = 0; i < WIDTH * HEIGHT - WIDTH; i++) {
			vbuf[i] = vbuf[i + WIDTH];
		}

		for (i = WIDTH * HEIGHT - WIDTH; i < WIDTH * HEIGHT; i++) {
			vbuf[i] = 0x0F20;
		}
		
		c_base -= WIDTH;
		cursor -= WIDTH;
	}
}
