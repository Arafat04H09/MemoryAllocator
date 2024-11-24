/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "sfmm.h"
#include <errno.h>

#define MIN_BLOCK_SIZE 32
#define ALIGN_SIZE 16
#define GET_BLOCK_SIZE 0xfffffffffffffff8

double maxUtilization = 0.0;

void checkAndUpdateMaxUtilization() {
    sf_block* currentBlock = (sf_block*)((void*) sf_mem_start() + 32);
    double totalPayload;
    while ((void*)currentBlock < sf_mem_end() - 31) { 
        sf_header currentHeader = currentBlock->header ^ MAGIC; 
        size_t blockSize = currentHeader & GET_BLOCK_SIZE;
        
        if (currentHeader & THIS_BLOCK_ALLOCATED) {
            totalPayload += (blockSize - sizeof(sf_header) - sizeof(sf_footer));
        }

        currentBlock = (sf_block*)((void*)currentBlock + blockSize);
    }
    double heapSize = (sf_mem_end() - sf_mem_start());

    double currentUtilization = (totalPayload / heapSize) * 100;
    if ( currentUtilization > maxUtilization) {
        maxUtilization = currentUtilization;
        
    }
}

size_t resize(size_t size) {
    size_t newSize = size + 8;

    if (newSize % 16 != 0) {
        newSize += 16 - (newSize % 16);
    }

    if (newSize < 32) {
        newSize = 32;
    }

    return newSize;
}

int getIndex(size_t size) {
    if (size == MIN_BLOCK_SIZE) {
        return 0;
    }
    else if (size >= MIN_BLOCK_SIZE && size < 2 * MIN_BLOCK_SIZE) {
        return 1;
    } else if (size >= MIN_BLOCK_SIZE * 2 && size < MIN_BLOCK_SIZE * 4) {
        return 2;
    } else if (size >= MIN_BLOCK_SIZE * 4 && size < MIN_BLOCK_SIZE * 8) {
        return 3;
    } else if (size >= MIN_BLOCK_SIZE * 8 && size < MIN_BLOCK_SIZE * 16) {
        return 4;
    } else if (size >= MIN_BLOCK_SIZE * 16 && size < MIN_BLOCK_SIZE * 32) {
        return 5;
    } else if (size >= MIN_BLOCK_SIZE * 32 && size < MIN_BLOCK_SIZE * 64) {
        return 6;
    } else if (size >= MIN_BLOCK_SIZE * 64 && size < MIN_BLOCK_SIZE * 128) {
        return 7;
    } else if (size >= MIN_BLOCK_SIZE * 128 && size < MIN_BLOCK_SIZE * 256) {
        return 8;
    }
    else {
        return 9;
    }
}


void removeBlockFromFreeList(sf_block *block) {
    if (block->body.links.next == NULL || block->body.links.prev == NULL) {
        return;
    }

    block->body.links.prev->body.links.next = block->body.links.next;

    block->body.links.next->body.links.prev = block->body.links.prev;

    block->body.links.next = NULL;
    block->body.links.prev = NULL;
}

sf_block* coalesce(sf_block* block) {
    sf_header header = (block->header) ^ MAGIC;
    size_t blockSize = header & GET_BLOCK_SIZE;

    if (!(header & PREV_BLOCK_ALLOCATED)) {
        sf_footer prevFooter = block->prev_footer;
        size_t prevBlockSize = (prevFooter ^ MAGIC) & GET_BLOCK_SIZE;
        sf_block* prevBlock = (sf_block*)((void*)block - prevBlockSize);
        if ((prevFooter ^ MAGIC) & PREV_BLOCK_ALLOCATED) {
            header = (header | PREV_BLOCK_ALLOCATED);
        }
        removeBlockFromFreeList(prevBlock);
        blockSize += prevBlockSize;
        block = prevBlock; 
    }

    sf_block* nextBlock = (sf_block*)((void*)block + blockSize);
    if (!((nextBlock->header ^ MAGIC) & THIS_BLOCK_ALLOCATED) && ((void*)nextBlock < sf_mem_end() - sizeof(sf_header))) {
        size_t nextBlockSize = (nextBlock->header ^ MAGIC) & GET_BLOCK_SIZE;
        removeBlockFromFreeList(nextBlock);
        blockSize += nextBlockSize; 
    }

    block->header = (blockSize |  (header & PREV_BLOCK_ALLOCATED)) ^ MAGIC;
    sf_footer* newFooter = (sf_footer*)((void*)block + blockSize);
    *newFooter = block->header;

    sf_block* postCoalesceBlock = (sf_block*)((void*)block + blockSize);
    if ((void*)postCoalesceBlock < sf_mem_end() - sizeof(sf_header)) {
        postCoalesceBlock->prev_footer = block->header;
        postCoalesceBlock -> header = (((postCoalesceBlock -> header)^ MAGIC) & ~PREV_BLOCK_ALLOCATED)^MAGIC;
    }
    
    return block;
}

