//
// Created by bill on 8/15/2022.
//

#include "tbook.h"
#include "buffers.h"

//#define DEBUG_BUFFERS

osMemoryPoolId_t mpid_MemPool;

#if defined(DEBUG_BUFFERS)
// To track which buffer is which.
static Buffer_t *buffers;
static int bufferIx(Buffer_t *buffer) {
    return (buffer-buffers)/kBufferSize;
}
static const char *ownerList[BUFFER_POOL_SIZE];
#endif

void initBufferPool(void) {
    if (mpid_MemPool != NULL) {
        return;
    }
    // For an unknown reason we must provide the memory to the buffer pool. My speculation is because
    // the C runtime already owns too much memory, but that's just speculation.
    // Regardless, the buffer pool is still useful because it is thread safe and ISR safe, and when
    // recording we need to allocate buffers from an ISR, and free them from a thread context.
    osMemoryPoolAttr_t mem_attr = {.name="io buffers",
            .mp_size = kBufferSize * kBufferPoolSize,
            .mp_mem = tbAlloc(kBufferSize * kBufferPoolSize, "io buffers")
    };
    mpid_MemPool = osMemoryPoolNew(kBufferPoolSize, kBufferSize, &mem_attr);
    if (mpid_MemPool == NULL) {
        tbErr("buffer pool alloc failed");
    }
#if defined(DEBUG_BUFFERS)
    buffers = mem_attr.mp_mem;
#endif
}

/**
 * Allocate a buffer from the pool.
 *
 * @return a pointer to the buffer, or NULL if no buffers are available.
 */
Buffer_t *allocateBuffer(const char *owner) {
    #if defined(DEBUG_BUFFERS)
    int inUse = osMemoryPoolGetCount(mpid_MemPool);
    int available = osMemoryPoolGetSpace(mpid_MemPool);
    #endif
    Buffer_t *buffer = (Buffer_t *) osMemoryPoolAlloc(mpid_MemPool, 0U);  // get Mem Block
    if (buffer == NULL) {
         printf("Buffer allocation failed.");
   }
#if defined(DEBUG_BUFFERS)
    else {
        int ix =  bufferIx(buffer);
        ownerList[ix] = owner;
    }
#endif
    return buffer;
}

extern void transferBuffer(Buffer_t *buffer, const char *newOwner) {
#if defined(DEBUG_BUFFERS)
    ownerList[bufferIx(buffer)] = newOwner;
#endif
}

void releaseBuffer(Buffer_t *buffer) {
    if (buffer == NULL) return;
#if defined(DEBUG_BUFFERS)
    ownerList[bufferIx(buffer)] = NULL;
#endif
    osMemoryPoolFree(mpid_MemPool, buffer);
    dbgEvt(TB_relBuff, (int) buffer, numBuffersAvailable(), 0, 0);
}

extern int numBuffersAvailable(void) {
    return osMemoryPoolGetSpace(mpid_MemPool);
}

extern int numBuffersInUse(void) {
    return osMemoryPoolGetCount(mpid_MemPool);
}
