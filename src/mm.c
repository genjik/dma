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

/* When requesting memory from the OS using sbrk(), request it in
 * increments of CHUNK_SIZE. */
#define CHUNK_SIZE (1<<12)
#define DEBUG1

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

typedef struct Node {
	uintptr_t size;
	uintptr_t *next_addr;
} Node;

// [0-63] memory addresses are reserved for free_list data structure
// [64-4095] will be splited into 63 blocks of size 64 bytes and added to
// free_list
static void *free_list = NULL;

static int init_heap() {
	free_list = sbrk(CHUNK_SIZE);
	if (sbrk == (void*) -1) { 
		return -1;
	}

	for (int i = 0; i < 8; i++) {
		void *curr_addr = free_list + (i * sizeof(uintptr_t));
		*(uintptr_t *) curr_addr = 0;
	}

	void *offset = free_list + 64;
	for (int i = 0; i < 63; i++) {
		void *curr_addr = offset + (i * 64);

		Node *curr_node = (Node *) curr_addr;
		curr_node->size = 64;
		if (i != 62) {
			curr_node->next_addr = curr_addr + 64;
		} else {
			curr_node->next_addr = NULL;
		}
	}

	*(uintptr_t **) (free_list + 8) = offset;

	return 0;
}

void print_ds() {
	#ifdef DEBUG
	if (free_list == NULL) {
		fprintf(stderr, "free_list is empty\n");
		return;
	}

	for (int i = 0; i < 8; i++) {
		void *curr_addr = free_list + (i * sizeof(uintptr_t));

		fprintf(stderr, "\n%d) %p = ", i, curr_addr);

		void *temp_addr = *(uintptr_t **) curr_addr;
		Node *temp_node = (Node *) temp_addr;
		int count = 0;
		
		if (temp_node == NULL) {
			fprintf(stderr, "%p\n", temp_addr);
		} else {
			while (temp_node != NULL) {
				fprintf(stderr, "%p=[size:%ld, next:%p] -> ", temp_node, temp_node->size, temp_node->next_addr);
				temp_node = (Node *) temp_node->next_addr;
				count++;
			}
			fprintf(stderr, " free blocks in the list: %d\n\n", count);
		}
	}
	#endif
}

/*
 * You must implement malloc().  Your implementation of malloc() must be
 * the multi-pool allocator described in the project handout.
 */
void *malloc(size_t size) {
	if (size == 0) {
		return NULL;
	}

	// Check if heap is initialized
	if (free_list == NULL) {
		if (init_heap() == -1) {
			return NULL;
		}
		//print_ds();
	}

	if (size > CHUNK_SIZE - sizeof(uintptr_t)) {
		size_t total_size = size + sizeof(uintptr_t);
		void *block_addr = bulk_alloc(total_size);

		if (block_addr == NULL) { 
			return NULL; 
		}
		((Node *) block_addr)->size = total_size;

		return block_addr + sizeof(uintptr_t);
	}

	// Subtracting 5 because heap data structure starts at index=0, not
	// index=5
	int i = block_index(size) - 5;
	void *free_list_addr = free_list + (i * sizeof(uintptr_t));

	if (*(Node **) free_list_addr == NULL) {
		void *new_addr = sbrk(CHUNK_SIZE);

		if (new_addr == (void *) -1) {
			return NULL;
		}

		size_t block_size = 1 << block_index(size);	
		int list_size = CHUNK_SIZE / block_size;

		for (int j = 0; j < list_size; j++) {
			void *curr_addr = new_addr + (j * block_size);

			((Node *) curr_addr)->size = block_size;

			if (j == list_size - 1) {
				((Node *) curr_addr)->next_addr = NULL;
			} else {
				((Node *) curr_addr)->next_addr = curr_addr + block_size;
			}
		}

		*(Node **) free_list_addr = (Node *) ((Node *) new_addr)->next_addr;

		return new_addr + sizeof(uintptr_t);
	} else {
		Node *return_node = *(Node **) free_list_addr;	
		Node *next_node = (Node *) return_node->next_addr;

		*(Node **) free_list_addr = next_node;

		void *ret_addr = ((void *) return_node) + sizeof(uintptr_t);

		print_ds();
		return ret_addr;
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
	if (nmemb == 0 || size == 0) {
		return NULL;
	}

	size_t total_size = nmemb * size;

	// WARNING!!! DON'T FORGET TO IMPLEMENT THIS LATER
	// calloc requirement: catching integer overflow on multiplication
	// of nmemb * size

	void *ret_addr = malloc(total_size);
	if (ret_addr == NULL) {
		return ret_addr;
	}

	memset(ret_addr, 0, total_size);
	
	return ret_addr;
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

	uintptr_t old_size = *(uintptr_t *) (ptr - sizeof(uintptr_t));
	
	if (size <= old_size - sizeof(uintptr_t)) {
		return ptr;
	}



	void *new_addr = malloc(size);
	if (new_addr == NULL) {
		return NULL;
	}

	for (int i = 0; i < old_size - sizeof(uintptr_t); i++) {
		*(char *) (new_addr + i) = *(char *) (ptr + i);	
	}

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
	if (ptr == NULL) {
		return;
	}

	void *block_addr = ptr - sizeof(uintptr_t);
	size_t block_size = *(uintptr_t *) block_addr;

	if (block_size > CHUNK_SIZE) {
		bulk_free(block_addr, block_size);
		return;
	}

	if (free_list == NULL) {
		return;
	}

	int i = 32 - __builtin_clz((int) block_size) - 6;
	
	void *free_list_addr = free_list + (i * sizeof(uintptr_t));
	Node *prev_head = *(Node **) free_list_addr;

	Node *freeing_block = (Node *) block_addr;
	freeing_block->next_addr = (uintptr_t *) prev_head;

	*(Node **) free_list_addr = freeing_block;
	print_ds();
}
