//
// Created by bill on 9/5/2022.
//

#include <stdbool.h>
#include <stdint.h>
//#include <malloc.h>
#include <string.h>
//#include <minmax.h>

#include "tbook.h"

#include "inc/mp3timing.h"

#if defined(DEBUG_MP3_TIMINGS)
static int32_t *offsets;
static int32_t nOffsets;
static int32_t operationCount;
static bool bug = false;
#define INCREMENT_COUNT() (++operationCount)
#else
#define INCREMENT_COUNT()
#endif

#define M3T_SIGNATURE  {'m', '3', 't', M3tVersion}
static const uint8_t M3tVersion = 1;
//static const uint8_t M3tSignature[4] = M3T_SIGNATURE;

const M3tSignature_t m3tSignature = {M3T_SIGNATURE};
const struct M3tHeader m3tProtoHeader = {.signature=M3T_SIGNATURE, 0};
const char * const m3tExtension = ".m3t";

/**
* Opens a ".m3t" file, for translating file offset <--> audio timestamp.
* @param m3tFilename of the .m3t file.
* @param pContext a M3tContext structure to be filled in.
* @return true if the file was opened successfully, and the header matched.
*/
bool openMp3Timings(const char *m3tFilename, struct M3tContext *pContext) {
  FILE *offsetsFile = fopen(m3tFilename, "rb");
    if (offsetsFile == NULL) return false;
    struct M3tHeader m3tHeader;
    int32_t nRead = fread(&m3tHeader, sizeof(struct M3tHeader), 1, offsetsFile);
    if (nRead != 1) {
        fprintf(stderr, "Can't read .m3t file\n");
        fclose(offsetsFile);
        return false;
    }
    if (memcmp(&m3tHeader.signature, &m3tSignature, sizeof(m3tSignature)) != 0) {
        fprintf(stderr, "Bad signature in .m3t file\n");
        fclose(offsetsFile);
        return false;
    }

    // Looks good, proceed.
    fseek(offsetsFile, 0, SEEK_END);
    long fileLength = ftell(offsetsFile);
    long numOffsets = (fileLength - sizeof(struct M3tHeader)) / sizeof(int32_t);
    fseek(offsetsFile, 0, SEEK_SET);
    pContext->offsetsFile = offsetsFile;
    pContext->numOffsets = numOffsets;
    pContext->frameDuration = m3tHeader.frameDuration;
    // Read the first and last offsets, for optimized searching.
    fseek(offsetsFile, sizeof(struct M3tHeader), SEEK_SET);
    fread(pContext->lowestOffsets, sizeof(int32_t), 3, offsetsFile);
    fseek(offsetsFile, -2 * ((int32_t) sizeof(int32_t)), SEEK_END);
    fread(pContext->highestOffsets, sizeof(int32_t), 2, offsetsFile);

#if defined(DEBUG_MP3_TIMINGS)
    // Read the offsets for testing.
    offsets = malloc(numOffsets * sizeof(int32_t));
    fseek(offsetsFile, sizeof(struct M3tHeader), SEEK_SET);
    nOffsets = numOffsets;
    nRead = fread(offsets, sizeof(long), nOffsets, offsetsFile);
#endif

    return true;
}

/**
 * Closes a previously opened ".m3t" file.
 * @param pContext from the opened .m3t file.
 */
void closeMp3Timings(struct M3tContext *pContext) {
    if (pContext->offsetsFile)
        fclose(pContext->offsetsFile);
    memset(pContext, 0, sizeof(struct M3tContext));
}

/**
 * Given an offset, return the timestamp of the frame containing that offset.
 * @param pContext of a previously opened .m3t file.
 * @param offset for which the timestamp is needed.
 * @return timestamp, in milliseconds, of the frame containing the offset.
 */
int32_t timestampForOffset(struct M3tContext *pContext, int32_t offset) {
    if (pContext->numOffsets <= 1 || offset < pContext->lowestOffsets[1]) {
        // No frames (empty file), or only one, or within first: first frame.
        return 0;
    } else if (offset >= pContext->highestOffsets[1]) {
        // In or beyond last frame: last frame
        return pContext->numOffsets * pContext->frameDuration;
    }
    // The offset is at >=low and <=high, and there are multiple offsets.
    int32_t offsetsLow[2], offsetsHigh[2], offsetsGuess[4];
    memcpy(offsetsLow, pContext->lowestOffsets + 1, sizeof(offsetsLow));
    memcpy(offsetsHigh, pContext->highestOffsets, sizeof(offsetsHigh));

    int low = 1;
    int high = pContext->numOffsets - 2;
    int nIterations = 0;
    while (true) {
        if (nIterations++ > 20) {
            printf("Panic!");
            fflush(stdout);
            return -1;
        }
        // Scale the "guess" proportionally
        int32_t low_v = offsetsLow[0];
        int32_t high_v = offsetsHigh[0];
        int32_t range = max(high_v - low_v, 1); // don't zerodiv
        // Force the computation in 64 bits because it overflows 32 bits.
        int delta = ((int64_t) (offset - low_v) * (high - low)) / range;
        int guess = max(low, min(high, low + delta));
        fseek(pContext->offsetsFile, sizeof(struct M3tHeader)+(guess - 1) * sizeof(int32_t), SEEK_SET);
        fread(offsetsGuess, sizeof(int32_t), 4, pContext->offsetsFile);
        INCREMENT_COUNT();
#if defined(DEBUG_MP3_TIMING)
        if (bug) {
            int32_t guess_v = offsetsGuess[1];
            printf("offset: %d, low:%d, high:%d, low_v:%d, high_v:%d, range: %d, delta: %d, guess: %d, guess_v: %d\n",
                   offset, low, high, low_v, high_v, range, delta, guess, guess_v);
            fflush(stdout);
        }
#endif
        if (offset >= offsetsGuess[0] && offset < offsetsGuess[3]) {
            if (offset >= offsetsGuess[2])
                return (guess + 1) * pContext->frameDuration;
            else if (offset >= offsetsGuess[1])
                return guess * pContext->frameDuration;
            else
                return (guess - 1) * pContext->frameDuration;
        }
        if (offset < offsetsGuess[1]) {
            high = guess - 1;
            memcpy(offsetsHigh, offsetsGuess, sizeof(offsetsHigh));
        } else if (offset >= offsetsGuess[1]) {
            if (offset < offsetsGuess[2]) {
                return guess * pContext->frameDuration;
            }
            low = guess + 1;
            memcpy(offsetsLow, offsetsGuess + 2, sizeof(offsetsLow));
        }
        if (high < low) {
            return -1;
        }
    }
}

