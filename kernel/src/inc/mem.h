// Copyright 2009 Nick Johnson

/* General virtual memory map:
0x00000000 - 0xF3FFFFFF: userspace 		(user, read-write, cloned)
	0x00001000: process image
	0xF0000000: libc image
	0xF3FF0000: standard stack
	0xF5FFE000: state saving stack
	0xF7FFF000: signal handler table
0xF8000000 - 0xFEFFFFFF: libspace 		(user, readonly, linked)
	0xF8000000: signal override table
	0xF8001000: static libsys data
	0xFC000000: libsys image
0xFF000000 - 0xFFFFFFFF: kernelspace 	(kernel, linked)
	0xFF000000: remapped lower memory
	0xFF100000: kernel image 
	0xFF400000: task table
	0xFF500000: none/various
	0xFF800000: temporary map
	0xFFC00000: resident map
*/

#ifndef MEM_H
#define MEM_H

/***** PAGE FLAGS *****/

// Normal page flags
#define PF_PRES 0x1		// Is present
#define PF_RW   0x2		// Is writeable
#define PF_USER 0x4		// Is user-mode
#define PF_DIRT 0x20	// Is dirty
#define PF_ACCS 0x40	// Has been accessed

// Special page flags
#define PF_LINK 0x200	// Is linked from elsewhere (do not free frame)
#define PF_REAL 0x400	// Is linked to elsewhere (do not free at all)
#define PF_SWAP 0x800	// Is swapped out

/***** DATA TYPES *****/
typedef u32int page_t;
typedef u32int ptbl_t;
typedef u32int map_t;

/****** SYSTEM CALLS *****/
u32int mmap(u32int base, u32int flags);	// Map memory to a page
u32int umap(u32int base);				// Unmap memory from a page
u32int rmap(u32int base, u32int src);	// Move a mapped frame
u32int fmap(u16int task, u32int base);	// Force a remote remapping (task 0 == physical mem)

//extern map_t kmap;
void  map_temp(map_t map);				// Puts a map in temporary space
map_t map_alloc(map_t map);				// Allocates a new map
map_t map_free(map_t map);				// Frees a map (does not clean)
map_t map_clean(map_t map);				// Cleans a map (frees all *user* memory)
map_t map_clone();						// Clones the current map
map_t map_load(map_t map);				// Activates a new map

/***** FRAME.C ******/
extern pool_t fpool[MAX_PHMEM / 4];
#define frame_new() (pool_alloc(fpool) << 12)			// Allocates a new frame	
#define frame_free(addr) (pool_free(fpool, addr >> 12))	// Frees a frame

/***** PAGE.C *****/
extern ptbl_t *cmap, *tmap;					// Address of current page directory
extern page_t *ctbl, *ttbl;					// Base of current page tables

void   page_touch(u32int page);				// Makes sure a page exists
void   page_set(u32int page, page_t value);	// Sets the value of a page
page_t page_get(u32int page);				// Returns the value of a page
#define PF_MASK 0x0C67						// Page flags that can be used
#define page_fmt(base,flags) (((base)&0xFFFFF000)|((flags)&PF_MASK))
#define page_ufmt(page) ((page)&0xFFFFF000)
#define p_alloc(addr, flags) (page_set(addr, page_fmt(frame_new(), (flags) | PF_PRES)))
#define p_free(addr) do { \
frame_free(page_ufmt(page_get(addr))); \
page_set(addr, 0); \
} while(0);

#endif /*MEM_H*/
