//
// Created by bill on 9/5/2022.
//

#ifndef MP3FRAMES_MP3TIMING_H
#define MP3FRAMES_MP3TIMING_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

//#define DEBUG_MP3_TIMINGS


typedef struct {
    uint8_t signature[4];
} M3tSignature_t;

struct M3tHeader {
    M3tSignature_t signature;
    int32_t frameDuration;
};

extern const char * const m3tExtension;
extern const M3tSignature_t m3tSignature;
extern const struct M3tHeader m3tProtoHeader;

typedef struct M3tContext {
    FILE * offsetsFile;
    int32_t frameDuration;
    int32_t lowestOffsets[3];
    int32_t highestOffsets[2];
    int32_t numOffsets;
} M3tContext_t;

/**
 * Opens a ".m3t" file, for translating file offset <--> audio timestamp.
 * @param m3tFilename of the .m3t file.
 * @param pContext a M3tContext structure to be filled in.
 * @return true if the file was opened successfully, and the header matched.
 */
bool openMp3Timings(const char *m3tFilename, struct M3tContext *pContext);
/**
 * Closes a previously opened ".m3t" file.
 * @param pContext from the opened .m3t file.
 */
void closeMp3Timings(struct M3tContext *pContext);
/**
 * Given an offset, return the timestamp of the frame containing that offset.
 * @param pContext of a previously opened .m3t file.
 * @param offset for which the timestamp is needed.
 * @return timestamp, in milliseconds, of the frame containing the offset.
 */
int32_t timestampForOffset(struct M3tContext *pContext, int32_t offset);
/**
 * Given a timestmap, return the offset of the frame containing that timestamp.
 * @param pContext of a previously opened .m3t file.
 * @param timestamp in milliseconds, for which the offset is needed.
 * @return offset of the frame containing the timestamp.
 */
int32_t offsetForTimestamp(struct M3tContext *pContext, int32_t timestamp);
/**
 * Retrieves the total duration of the file.
 * @param pContext of a previously opened .m3t file.
 * @return the duration of the file, in milliseconds.
 */
int32_t getTotalDuration(struct M3tContext *pContext);

#if defined(DEBUG_MP3_TIMINGS)
int32_t getNumOffsets(void);
int32_t getHightestOffset(void);
int32_t variantTimestampForOffset(struct M3tContext *pContext, int variant, int32_t offset, int *pOperationCount);
void setMp3Logging(bool log);
#endif

#endif //MP3FRAMES_MP3TIMING_H