void insertIntoFreeList(sf_block* block) {
    block = coalesce(block);
    size_t size = (block->header ^ MAGIC) & GET_BLOCK_SIZE;
    int index = getIndex(size);

    sf_block* freeListHead = &sf_free_list_heads[index];

    if (freeListHead-> body.links.next == freeListHead) { 
        block->body.links.next = freeListHead;
        block->body.links.prev = freeListHead; 
        freeListHead->body.links.next = block; 
        freeListHead->body.links.prev = block;
    }
    else { 
        block->body.links.next = freeListHead->body.links.next; 
        block->body.links.prev = freeListHead; 
        freeListHead->body.links.next->body.links.prev = block; 
        freeListHead->body.links.next = block; 
    }
}

sf_block *allocateFromQuick(size_t size) {
    int index = (size - MIN_BLOCK_SIZE) / ALIGN_SIZE;
    if (index >= 0 && index < NUM_QUICK_LISTS) {
        if (sf_quick_lists[index].length > 0) {
            sf_block *block = sf_quick_lists[index].first;

            sf_quick_lists[index].first = block->body.links.next;
            
            block->body.links.next = NULL;
            if (sf_quick_lists[index].first != NULL) {
                sf_quick_lists[index].first->body.links.prev = NULL;
            }
            
            block -> header ^= MAGIC;
            size_t blockSize = (block-> header) & GET_BLOCK_SIZE;
            block->header &= ~IN_QUICK_LIST;
            block->header |= THIS_BLOCK_ALLOCATED;
            block -> header ^= MAGIC;

            sf_block*  nextBlock = (sf_block*) ((void*)block + blockSize);
            nextBlock-> prev_footer = block -> header;
            sf_quick_lists[index].length--;

            return block; 
        }
    }
    return NULL;
}

void initializeFreeLists() {
    for (int i = 0; i < NUM_FREE_LISTS; i++) {
        sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
        sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
    }
}

sf_block *allocateFromSegregated(size_t size) {
    int index = getIndex(size);
    for (int i = index; i < NUM_FREE_LISTS; i++) {
        sf_block* current = sf_free_list_heads[i].body.links.next; 
        while (current != &sf_free_list_heads[i]) { 
            sf_header currentHeader = (current-> header) ^ MAGIC;
            size_t currentBlockSize = currentHeader & GET_BLOCK_SIZE;

            if (currentBlockSize >= size) { 
                removeBlockFromFreeList(current);
                if (currentBlockSize >= size + MIN_BLOCK_SIZE) { 
                    
                sf_block* newBlock = (sf_block*)((char*)current + size);
                size_t newSize = currentBlockSize - size; 
                current->header = ((size) | (currentHeader & PREV_BLOCK_ALLOCATED) |THIS_BLOCK_ALLOCATED) ^ MAGIC; 

                newBlock->header = (newSize | PREV_BLOCK_ALLOCATED) ^ MAGIC;
                newBlock-> prev_footer = current-> header;

                sf_block* nextBlock = (sf_block*)((char*)newBlock + newSize);
                nextBlock->prev_footer = newBlock->header; 

                sf_footer* newBlockFooter = (sf_footer*)((char*)newBlock + newSize);
                *newBlockFooter = newBlock->header;  

                insertIntoFreeList(newBlock);
                } else {
                    sf_header currentHeader = (current -> header)^ MAGIC;
                    current -> header = (currentBlockSize | THIS_BLOCK_ALLOCATED | (currentHeader & PREV_BLOCK_ALLOCATED) ) ^ MAGIC; 
                    size_t blockSize = ((current -> header) ^ MAGIC) & GET_BLOCK_SIZE;
                    sf_block* nextBlock = ((void*)current + blockSize);
                    sf_header nextHeader = ((nextBlock -> header)^ MAGIC) & ~PREV_BLOCK_ALLOCATED;
                    nextBlock -> header = (nextHeader | PREV_BLOCK_ALLOCATED) ^ MAGIC;
                }
                return current; 
            }
            current = current->body.links.next;
        }
    }

    return NULL;
}


