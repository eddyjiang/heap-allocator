# Heap Allocator
Implementation of implicit and explicit free list heap allocators from ground up, integrating support for essential memory management functions, including malloc, realloc, and free in C.

## Implicit Free List Allocator
Features:
- 8-byte headers track block information regarding size and status (used/free)
- Free blocks are recycled and reused for subsequent malloc requests if possible
- Malloc searches the heap for free blocks using an implicit free list (traverses block by block)
- First-fit search sacrifices utilization (memory usage) for throughput (speed)

## Explicit Free List Allocator
Features:
- 8-byte headers track block information regarding size and status (used/free)
- All free blocks are managed as a doubly-linked list
  - The first 16 bytes of each free block's payload is used to store 2 pointers to the previous and next free block
- Malloc searches explicit linked list of free blocks
- Freed blocks are coalesced with all consecutive right neighbor free blocks
- Realloc resizes block in place if possible and absorbs all consecutive right neighbor free blocks
- First-fit search sacrifices utilization (memory usage) for throughput (speed)
