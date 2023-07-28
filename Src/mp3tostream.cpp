
#include <stdio.h>
#include <stdint.h>
#include "wav.h"
#include "mad.h"
#include "tbook.h"
#include "fileOps.h"

#include "mp3timing.h"
#include "mp3tostream.h"

#include "encAudio.h"

static const int INPUT_BUF_SIZE = kBufferSize;
static const int OUTPUT_BUF_SIZE = kBufferSize;
static const int MP3_FRAME_SIZE = 2881;

static struct Mp3ToStream {
    FILE *          mp3File;
    M3tContext_t *  pM3tContext;
    int             mp3FileLength;
    bool            mp3Eof;
    WavFmtData_t    wavFormat;          
    volatile bool   quitRequested;
    volatile bool   seekPending;        // When this is true, output_func should ignore output.
    volatile int    seekAddress;

    bool            sentHeader;     // So we can send the .WAV format exactly once.

    int             inputPosition;  // Where the input buffer is from in the file.
    int             outputPosition; // Position of first frame that contributed to output buffer.
    int             outputBytes;    // Number of bytes currently in the output buffer.
    Buffer_t *      inputBuf;
    
    volatile Buffer_t * outputBuffer;   // When non-NULL, results are placed here.
    int                 resultBytes;    // The length of data in the buffer, when it is released.
    int                 resultPosition; // The position in the mp3 file of the first audio in the buffer.
} mp3ToStream = {0};

struct M3tContext m3tContext;

#if defined(HD_HEX_DUMP)
void hexDump(const char * pTitle, const void * pRaw, int len) {
    char buf[100];
    while (len & 0xf) len++; // ceiling
    const uint8_t *pData = (uint8_t*)pRaw;
    const uint8_t *pEnd = pData + len;
    osDelay(1);
    printf("\n%s\n", pTitle);
    osDelay(1);
    while (pData < pEnd) {
        char *pBuf = buf;
        // Address
        pBuf += sprintf(buf, "%p  ", pData);
        // Skip zeros.
        const uint32_t *pz = (const uint32_t *)pData;
        if (pz[0] == 0 && pz[1] == 0 && pz[2] == 0 && pz[3] == 0) {
            int numLinesOfZeros = 0;
            while (pz[0] == 0 && pz[1] == 0 && pz[2] == 0 && pz[3] == 0) {
                ++numLinesOfZeros;
                pz += 4;
            }
            osDelay(1);
            printf("%s %d lines of zeros\n", buf, numLinesOfZeros);
            osDelay(1);
            pData = (const uint8_t *)pz;
            continue;
        }
        // Four words of 4-byte data
        const uint8_t *p = pData;
        for (int i=0; i<4; ++i) {
            pBuf += sprintf(pBuf, "%02x %02x %02x %02x  ", p[0], p[1], p[2], p[3]);
            p += 4;
        }
        // 16 characters of printables
        p = pData;
        for (int i=0; i<16; ++i, ++pBuf, ++p)
            *pBuf = (isprint(*p) ? *p : '.');
        *pBuf++ = '\n';
        *pBuf = '\0';
        osDelay(1);
        printf(buf);
        osDelay(1);
        pData += 16;
    }
    osDelay(1);
    printf("\n");
    osDelay(1);
}
#define hd(a,b,c) hexDump(a,b,c)
#elif defined(HD_TRACE_HASH)
static char hashHistory[512];
static char *pHH = hashHistory;
void hexDump(const char * pTitle, const void * pRaw, int len) {
    uint8_t hash = 0;
    const uint8_t *pData = (uint8_t *) pRaw;
    const uint8_t *pEnd = pData + len;
    while(pData<pEnd) hash ^= *pData++;
    pHH += sprintf(pHH, "%s:%02x  ", pTitle, hash);
    if (pHH >= hashHistory+sizeof(hashHistory)-20) {
        pHH = hashHistory;
        memset(hashHistory, 0, sizeof(hashHistory));
    }
}
#define hd(a,b,c) hexDump(a,b,c)
#else
#define hd(a,b,c)
#endif


static enum mad_flow input_func(void *context, struct mad_stream *stream) {
    struct Mp3ToStream *pMp3ToStream = (struct Mp3ToStream *) context;
    if (pMp3ToStream->quitRequested) {
        return MAD_FLOW_STOP;
    }

