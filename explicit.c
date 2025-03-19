/* Explicit.c is my own explicit heap allocator that builds on what I
 * learned from writing the implicit version. I have developed a
 * high-performance heap allocator that adds an explicit free list as
 * specified in class and in the textbook and includes support for coalescing
 * and in-place realloc. Please enjoy!
 */
#include "./allocator.h"
#include "./debug_break.h"
#include <string.h>
#include <stdio.h>

static void *segment_start;
static size_t segment_size;
static size_t nused;
static void *free_start;

void **get_next_free(void *ptr);
void **get_prev_free(void *ptr);
void dump_heap();

/* This must be called by a client before making any allocation
 * requests. The function returns true if initialization was
 * successful, or false otherwise. The myinit function can be
 * called to reset the heap to an empty state. When running
 * against a set of of test scripts, our test harness calls
 * myinit before starting each new script.
 */
bool myinit(void *heap_start, size_t heap_size) {
    segment_start = heap_start;
    if (heap_size < 3 * ALIGNMENT) {
        return false; // no room for a header and its smallest block
    }
    segment_size = heap_size;
    *(size_t *)segment_start = (segment_size - ALIGNMENT);

    free_start = segment_start; // doubly linked list of free blocks
    *get_prev_free(free_start) = NULL;
    *get_next_free(free_start) = NULL;
    
    nused = 0; // keeps track of allocated memory, not considering headers
    return true;
}

/* Helper: Rounds up size_t size to the given multiple, which must be a
 * power of 2, and returns the result. Must also be at least 2 * ALIGNMENT,
 * or 16 bytes to be able to store prev and next pointers for the doubly
 * linked list of free blocks as a free block.
 */
