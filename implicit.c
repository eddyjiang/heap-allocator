/* Implicit.c is my own implicit-free heap allocator that functions
 * accordingly with class and textbook guidelines. It uses headers located
 * before each allocation that contain information about each block's size
 * as well as whether or not it is in use. Please enjoy!
 */
#include "./allocator.h"
#include "./debug_break.h"
#include <string.h>
#include <stdio.h>

static void *segment_start;
static size_t segment_size;
static size_t nused;

/* This must be called by a client before making any allocation
 * requests. The function returns true if initialization was
 * successful, or false otherwise. The myinit function can be
 * called to reset the heap to an empty state. When running
 * against a set of of test scripts, our test harness calls
 * myinit before starting each new script.
 */
bool myinit(void *heap_start, size_t heap_size) {
    segment_start = heap_start;
    if (heap_size < 2 * ALIGNMENT) {
        return false; // no room for a header and its smallest block
    }
    segment_size = heap_size;
    *(size_t *)segment_start = (segment_size - ALIGNMENT);
    nused = 0; // keeps track of allocated memory, not considering headers
    return true;
}

/* Helper: Rounds up size_t size to the given multiple, which must be a
 * power of 2, and returns the result.
 */
size_t roundup(size_t size, size_t multiple) {
    return (size + multiple - 1) & ~(multiple - 1);
}

/* Helper: Takes in a ptr to a header and looks at its last bit to
 * determine if it is used or free.
 */
bool is_used(void *ptr) {
    return *(size_t *)ptr & 1; // returns last bit, 0 = free, 1 = used
}

/* Helper: Takes in a ptr to a header and returns the size_t given by
 * the header without considering its last bit.
 */
size_t get_size(void *ptr) {
    return *(size_t *)ptr & ~1; // returns with last bit to 0
}

/* Helper: Takes in a pointer to some memory location on the heap and
 * determines if it is beyond the allocated heap.
 */
bool is_past_end(void *ptr) {
    return (char *)ptr >= (char *)segment_start + segment_size;
}

/* Helper: Takes in a block of memory on the heap and returns its
 * header.
 */
void *get_header(void *ptr) {
    return (char *)ptr - ALIGNMENT;
}

/* Helper: Takes in a ptr to a header and returns the next header by looking at
 * the size of the block specified by the header and traversing that
 * many bytes past the header.
 */
void *get_next_header(void *ptr) {
    return (char *)ptr + get_size(ptr) + ALIGNMENT;
}

/* Returns the first fit free block for the requested_size by traversing
 * all blocks on the heap and returning the first free block with enough
 * space. If the requested_size is 0 or greater than the MAX_REQUEST_SIZE,
 * mymalloc returns NULL.
 */
void *mymalloc(size_t requested_size) {
    if (requested_size == 0 || requested_size > MAX_REQUEST_SIZE) {
        return NULL;
    }
    size_t needed = roundup(requested_size, ALIGNMENT);
    void *cur = segment_start;
    while (is_used(cur) || get_size(cur) < needed) {
        cur = get_next_header(cur);
        if (is_past_end(cur)) {
            return NULL;
        }
    }
    
    size_t block_size = get_size(cur);
    if (block_size < needed + 2 * ALIGNMENT) {
        needed = block_size; // gives extra space to allocation request
    }
    *(size_t *)cur = needed;
    *(size_t *)cur |= 1;
    nused += needed;
    void *ptr = (char *)cur + ALIGNMENT;

    if (block_size > needed) { // creates header for rest of free block
        cur = get_next_header(cur);
        *(size_t *)cur = (block_size - needed - ALIGNMENT);
    }
    
    return ptr;
}

/* Accepts a ptr to some used block of memory on the heap and manipulates
 * its header to say that it's free. Does not coalesce adjacent free blocks.
 * Does not zero out "freed" memory.
 */
void myfree(void *ptr) {
    if (ptr != NULL) {
        void *header = get_header(ptr);
        nused -= get_size(header);
        *(size_t *)header &= ~1; // sets last bit to 0
    }
}

/* Accepts an old_ptr to some used block of memory on the heap and uses
 * mymalloc, memcpy, and myfree to copy the memory at the old_ptr into a
 * new_ptr with the specified new_size before freeing the memory at the
 * old_ptr. If old_ptr is NULL, acts the same as mymalloc. If new_size is 0,
 * acts the same as myfree and returns NULL. If there is no suitable block
 * on the heap that can fit the new_size, returns NULL. Does not perform
 * in-place realloc.
 */
void *myrealloc(void *old_ptr, size_t new_size) {
    void *new_ptr = mymalloc(new_size);
    if (old_ptr == NULL) {
        return new_ptr; // acts same as mymalloc
    }
    if (new_size == 0) {
        myfree(old_ptr); // acts same as myfree
    }
    else {
        void *old_header = get_header(old_ptr);
        size_t old_size = get_size(old_header);
        if (new_ptr == NULL) {
            return NULL;
        }
        if (new_size < old_size) {
            old_size = new_size; // only copies memory up to new_size
        }
        memcpy(new_ptr, old_ptr, old_size);
        myfree(old_ptr);
    }
    return new_ptr;
}

/* validate_heap checks internal structures.
 * Returns true if all is ok, or false otherwise.
 * This function is called periodically by the test
 * harness to check the state of the heap allocator.
 * can also use the breakpoint() function to stop
 * in the debugger - e.g. if (something_is_wrong) breakpoint();
 */
bool validate_heap() {
    void *cur = segment_start;
    size_t total_size = 0; // counts header and block sizes together
    while (!is_past_end(cur)) {
        total_size += get_size(cur) + ALIGNMENT;
        cur = get_next_header(cur);
    }
    if (total_size != segment_size) {
        printf("Ooops! Heap doesn't add up to heap?!\n");
        printf("Counted: %lu != Heap size: %lu\n",
               total_size, segment_size);
        breakpoint();
        return false;
    }
    return true;
}

/* dump_heap prints out the the block contents of the heap. It is not
 * called anywhere, but is a useful helper function to call from gdb when
 * tracing through programs. It prints out the total range of the heap, and
 * information about each block within it.
 */
void dump_heap() {
    printf("Heap segment starts at address %p, ends at %p.\n",
           segment_start, (char *)segment_start + segment_size);
    printf("%lu bytes currently used.\n", nused);
    void *cur = segment_start;
    while (!is_past_end(cur)) {
        printf("Block address: %p ", cur);
        printf("Used? %d ", is_used(cur));
        printf("Size: %lu\n", get_size(cur));
        cur = get_next_header(cur);
    }
}