    // adapted from https://stackoverflow.com/questions/39803572/libmad-playback-too-fast-if-read-in-chunks
    unsigned char *mp3_buf = (unsigned char *) pMp3ToStream->inputBuf; /* MP3 data buffer. */
    int retval; /* Return value from read(). */
    int len; /* Length of the new buffer. */
    int eof; /* Whether this is the last buffer that we can provide. */

    int keep; /* Number of bytes to keep from the previous buffer. */

    // Any commands from the caller? (seek, quit)
    uint32_t eventFlags = osEventFlagsWait(fileOpRequestEventId, kStopRequest|kSeekRequest, osFlagsWaitAny, 0);
    if ((eventFlags & 1u<<31) == 0) {
        if (eventFlags & kStopRequest) {
            return MAD_FLOW_STOP;
        }
    }
    if (pMp3ToStream->seekPending) {
        // Seeking to a new point in the file, so don't keep anything.
        fseek( pMp3ToStream->mp3File, pMp3ToStream->seekAddress, SEEK_SET );      // seek wav file to byte pos of stSample
        keep = 0;
        pMp3ToStream->seekPending = false;
    } else {
        /* Figure out how much data we need to move from the end of the previous
       buffer into the start of the new buffer. */
        if (stream->error != MAD_ERROR_BUFLEN) {
            /* All data has been consumed, or this is the first call. */
            keep = 0;
        } else if (stream->next_frame != NULL) {
            /* The previous buffer was consumed partially. Move the unconsumed portion
            into the new buffer. */
            keep = stream->bufend - stream->next_frame;
        } else if ((stream->bufend - stream->buffer) < INPUT_BUF_SIZE) {
            /* No data has been consumed at all, but our read buffer isn't full yet,
            so let's just read more data first. */
            keep = stream->bufend - stream->buffer;
        } else {
            /* No data has been consumed at all, and our read buffer is already full.
            Shift the buffer to make room for more data, in such a way that any
            possible frame position in the file is completely in the buffer at least
            once. */
            keep = INPUT_BUF_SIZE - MP3_FRAME_SIZE;
        }
    }

    /* Shift the end of the previous buffer to the start of the new buffer if we
    want to keep any bytes. */
    if (keep) {
        memmove(mp3_buf, stream->bufend - keep, keep);
    }

    /* Append new data to the buffer. */
    int readPosition = ftell(pMp3ToStream->mp3File);
    retval = fread(mp3_buf + keep, 1, INPUT_BUF_SIZE - keep, pMp3ToStream->mp3File);

    if (retval < 0) {
        /* Read error. */
        errLog("Mp3 failed to read: %d \n", retval);
        return MAD_FLOW_STOP;
    }
    pMp3ToStream->inputPosition = readPosition - keep;
    if (retval == 0) {
        pMp3ToStream->mp3Eof = true;
        /* End of file. Append MAD_BUFFER_GUARD zero bytes to make sure that the
        last frame is properly decoded. */
        if (keep + MAD_BUFFER_GUARD <= INPUT_BUF_SIZE) {
            /* Append all guard bytes and stop decoding after this buffer. */
            memset(mp3_buf + keep, 0, MAD_BUFFER_GUARD);
            len = keep + MAD_BUFFER_GUARD;
            eof = 1;
        } else {
            /* The guard bytes don't all fit in our buffer, so we need to continue
            decoding and write all fo teh guard bytes in the next call to input(). */
            memset(mp3_buf + keep, 0, INPUT_BUF_SIZE - keep);
            len = INPUT_BUF_SIZE;
            eof = 0;
        }
    } else {
        /* New buffer length is amount of bytes that we kept from the previous
        buffer plus the bytes that we read just now. */
        len = keep + retval;
        eof = 0;
    }

    hd("Inp2Mad", mp3_buf, len);
    /* Pass the new buffer information to libmad. */
    mad_stream_buffer(stream, mp3_buf, len);
    return eof ? MAD_FLOW_STOP : MAD_FLOW_CONTINUE;
}

static enum mad_flow header_func(void *context, struct mad_header const *header) {
    // un-comment for debugging
    //struct Mp3ToStream *pMp3ToStream = (struct Mp3ToStream *) context;
    //mad_timer_t duration = header->duration;
    //long resolution = 352800000UL;
    //int position = pMp3ToStream->inputPosition;
    return MAD_FLOW_CONTINUE;
}

