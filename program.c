/* This program is compiled to use the custom heap allocator
 * when allocating memory. Write any program that allocates memory
 * using mymalloc, myrealloc, and myfree instead of malloc, realloc,
 * and free.
 */
#include <stdio.h>
#include <stdlib.h>
#include "allocator.h"
#include "segment.h"

#define HEAP_SIZE 1L << 32

bool initialize_heap_allocator() {
    init_heap_segment(HEAP_SIZE);
    return myinit(heap_segment_start(), heap_segment_size());
}

int main(int argc, char *argv[]) {
    if (!initialize_heap_allocator()) {
        return 1;
    }

    // write code here
    
    return 0;
}
