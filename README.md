# Memory Management System

This repository implements a custom memory allocator (`sf_malloc`) using segregated free lists and quick lists. It supports basic dynamic memory allocation functions and ensures efficient memory utilization.

## Features
- **Dynamic Memory Allocation:** Implements `sf_malloc` for memory allocation.
- **Memory Coalescing:** Combines adjacent free memory blocks to minimize fragmentation.
- **Quick Lists:** Uses quick lists for efficient allocation and deallocation of small, frequent blocks.
- **Segregated Free Lists:** Maintains segregated free lists for different block sizes to optimize allocation.
- **Memory Alignment:** Ensures all allocated memory is aligned to 16 bytes.
- **Utilization Tracking:** Tracks maximum memory utilization during program execution.

## File Structure
- `sfmm.h`: Header file containing macro definitions, constants, and function declarations.
- `sfmm.c`: Core implementation of the memory management functions.
- `debug.h`: Utility functions for debugging.
- `Makefile`: Build system for compiling and linking the project.
- `README.md`: Documentation for the repository.

## Functions
### `sf_malloc(size_t size)`
Allocates a block of memory of at least `size` bytes. Aligns and resizes the requested size to meet the minimum block size and alignment requirements.

### `sf_free(void *ptr)`
Frees a previously allocated block of memory and coalesces adjacent free blocks.

### `sf_realloc(void *ptr, size_t size)`
Reallocates a previously allocated block to a new size.

### `sf_mem_grow()`
Expands the heap memory by one page size.

### `coalesce(sf_block *block)`
Combines adjacent free blocks into a single block to reduce fragmentation.

### `insertIntoFreeList(sf_block *block)`
Adds a free block into the appropriate segregated free list.

### `insertIntoQuickList(sf_block *block, int index)`
Adds a block into the quick list for reuse.

## Key Concepts
- **Block Header/Footer:** Each block has a header and footer for metadata storage.
- **MAGIC Number:** Used to validate memory integrity during operations.
- **Alignment:** Ensures all allocated blocks are aligned to 16 bytes.
- **Segregated Lists:** Organizes free blocks by size for fast allocation.