/**
 * Scales a sample to the output size. This is fast, not high-fidelity.
 * TODO: replace with dithering?
 * @param sample to be scaled
 * @return scaled sample
 */
static inline int scale(mad_fixed_t sample) {
    /*
     * The following utility routine performs simple rounding, clipping, and
     * scaling of MAD's high-resolution samples down to 16 bits. It does not
     * perform any dithering or noise shaping, which would be recommended to
     * obtain any exceptional audio quality. It is therefore not recommended to
     * use this routine if high-quality output is desired.
     */

    sample += (1L << (MAD_F_FRACBITS - 16));  /* round */

    /* clip */
    if (sample >= MAD_F_ONE)
        sample = MAD_F_ONE - 1;
    else if (sample < -MAD_F_ONE)
        sample = -MAD_F_ONE;

    return sample >> (MAD_F_FRACBITS + 1 - 16);  /* quantize */
}

static void sendHeader(struct Mp3ToStream *pMp3ToStream) {
    osEventFlagsSet(fileOpResultEventId, kHeaderResult);
    pMp3ToStream->sentHeader = true;
}

static void sendBuffer(struct Mp3ToStream *pMp3ToStream) {
    hd("Q", (const void *)pMp3ToStream->outputBuffer, pMp3ToStream->outputBytes);
    pMp3ToStream->resultBytes = pMp3ToStream->outputBytes;
    pMp3ToStream->resultPosition = pMp3ToStream->outputPosition;
    // Let the media thread know it owns the buffer again.
    osEventFlagsSet(fileOpResultEventId, kFilledResult);
    // Remember that this thread does not own the buffer.
    pMp3ToStream->outputBuffer = NULL;
    pMp3ToStream->outputBytes = 0;
}

static bool acquireOutputBuffer(struct Mp3ToStream *pMp3ToStream) {
    // Wait for the media thread to ask for a buffer of decoded data.
    uint32_t eventFlags = osEventFlagsWait(fileOpRequestEventId, kAnyRequest, osFlagsWaitAny, osWaitForever);
    if ((eventFlags & 1u<<31) == 0) {
        if (eventFlags & (kStopRequest|kSeekRequest)) {
            return false;
        } else if (eventFlags & kFillRequest) {
            pMp3ToStream->outputBytes = 0;
            pMp3ToStream->outputPosition = pMp3ToStream->inputPosition;
            if (pMp3ToStream->outputBuffer == NULL) {
                errLog("Failed to get an output buffer!");
                return false;
            }
            return true;
        }
    }
    return false;
}

static enum mad_flow output_func(void *context, struct mad_header const *header, struct mad_pcm *pcm) {
    /*
     * This is the output callback function. It is called after each frame of
     * MPEG audio data has been completely decoded. The purpose of this callback
     * is to output (or play) the decoded PCM audio.
     */
    struct Mp3ToStream *pMp3ToStream = (struct Mp3ToStream *) context;
    if (pMp3ToStream->quitRequested) {
        return MAD_FLOW_STOP;
    }
    if (pMp3ToStream->seekPending) {
        return MAD_FLOW_IGNORE;
    }
    // Any commands from the caller? (seek, quit)
    uint32_t eventFlags = osEventFlagsWait(fileOpRequestEventId, kStopRequest, osFlagsWaitAny, 0);
    if ((eventFlags & 1u<<31) == 0) {
        if (eventFlags & kStopRequest) {
            return MAD_FLOW_STOP;
        }
    }

    // Send the header one time.
    if (!pMp3ToStream->sentHeader) {
        // We always send 2 channels, even if the source material is 1 channel.
        // This is because we need to send 2 channels to the codec. If we fill
        // a buffer with 1 channel of data there won't be room to expand to
        // 2 channels for the codec. So do the expansion before we send.
        initWavFormat(&pMp3ToStream->wavFormat, header->samplerate, 2, 16);
        // Send to caller
        sendHeader(pMp3ToStream);
    }

    unsigned int nchannels = pcm->channels;
    unsigned int samplesRemaining = pcm->length;
    mad_fixed_t const *left_ch = pcm->samples[0];
    mad_fixed_t const *right_ch = pcm->samples[1];

