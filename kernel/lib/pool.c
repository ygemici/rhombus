// Copyright 2009 Nick Johnson

#include <lib.h>
#include <mem.h>

__attribute__ ((section(".ttext"))) 
pool_t *pool_new(u32int size, pool_t *pool) {
	u32int i, npool, extra;

	npool = (size - 1) / 1024 + 1; // 1024 entries per pool (and round up)
	extra = size - ((npool - 1) * 1024);
	
	for (i = 0; i < npool; i++) {
		memclr(pool[i].word, sizeof(u32int) * 32);
		pool[i].first = 0x0000;
		pool[i].total = (i != npool - 1) ? 1024 : extra;
		pool[i].setup = 0x4224;
		pool[i].upper = pool[i].total;
	}

	return pool;
}

u32int pool_alloc(pool_t *pool) {
	u32int p, w, b;	// pool, word, bit

	// Find suitable pool
	for (p = 0; pool[p].total == 0; p++) 
		if (pool[p].setup != 0x4224) goto full;
	if (pool[p].setup != 0x4224) goto full;

	// Find suitable word within pool
	for (w = pool[p].first / 32; w < pool[p].upper / 32; w++)
		if (pool[p].word[w] != 0xFFFFFFFF) break;

	// Find open bit within word
	for (b = 0; pool[p].word[w] & (0x1 << b); b++);
	if (b == 32) goto full;

	pool[p].word[w] |= (0x1 << b);
	pool[p].total --;
	if (pool[p].first == ((w << 5) | b)) pool[p].first++;
	return ((p << 10) | ((w << 5) | b));

	full:
	printk("%x\n", pool_query(pool));
	panic("pool allocator full");
	return 0;
}

u32int pool_free(pool_t *pool, u32int pos) {
	u32int p, w, b;

	// Convert to bit coordinates
	p = pos >> 10;
	w = (pos >> 5) % 1024;
	b = pos % 32;

	// Clear bit and set metadata
	pool[p].word[w] &= ~(0x1 << b);
	pool[p].first = min(pool[p].first, ((w << 5) | b));
	pool[p].total ++;

	return 0;
}

u32int pool_query(pool_t *pool) {
	u32int total, p;
	
	// Tally usage of all pools
	total = 0;
	for (p = 0; pool[p].setup; p++)
		total += (pool[p].upper - pool[p].total);

	return total;
}