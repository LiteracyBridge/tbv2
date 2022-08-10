//
// Created by bill on 8/15/2022.
//

#include "tbook.h"
#include "buffers.h"

#define DEBUG_BUFFERS

static osMutexId_t allocationMutexId;
static Buffer_t *freeList[kBufferPoolSize];
#if defined(DEBUG_BUFFERS)
// To verify that buffers being freed are, in fact, from our pool.
static Buffer_t *bufferList[kBufferPoolSize];
static const char * ownerList[kBufferPoolSize];
#endif

void initBufferPool(void) {
    for (int ix = 0; ix < kBufferPoolSize; ++ix) {
        freeList[ix] = (Buffer_t *) tbAlloc(kBufferSize, "audio buffer");
    }
    osMutexAttr_t mutexAttr = {.attr_bits = osMutexRobust | osMutexPrioInherit};
    allocationMutexId = osMutexNew(&mutexAttr);
#if defined(DEBUG_BUFFERS)
    
    memcpy(bufferList, freeList, sizeof(bufferList));
#endif
}

/**
 * Allocate a buffer from the pool.
 *
 * This, and the companion releaseBuffer() only work because there is only one
 * allocator process and one release process at a time. If there ever could be
 * multiple allocators or releasers, the code would need to use compare-and-swap
 * operations.
 *
 * @return a pointer to the buffer, or NULL if no buffers are available.
 */
Buffer_t *allocateBufferNoLock(const char * owner) {
    Buffer_t *result = NULL;
    for (int ix = 0; ix < kBufferPoolSize; ++ix) {
        if (freeList[ix]) {
            // Want to use: __atomic_compare_exchange(&freeList[ix], &found, &null, false, 0, 0);
            result = freeList[ix];
            freeList[ix] = NULL;
#if defined(DEBUG_BUFFERS)
            ownerList[ix] = owner;
#endif
            break;
        }
    }
    return result;
}

Buffer_t *allocateBuffer(const char * owner) {
    Buffer_t *result = NULL;
    osStatus_t status = osMutexAcquire(allocationMutexId, osWaitForever);
    if (status != osOK) {
        tbErr("Couldn't acquire buffer allocation mutex\n");
    }

    result = allocateBufferNoLock(owner);

    status = osMutexRelease(allocationMutexId);
    if (status != osOK) {
        tbErr("Couldn't release buffer allocation mutex\n");
    }
    return result;
}

extern void transferBuffer( Buffer_t * buffer, const char * newOwner) {
#if defined(DEBUG_BUFFERS)
    for (int ix=0; ix<kBufferPoolSize; ++ix) {
        if (bufferList[ix] == buffer) {
            ownerList[ix] = newOwner;
            return;
        }
    }
#endif
}


void releaseBuffer(Buffer_t *buffer) {
    if (buffer == NULL) return;
#if defined(DEBUG_BUFFERS)
    bool owned = false;
    bool free = false;
    int bufferIx = -1;
    for (int ix=0; ix<kBufferPoolSize; ++ix) {
        owned |= bufferList[ix] == buffer;
        free |= freeList[ix] == buffer;
        if (buffer == bufferList[ix]) bufferIx = ix;
    }
    if (!owned) {
        tbErr( "Attempt to free non-pool buffer" );
    } else if (free) {
        tbErr( "Attempt to double-free pool buffer" );
    }
#endif
//    for (int ix = 0; ix < kBufferPoolSize; ++ix) {
        if (freeList[bufferIx] == NULL) {
#if defined(DEBUG_BUFFERS)
            ownerList[bufferIx] = NULL;
#endif
            freeList[bufferIx] = buffer;
//            break;
        }
//    }
    dbgEvt( TB_relBuff, (int) buffer, numBuffersAvailable(), 0, 0 );
}

extern int numBuffersAvailable(void) {
    int result = 0;
    for (int ix = 0; ix < kBufferPoolSize; ++ix) {
        if (freeList[ix]) ++result;
    }
    return result;
}

extern int numBuffersInUse(void) {
    return kBufferPoolSize - numBuffersAvailable();
}
