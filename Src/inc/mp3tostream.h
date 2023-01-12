#ifndef MP3TOSTREAM_H
#define MP3TOSTREAM_H

#include <stdbool.h>

#include "buffers.h"
#include "wav.h"

typedef enum Mp3ToStreamResultType {
    kMp3WavFormat,            // wav format equivalent for the mp3
    kMp3WavBuffer,            // one buffer's worth of audio data
    kMp3DidSeek,              // Sent after a seek, so consumer knows which buffers to discard.
    kMp3EOF                   // Done, no more data.
} Mp3ToStreamResultType_t;

typedef struct Mp3ToStreamResult {
    Mp3ToStreamResultType_t resultType;     // The type indicates which fields will be valid.
    int positionInFile;                     // A position somewhere before the data (because it isn't a perfect match).
    int earliestTimeInMessage;              // If kMp3WavBuffer, the first known time in the buffer.
    int nSamples;                           // If kMp3WavBuffer, the number of 16-bit samples in data.
    void * data;                            // If kMp3WavFormat: wav format; kMp3WavBuffer: audio data.
} Mp3ToStreamResult_t;

/**
 * The mp3 decoder loop, called from the FileOps thread.
 */
extern void streamMp3DecodeLoop(void);

/**
 * Fill the buffer from the current position in the MP3 file. Return the number of bytes
 * returned.
 * @param buffer to be filled with audio data.
 * @return number of bytes in the buffer, 0 at EOF.
 */
extern int streamMp3FillBuffer( Buffer_t *buffer);

/**
 * Sets up the mp3 decode process for the given file.
 * @param fname The full path of the .mp3 file. It is required that a .m3t file be parallel to the .mp3,
 *              containing timing offsets (used for reporting position, and for seeking).
 * @param pWavFormat The equivalent WAVE file header is constructed here.
 * @param mp3FileLength The length of the file in bytes is stored here.
 * @return true if successfully opened, false otherwise.
 */
extern bool streamMp3Start(const char *fname, WavFmtData_t *pWavFormat, int *mp3FileLength);

/**
 * Terminates mp3 decoding before the end of file (ie, early).
 */
extern void streamMp3Stop( void );

//extern void streamMp3Seek( int position, int assumedTimestamp );
/**
 * @return the duration of the file, in milliseconds.
 */
extern int streamMp3GetTotalDuration(void);

/**
 * @return the current position in the file, in milliseconds.
 */
extern int streamMp3GetLatestTime(void);

/**
 * Seek to a specific time in the file. Will start at the MP3 frame containing the given timestamp.
 * @param newMs at which to (re-)start.
 */
extern void streamMp3JumpToTime( int newMs );

#endif // MP3TOSTREAM_H