/**
 * Given a timestmap, return the offset of the frame containing that timestamp.
 * @param pContext of a previously opened .m3t file.
 * @param timestamp in milliseconds for which the offset is needed.
 * @return offset of the frame containing the timestamp.
 */
int32_t offsetForTimestamp(struct M3tContext *pContext, int32_t timestamp) {
    int frame = timestamp / pContext->frameDuration;
    fseek(pContext->offsetsFile, sizeof(struct M3tHeader) + frame * sizeof(int32_t), SEEK_SET);
    int32_t result;
    fread(&result, sizeof(result), 1, pContext->offsetsFile);
    return result;

}

int32_t getTotalDuration(struct M3tContext *pContext) {
    return pContext->numOffsets * pContext->frameDuration + pContext->frameDuration;
}

#if defined(DEBUG_MP3_TIMINGS)

int32_t timestampForOffset1(struct M3tContext *pContext, int32_t offset) {
    if (nOffsets <= 1 || offset < offsets[1]) {
        // No frames (empty file), or only one, or within first: first frame.
        return 0;
    } else if (offset >= offsets[nOffsets - 1]) {
        // In or beyond last frame: last frame
        return nOffsets * pContext->frameDuration;
    }
    int low = 0;
    int high = nOffsets - 1;
    int mid = (low + high) / 2;
    while (true) {
        INCREMENT_COUNT();
        if (offset >= offsets[mid] && offset < offsets[mid + 1]) {
            return mid * pContext->frameDuration;
        }
        if (offset >= offsets[mid])
            low = mid + 1;
        else
            high = mid - 1;
        mid = (low + high) / 2;
        if (high < low) {
            return -1;
        }
    }
}

int32_t timestampForOffset2(struct M3tContext *pContext, int32_t offset) {
    if (nOffsets <= 1 || offset < offsets[1]) {
        // No frames (empty file), or only one, or within first: first frame.
        return 0;
    } else if (offset >= offsets[nOffsets - 1]) {
        // In or beyond last frame: last frame
        return nOffsets * pContext->frameDuration;
    }
    // The offset is at >=low and <=high, and there are multiple offsets.
    int low = 0;
    int high = nOffsets - 2;
    int nIterations = 0;
    while (true) {
        INCREMENT_COUNT();
        if (nIterations++ > 20) {
            printf("Panic!");
            fflush(stdout);
        }
        // Scale the "guess" proportionally
        int32_t low_v = offsets[low];
        int32_t high_v = offsets[high];
        int32_t range = max(high_v - low_v, 1); // don't zerodiv
        // Force the computation in 64 bits because it overflows 32 bits.
        int delta = ((int64_t) (offset - low_v) * (high - low)) / range;
        int guess = max(low, min(high, low + delta));
        int32_t guess_v = offsets[guess];
        if (bug) {
            printf("offset: %d, low:%d, high:%d, low_v:%d, high_v:%d, range: %d, delta: %d, guess: %d, guess_v: %d\n",
                   offset, low, high, low_v, high_v, range, delta, guess, guess_v);
            fflush(stdout);
        }
        if (offset >= offsets[guess] && offset < offsets[guess + 1]) {
            return guess * pContext->frameDuration;
        }
        if (offset < offsets[guess])
            high = guess - 1;
        else if (offset >= offsets[guess])
            low = guess + 1;
        if (high < low) {
            return -1;
        }
    }
}

int32_t variantTimestampForOffset(struct M3tContext *pContext, int variant, int32_t offset, int *pOperationCount) {
    int32_t result;
    operationCount = 0;
    if (variant == 0) {
        result = timestampForOffset(pContext, offset);
    } else if (variant == 1) {
        result = timestampForOffset1(pContext, offset);
    } else if (variant == 2) {
        result = timestampForOffset2(pContext, offset);
    } else {
        result = -1;
    }
    *pOperationCount = operationCount;
    return result;
}

void setMp3Logging(bool log) {
    bug = log;
}

int32_t getNumOffsets() {
    return nOffsets;
}
int32_t getHightestOffset() {
    return offsets[nOffsets-1];
}

#endif

