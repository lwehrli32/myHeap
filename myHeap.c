///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2019-2020 Jim Skrentny
// Posting or sharing this file is prohibited, including any changes/additions.
// Used by permission Fall 2020, CS354-deppeler
//
///////////////////////////////////////////////////////////////////////////////
 
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include "myHeap.h"
 
/*
 * This structure serves as the header for each allocated and free block.
 * It also serves as the footer for each free block but only containing size.
 */
typedef struct blockHeader {           
    int size_status;
    /*
    * Size of the block is always a multiple of 8.
    * Size is stored in all block headers and free block footers.
    *
    * Status is stored only in headers using the two least significant bits.
    *   Bit0 => least significant bit, last bit
    *   Bit0 == 0 => free block
    *   Bit0 == 1 => allocated block
    *
    *   Bit1 => second last bit 
    *   Bit1 == 0 => previous block is free
    *   Bit1 == 1 => previous block is allocated
    * 
    * End Mark: 
    *  The end of the available memory is indicated using a size_status of 1.
    * 
    * Examples:
    * 
    * 1. Allocated block of size 24 bytes:
    *    Header:
    *      If the previous block is allocated, size_status should be 27
    *      If the previous block is free, size_status should be 25
    * 
    * 2. Free block of size 24 bytes:
    *    Header:
    *      If the previous block is allocated, size_status should be 26
    *      If the previous block is free, size_status should be 24
    *    Footer:
    *      size_status should be 24
    */
} blockHeader;         

/* Global variable - DO NOT CHANGE. It should always point to the first block,
 * i.e., the block at the lowest address.
 */
blockHeader *heapStart = NULL;     

/* Size of heap allocation padded to round to nearest page size.
 */
int allocsize;

/*
 * Additional global variables may be added as needed below
 */
blockHeader *previousStart = NULL;
 
/* 
 * Function for allocating 'size' bytes of heap memory.
 * Argument size: requested size for the payload
 * Returns address of allocated block on success.
 * Returns NULL on failure.
 * This function should:
 * - Check size - Return NULL if not positive or if larger than heap space.
 * - Determine block size rounding up to a multiple of 8 and possibly adding padding as a result.
 * - Use NEXT-FIT PLACEMENT POLICY to chose a free block
 * - Use SPLITTING to divide the chosen free block into two if it is too large.
 * - Update header(s) and footer as needed.
 * Tips: Be careful with pointer arithmetic and scale factors.
 */
void* myAlloc(int size) {     
	blockHeader *current = NULL;
    blockHeader *previous = current;
    int t_size;
    int foundFreeBlock = 0;
	int prevAlloc = 0;
	int sizeWOStatus;
	int firstRun = 1;
		
	// checks if the heap size can fit the size trying to be allocated or if it is less than 0
    if (size < 0 || size > allocsize){
		return NULL;
    }
	
	// add padding and header
    size = size + sizeof(blockHeader);
	
	// increases the size to a multiple of 8
    int padding = (8 - (size % 8));
	
	size = size + padding;

	// checks if the start is at the start of the heap of somewhere else
	// or if the heap is empty
	
	int checkHeapSize = heapStart->size_status - (heapStart->size_status % 8);
	
	if (previousStart == NULL || (checkHeapSize == allocsize)){
		current = heapStart;
	}else{
		current = previousStart;
	}
    
    // checking for a free block
    // find large enough free block
    // while the current block is allocated, skip to next block
    while(foundFreeBlock == 0){

		// gets the status of the current block
		t_size = current->size_status;

		if (t_size % 4 == 0){

			// p and a bit are 0
			sizeWOStatus = t_size;	
			prevAlloc = 0;
			if(sizeWOStatus >= size){
				foundFreeBlock = 1;
			}
		}else{
			if(t_size % 2 == 0){

				// p bit = 1 and a bit is 0
				sizeWOStatus = t_size - 2;				
				prevAlloc = 1;
				if(sizeWOStatus >= size){
					foundFreeBlock = 1;
				}
			}else{
				if((t_size - 1) % 4 == 0){

					// p bit is 0 and a bit is 1
					sizeWOStatus = t_size - 1;
					prevAlloc = 0;
				}else{

					// p and a bit are 1
					sizeWOStatus = t_size - 3;
					prevAlloc = 1;
				}
			}
		}
		
		// gets the previous block and checks if the next block is the end of the heap
		previous = current;
		
		// if it is the end of the heap, start at the beginning
		if (t_size == 1){
			current = heapStart;
		}else{	
			// checks if the current block is where we started
			if (current == previousStart && firstRun == 0){
				return NULL;
			}
			firstRun = 0;
			current = (blockHeader*)((char*)current + sizeWOStatus);	
		}
    }

    // check for free block
    // recompute t_size
	t_size = previous->size_status;
	if((sizeWOStatus & 1) == 0){
				
		// check for if the size is greater than the free block
		if (size > sizeWOStatus){
			return NULL;
		}

		// is not used
		if(prevAlloc == 0){
			// previous is empty
			previous->size_status = size + 1;
	    }else{
			// previous is allocate
			// have to set p bit and a bit
			previous->size_status = size + 2 + 1;
	    }	
		
		// check if there was splitting
		if(size < sizeWOStatus){
			blockHeader *splitHeader = (blockHeader*)((void*)previous + size);
			splitHeader->size_status = sizeWOStatus - size + 2;
			
			
			// now have to set the footer
			blockHeader *footer = (blockHeader*) ((void*)current - sizeof(blockHeader));
			footer->size_status = sizeWOStatus - size;	
		}
		
		previousStart = (void*)previous;

		return (void*)previous + sizeof(blockHeader); 
	}
    return NULL;
} 
 
