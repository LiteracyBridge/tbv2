//
// Created by bill on 8/11/2022.
//
#include <stdint.h>
#include <string.h>

#include "tbook.h"
#include "wav.h"

WavFmtData_t * initWavFormat( WavFmtData_t *pFormat, int samplesPerSecond, int channels, int bitsPerSample) {
    unsigned long bytesPerSecond = (bitsPerSample * channels * samplesPerSecond) / 8;
    unsigned int  blockSize      = (bitsPerSample * channels) / 8;
    pFormat->formatCode       = 1; // PCM
    pFormat->numChannels      = channels;
    pFormat->samplesPerSecond = samplesPerSecond;
    pFormat->bytesPerSecond   = bytesPerSecond;
    pFormat->blockSize        = blockSize;
    pFormat->bitsPerSample    = bitsPerSample;
    return pFormat;
}

WavFileHeader_t * initWavHeader( WavFileHeader_t * pHeader, int samplesPerSecond, int channels, int bitsPerSample ) {
    int audioSize = 100000; // just made up.
    char   hdr[44]         = "RIFFsizeWAVEfmt \x10\0\0\0\1\0ch_hz_abpsbabsdatasize";
    unsigned long bytesPerSecond = (bitsPerSample * channels * samplesPerSecond) / 8;
    unsigned int  blockSize      = (bitsPerSample * channels) / 8;
    // @formatter:off
    *(int32_t *)(void*)(hdr + 0x04) = 44 + audioSize - 8;   /* File size - 8 */
    *(int16_t *)(void*)(hdr + 0x14) = 1;                     /* Integer PCM format */
    *(int16_t *)(void*)(hdr + 0x16) = channels;
    *(int32_t *)(void*)(hdr + 0x18) = samplesPerSecond;
    *(int32_t *)(void*)(hdr + 0x1C) = bytesPerSecond;
    *(int16_t *)(void*)(hdr + 0x20) = blockSize;
    *(int16_t *)(void*)(hdr + 0x22) = bitsPerSample;
    *(int32_t *)(void*)(hdr + 0x28) = audioSize;
    // @formatter:on

    *pHeader = newWavHeader;
    pHeader->wavHeader.waveSize       = 44 + audioSize - 8; // file size -8 (eventually)
    initWavFormat(&pHeader->fmtData, samplesPerSecond, channels, bitsPerSample);
    pHeader->audioHeader.chunkSize    = audioSize; // The initial value is just made up

    // @TODO: verify that the "named" header matches the "hand-rolled" one, then return it.
    int cmp = memcmp(pHeader, &hdr, sizeof(WavFileHeader_t));

    return pHeader;
}

WavFileHeader_t * updateAudioSize( WavFileHeader_t * pHeader, int audioSize ) {
    pHeader->wavHeader.waveSize    = 44 + audioSize - 8;
    pHeader->audioHeader.chunkSize = audioSize;
    return pHeader;
}

/**
 * Reads the .wav format header from a .wav file.
 * @param wavFile The .wav file handle.
 * @param pWavFmtData Pointer to a WavFmtData_t structure to be filled in.
 * @param pDataChunkSize Pointer ot an integer to receive the size of the data.
 * @param pDataOffset Pointer to an integer to receive the file offset of the start of the data.
 * @return True if we found the format and the file handle now points to the DATA.
 *         False otherwise.
 */
bool readWavFormat(FILE *wavFile, const char *fname, WavFmtData_t *pWavFmtData, int *pDataChunkSize, int *pDataOffset) {
    fseek( wavFile, 0, SEEK_SET );

    WavHeader_t wavFileHeader;
    if ( fread( &wavFileHeader, 1, sizeof( WavHeader_t ), wavFile ) != sizeof( WavHeader_t )) {
        errLog( "read wav header failed, %s", fname );
        return false;
    }
    if ( wavFileHeader.riffId != newWavHeader.wavHeader.riffId ||
         wavFileHeader.waveId != newWavHeader.wavHeader.waveId ) {
        errLog( "bad wav: %s  chunkId=0x%x format=0x%x \n", fname, wavFileHeader.riffId, wavFileHeader.waveId );
        return false;
    }

    // Read file chunks until we find "data". When we find "fmt ", load it into pSt.
    bool haveFmt = false;
    while (1) {
        // Read the next chunk header.
        RiffChunk_t riffChunkHeader;
        if ( fread( &riffChunkHeader, 1, sizeof( RiffChunk_t ), wavFile ) != sizeof( RiffChunk_t )) {
            errLog( "read wav file failed, %s", fname );
            return false;
        }

        if ( riffChunkHeader.chunkId == newWavHeader.audioHeader.chunkId ) {
            // Is it "data"? If so, we're done! (And we had better have read "fmt");
            *pDataChunkSize = riffChunkHeader.chunkSize;
            *pDataOffset = ftell( wavFile );
            return haveFmt;

        } else if ( riffChunkHeader.chunkId == newWavHeader.fmtHeader.chunkId ) {
            // Is it "fmt "? If so, validate and read the format details.
            if ( riffChunkHeader.chunkSize != newWavHeader.fmtHeader.chunkSize ) {
                errLog( "Unexpected fmt size in wav file, %s, expected %d, got %d", fname, riffChunkHeader.chunkSize,
                        newWavHeader.fmtHeader.chunkSize );
                return false;
            }
            if ( fread( pWavFmtData, 1, sizeof( WavFmtData_t ), wavFile ) != sizeof( WavFmtData_t )) {
                errLog( "read wav file fmt data failed, %s", fname );
                return false;
            } else {
                haveFmt = true;
            }
        } else {
            // Something else, just skip over it.
            fseek( wavFile, riffChunkHeader.chunkSize, SEEK_CUR );
        }
    }
    // Never reaches this point.
}
