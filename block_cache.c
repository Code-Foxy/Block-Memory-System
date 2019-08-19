////////////////////////////////////////////////////////////////////////////////
//
//  File           : block_cache.c
//  Description    : This is the implementation of the cache for the BLOCK
//                   driver.
//
//  Author         : Michael Fox
//  Last Modified  : 8/5/2019
//

// Includes
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

// Project includes
#include <block_cache.h>
#include <cmpsc311_log.h>

uint32_t block_cache_max_items = DEFAULT_BLOCK_FRAME_CACHE_SIZE; // Maximum number of items in cache

typedef char Frame[BLOCK_FRAME_SIZE];

struct cacheEntry{
	BlockIndex block;
	BlockFrameIndex frm;
	uint16_t access;
	Frame cacheFrame;
};

typedef struct cacheEntry blockCache;
blockCache* cache;


uint16_t putTracker = 0;
uint16_t lastAccess = 0;

int cacheOn = 0;

//
// Functions

////////////////////////////////////////////////////////////////////////////////
//
// Function     : set_block_cache_size
// Description  : Set the size of the cache (must be called before init)
//
// Inputs       : max_frames - the maximum number of items your cache can hold
// Outputs      : 0 if successful, -1 if failure

int set_block_cache_size(uint32_t max_frames){
	//change the default value of the cache size to that passed by user
	block_cache_max_items = max_frames;
       	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : init_block_cache
// Description  : Initialize the cache and note maximum frames
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int init_block_cache(void){

	//if cache is already initialized, return -1
	if(cacheOn){
		return -1;
	}
	
	//allocate space for the cache given the size
	cache = malloc(sizeof(blockCache) * block_cache_max_items);

	//zero out the block cache
	for (int i = 0; i < block_cache_max_items; i++){
		memset(&cache[i], 0, sizeof(blockCache));
		cache[i].frm = -1;
	}

	//set cache to on
	cacheOn = 1;
       	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : close_block_cache
// Description  : Clear all of the contents of the cache, cleanup
//
// Inputs       : none
// Outputs      : o if successful, -1 if failure

int close_block_cache(void){

	//if cache is not on return -1
	if(!cacheOn){
		return -1;
	}

	//clear all cache data
	for (int i = 0; i < block_cache_max_items; i++){
		memset(&cache[i], 0, sizeof(blockCache));
	}

	//free the pointer
	free(cache);

	//set cache to off
	cacheOn = 0;
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : put_block_cache
// Description  : Put an object into the frame cache
//
// Inputs       : block - the block number of the frame to cache
//                frm - the frame number of the frame to cache
//                buf - the buffer to insert into the cache
// Outputs      : 0 if successful, -1 if failure

int put_block_cache(BlockIndex block, BlockFrameIndex frm, void* buf){

	//if cache is not on return -1
	if(!cacheOn){
		return -1;
	}

	uint16_t replaceTracker;
	uint16_t index=0;

	//if the frame already exists update the access and return
	
	for (int i = 0; i < block_cache_max_items; i++){
		if (cache[i].frm == frm){
			lastAccess++;
			memcpy(cache[i].cacheFrame, buf, BLOCK_FRAME_SIZE);
			cache[i].access = lastAccess;
			return (0);
		}
	}

	//if the cache is not full, fill in first available spot
	if (putTracker < block_cache_max_items){
		lastAccess++;
		cache[putTracker].block = block;
		cache[putTracker].frm = frm;
		cache[putTracker].access = lastAccess;
		memcpy(cache[putTracker].cacheFrame, buf, BLOCK_FRAME_SIZE);
		putTracker++;
	}

	//else find the least recently used frame and overwrite
	else{
		replaceTracker = cache[0].access;
		for (int i = 0; i < block_cache_max_items; i++){
			if (cache[i].access < replaceTracker){
				replaceTracker = cache[i].access;
				index = i;
			}
		}
		lastAccess++;
		cache[index].block = block;
		cache[index].frm = frm;
		cache[index].access = lastAccess;
		memcpy(cache[index].cacheFrame, buf, BLOCK_FRAME_SIZE);
	}	
	return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : get_block_cache
// Description  : Get an frame from the cache (and return it)
//
// Inputs       : block - the block number of the block to find
//                frm - the  number of the frame to find
// Outputs      : pointer to cached frame or NULL if not found

void* get_block_cache(BlockIndex block, BlockFrameIndex frm){
	
	//search for the frame in hte block cache and return it if present
	for (int i = 0; i < block_cache_max_items; i++){
		if(cache[i].frm == frm){
			lastAccess++;
			cache[i].access = lastAccess;
			return cache[i].cacheFrame;
		}
	}
       	return (NULL);
}


//
// Unit test

////////////////////////////////////////////////////////////////////////////////
//
// Function     : blockCacheUnitTest
// Description  : Run a UNIT test checking the cache implementation
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int blockCacheUnitTest(void)
{
    // Return successfully
    logMessage(LOG_OUTPUT_LEVEL, "Cache unit test completed successfully.");
    return (0);
}
