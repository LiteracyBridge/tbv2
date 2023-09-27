#ifndef AUDIO_H
#define AUDIO_H

#include "main.h"
#include "Driver_SAI.h"
#include "wav.h"

#include "codecI2C.h"

#include "rl_fs_lib.h"

extern "C" ARM_DRIVER_SAI Driver_SAI0;      // from I2S_stm32F4xx.c

// constants defined in mediaplayer.c & referenced in audio.c
extern const int CODEC_DATA_SEND_DONE;      // signal sent by SAI callback when buffer TX done
extern const int CODEC_PLAYBACK_DONE;       // signal sent by SAI callback when playback complete
extern const int CODEC_DATA_RECEIVE_DONE;   // signal sent by SAI callback when a buffer has been filled
extern const int CODEC_RECORD_DONE;         // signal sent by SAI callback when recording stops
extern const int MEDIA_PLAY_EVENT;
extern const int MEDIA_RECORD_START;


// Audio encoding formats
typedef enum AudioFormat {
    kAudioNone,
    kAudioWav,
    kAudioMp3,
    kAudioTone           // sqrWAV generated tone
}                            AudioFormat_t;

extern const char * const preferredAudioExtensions[];

typedef enum {        // pnRes_t            -- return codes from PlayNext
    pnDone, pnPaused, pnPlaying
}                            pnRes_t;

typedef enum {        // BuffState          -- audio buffer states
    bFree, bAlloc, bEmpty, bFull, bDecoding, bPlaying, bRecording, bRecorded
}                            BuffState;

//typedef struct {      // Buffer_t           -- audio buffer
//    BuffState bufState;     // Only tested "free" or "not free"
//    uint32_t  firstSample;  // Never read, except for "dbgEvt"
//    uint32_t  cntBytes;     // Never read. or timestamp for bRecorded
//    uint16_t  *data;         // buffer of 16bit samples
//}                            Buffer_t;

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

typedef enum PlaybackType {      // PlaybackType_t
    kPlayTypeNone,          // reset value
    kPlayTypePackagePrompt, // package prompt
    kPlayTypeSystemPrompt,  // system prompt
    kTypeSubject,      // subject name
    kPlayTypeInvitation,    // subject invitation
    kPlayTypeMessage,       // subject message
    kPlayTypeRecording,     // recorded file
    kPlayTypeTone           // tone
} PlaybackType_t;


// functions called from mediaplayer.c
extern void audInitialize( void );                      // sets up I2S
extern void audPowerDown( void );                       // shut down and reset everything
extern MediaState audGetState( void );                  // => Ready or Playing or Recording
extern uint16_t audAdjVolume( int16_t adj );            // change volume by 'adj'  0..10
extern int32_t audPlayPct( void );                      // => current playback pct of file
extern void audAdjPlayPos( int32_t adj );               // shift current playback position +/- by 'adj' seconds
extern void audAdjPlaySpeed( int16_t adj );             // change playback speed by 'adj' * 10%
extern void audPlayAudio( const char *audioFileName, MsgStats *stats,
                          PlaybackType_t pbType, int32_t playCounter );        // start playing from file eventId=sys/pkg/msg/nm/pr/rec
extern void audPlayTone( int i, int freq,
                         int nEighths, int32_t playCounter );                // play 'i'th note: 'freq' sqr wave tone for 'nEighths' 128 msec buffers
extern void audStopAudio( void );                       // signal playback loop to stop
extern bool audStartRecording( const char *recordFilename, MsgStats *stats );  // start recording into file
extern void audFinalizeRecord( bool keep);
extern void audRequestRecStop( void );                  // signal record loop to stop
extern void audPauseResumeAudio( void );                // signal playback loop to request Pause or Resume
extern void audPlaybackDn( void );                      // handle end of buffer event
extern void audSquareWav( int nsecs, int hz );          // square: 'nsecs' seconds of 'hz' squareWave
extern void audLoadBuffs( void );                       // pre-load playback data (from mediaThread)
extern void audSaveBuffs( void );                       // save recorded buffers (called from mediaThread)
extern void audPlaybackComplete( void );                // playback complete (from mediaThread)
extern void audRecordComplete( void );                  // last buff recorded, finish save

//DEBUG
extern bool playWave( const char *fname );              // play WAV file, true if started

#endif /* AUDIO_H */