size_t roundup(size_t size, size_t multiple) {
    size_t rounded = (size + multiple - 1) & ~(multiple - 1);
    if (rounded < 2 * ALIGNMENT) {
        rounded = 2 * ALIGNMENT;
    }
    return rounded;
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

/* Helper: Takes in a ptr to the header of a free block and returns the address
 * of the next free block header using the doubly linked list as stored
 * in the "contents" of the free block.
 */
void **get_next_free(void *ptr) {
    return (void **)((char *)ptr + 2 * ALIGNMENT);
}

/* Helper: Takes in a ptr to the header of a free block and returns the address
 * of the previous free block header using the doubly linked list as stored
 * in the "contents" of the free block.
 */
void **get_prev_free(void *ptr) {
    return (void **)((char *)ptr + ALIGNMENT);
}

/* Helper: Takes in a ptr to the header of a previously free block (now used) and
 * updates the doubly linked list of free blocks accordingly to account for
 * its absence in the list now. Rearranges the linked list around the
 * previously free block.
 */
void remove_free(void *ptr) {
    void *prev = *get_prev_free(ptr);
    void *next = *get_next_free(ptr);
    if (prev != NULL) { // not free_start
        *get_next_free(prev) = next;
    }
    else { // ptr = free_start
        free_start = next;
    }
    if (next != NULL) { // not end
        *get_prev_free(next) = prev;
    }
}

/* Helper: Takes in a ptr to a just-freed block and uses last-in first-out principle
 * to add the newly freed block to the start of the linked list.
 */
void add_free(void *ptr) {
    *get_prev_free(ptr) = NULL;
    *get_next_free(ptr) = free_start;
    if (free_start != NULL) {
        *get_prev_free(free_start) = ptr;
    }
    free_start = ptr;
}

/* Helper: Takes in a ptr to a header and manipulates its bit pattern to
 * reflect that the block is now used.
 */
void set_used(void *ptr) {
    *(size_t *)ptr |= 1; // sets last bit to 1
}

/* Helper: Takes in a ptr to a header and manipulates its bit pattern to
 * reflect that the block is now free.
 */
void set_free(void *ptr) {
    *(size_t *)ptr &= ~1; // sets last bit to 0
}

/* Returns the first fit free block for the requested_size by traversing
 * the doubly linked list of free blocks on the heap and returning the first
 * free block with enough space. If the requested_size is 0 or greater than
 * the MAX_REQUEST_SIZE, mymalloc returns NULL.
 */
void *mymalloc(size_t requested_size) {
    if (requested_size == 0 || requested_size > MAX_REQUEST_SIZE) {
        return NULL;
    }
    size_t needed = roundup(requested_size, ALIGNMENT);
    void *cur = free_start;
    while (cur != NULL && get_size(cur) < needed) {
        cur = *get_next_free(cur);
    }
    if (cur == NULL) {
        return NULL; // no free blocks left
    }
    
    size_t block_size = get_size(cur);
    if (block_size < needed + 3 * ALIGNMENT) {
        needed = block_size; // gives extra space to allocation request
    }
    *(size_t *)cur = needed;
    set_used(cur);
    nused += needed;
    remove_free(cur);
    
    void *ptr = (char *)cur + ALIGNMENT;

    if (block_size >= needed + 3 * ALIGNMENT) { // creates header
        cur = get_next_header(cur); // for rest of free block
        *(size_t *)cur = block_size - needed - ALIGNMENT;
        add_free(cur);
    }
    
    return ptr;
}

/* Accepts a ptr to some used block of memory on the heap and manipulates
 * its header accordingly to reflect that it is free. Also, coalesces the
 * neighboring block to its right if it is free and removes that block from
 * the doubly linked list. Does not zero out "freed" memory.
 */
void myfree(void *ptr) {
    if (ptr != NULL) {
        void *header = get_header(ptr);
        nused -= get_size(header);
        set_free(header);
        add_free(header);

        // coalesces all free neighbor blocks to the right
        void *neighbor = get_next_header(header);
        while (!is_past_end(neighbor) && !is_used(neighbor)) {
            remove_free(neighbor);
            *(size_t *)header += get_size(neighbor) + ALIGNMENT;
            neighbor = get_next_header(neighbor);
        }
    }
}

/* Accepts an old_ptr to some used block of memory on the heap.
 * If old_ptr is NULL, acts same as mymalloc. If new_size is 0, acts same as
 * myfree. Case 1: If new_size requested is less than or equal to the current
 * old_size, implements in-place realloc successfully, as well as recycling
 * any previously used space large enough into another free block. Case 2: If
 * new_size requested is greater than the current old_size, coalesces all
 * consecutive free neighboring blocks to its right until it is large enough
 * to satisfy the requested new_size (in which case it calls one recursive
 * call of myrealloc that satisfies case 1), or there are no more free blocks
 * to the right to coalesce. In this case 3, it uses mymalloc, memcpy, and
 * myfree as in the implicit allocator.
 */
void *myrealloc(void *old_ptr, size_t new_size) {
    if (old_ptr == NULL) {
        return mymalloc(new_size); // acts same as mymalloc
    }
    if (new_size == 0) {
        myfree(old_ptr); // acts same as myfree
        return NULL;
    }
    void *old_header = get_header(old_ptr);
    size_t old_size = get_size(old_header);
    new_size = roundup(new_size, ALIGNMENT);
    nused -= old_size;
    if (new_size <= old_size) { // Case 1: in-place realloc
        if (old_size >= new_size + 3 * ALIGNMENT) { // space for free block
            *(size_t *)old_header = new_size;
            set_used(old_header);
            void *new_free = get_next_header(old_header);
            *(size_t *)new_free = old_size - new_size - ALIGNMENT;
            add_free(new_free);
        }
        nused += get_size(old_header);
        return old_ptr;
    }
    
    // attempts in-place realloc
    void *neighbor = get_next_header(old_header);
    while (!is_past_end(neighbor) && !is_used(neighbor)) {
        remove_free(neighbor);
        *(size_t *)old_header += get_size(neighbor) + ALIGNMENT;
        if (get_size(old_header) >= new_size) { // Case 2:
            return myrealloc(old_ptr, new_size); // 1 recursive call,
        } // satisfies case 1
        neighbor = get_next_header(neighbor);
    }

    // in-place realloc failed, case 3
    void *new_ptr = mymalloc(new_size);
    if (new_ptr == NULL) {
        return NULL;
    }
    memcpy(new_ptr, old_ptr, old_size);
    nused += get_size(old_header);
    myfree(old_ptr);
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

    cur = free_start;
    if (cur != NULL && *get_prev_free(cur) != NULL) {
        printf("Oops! Start of linked list is not the start?!\n");
        breakpoint();
        return false;
    }
    while (cur != NULL) { // check every node in linked list is free
        if (is_used(cur)) {
            printf("Oops! Free node in linked list is used?!\n");
            breakpoint();
            return false;
        }
        cur = *get_next_free(cur);
    }

    // check every free block is in linked list of free nodes
    cur = segment_start;
    while (!is_past_end(cur)) {
        if (!is_used(cur)) {
            void *temp = free_start;
            while (temp != NULL) { // check every node in linked list
                if (temp == cur) {
                    break; // found free block in free list
                }
                temp = *get_next_free(temp);
            }
            if (temp == NULL) {
                printf("Oops! Free block not in free linked list?!\n");
                breakpoint();
                return false;
            }
        }
        cur = get_next_header(cur);
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