void *sf_malloc(size_t size) {
    if (size <= 0) { return NULL;} 

    if (sf_mem_start() == sf_mem_end()) {
        if (sf_mem_grow() == NULL) {
            sf_errno = ENOMEM;
            return NULL;
        }
        sf_block* prologue = (sf_block*)(sf_mem_start());
        prologue-> header = (MIN_BLOCK_SIZE | THIS_BLOCK_ALLOCATED | PREV_BLOCK_ALLOCATED) ^ MAGIC;        
        
        sf_block* freeBlock = (sf_block*)(sf_mem_start() + 32);
        size_t freeBlockSize = 8144;
        freeBlock->header = (freeBlockSize | PREV_BLOCK_ALLOCATED) ^ MAGIC;
        freeBlock->prev_footer = prologue -> header;

        sf_footer* freeBlockFooter = (void*) freeBlock + 8144; 
        *freeBlockFooter = freeBlock -> header;

        sf_block* epilogue = (sf_block*)(sf_mem_end() - 2*sizeof(sf_header));
        epilogue->header = (THIS_BLOCK_ALLOCATED) ^ MAGIC;

        initializeFreeLists();
        insertIntoFreeList(freeBlock);  
     }

    size_t realSize = resize(size);

    sf_block* quickBlock = allocateFromQuick(realSize);
    if (quickBlock != NULL) {
        checkAndUpdateMaxUtilization();
        return &(quickBlock->body.payload);
    }

    sf_block* freeListBlock = allocateFromSegregated(realSize);
    if (freeListBlock != NULL) {
        checkAndUpdateMaxUtilization();
        return &(freeListBlock -> body.payload);
    }
    while (true) {
        sf_block* lastBlock = (sf_block*) ((void*)sf_mem_end() - 16);
        void *newPage = sf_mem_grow(); 
        if (newPage == NULL) { 
            sf_errno = ENOMEM;
            return NULL;
        }
        sf_header prevBlockHeader = (lastBlock -> prev_footer) ^ MAGIC;
        sf_block* beforeEpilogue = ((void*)lastBlock - (prevBlockHeader & GET_BLOCK_SIZE));

        bool prevBlockAllocated = (prevBlockHeader & THIS_BLOCK_ALLOCATED);
        
        sf_block* newBlock = (sf_block*)(newPage - 2*sizeof(sf_header));
        size_t newFreeBlockSize = PAGE_SZ; 
        
        newBlock -> header = (PAGE_SZ | (prevBlockAllocated ? PREV_BLOCK_ALLOCATED : 0)) ^ MAGIC;
        newBlock-> prev_footer = beforeEpilogue-> header;
        sf_footer* footer = (sf_footer*)((void*)newBlock + newFreeBlockSize);
        *footer = newBlock->header; 
        
        sf_block* newEpilogue = (sf_block*)((void*)newBlock + newFreeBlockSize);
        newEpilogue->header = (THIS_BLOCK_ALLOCATED) ^ MAGIC;
        insertIntoFreeList(newBlock);

        freeListBlock = allocateFromSegregated(realSize);

        if (freeListBlock != NULL) {
            checkAndUpdateMaxUtilization();
            return &(freeListBlock->body.payload);
        } 
    }
    checkAndUpdateMaxUtilization();
    sf_errno = ENOMEM;
    return NULL;
}

