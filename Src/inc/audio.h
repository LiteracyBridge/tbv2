#ifndef AUDIO_H
#define AUDIO_H

#include "main.h"
#include "Driver_SAI.h"

#include "codecI2C.h"

#include "rl_fs_lib.h"

extern ARM_DRIVER_SAI Driver_SAI0;      // from I2S_stm32F4xx.c

// constants defined in mediaplayer.c & referenced in audio.c
extern const int CODEC_DATA_TX_DN;   // signal sent by SAI callback when buffer TX done
extern const int CODEC_PLAYBACK_DN;  // signal sent by SAI callback when playback complete
extern const int CODEC_DATA_RX_DN;   // signal sent by SAI callback when a buffer has been filled
extern const int CODEC_RECORD_DN;    // signal sent by SAI callback when recording stops
extern const int MEDIA_PLAY_EVENT;
extern const int MEDIA_RECORD_START;

// The header for a .riff file, of which .wav is one.
typedef struct {
    uint32_t riffId;       // 'RIFF'
    uint32_t waveSize;     // Size of 'WAVE' and chunks and data that follow
    uint32_t waveId;       // 'WAVE'
}                WavHeader_t;

// Header for "chunks" inside a .riff file. Each header is followed by
// chunkSize bytes of chunk-specific data.
typedef struct {
    uint32_t chunkId;      // "fmt ", "data", "LIST", and so forth
    uint32_t chunkSize;    // Size of the data starting with the next byte.
}                RiffChunk_t;

// The data layout for a PCM "fmt " chunk. Describes a PCM recording.
typedef struct {
    uint16_t formatCode;   // format code; we only support '1', WAVE_FORMAT_PCM
    uint16_t numChannels;  // Number of interleaved channels
    uint32_t samplesPerSecond;
    uint32_t bytesPerSecond;
    uint16_t blockSize;    // Block size in bytes (16-bits mono -> 2, 32-bit stereo -> 8, etc)
    uint16_t bitsPerSample;
    // non-PCM formats can have extension data here.
}                WavFmtData_t;


// Audio encoding formats
typedef enum {        // audType
    audUNDEF, audWave, audMP3, audOGG, audTONE           // sqrWAV generated tone
}                audType_t;

typedef enum {        // pnRes_t            -- return codes from PlayNext
    pnDone, pnPaused, pnPlaying
}                pnRes_t;

typedef enum {        // BuffState          -- audio buffer states
    bFree, bAlloc, bEmpty, bFull, bDecoding, bPlaying, bRecording, bRecorded
}                BuffState;

typedef struct {      // Buffer_t           -- audio buffer
    BuffState state;
    uint32_t  firstSample;
    uint32_t  cntBytes;     // or timestamp for bRecorded
    uint16_t *data;         // buffer of 16bit samples
} Buffer_t;

#define N_AUDIO_BUFFS     8
typedef enum {      // playback_state_t   -- audio playback state codes
    pbIdle,         // not playing anything
    pbOpening,      // calling fopen
    pbLdHdr,        // fread header
    pbGotHdr,       // fread hdr done
    pbFilling,      // fread 1st data
    pbFull,         // fread data done
    pbPlaying,      // I2S transfer started
    pbPlayFill,     // fread data under I2S transfer
    pbLastPlay,     // fread data got 0
    pbFullPlay,     // fread data under I2S transfer done
    pbDone,         // I2S transfer of audio done
    pbPaused,

    pbWroteHdr,     // recording, wrote header
    pbRecording,    // started recording
    pbRecStop,      // record stop requested
    pbRecPaused     // recording paused -- NYI

} playback_state_t;

typedef enum {      // playback_type_t            sys/pkg/msg/nm/pr
    ptUNDEF,        // reset value
    ptPkg,          // package prompt
    ptSys,          // system prompt
    ptNm,           // subject name
    ptInv,          // subject invitation
    ptMsg,          // subject message
    ptRec,          // recorded file
    ptTone          // tone
} playback_type_t;


// functions called from mediaplayer.c
extern void audInitialize( void );                      // sets up I2S
extern void audPowerDown( void );                       // shut down and reset everything
extern MediaState audGetState( void );                  // => Ready or Playing or Recording
extern uint16_t audAdjVolume( int16_t adj );            // change volume by 'adj'  0..10
extern int32_t audPlayPct( void );                      // => current playback pct of file
extern void audAdjPlayPos( int32_t adj );               // shift current playback position +/- by 'adj' seconds
extern void audAdjPlaySpeed( int16_t adj );             // change playback speed by 'adj' * 10%
extern void audPlayAudio( const char *audioFileName, MsgStats *stats,
                          playback_type_t typ );        // start playing from file typ=sys/pkg/msg/nm/pr/rec
extern void audPlayTone( int i, int freq,
                         int nEighths );                // play 'i'th note: 'freq' sqr wave tone for 'nEighths' 128 msec buffers
extern void audStopAudio( void );                       // signal playback loop to stop
extern void audStartRecording( FILE *outFP, MsgStats *stats );  // start recording into file -- TBV2_REV1 version
extern void audRequestRecStop( void );                  // signal record loop to stop
extern void audPauseResumeAudio( void );                // signal playback loop to request Pause or Resume
extern void audPlaybackDn( void );                      // handle end of buffer event
#ifdef _SQUARE_WAVE_SIMULATOR
extern void audSquareWav( int nsecs, int hz );          // square: 'nsecs' seconds of 'hz' squareWave
#endif
extern void audLoadBuffs( void );                       // pre-load playback data (from mediaThread)
extern void audSaveBuffs( void );                       // save recorded buffers (called from mediaThread)
extern void audPlaybackComplete( void );                // playback complete (from mediaThread)
extern void audRecordComplete( void );                  // last buff recorded, finish save

//DEBUG
extern bool playWave( const char *fname );              // play WAV file, true if started

#endif /* AUDIO_H */