/* 
 * Function for freeing up a previously allocated block.
 * Argument ptr: address of the block to be freed up.
 * Returns 0 on success.
 * Returns -1 on failure.
 * This function should:
 * - Return -1 if ptr is NULL.
 * - Return -1 if ptr is not a multiple of 8.
 * - Return -1 if ptr is outside of the heap space.
 * - Return -1 if ptr block is already freed.
 * - USE IMMEDIATE COALESCING if one or both of the adjacent neighbors are free.
 * - Update header(s) and footer as needed.
 */                    
int myFree(void *ptr) {    
    
	int foundBlock = 0;
	blockHeader *current = heapStart;
	blockHeader *previous = NULL;
	int sizeWOStatus;
	int t_size;
	int prevAlloc = 0;
	int isFree = 0;

	// checks if the ptr has the correct information
	if (ptr == NULL){
		return -1;
	}else if ((int) ptr % 8 != 0){
		return -1;
	}

	// goes through the heap til the block is found
	while(foundBlock == 0){
		
		// gets the status of the current block
		t_size = current->size_status;
		
		// check if the current block is the one we are looking for
		if (current == (void*)ptr - sizeof(blockHeader)){
			foundBlock = 1;
		}else if(t_size == 1){
			// we reached the end of the heap and didn't find the block;
			return -1;
		}		

		if (t_size % 4 == 0){

			// p and a bit are 0
			sizeWOStatus = t_size;
			prevAlloc = 0;
			isFree = 0;
		}else{
			if(t_size % 2 == 0){

				// p bit = 1 and a bit is 0
				sizeWOStatus = t_size - 2;
				prevAlloc = 1;
				isFree = 0;
			}else{
				if((t_size - 1) % 4 == 0){
						
					// p bit is 0 and a bit is 1
					sizeWOStatus = t_size - 1;
					prevAlloc = 0;
					isFree = 1;
				}else{
					
					// p and a bit are 1
					sizeWOStatus = t_size - 3;
					prevAlloc = 1;
					isFree = 1;
				}
			}
		}
		
		// update previous and go to next block
		previous = current;
		current = (blockHeader* )((char*)current + sizeWOStatus);
	}
	
	// now that we have found the block we want
	// check if the block is allocated or not
	if (isFree == 0){
		return -1;
	}
	
	// change header and footer for block
	blockHeader *newHeader = (blockHeader*)((void*)previous);
	
	// checks if the next block is free or not
	// we want where the a bit is 0 to coalesce
	// we also have to change the status of the p bit
	int nextBlockStat = current->size_status;
	int currSizeWOStat = (nextBlockStat - (nextBlockStat % 8));
	if (nextBlockStat % 8 == 0){
		
		// p bit and a bit are 0
		// coalesce here for next block				
		if (prevAlloc == 1){
		
			// no coalescing needed for previous block but it is needed for next block
			newHeader->size_status = sizeWOStatus + currSizeWOStat + 2;

			// now set new footer
			blockHeader *newFooter = (blockHeader*)((void*)previous + sizeWOStatus + currSizeWOStat - sizeof(blockHeader));
			newFooter->size_status = sizeWOStatus + currSizeWOStat;
		}else{
	
			// coalescing needed for previous block and next block
			// here the next block is taken care of
			int tempStatus = sizeWOStatus + currSizeWOStat;
			
			// now have to coalesce for the previous block
			blockHeader *previousFooter = (blockHeader*)((void*) previous - sizeof(blockHeader));
			int previousSize = previousFooter->size_status;			
			
			blockHeader *previousFreeBlock = (blockHeader*)((void*)newHeader - previousSize);
			previousFreeBlock->size_status = tempStatus + previousSize + 2;

			// now set footer for new free block
			blockHeader *newFooter = (blockHeader*)((void*)previousFreeBlock + tempStatus + previousSize - sizeof(blockHeader));
			newFooter->size_status = tempStatus + previousSize;
		}
	// the nextBlockSat should never equal one
	}else if (nextBlockStat % 8 == 2){
		
		// p bit is 1 and a bit is 0
		// change p bit to 0 and coalesce

		current->size_status = current->size_status - 2;
		
		if (prevAlloc == 1){
		
			// no coalescing needed for previous block but it is needed for next block
			newHeader->size_status = sizeWOStatus + currSizeWOStat + 2;

			// now set new footer
			blockHeader *newFooter = (blockHeader*)((void*)previous + sizeWOStatus + currSizeWOStat - sizeof(blockHeader));
			newFooter->size_status = sizeWOStatus + currSizeWOStat;
		}else{
	
			// coalescing needed for previous block and next block
			// here the next block is taken care of
			int tempStatus = sizeWOStatus + currSizeWOStat;
			
			// now have to coalesce for the previous block
			blockHeader *previousFooter = (blockHeader*)((void*) previous - sizeof(blockHeader));
			int previousSize = previousFooter->size_status;			
			
			blockHeader *previousFreeBlock = (blockHeader*)((void*)newHeader - previousSize);
			previousFreeBlock->size_status = tempStatus + previousSize + 2;

			// now set footer for new free block
			blockHeader *newFooter = (blockHeader*)((void*)previousFreeBlock + tempStatus + previousSize - sizeof(blockHeader));
			newFooter->size_status = tempStatus + previousSize;
		}
	}else if (nextBlockStat % 8 == 3){
		
		// p and a bit are 1
		// change p bit to 0
		// no coalescing for next block. Still need to check for previous
		current->size_status = current->size_status - 2;

		if (prevAlloc == 0){
			 
			// now have to coalesce for the previous block
			blockHeader *previousFooter = (blockHeader*)((void*) newHeader - sizeof(blockHeader));
			int previousSize = previousFooter->size_status;			
			
			blockHeader *previousFreeBlock = (blockHeader*)((void*)newHeader - previousSize);
			previousFreeBlock->size_status = sizeWOStatus + previousSize + 2;

			// now set footer for new free block
			blockHeader *newFooter = (blockHeader*)((void*)previousFreeBlock + sizeWOStatus + previousSize - sizeof(blockHeader));
			newFooter->size_status = sizeWOStatus + previousSize;
		}else{
			// no need for coalescing
			newHeader->size_status = sizeWOStatus + 2;
			
			// now set footer for new free block
			blockHeader *newFooter = (blockHeader*)((void*)newHeader +sizeWOStatus - sizeof(blockHeader));
			newFooter->size_status = sizeWOStatus;
		}
	}

	return 0;
} 
 
