/*
 * Copyright (C) 2011 Nick Johnson <nickbjohnson4224 at gmail.com>
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

#include <stdint.h>
#include <string.h>

#include <rdi/core.h>
#include <rdi/arch.h>

#include "ata.h"

struct ata_drive ata[4];

void ata_sleep400(uint8_t drive) {

	inb(ata[drive].base + REG_ASTAT);
	inb(ata[drive].base + REG_ASTAT);
	inb(ata[drive].base + REG_ASTAT);
	inb(ata[drive].base + REG_ASTAT);
}

void ata_select(uint8_t drive) {

	outb(ata[drive].base + REG_SELECT, SEL(drive) ? 0xB0 : 0xA0);
	ata_sleep400(drive);
}

void ata_init(void) {
	size_t dr, i;
	uint8_t status, err, cl, ch;
	uint16_t c;
	uint16_t buffer[256];

	/* detect I/O ports */
	ata[ATA0].base = 0x1F0;
	ata[ATA1].base = 0x170;
	ata[ATA0].dma  = 0; // FIXME
	ata[ATA1].dma  = 0; // FIXME

	/* detect IRQ numbers */
	ata[ATA0].irq = 14;
	ata[ATA1].irq = 15;

	/* copy controller ports for drives */

	/* ATA 0 master */
	ata[ATA00].base = ata[ATA0].base;
	ata[ATA00].dma  = ata[ATA0].dma;
	ata[ATA00].irq  = ata[ATA0].irq;

	/* ATA 0 slave */
	ata[ATA01].base = ata[ATA0].base;
	ata[ATA01].dma  = ata[ATA0].dma;
	ata[ATA01].irq  = ata[ATA0].irq;

	/* ATA 1 master */
	ata[ATA10].base = ata[ATA1].base;
	ata[ATA10].dma  = ata[ATA1].dma;
	ata[ATA10].irq  = ata[ATA1].irq;

	/* ATA 1 slave */
	ata[ATA11].base = ata[ATA1].base;
	ata[ATA11].dma  = ata[ATA1].dma;
	ata[ATA11].irq  = ata[ATA1].irq;

	/* disable controller IRQs */
	outw(ata[ATA0].base + REG_CTRL, CTRL_NEIN);
	outw(ata[ATA1].base + REG_CTRL, CTRL_NEIN);

	/* detect drives */
	for (dr = 0; dr < 4; dr++) {
		ata_select(dr);

		/* send IDENTIFY command */
		outb(ata[dr].base + REG_COUNT0, 0);
		outb(ata[dr].base + REG_LBA0,   0);
		outb(ata[dr].base + REG_LBA1,   0);
		outb(ata[dr].base + REG_LBA2,   0);
		ata_sleep400(dr);
		outb(ata[dr].base + REG_CMD, CMD_ID);
		ata_sleep400(dr);

		/* read status */
		status = inb(ata[dr].base + REG_STAT);

		/* check for drive response */
		if (!status) continue;

		ata[dr].flags = FLAG_EXIST;

		/* poll for response */
		while (1) {
			status = inb(ata[dr].base + REG_STAT);

			if (status & STAT_ERROR){
				err = 1;
				break;
			}

			if (status & STAT_BUSY) {
				continue;
			}

			err = 0;
			break;
		}

		/* try to catch ATAPI and SATA devices */
		if (err && (inb(ata[dr].base + REG_LBA1) || inb(ata[dr].base + REG_LBA2))) {
			cl = inb(ata[dr].base + REG_LBA1);
			ch = inb(ata[dr].base + REG_LBA2);
			c = cl | (ch << 8);
			
			ata_sleep400(dr);

			if (c == 0xEB14 || c == 0x9669) {
				/* is ATAPI */
				ata[dr].flags |= FLAG_ATAPI;
				ata[dr].sectsize = 11;

				/* use ATAPI IDENTIFY */
				outb(ata[dr].base + REG_CMD, CMD_ATAPI_ID);
				ata_sleep400(dr);
			}
			else if (c == 0xC33C) {
				/* is SATA */
				ata[dr].flags |= FLAG_SATA;
			}
			else {
				/* unknown device; ignore */
				ata[dr].flags = 0;
				continue;
			}
		}
		else {
			/* assume 512-byte sectors for ATA */
			ata[dr].sectsize = 9;
		}

		/* wait for IDENTIFY to be ready */
		while (1) {
			status = inb(ata[dr].base + REG_STAT);

			if (status & STAT_ERROR) {
				err = 1;
				break;
			}

			if (!(status & STAT_BUSY) && (status & STAT_DRQ)) {
				err = 0;
				break;
			}
		}

		/* read in IDENTIFY space */
		for (i = 0; i < 256; i++) {
			buffer[i] = inw(ata[dr].base + REG_DATA);
		}

		if (!buffer[ID_TYPE]) {
			/* no drive */
			ata[dr].flags = 0;
			continue;
		}

		ata[dr].signature    = buffer[ID_TYPE];
		ata[dr].capabilities = buffer[ID_CAP];
		ata[dr].commandsets  = buffer[ID_CMDSET] | (uint32_t) buffer[ID_CMDSET+1] << 16;

		/* get LBA mode and disk size */
		if (ata[dr].commandsets & (1 << 26)) {
			/* is LBA48 */
			ata[dr].flags |= FLAG_LONG;
			ata[dr].size   = (uint64_t) buffer[ID_MAX_LBA48+0];
			ata[dr].size  |= (uint64_t) buffer[ID_MAX_LBA48+1] << 16;
			ata[dr].size  |= (uint64_t) buffer[ID_MAX_LBA48+2] << 32;
			ata[dr].size  |= (uint64_t) buffer[ID_MAX_LBA48+3] << 48;
		}
		else {
			/* is LBA24 */
			ata[dr].size  = (uint32_t) buffer[ID_MAX_LBA+0];
			ata[dr].size |= (uint32_t) buffer[ID_MAX_LBA+1] << 16;
		}

		/* get model string */
		for (i = 0; i < 40; i += 2) {
			ata[dr].model[i]   = buffer[ID_MODEL + (i / 2)] >> 8;
			ata[dr].model[i+1] = buffer[ID_MODEL + (i / 2)] & 0xFF;
		}
		ata[dr].model[40] = '\0';
		for (i = 39; i > 0; i--) {
			if (ata[dr].model[i] == ' ') {
				ata[dr].model[i] = '\0';
			}
			else break;
		}

		printf("found drive: ");

		switch (dr) {
		case ATA00: printf("ATA 0 Master\n"); break;
		case ATA01: printf("ATA 0 Slave\n");  break;
		case ATA10: printf("ATA 1 Master\n"); break;
		case ATA11: printf("ATA 1 Slave\n");  break;
		}

		printf("\t");
		printf((ata[dr].flags & FLAG_SATA) ? "S" : "P");
		printf((ata[dr].flags & FLAG_ATAPI) ? "ATAPI " : "ATA ");
		printf((ata[dr].flags & FLAG_LONG) ? "LBA48 " : "LBA24 ");
		printf("\n");

		printf("\tsize: %d KB (%d sectors)\n",
			(uint32_t) ata[dr].size * (1 << ata[dr].sectsize) >> 10,
			(uint32_t) ata[dr].size);
		printf("\tmodel: %s\n", ata[dr].model);
	}
}

int main(int argc, char **argv) {
	
	ata_init();

	return 0;
}
