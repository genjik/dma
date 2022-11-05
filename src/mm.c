#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

/* The standard allocator interface from stdlib.h.  These are the
 * functions you must implement, more information on each function is
 * found below. They are declared here in case you want to use one
 * function in the implementation of another. */
void *malloc(size_t size);
void free(void *ptr);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);

void free_list_validator();

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
static void *free_list = NULL;

static int init_heap() {
	free_list = sbrk(CHUNK_SIZE);
	if (sbrk == (void*) -1) { 
		return -1;
	}

	for (int i = 0; i < 512; i++) {
		void *curr_addr = free_list + (i * sizeof(uintptr_t));
		*(uintptr_t *) curr_addr = 0;
	}
	
	return 0;
}

/*
 * You must implement malloc().  Your implementation of malloc() must be
 * the multi-pool allocator described in the project handout.
 */
void *malloc(size_t size) {
	if (size <= 0) { 
		return NULL; 
	}

	// Check if free_list is initialized. If it isn't, initialize it.
	if (free_list == NULL) {
		int error = init_heap();
		if (error == -1) { 
			return NULL; 
		}
	}


	// Use bulk_alloc for size > 4088
	if (size > CHUNK_SIZE - sizeof(uintptr_t)) {
		size_t total_size = size + sizeof(uintptr_t);
		void *addr = bulk_alloc(total_size);	

		if (addr == NULL) { 
			return NULL; 
		}

		*(uintptr_t *) addr = total_size;
		
		fprintf(stderr, "malloc req %ld address=%p\n", size, addr + sizeof(uintptr_t));
		return addr + sizeof(uintptr_t);
	}

	size_t i = block_index(size);
	size_t offset = i * sizeof(uintptr_t);
	void *ptr_to_list = free_list + offset;

	if (*(uintptr_t **) ptr_to_list == NULL) {
		void *new_addr = sbrk(CHUNK_SIZE);	
		if (new_addr == (void *) -1) {
			return NULL;
		}

		size_t size_of_block = 1 << i;
		for (int j = 0; j < CHUNK_SIZE; j += size_of_block) {
			void *curr_addr = new_addr + j;
			*(uintptr_t *) curr_addr = size_of_block;

			void *adj_addr = curr_addr + sizeof(uintptr_t);

			if (j == CHUNK_SIZE - size_of_block) {
 				*(uintptr_t **) adj_addr = NULL;
			} else {
				*(uintptr_t **) adj_addr = curr_addr + size_of_block;
			}
		}

		*(uintptr_t **) ptr_to_list = new_addr + size_of_block;

		fprintf(stderr, "malloc req %ld address=%p\n", size, new_addr + sizeof(uintptr_t));

		return new_addr + sizeof(uintptr_t);
	} else {
		void *block_addr = *(uintptr_t **) ptr_to_list;
		*(uintptr_t **) ptr_to_list = *(uintptr_t **) (block_addr + sizeof(uintptr_t));	

		fprintf(stderr, "malloc req %ld address=%p\n", size, block_addr + sizeof(uintptr_t));
		return block_addr + sizeof(uintptr_t);
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
	if (nmemb <= 0 || size <= 0) {
		return NULL;
	}

	size_t total_size = nmemb * size;

	if (total_size <= 0) {
		return NULL;
	}

    void *ptr = malloc(total_size);
	if (ptr == NULL) {
		return NULL;
	}

    memset(ptr, 0, total_size);
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
	if (ptr != NULL && size == 0) {
		free(ptr);
		return NULL;
	}
	
	if (ptr == NULL) { 
		return malloc(size);
	}

	
	size_t old_size = *(uintptr_t *) (ptr - sizeof(uintptr_t));
	if (size <= old_size - sizeof(uintptr_t)) {
		return ptr;
	}

	void *new_addr = malloc(size);
	if (new_addr == NULL) {
		return NULL;
	}

	for (int i = 0; i < old_size - sizeof(uintptr_t); i++) {
		*(char *) (new_addr + i) = *(char *) (ptr + i + sizeof(uintptr_t));
	}

	fprintf(stderr, "old_size: %ld, new_size: %ld requested: %ld ptr=%p\n", old_size, *(uintptr_t *) (new_addr - sizeof(uintptr_t)), size, ptr);
	fprintf(stderr, "next ptr=%p\n", *(uintptr_t **) new_addr);
	free(ptr);

	return new_addr;
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

	void *ptr_to_block = ptr - sizeof(uintptr_t);
	size_t size_of_block = *(uintptr_t *) ptr_to_block;

	if (size_of_block > CHUNK_SIZE) {
		bulk_free(ptr_to_block, size_of_block);
	} else {
		int block_size = (int) size_of_block;
		int i = 32 - __builtin_clz(block_size) - 1;				
		if (free_list != NULL) {
			void *free_list_addr = free_list + (i * sizeof(uintptr_t));
			*(uintptr_t **) ptr = *(uintptr_t **) free_list_addr;
			*(uintptr_t **) free_list_addr = ptr_to_block;
		}
	}
}
