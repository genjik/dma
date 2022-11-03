#include <string.h>
#include <stdio.h>
#include <stdint.h>

/* The standard allocator interface from stdlib.h.  These are the
 * functions you must implement, more information on each function is
 * found below. They are declared here in case you want to use one
 * function in the implementation of another. */
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void *sbrk(intptr_t increment);

/* When requesting memory from the OS using sbrk(), request it in
 * increments of CHUNK_SIZE. */
#define CHUNK_SIZE (1<<12)

/*
 * This function, defined in bulk.c, allocates a contiguous memory
 * region of at least size bytes.  It MAY NOT BE USED as the allocator
 * for pool-allocated regions.  Memory allocated using bulk_alloc()
 * must be freed by bulk_free().
 *
 * This function will return NULL on failure.
 */
extern void *bulk_alloc(size_t size);

/*
 * This function is also defined in bulk.c, and it frees an allocation
 * created with bulk_alloc().  Note that the pointer passed to this
 * function MUST have been returned by bulk_alloc(), and the size MUST
 * be the same as the size passed to bulk_alloc() when that memory was
 * allocated.  Any other usage is likely to fail, and may crash your
 * program.
 */
extern void bulk_free(void *ptr, size_t size);

/*
 * This function computes the log base 2 of the allocation block size
 * for a given allocation.  To find the allocation block size from the
 * result of this function, use 1 << block_size(x).
 *
 * Note that its results are NOT meaningful for any
 * size > 4088!
 *
 * You do NOT need to understand how this function works.  If you are
 * curious, see the gcc info page and search for __builtin_clz; it
 * basically counts the number of leading binary zeroes in the value
 * passed as its argument.
 */
static inline __attribute__((unused)) int block_index(size_t x) {
    if (x <= 8) {
        return 5;
    } else {
        return 32 - __builtin_clz((unsigned int)x + 7);
    }
}

// Initializes multi-pool allocator's free list.
// Keep in mind that indexes [0, 4] inclusive should remain NULL at all time
// For now, init requires using sbrk(CHUNK_SIZE=4096), but the data structure
// only needs 104/4096 bytes. Any memory address after 103 should not be used.
static intptr_t** free_list = NULL;
static int init_heap() {
	free_list = sbrk(CHUNK_SIZE);
	if (sbrk == (void*) -1) { return -1; }

	for (int i = 0; i <= 12; i++) {
		*(free_list + i) = NULL;
	}
	
	return 0;
}

/*
 * You must implement malloc().  Your implementation of malloc() must be
 * the multi-pool allocator described in the project handout.
 */
void *malloc(size_t size) {
	if (size == 0) { return NULL; }

	if (free_list == NULL) {
		int ret_val = init_heap();
		if (ret_val == -1) { return NULL; }
	}

	if (size > 4088) {
		void* addr = bulk_alloc(size + sizeof(size_t));	

		if (addr == NULL) { 
			return NULL; 
		}

		*(size_t *) addr = size + sizeof(size_t);
		return addr + sizeof(size_t);
	}

	int i = block_index(size);

	if (*(free_list + i) == NULL) {
		void *new_addr = sbrk(CHUNK_SIZE);	
		if (new_addr == (void *) -1) {
			return NULL;
		}

		size_t size_of_block = 1 << i;
		for (int j = 0; j < CHUNK_SIZE; j += size_of_block) {
			*(size_t *) (new_addr + j) = size_of_block;
			if (j == CHUNK_SIZE - size_of_block) {
				*(intptr_t **) (new_addr + j + sizeof(size_t)) = NULL;
			} else {
				*(intptr_t **) (new_addr + j + sizeof(size_t)) = new_addr + (j * 2);	
			}
		}

		*(free_list + i) = new_addr + size_of_block;
		return new_addr + sizeof(size_t);
	} else {
		void *block = *(free_list + i);
		*(free_list + i) = block + sizeof(size_t);
		return block + sizeof(size_t);
	}
}

/*
 * You must also implement calloc().  It should create allocations
 * compatible with those created by malloc().  In particular, any
 * allocations of a total size <= 4088 bytes must be pool allocated,
 * while larger allocations must use the bulk allocator.
 *
 * calloc() (see man 3 calloc) returns a cleared allocation large enough
 * to hold nmemb elements of size size.  It is cleared by setting every
 * byte of the allocation to 0.  You should use the function memset()
 * for this (see man 3 memset).
 */
void *calloc(size_t nmemb, size_t size) {
    void *ptr = bulk_alloc(nmemb * size);
    memset(ptr, 0, nmemb * size);
    return ptr;
}

/*
 * You must also implement realloc().  It should create allocations
 * compatible with those created by malloc(), honoring the pool
 * alocation and bulk allocation rules.  It must move data from the
 * previously-allocated block to the newly-allocated block if it cannot
 * resize the given block directly.  See man 3 realloc for more
 * information on what this means.
 *
 * It is not possible to implement realloc() using bulk_alloc() without
 * additional metadata, so the given code is NOT a working
 * implementation!
 */
void *realloc(void *ptr, size_t size) {
    fprintf(stderr, "Realloc is not implemented!\n");
    return NULL;
}

/*
 * You should implement a free() that can successfully free a region of
 * memory allocated by any of the above allocation routines, whether it
 * is a pool- or bulk-allocated region.
 *
 * The given implementation does nothing.
 */
void free(void *ptr) {
	if (ptr == NULL) { return; }

	size_t size_of_block = *(size_t *) (ptr - sizeof(size_t));

	if (size_of_block > CHUNK_SIZE) {
		bulk_free(ptr - sizeof(size_t), size_of_block);
	} else {
		int i = size_of_block - __builtin_clz(size_of_block) - 1;				
		if (free_list != NULL) {
			*(intptr_t *) ptr = *(free_list + i);
		}
	}
}