    // If we have data to send and nowhere to put it, allocate a buffer.
    if (samplesRemaining && !pMp3ToStream->outputBuffer) {
        if (!acquireOutputBuffer(pMp3ToStream)) {
            if (pMp3ToStream->seekPending) return MAD_FLOW_IGNORE;
            return MAD_FLOW_STOP;
        }
    }
    // Get character pointers because we're writing character at a time.
    uint8_t *pBegin = ((uint8_t*)pMp3ToStream->outputBuffer);
    uint8_t *pOut = pBegin + pMp3ToStream->outputBytes;
    uint8_t *pEnd = pBegin + OUTPUT_BUF_SIZE;
    while (samplesRemaining--) {
        // Each sample is a power-of-two in size (2 or 4), and the buffer is a
        // larger power-of-two, so we know that samples are never split.
        signed int sample;

        if (pBegin == NULL || pMp3ToStream->outputBuffer == NULL) {
            errLog("madlib output_func NULL output buffer!");
            return MAD_FLOW_STOP;
        }

        /* output sample(s) in 16-bit signed little-endian PCM */

        sample = scale(*left_ch++);
        //sample = (samplesRemaining & 0x04) ? 0x8888 : 0x0001;
        *pOut++ = (sample >> 0) & 0xff;
        *pOut++ = (sample >> 8) & 0xff;

        if (nchannels == 2) {
            sample = scale(*right_ch++);
        }
        *pOut++ = (sample >> 0) & 0xff;
        *pOut++ = (sample >> 8) & 0xff;

        if (pOut == pEnd) {
            // Send to caller
            pMp3ToStream->outputBytes = pOut - pBegin;
            sendBuffer(pMp3ToStream);
            // If we still have data to send, allocate another buffer.
            if (samplesRemaining > 0) {
                if (!acquireOutputBuffer(pMp3ToStream)) {
                    if (pMp3ToStream->seekPending) return MAD_FLOW_IGNORE;
                    return MAD_FLOW_STOP;
                }
                pBegin = ((uint8_t*)pMp3ToStream->outputBuffer);
                pOut = pBegin + pMp3ToStream->outputBytes;
                pEnd = pBegin + OUTPUT_BUF_SIZE;
            }
        }
    }
    pMp3ToStream->outputBytes = pOut - pBegin;

    return MAD_FLOW_CONTINUE;
}

/**
 * libmad calls this to report an error
 */
static enum mad_flow error_func(void *context, struct mad_stream *stream, struct mad_frame *frame) {
    /*
     * This is the error callback function. It is called whenever a decoding
     * error occurs. The error is indicated by stream->error; the list of
     * possible MAD_ERROR_* errors can be found in the mad.h (or stream.h)
     * header file.
     */
    //struct Mp3ToStream *pMp3ToStream = (struct Mp3ToStream *) context;

    /* return MAD_FLOW_BREAK here to stop decoding (and propagate an error) */
    return MAD_FLOW_CONTINUE;
}

void streamMp3DecodeLoop(void) {
    struct mad_decoder decoder;
    struct Mp3ToStream *pMp3ToStream = &mp3ToStream;

    osMutexAcquire(fileOpMutex, osWaitForever);

    // Set up and run the decoder.
    mad_decoder_init(&decoder, pMp3ToStream, input_func, header_func, 0 /* filter */, output_func, error_func,
                     0 /* message */);
    int result = mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);  // start decoding
    mad_decoder_finish( &decoder );
    if (pMp3ToStream->outputBuffer) {
        sendBuffer(pMp3ToStream);
    }
    tbFclose(pMp3ToStream->mp3File);
    closeMp3Timings(&m3tContext);

    releaseBuffer(pMp3ToStream->inputBuf);
    dbgLog("D MP3 playback finished\n");

    FileSysPower( false );

    osMutexRelease(fileOpMutex);
}

/**
 * Open an mp3 file for decoding, and initialize the decoding context.
 * @param fname Name of the mp3 file.
 * @param pMp3ToStream The decoding context.
 * @return true if the file opened OK, false otherwise.
 */
