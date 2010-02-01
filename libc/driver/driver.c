/* Copyright 2010 Nick Johnson */

#include <driver.h>
#include <flux.h>

void rirq(uint8_t irq) {
	_ctrl(CTRL_IRQRD | CTRL_IRQ(irq), CTRL_IRQRD | CTRL_IRQMASK);
}