int verifyPrevFooter(sf_block* currentBlock) {
    if (currentBlock == NULL) {
        return -1;
    }
    sf_footer prevFooter = (currentBlock -> prev_footer)^MAGIC;

    size_t prevBlockSize = prevFooter & 0xfffffffffffffff8;
    sf_block *prevBlock = (sf_block*)((void*)currentBlock - prevBlockSize);

    sf_header actualPrevFooterValue = (prevBlock-> header) ^ MAGIC;

    bool prevBlockAllocated = (actualPrevFooterValue & THIS_BLOCK_ALLOCATED);
    bool currentPrevAllocBit = (((currentBlock->header)^MAGIC) & PREV_BLOCK_ALLOCATED);
   
    if ((prevBlockAllocated != currentPrevAllocBit)) {
        return -1;
    }

    return 0;
}

void removeBlockFromQuickList(int index) {
    sf_block* blockToRemove = sf_quick_lists[index].first;
    if (blockToRemove != NULL) {
        sf_quick_lists[index].first = blockToRemove->body.links.next;

        if (blockToRemove->body.links.next != NULL) {
            blockToRemove->body.links.next->body.links.prev = NULL;
        }

        blockToRemove->body.links.next = NULL;
        blockToRemove-> header ^= MAGIC;
        blockToRemove -> header = (blockToRemove -> header) & ~THIS_BLOCK_ALLOCATED;
        blockToRemove -> header = (blockToRemove -> header) & ~IN_QUICK_LIST;
        blockToRemove -> header ^= MAGIC;
        insertIntoFreeList(blockToRemove);
        sf_quick_lists[index].length--;
    }
}

void flushQuickList(int index) {
    int length = sf_quick_lists[index].length;

    while (length > 0) {
        removeBlockFromQuickList(index);
        length--;
    }

    sf_quick_lists[index].first = NULL;
    sf_quick_lists[index].length = 0;
}

void insertIntoQuickList(sf_block* block, int index) {
    sf_block* firstBlock = sf_quick_lists[index].first;

    sf_quick_lists[index].first = block;

    block->body.links.next = firstBlock;
    sf_header currentBlockHeader = (block->header) ^ MAGIC;

    size_t size = currentBlockHeader & GET_BLOCK_SIZE; 
    block->header = (currentBlockHeader | IN_QUICK_LIST | THIS_BLOCK_ALLOCATED) ^ MAGIC; 

    sf_block* nextBlock = (sf_block*) ((void*)block + size);
    nextBlock -> header = (((nextBlock -> header) ^ MAGIC) | PREV_BLOCK_ALLOCATED) ^ MAGIC;
    size_t nextBlockSize = ((nextBlock -> header) ^ MAGIC) & GET_BLOCK_SIZE;

    sf_block* afterNextBlock = (sf_block*) ((void*) nextBlock + nextBlockSize);
    afterNextBlock -> prev_footer = nextBlock -> header;

    sf_quick_lists[index].length++;
}