static bool initMp3FileForStreaming(const char *fname, struct Mp3ToStream *pMp3ToStream) {
    pMp3ToStream->mp3Eof = false;
    pMp3ToStream->quitRequested = false;
    pMp3ToStream->seekAddress = 0;
    pMp3ToStream->sentHeader = false;
    pMp3ToStream->inputPosition = 0;
    pMp3ToStream->outputPosition = 0;
    pMp3ToStream->outputBytes = 0;

    printf("Open mp3 file %s\n", fname);
    pMp3ToStream->mp3File = tbFopen(fname, "rb");
    if (!pMp3ToStream->mp3File) {
        errLog("mp3 file %s not found\n", fname);
        return false;
    }
    pMp3ToStream->inputBuf = allocateBuffer("read mp3");
    fseek(pMp3ToStream->mp3File, 0, SEEK_END);
    pMp3ToStream->mp3FileLength = ftell(pMp3ToStream->mp3File);
    fseek(pMp3ToStream->mp3File, 0, SEEK_SET);

    memset(&m3tContext, 0, sizeof(m3tContext));
    char m3tname[MAX_PATH];
    strcpy(m3tname, fname);
    strcpy(strrchr(m3tname, '.'), m3tExtension);
    if (openMp3Timings(m3tname, &m3tContext)) {
      pMp3ToStream->pM3tContext = &m3tContext;
    }
    
    return true;
}

/**
 * Open an mp3 file for decoding.
 * @param fname The file to be opened.
 * @param pWavFormat Put the equivalent wav header here.
 * @param pMp3FileLength Put the mp3 file length here.
 * @return true if the file is ready for decoding, false if there was an error.
 */
bool streamMp3Start(const char *fname, WavFmtData_t *pWavFormat, int *pMp3FileLength) {
    struct Mp3ToStream *pMp3ToStream = &mp3ToStream;

    osEventFlagsSet(fileOpRequestEventId, kStopRequest);
    osMutexAcquire(fileOpMutex, osWaitForever);

    bool result = initMp3FileForStreaming(fname, &mp3ToStream);
    if (result) {
        *pMp3FileLength = pMp3ToStream->mp3FileLength;

        osEventFlagsClear(fileOpRequestEventId, kAnyRequest);
        osEventFlagsClear(fileOpResultEventId, kAnyResult);
        osEventFlagsSet(fileOpRequestEventId, kDecodeRequest);
    }
    // Release this now so the decoder can proceed, so we can get the header from the first frame.
    osMutexRelease(fileOpMutex);
    if (result) {
        osEventFlagsWait(fileOpResultEventId, kHeaderResult, osFlagsWaitAny, osWaitForever);
        *pWavFormat = pMp3ToStream->wavFormat;
    }
    return result;
}

/**
 * Call this to stop decoding the mp3 file.
 */
void streamMp3Stop(void) {
    dbgLog("D Requesting libmad quit\n");
    mp3ToStream.quitRequested = true;
    osEventFlagsSet(fileOpRequestEventId, kStopRequest);
}

void streamMp3JumpToTime(int newMs) {
    int seekAddress = offsetForTimestamp(mp3ToStream.pM3tContext, newMs);
    mp3ToStream.seekAddress = seekAddress;
    mp3ToStream.seekPending = true;
    osEventFlagsSet(fileOpRequestEventId, kSeekRequest);
}

int streamMp3GetLatestTime() {
    int timestamp = timestampForOffset(mp3ToStream.pM3tContext, mp3ToStream.outputPosition);
    return timestamp;
}

int streamMp3GetTotalDuration() {
    return getTotalDuration(mp3ToStream.pM3tContext);
}

/**
 * Fill a buffer with decoded contents from the mp3 file.
 * @param pBuffer The buffer to be filled.
 * @return The number of bytes read, or 0 for EOF.
 */
int streamMp3FillBuffer( Buffer_t *pBuffer) {
    struct Mp3ToStream * pMp3ToStream = &mp3ToStream;
    if (pMp3ToStream->mp3Eof) {
        return 0;
    }
    // Give the decoder a buffer to fill, and request the filling of it.
    pMp3ToStream->outputBuffer = pBuffer;
    osEventFlagsSet(fileOpRequestEventId, kFillRequest);
    // Wait for it to fill the buffer.
    osEventFlagsWait(fileOpResultEventId, kFilledResult, osFlagsWaitAny, osWaitForever);
    pMp3ToStream->outputBuffer = NULL;
    
    hd("DQ", pBuffer, pMp3ToStream->resultBytes);

    return pMp3ToStream->resultBytes;
}