/*
 * Function used to initialize the memory allocator.
 * Intended to be called ONLY once by a program.
 * Argument sizeOfRegion: the size of the heap space to be allocated.
 * Returns 0 on success.
 * Returns -1 on failure.
 */                    
int myInit(int sizeOfRegion) {    
 
    static int allocated_once = 0; //prevent multiple myInit calls
 
    int pagesize;  // page size
    int padsize;   // size of padding when heap size not a multiple of page size
    void* mmap_ptr; // pointer to memory mapped area
    int fd;

    blockHeader* endMark;
  
    if (0 != allocated_once) {
        fprintf(stderr, 
        "Error:mem.c: InitHeap has allocated space during a previous call\n");
        return -1;
    }
    if (sizeOfRegion <= 0) {
        fprintf(stderr, "Error:mem.c: Requested block size is not positive\n");
        return -1;
    }

    // Get the pagesize
    pagesize = getpagesize();

    // Calculate padsize as the padding required to round up sizeOfRegion 
    // to a multiple of pagesize
    padsize = sizeOfRegion % pagesize;
    padsize = (pagesize - padsize) % pagesize;

    allocsize = sizeOfRegion + padsize;

    // Using mmap to allocate memory
    fd = open("/dev/zero", O_RDWR);
    if (-1 == fd) {
        fprintf(stderr, "Error:mem.c: Cannot open /dev/zero\n");
        return -1;
    }
    mmap_ptr = mmap(NULL, allocsize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (MAP_FAILED == mmap_ptr) {
        fprintf(stderr, "Error:mem.c: mmap cannot allocate space\n");
        allocated_once = 0;
        return -1;
    }
  
    allocated_once = 1;

    // for double word alignment and end mark
    allocsize -= 8;

    // Initially there is only one big free block in the heap.
    // Skip first 4 bytes for double word alignment requirement.
    heapStart = (blockHeader*) mmap_ptr + 1;

    // Set the end mark
    endMark = (blockHeader*)((void*)heapStart + allocsize);
    endMark->size_status = 1;

    // Set size in header
    heapStart->size_status = allocsize;

    // Set p-bit as allocated in header
    // note a-bit left at 0 for free
    heapStart->size_status += 2;

    // Set the footer
    blockHeader *footer = (blockHeader*) ((void*)heapStart + allocsize - 4);
    footer->size_status = allocsize;
  
    return 0;
} 
                  
/* 
 * Function to be used for DEBUGGING to help you visualize your heap structure.
 * Prints out a list of all the blocks including this information:
 * No.      : serial number of the block 
 * Status   : free/used (allocated)
 * Prev     : status of previous block free/used (allocated)
 * t_Begin  : address of the first byte in the block (where the header starts) 
 * t_End    : address of the last byte in the block 
 * t_Size   : size of the block as stored in the block header
 */                     
void dispMem() {     
 
    int counter;
    char status[5];
    char p_status[5];
    char *t_begin = NULL;
    char *t_end   = NULL;
    int t_size;

    blockHeader *current = heapStart;
    counter = 1;

    int used_size = 0;
    int free_size = 0;
    int is_used   = -1;

    fprintf(stdout, "************************************Block list***\
                    ********************************\n");
    fprintf(stdout, "No.\tStatus\tPrev\tt_Begin\t\tt_End\t\tt_Size\n");
    fprintf(stdout, "-------------------------------------------------\
                    --------------------------------\n");
  
    while (current->size_status != 1) {
        t_begin = (char*)current;
        t_size = current->size_status;
    
        if (t_size & 1) {
            // LSB = 1 => used block
            strcpy(status, "used");
            is_used = 1;
            t_size = t_size - 1;
        } else {
            strcpy(status, "Free");
            is_used = 0;
        }

        if (t_size & 2) {
            strcpy(p_status, "used");
            t_size = t_size - 2;
        } else {
            strcpy(p_status, "Free");
        }

        if (is_used) 
            used_size += t_size;
        else 
            free_size += t_size;

        t_end = t_begin + t_size - 1;
    
        fprintf(stdout, "%d\t%s\t%s\t0x%08lx\t0x%08lx\t%d\n", counter, status, 
        p_status, (unsigned long int)t_begin, (unsigned long int)t_end, t_size);
    
        current = (blockHeader*)((char*)current + t_size);
        counter = counter + 1;
    }

    fprintf(stdout, "---------------------------------------------------\
                    ------------------------------\n");
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fprintf(stdout, "Total used size = %d\n", used_size);
    fprintf(stdout, "Total free size = %d\n", free_size);
    fprintf(stdout, "Total size = %d\n", used_size + free_size);
    fprintf(stdout, "***************************************************\
                    ******************************\n");
    fflush(stdout);

    return;  
} 


// end of myHeap.c (fall 2020)

