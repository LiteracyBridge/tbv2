//
// Created by bill on 8/15/2022.
//

#ifndef TBV2_BUFFERS_H
#define TBV2_BUFFERS_H

#include <stdint.h>

typedef uint16_t Buffer_t;

// Size of an audio buffer in bytes, words
static const int kBufferSize = 4096;
static const int kBufferWords = kBufferSize / 2;

// Number of audio buffers to be allocated
static const int kBufferPoolSize = 8;

/**
 * Allocate the pool buffers and initialize the control structures.
 */
extern void initBufferPool( void );

/**
 * Allocate a buffer from the pool.
 * @return a pointer to the buffer, or NULL if no buffers are available.
 */
extern Buffer_t * allocateBufferNoLock( const char * owner );
extern Buffer_t * allocateBuffer( const char * owner );

extern void transferBuffer( Buffer_t * buffer, const char * newOwner);

/**
 * Return a buffer to the pool, making it available for re-use.
 * @param buffer A pointer to the buffer to be released.
 */
extern void releaseBuffer( Buffer_t * buffer );

/**
 * Return the number of available buffers, or the number in use.
 */
extern int numBuffersAvailable( void );
extern int numBuffersInUse( void );
#endif //TBV2_BUFFERS_H