void sf_free(void *pp) {
    if (pp == NULL) {
        abort();
    }

    sf_block* block = pp - 2*sizeof(sf_header);

    sf_header headerValue = (block-> header) ^ MAGIC;

    if (((uintptr_t)pp & 0xF)|| pp < sf_mem_start() || pp >= sf_mem_end() - 31) {
        abort();
    }

    if (!(headerValue & THIS_BLOCK_ALLOCATED)) {
        abort();
    }

    if (headerValue & IN_QUICK_LIST) {
        abort();
    }

    if (verifyPrevFooter(block) == -1) {
        abort();
    }

    block-> header = (headerValue & ~THIS_BLOCK_ALLOCATED) ^ MAGIC;
    size_t blockSize = headerValue & GET_BLOCK_SIZE;   

    sf_block* nextBlock = (sf_block*) ((void*)block + blockSize);
    nextBlock -> header = (((nextBlock-> header) ^ MAGIC) & ~PREV_BLOCK_ALLOCATED) ^ MAGIC;
    size_t nextBlockSize = ((nextBlock-> header) ^ MAGIC) & GET_BLOCK_SIZE;
    sf_footer* nextBlockFooter = (sf_footer*)((void*) nextBlock + nextBlockSize);
    *nextBlockFooter = nextBlock -> header;
    size_t size = headerValue & 0xfffffffffffffff8;
    int quickListIndex = (size - MIN_BLOCK_SIZE)/ALIGN_SIZE;

    if (quickListIndex >= 0 && quickListIndex < NUM_QUICK_LISTS) {
        block -> header = (headerValue | THIS_BLOCK_ALLOCATED| IN_QUICK_LIST)^ MAGIC;
        nextBlock -> prev_footer = block-> header;
        nextBlock-> header = (((nextBlock-> header)^ MAGIC) | PREV_BLOCK_ALLOCATED)^ MAGIC;

        sf_block* afterBlock = (void*)nextBlock + nextBlockSize;
        afterBlock -> prev_footer = nextBlock -> header;
        if (sf_quick_lists[quickListIndex].length >= QUICK_LIST_MAX) {
            flushQuickList(quickListIndex); 
        }
        insertIntoQuickList(block, quickListIndex); 

    } else {
        sf_block* nextBlock = (void*)block + blockSize;
        nextBlock-> prev_footer = block-> header;
        insertIntoFreeList(block); 
    }
    checkAndUpdateMaxUtilization();
}

void *sf_realloc(void *pp, size_t rsize) {
     if (pp == NULL) {
        sf_errno = EINVAL;
        return NULL;
    }

    sf_block* block = pp - 2*sizeof(sf_header);

    sf_header headerValue = (block-> header) ^ MAGIC;

    if (((uintptr_t)pp & 0xF)|| pp < sf_mem_start() || pp >= sf_mem_end() - 31) {
        sf_errno = EINVAL;
        return NULL;
    }

    if (!(headerValue & THIS_BLOCK_ALLOCATED)) {
        sf_errno = EINVAL;
        return NULL;
    }

    if (headerValue & IN_QUICK_LIST) {
        sf_errno = EINVAL;
        return NULL;
    }

    if (verifyPrevFooter(block) == -1) {
        sf_errno = EINVAL;
        return NULL;
    }
    
    if (rsize == 0) {
        sf_free(block);
        return NULL;
    }
    size_t beforeSize = headerValue & GET_BLOCK_SIZE;

    if (beforeSize == rsize) {
        checkAndUpdateMaxUtilization();
        return pp;
    }
    if (beforeSize < rsize) {
        void* newPtr = sf_malloc(rsize);
        if (newPtr == NULL) {
            sf_errno = ENOMEM;
            return NULL;
        }
        memcpy(newPtr, pp, beforeSize); 
        checkAndUpdateMaxUtilization();
        sf_free(pp);
        return newPtr;
    }
    else  {
        size_t newSize = resize(rsize);
        
        if (beforeSize - newSize >= MIN_BLOCK_SIZE) {
            sf_block* newFreeBlock = (sf_block*)((void*)block + newSize);
            size_t newFreeBlockSize = beforeSize - newSize;

            block->header = ((newSize | (headerValue & PREV_BLOCK_ALLOCATED) | THIS_BLOCK_ALLOCATED) ^ MAGIC);
            newFreeBlock->header = ((newFreeBlockSize | PREV_BLOCK_ALLOCATED) ^ MAGIC);
            newFreeBlock-> prev_footer = block-> header;

            sf_block* afterFreeBlock = (void*)newFreeBlock + newFreeBlockSize;
            afterFreeBlock-> header ^= MAGIC;
            afterFreeBlock-> header= (afterFreeBlock -> header) & (~PREV_BLOCK_ALLOCATED);
            afterFreeBlock-> header ^= MAGIC;

            insertIntoFreeList(newFreeBlock);
        }
        checkAndUpdateMaxUtilization();
        return pp;
    }
}

double sf_fragmentation() {
    abort();
}

double sf_utilization() {
    return maxUtilization;
}