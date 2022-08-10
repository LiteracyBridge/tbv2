#ifndef WAV_H
#define WAV_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// The header for a .riff file, of which .wav is one.
typedef struct {
    uint32_t        riffId;       // 'RIFF'
    uint32_t        waveSize;     // Size of 'WAVE' and chunks and data that follow
    uint32_t        waveId;       // 'WAVE'
}                WavHeader_t;

// Header for "chunks" inside a .riff file. Each header is followed by
// chunkSize bytes of chunk-specific data.
typedef struct {
    uint32_t        chunkId;      // "fmt ", "data", "LIST", and so forth
    uint32_t        chunkSize;    // Size of the data starting with the next byte.
}                RiffChunk_t;

// The data layout for a PCM "fmt " chunk. Describes a PCM recording.
typedef struct {
    uint16_t        formatCode;   // format code; we only support '1', WAVE_FORMAT_PCM
    uint16_t        numChannels;  // Number of interleaved channels
    uint32_t        samplesPerSecond;
    uint32_t        bytesPerSecond;
    uint16_t        blockSize;    // Block size in bytes (16-bits mono -> 2, 32-bit stereo -> 8, etc)
    uint16_t        bitsPerSample;
    // non-PCM formats can have extension data here.
}                WavFmtData_t;

typedef struct {
    WavHeader_t     wavHeader;
    RiffChunk_t     fmtHeader;
    WavFmtData_t    fmtData;
    RiffChunk_t     audioHeader;
}                WavFileHeader_t;

extern const WavFileHeader_t newWavHeader;

extern WavFileHeader_t * initWavHeader( WavFileHeader_t * header, int samplesPerSecond, int channels, int bitsPerSample );
extern WavFileHeader_t * updateAudioSize( WavFileHeader_t * header, int audioSize );
extern WavFmtData_t * initWavFormat( WavFmtData_t *pFormat, int samplesPerSecond, int channels, int bitsPerSample);

extern bool readWavFormat(FILE *wavFile, const char *fname, WavFmtData_t *pWavFmtData, int *pDataChunkSize, int *pDataOffset);
#endif // WAV_H
