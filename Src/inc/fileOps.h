#ifndef FILEOPS_H
#define FILEOPS_H

#include "tbook.h"

enum FILEOP_REQUEST {
    kDecodeRequest = 1 << 0,
    kSeekRequest = 1 << 1,
    kStopRequest = 1 << 2,
    kFillRequest = 1 << 3,
    kAnyMp3Request = kDecodeRequest | kSeekRequest | kStopRequest | kFillRequest,
    kEncryptPrepareRequest = 1 << 4,
    kEncryptBlockRequest = 1 << 5,
    kEncryptFinishRequest = 1 << 6,
    kAnyEncryptRequest = kEncryptPrepareRequest | kEncryptBlockRequest | kEncryptFinishRequest,
    kAnyRequest = kAnyMp3Request | kAnyEncryptRequest,
};

 enum FILEOP_RESULT {
    kFilledResult = 1,
    kHeaderResult = 2,
    kEncryptResult = 4,
    kAnyResult = kFilledResult | kHeaderResult | kEncryptResult
};

extern osEventFlagsId_t fileOpRequestEventId;
extern osEventFlagsId_t fileOpResultEventId;
extern osMutexId_t fileOpMutex;

/**
 * Called to initialze the FileOps thread.
 */
extern void fileOpInit(void);

#endif
