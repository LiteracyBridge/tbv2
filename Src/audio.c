// audio.c  -- playback & record functions called by mediaplayer

#include "audio.h"
#include "encAudio.h"
#include "mediaplayer.h"
#include "controlmanager.h"
#include "mp3toStream.h"
#include "buffers.h"
#include "fileOps.h"

extern TBook_t TBook;

const char * const preferredAudioExtensions[] = {".mp3", ".wav"};

const int WaveHdrBytes          = 44;           // length of .WAV file header
const int SAMPLE_RATE_MIN       = 8000;
const int SAMPLE_RATE_MAX       = 48000;

const int kAudioQueueSize   = 3;            // Max read-ahead play/rec buffers.
const int kWriteQueueSize     = kBufferPoolSize;  // Max recording buffers waiting to be written.

const int MIN_AUDIO_PAD         = 37;           // minimum 0-padding samples at end of file, to clear codec pipeline before powering down

const int RECORD_SAMPLE_RATE    = 16000;        // AIC3100  7.3.8.2:  ADC, Mono, 8 kHz, DVDD = 1.8 V, AVDD = 3.3 V  AOSR=128,  PRB_R4 (example)
const int RECORD_SEC            = 30;
const int WAV_DATA_SIZE         = RECORD_SAMPLE_RATE * 2 * RECORD_SEC;    // 30sec at 8K 16bit samples

const int WAV_FILE_SIZE         = WAV_DATA_SIZE + sizeof(WavFileHeader_t);
            
extern volatile char                   mRecordFilePath[MAX_PATH];

const WavFileHeader_t newWavHeader =  {
    0x46464952,             //  riffId = 'RIFF'
    0,                      //  waveSize -- TBD
    0x45564157,             //  waveId = 'WAVE'
    0x20746D66,             //  chunkId(fmt) = 'fmt '
    16,                     //  chunkSize(fmt) = 16, sizeof(WavFmtData_t)
    1,                      //  formatCode = 1 -> PCM
    1,                      //  numChannels = 1
    RECORD_SAMPLE_RATE,     //  samplesPerSecond = 8000
    RECORD_SAMPLE_RATE*2,   //  bytesPerSecond = 2*SampleRate
    2,                      //  blockSize = 2
    16,                     //  bitsPerSample = 16 = blocksize * 8
    0x61746164,             //  chunkId(data) = 'data'
    WAV_DATA_SIZE,          //  chunkSize(data) -- for 30sec of 16K 16bit mono};
};

// Audio state, both playing and recording.
static struct {       // PlaybackFile_t     -- audio state block
    AudioFormat_t           audioFormat;
    WavFmtData_t            fmtData;        // .wav specific info
    uint32_t                dataStartPos;   // file pos of data chunk (first audio in file)
    FILE *                  audF;           // stream for data
    bool                    mp3Eof;

    uint32_t                samplesPerSec;  // sample frequency
    uint32_t                bytesPerSample; // eg. 4 for 16bit stereo
    uint32_t                nSamples;       // total samples in file
    uint32_t                msecLength;     // length of file in msec

    uint32_t                tsOpen;         // timestamp at start of fopen
    uint32_t                tsPlay;         // timestamp at start of curr playback or resume
    uint32_t                tsPause;        // timestamp at pause
    uint32_t                tsResume;       // timestamp at resume (or start)
    uint32_t                tsRecord;       // timestamp at recording start

    playback_state_t        state;          // current (overly detailed) playback state
    PlaybackType_t          playbackTyp;
    int32_t                 playSubj;
    int32_t                 playMsg;
    uint32_t                nPauses;
    uint32_t                msFwd;
    uint32_t                msBack;

    bool                    monoMode;       // true if data is 1 channel (duplicate on send)
    uint32_t                nPerBuff;       // samples per buffer (always stereo)
    uint32_t                LastError;      // last audio error code
    uint32_t                ErrCnt;         // # of audio errors
    MsgStats *              stats;          // statistics to go to logger

    Buffer_t *              audioQueue[ kAudioQueueSize ];// pointers to playback/record buffers (for double buffering)
    Buffer_t *              writeQueue[ kWriteQueueSize ];// pointers to save buffers (waiting to write to file)

    // playback
    int32_t                 buffNum;        // idx in file of buffer currently playing
    uint32_t                nLoaded;        // samples loaded from file so far
    uint32_t                nPlayed;        // # samples in completed buffers
    uint32_t                msPlayed;       // elapsed msec playing file
    bool                    audioEOF;       // true if all sample data has been read
    //  uint32_t              msPos;          // msec position in file

    // recording
    const char *            recordFilename; // Pointer to mediaplayer's file name buffer.
    uint32_t                samplesPerBuff; // number of mono samples in kBufferSize (stereo) buffer
    uint32_t                msRecorded;     // elapsed msec recording file
    uint32_t                nRecorded;      // n samples in filled record buffers
    uint32_t                nSaved;         // recorded samples sent to file so far
    bool                    needHeader;     // True when we need to add the WAV header to the next buffer;
    bool                    privateFeedback;

    // simulated square wave data
    bool                    SqrWAVE;        // T => wavHdr pre-filled to generate square wave
    int32_t                 sqrSamples;     // samples still to send
    uint32_t                sqrHfLen;       // samples per half wave
    int32_t                 sqrWvPh;        // position in wave form: HfLen..0:HI 0..-HfLen: LO
    uint16_t                sqrHi;          //  = 0x9090;
    uint16_t                sqrLo;          //  = 0x1010;
    uint32_t                tuneSamples;    // accumulate total tune samples
    uint32_t                tunePlayed;     // total samples played
    uint32_t                tuneMsec;     // accumulate total tune msec
    uint32_t                tsTuneStart;    // timestamp at start of 1st tune note
} pSt;


static MsgStats *           sysStats;
const int SAI_OutOfBuffs    = 1 << 8;     // bigger than ARM_SAI_EVENT_FRAME_ERROR
static int minFreeBuffs = kBufferPoolSize;


// forward decls for internal functions
Buffer_t *          allocBuff(const char * owner);                // get a buffer for use
void                printAudSt( void );                           // DBG: display audio state
Buffer_t *          loadBuff( void );                             // read next block of audio into a buffer
void                audPlaybackComplete( void );                  // shut down after completed playback
void                audRecordComplete( void );                    // shut down recording after stop request
void                setMp3Pos(int msec);                            // Seek to time in MP3.
void                setWavPos( int msec );                        // set wav file playback position
void                startPlayback( void );                        // preload & play
void                haltPlayback( void );                         // just shutdown & update timestamps
bool                playWave( const char *fname );                // play a WAV file
bool                playMp3( const char *fname );                 // decode and play a MP3 file
void                saiEvent( uint32_t event );                   // called to handle transfer complete
void                startRecord( void );                          // preload buffers & start recording
void                haltRecord( void );                           // stop audio input
void                saveBuff( Buffer_t * pB );                    // write pSt.Buff[*] to file
void                freeBuffs( void );                            // release all pSt.audioQueue[]
void                testSaveWave( void );

void                fillSqrBuff( Buffer_t * pB, int nSamp, bool stereo );
// EXTERNAL functions


/**
 * Initialize the audio module. Once per execution.
 * Allocate buffers, set up SAI (Serial Audio Interface).
 */
void audInitialize( void ) { 
    pSt.state = pbIdle;
    initBufferPool();
    fileOpInit();
    encUfAudioInit();
    pSt.privateFeedback = encUfAudioEnabled;

    assert( sizeof( newWavHeader ) == WaveHdrBytes );

    sysStats = tbAlloc( sizeof( MsgStats ), "sysStats" );
    pSt.SqrWAVE = false;      // unless audSquareWav is called
    // Initialize I2S & register callback
    //  I2S_TX_DMA_Handler() overides DMA_STM32F10x.c's definition of DMA1_Channel5_Handler
    //    calls I2S_TX_DMA_Complete, which calls saiEvent with DMA events
    Driver_SAI0.Initialize( &saiEvent );
}


void audSquareWav( int nEighths, int hz ) {           // preload wavHdr & setup to load buffers with square wave
    pSt.SqrWAVE = true;     // set up to generate 'hz' square wave
    // header for 30 secs of mono 8K audio
    memcpy( &pSt.fmtData, &newWavHeader.fmtData, sizeof( WavFmtData_t ));
    pSt.fmtData.samplesPerSecond = 16000;                                    // switch to 16k
    pSt.fmtData.bytesPerSecond   = pSt.fmtData.samplesPerSecond * 2;

    pSt.sqrHfLen = pSt.fmtData.samplesPerSecond / ( hz * 2 );   // samples per half wave
    pSt.sqrWvPh  = 0;                            // start with beginning of LO
    pSt.sqrHi    = 0xAAAA;
    pSt.sqrLo    = 0x0001;

    if ( nEighths == 0 ) nEighths = 1;
    pSt.sqrSamples = nEighths * kBufferSize / 2;    // number of samples to send = 128ms * nEighths == 1/2 buffer at 16khz

    pSt.fmtData.numChannels   = 1;
    pSt.fmtData.bitsPerSample = 16;
    //pSt.fmtData->SubChunk2Size =  pSt.sqrSamples * 2; // bytes of data
}

/**
 * Initialize the "pSt" structure for playback of a single audio file.
 */
void audInitState( void ) {
    for (int i=0; i<kAudioQueueSize; ++i) {
        if (pSt.audioQueue[i] != NULL) {
            errLog("Leaking buffers");
            freeBuffs();
            break;
        }
    }

    pSt.audioFormat     = kAudioNone;
    pSt.playbackTyp = kPlayTypeNone;
    pSt.playSubj    = -1;
    pSt.playMsg     = -1;
    pSt.nPauses     = 0;
    pSt.msFwd       = 0;
    pSt.msBack      = 0;
    memset( &pSt.fmtData, 0, sizeof( WavFmtData_t ));
    if ( pSt.state != pbIdle && pSt.state != pbDone ) {  // starting new operation before previous completes
        dbgLog( "! audInit: in state %d \n", pSt.state );
    }
    if ( numBuffersInUse() != 0 )
        dbgLog( "! audInit: missing buffs %d \n", numBuffersInUse() );
    if ( pSt.audF != NULL ) {
        dbgLog( "! audInit close audF\n" );
        tbFclose( pSt.audF );  //int res = fclose( pSt.audF );
        //if ( res!=fsOK ) dbgLog( "! audInit fclose => %d \n", res );
        pSt.audF = NULL;
    }

    pSt.samplesPerSec  = 1;    // avoid /0 if never set
    pSt.bytesPerSample = 0;   // eg. 4 for 16bit stereo
    pSt.nSamples       = 0;         // total samples in file
    pSt.msecLength     = 0;       // length of file in msec
    pSt.samplesPerBuff = 0;   // num mono samples per stereo buff at curr freq

    pSt.tsOpen   = tbTimeStamp();   // before fopen
    pSt.tsPlay   = 0;
    pSt.tsPause  = 0;
    pSt.tsResume = 0;
    pSt.tsRecord = 0;

    pSt.state    = pbOpening;          // state at initialization
    pSt.monoMode = false;              // if true, mono samples will be duplicated to transmit
    pSt.nPerBuff = 0;                  // # source samples per buffer

    pSt.LastError = 0;                  // last audio error code
    pSt.ErrCnt    = 0;                  // # of audio errors
    pSt.stats     = sysStats;                 // gets overwritten for messages
    memset( pSt.stats, 0, sizeof( MsgStats ));

    // init all slots-- only Buffs[nPlayBuffs] & SvBuffs[kWriteQueueSize] will be used
    memset(pSt.audioQueue, 0, sizeof(pSt.audioQueue));
    memset(pSt.writeQueue, 0, sizeof(pSt.writeQueue));

    // playback
    pSt.buffNum  = -1;       // idx in file of buffer currently playing
    pSt.nLoaded  = 0;        // samples loaded from file so far
    pSt.nPlayed  = 0;        // # samples in completed buffers
    pSt.msPlayed = 0;        // elapsed msec playing file
    pSt.audioEOF = false;    // true if all sample data has been read

    // recording
    pSt.msRecorded = 0;      // elapsed msec while recording
    pSt.nRecorded  = 0;        // n samples in filled record buffers
    pSt.nSaved     = 0;      // recorded samples sent to file so far

    // don't initialize SqrWAVE fields
}


MediaState audGetState( void ) {                          // => Ready or Playing or Recording
    switch (pSt.state) {
        case pbDone:        // I2S of audio done
        case pbIdle:
            return Ready;   // not playing anything

        case pbOpening:     // calling fopen
        case pbLdHdr:       // fread header
        case pbGotHdr:      // fread hdr done
        case pbFilling:     // fread 1st data
        case pbFull:        // fread data done
        case pbPlaying:     // I2S started
        case pbPlayFill:    // fread data under I2S
        case pbLastPlay:    // fread data got 0
        case pbFullPlay:    // fread data after I2S done
        case pbPaused:
            return Playing;

        case pbRecording:   // recording in progress
        case pbWroteHdr:
        case pbRecPaused:
        case pbRecStop:
            return Recording;
    }
    return Ready;
}


// Gets the position in the file on a scale of 0-100
int32_t audPlayPct( void ) {                           // => current playback pct of file
    if ( pSt.state == pbDone ) return 100;  // completed

    if ( pSt.state == pbPlaying ) {
        uint32_t now    = tbRtcStamp();
        uint32_t played = pSt.msPlayed + ( now - pSt.tsResume );  // elapsed since last Play
        return played * 100 / pSt.msecLength;
    }
    if ( pSt.state == pbPaused )
        return pSt.msPlayed * 100 / pSt.msecLength;
    return 0;
}

// Seek
// TODO: argument in ms, not seconds
void audAdjPlayPos( int32_t adj ) {                 // shift current playback position +/- by 'adj' seconds
    if ( pSt.state == pbPlaying )
        audPauseResumeAudio();
    if ( pSt.state != pbPaused ) return;            // weren't playing or paused

    if (pSt.audioFormat == kAudioWav) {
        // TODO: This is not correct; msPlayed is *cumulative* time playing.
        int newMs = pSt.msPlayed + adj * 1000;
        if (adj > 0) pSt.msFwd += adj * 1000;
        if (adj < 0) pSt.msBack -= adj * 1000;
        logEvtNINI("adjPos", "bySec", adj, "newMs", newMs);
        printf("Adjust position by %d seconds, ms %d -> %d\n", adj, pSt.msPlayed, newMs);
        LOG_AUDIO_PLAY_JUMP(pSt.msecLength, pSt.msPlayed, adj * 1000);
        setWavPos(newMs);
    } else if (pSt.audioFormat == kAudioMp3) {
        int newMs = streamMp3GetLatestTime() + adj * 1000;
        if (adj > 0) pSt.msFwd += adj * 1000;
        if (adj < 0) pSt.msBack -= adj * 1000;
        logEvtNINI("adjPos", "bySec", adj, "newMs", newMs);
        printf("Adjust position by %d seconds, ms %d -> %d\n", adj, pSt.msPlayed, newMs);
        LOG_AUDIO_PLAY_JUMP(pSt.msecLength, pSt.msPlayed, adj * 1000);
        setMp3Pos(newMs);
    }
}


void audAdjPlaySpeed( int16_t adj ) {               // change playback speed by 'adj' * 10%
    logEvtNI( "adjSpd", "byAdj", adj );
}

/**
 * Given a filename (of an audio file), look for an audio file with the preferred extension.
 * The filename provided may have an extension, but does not need to, but the buffer must
 * be large enough to accommodate a ".xyz" extension.
 * @param fnameBuffer A filename, with an extension or space for one. Will be modified with
 *      the found most-preferred extension (.mp3, .wav).
 * @return A pointer to the extension, in the buffer, if a preferred file type was found, and
 *     NULL otherwise.
 */
const char * findAudioFileByExtension(  char *fnameBuffer ) {
    char *fileExt = strrchr(fnameBuffer, '.');
    if (fileExt == NULL) {
        // If no extension, find where it will be.
        fileExt = fnameBuffer + strlen(fnameBuffer);
    }
    for (int extIx=0; extIx<=sizeof(preferredAudioExtensions)/sizeof(preferredAudioExtensions[0]); ++extIx) {
        strcpy(fileExt, preferredAudioExtensions[extIx]);
        if (fexists(fnameBuffer))
            return fileExt;
    }
    return NULL;
}

void audPlayAudio( const char *audioFileName, MsgStats *stats, PlaybackType_t typ ) { // start playing from file
    audInitState();
    pSt.stats = stats;
    stats->Start++;
    pSt.playbackTyp = typ;        // current type of file being played
    if ( typ == kPlayTypeMessage ) {
        pSt.playSubj = TBook.iSubj;
        pSt.playMsg  = TBook.iMsg;
    }

    char fnameBuffer[MAX_PATH];
    strcpy(fnameBuffer, audioFileName);
    const char *fileExt = findAudioFileByExtension(fnameBuffer);

    if (!fileExt) {
        audStopAudio();
        return;
    }

    pSt.SqrWAVE = false;

    if ( strcasecmp( fileExt, ".wav" ) == 0 ) {
        pSt.audioFormat = kAudioWav;
        if ( playWave( fnameBuffer )) {
            // Let CSM know that playback has started
            sendEvent( AudioStart, 0 );
        }
    } else if ( strcasecmp( fileExt, ".mp3" ) == 0 ) {
        pSt.audioFormat = kAudioMp3;
        if ( playMp3( fnameBuffer )) {
            // Let CSM know that playback has started
            sendEvent( AudioStart, 0 );
        }
    } else {
        tbErr( "NYI file format" );
    }
}

void audPlayTone( int i, int freq, int nEighths ) {   // play 'i'th note: 'freq' sqr wave tone for 'nEighths' 128 msec buffers
    if ( i == 0 ) {    // first note of tune
        pSt.tuneSamples = 0;    // total samples in tune
        pSt.tuneMsec    = 0;       // length of tune in msec
        pSt.tunePlayed  = 0;     // total samples played
    } else
        pSt.tunePlayed += pSt.nSamples;  // completed previous note

    audInitState();
    audSquareWav( nEighths, freq ); // set up to play tone: pSt.SQRwave==true
    pSt.playbackTyp = kPlayTypeTone;
    pSt.audioFormat     = kAudioTone;
    pSt.nSamples    = pSt.sqrSamples;
    pSt.msecLength  = pSt.nSamples * 1000 / pSt.samplesPerSec;

    startPlayback();     // power up codec, preload buffers from current file pos, & start I2S transfers

    pSt.tuneSamples += pSt.nSamples;    // total samples in tune
    pSt.tuneMsec += pSt.msecLength;     // length of tune in msec
    if ( i == 0 ) {
        pSt.tsTuneStart = pSt.tsPlay;
        sendEvent( AudioStart, 0 );   // Let CSM know that playback has started
    }
}

void logPlayMsg( void ) {
    if ( pSt.playbackTyp != kPlayTypeMessage ) return;

    logEvtFmt( "MsgDone", "S:%d, M:%d, L_ms:%d, P_ms:%d, nPaus:%d, Fwd_ms:%d, Bk_ms:%d", pSt.playSubj, pSt.playMsg,
               pSt.msecLength, pSt.msPlayed, pSt.nPauses, pSt.msFwd, pSt.msBack );
}

void audPlayDone() {                                // close, report errs, => idle
    if ( pSt.audF != NULL ) { // normally closed by loadBuff
        tbFclose( pSt.audF );  //int res = fclose( pSt.audF );
        FileSysPower( false );    // power down SDIO after playback
        //if ( res!=fsOK ) dbgLog( "! PlyDn fclose => %d \n", res );
        pSt.audF = NULL;
    }
    if ( pSt.ErrCnt > 0 ) {
        dbgLog( "! %d audio Errors, Last=0x%x \n", pSt.ErrCnt, pSt.LastError );
        logEvtNINI( "PlyErr", "cnt", pSt.ErrCnt, "last", pSt.LastError );
        LOG_AUDIO_PLAY_ERRORS( pSt.ErrCnt, pSt.LastError );
    }
    pSt.state = pbIdle;
}


void audStopAudio( void ) {                         // abort any leftover operation
    MediaState st = audGetState();
    if ( st == Ready ) return;

    if ( st == Recording ) {
        logEvtNI( "CutRec", "state", pSt.state );
        if ( pSt.state == pbRecording || pSt.state == pbRecStop ) {
            dbgLog( "8 audStop Rec %x\n", pSt.audF );
            haltRecord();   // shut down dev, update timestamps
            audRecordComplete();  // close file, report errors
        }
        freeBuffs();
        pSt.state = pbIdle;
        return;
    }

    if ( st == Playing ) {
        PlaybackType_t playtyp = pSt.playbackTyp;   // remember across reset

        if ( pSt.state == pbPlaying ) {
            haltPlayback();            // stop device & update timestamps
            audPlayDone();
        }
        freeBuffs();
        pSt.stats->Left++;        // update stats for interrupted operation

        /////////////////////////////////////////////////////////////////////////////////
        /////////////////////////////////////////////////////////////////////////////////
        // TODO: Fix the problem of 0-length files, don't just work around it!
        // msecLength is somehow sometimes getting a value of zero. Don't divide by zero.
        int pct = 100 * pSt.msPlayed /( pSt.msecLength ? pSt.msecLength : 1 );     // already stopped, so calc final pct
        /////////////////////////////////////////////////////////////////////////////////
        /////////////////////////////////////////////////////////////////////////////////
        dbgLog( "D audStop play %d \n", pct );
        pSt.stats->LeftSumPct += pct;
        if ( pct < pSt.stats->LeftMinPct ) pSt.stats->LeftMinPct = pct;
        if ( pct > pSt.stats->LeftMaxPct ) pSt.stats->LeftMaxPct = pct;
        logEvtNININI( "CutPlay", "ms", pSt.msPlayed, "pct", pct, "nSamp", pSt.nPlayed );
        if ( playtyp == kPlayTypeMessage )
            LOG_AUDIO_PLAY_STOP( pSt.msecLength, pSt.msPlayed, pct, pSt.playSubj, pSt.playMsg );
        logPlayMsg();
    }
}

bool audStartRecording( const char * fname, MsgStats *stats ) {  // start recording into file
    //  EventRecorderEnable( evrEAO,    EvtFsCore_No,  EvtFsCore_No );
    //  EventRecorderEnable( evrEAO,    EvtFsFAT_No,   EvtFsFAT_No );
    //  EventRecorderEnable( evrEAO,    EvtFsMcSPI_No, EvtFsMcSPI_No );
    //  EventRecorderEnable( evrEAOD,     TBAud_no, TBAud_no );
    //  EventRecorderEnable( evrEAOD,     TBsai_no, TBsai_no );             //SAI:  codec & I2S drivers

    //  testSaveWave( );    //DEBUG

    FILE *audF = tbOpenWriteBinary( fname );
    if ( audF != NULL ) {
        dbgLog( "8 Rec fnm: %s \n", fname );
    } else {
        printf( "Cannot open record file to write\n\r" );
        return false;
    }
    pSt.recordFilename = fname;
    // Write the recording props file here. Heavy lifting of encryption will be done on another thread.
    saveAuxProperties((char *) mRecordFilePath );

    minFreeBuffs = numBuffersAvailable();
    audInitState();
    pSt.audioFormat = kAudioWav;
    
    dbgEvt( TB_recWv, 0, 0, 0, 0 );
    pSt.stats = stats;
    stats->RecStart++;

    pSt.audF = audF;
    memcpy( &pSt.fmtData, &newWavHeader.fmtData, sizeof( WavFmtData_t ));

    // Create key file, prepare for encryption.
    if (pSt.privateFeedback) {
        encUfAudioPrepare((char*)mRecordFilePath);
    }

    pSt.needHeader  = true;
    int audioFreq = pSt.fmtData.samplesPerSecond;
    if (( audioFreq < SAMPLE_RATE_MIN ) || ( audioFreq > SAMPLE_RATE_MAX ))
        errLog( "bad audioFreq, %d", audioFreq );
    pSt.samplesPerSec = audioFreq;

    Driver_SAI0.PowerControl( ARM_POWER_FULL );   // power up audio
    pSt.monoMode = ( pSt.fmtData.numChannels == 1 );

    uint32_t ctrl = ARM_SAI_CONFIGURE_RX |
                    ARM_SAI_MODE_SLAVE |
                    ARM_SAI_ASYNCHRONOUS |
                    ARM_SAI_PROTOCOL_I2S |
                    ARM_SAI_MONO_MODE |
                    ARM_SAI_DATA_SIZE( 16 );
    Driver_SAI0.Control( ctrl, 0, pSt.samplesPerSec );  // set sample rate, init codec clock

    // = 4 bytes per stereo sample -- same as ->BlockAlign
    pSt.bytesPerSample = pSt.fmtData.numChannels * pSt.fmtData.bitsPerSample / 8;
    pSt.nSamples       = 0;
    pSt.msecLength     = pSt.nSamples * 1000 / pSt.samplesPerSec;
    pSt.samplesPerBuff = kBufferWords / 2;
    startRecord();
    return true;
}


void audRequestRecStop( void ) {                    // signal record loop to stop
    if ( pSt.state == pbRecording ) {
        //dbgLog( "8 reqRecStp \n");
        pSt.state = pbRecStop;    // handled by SaiEvent on next buffer complete
    } else if ( pSt.state == pbRecPaused ) {
        ledFg( NULL );
        audRecordComplete();      // already paused-- just finish up
    }
}


void audPauseResumeAudio( void ) {                  // signal playback to request Pause or Resume
    int      pct = 0;
    uint32_t ctrl;
    switch (pSt.state) {
        case pbPlaying:
            if ( pSt.SqrWAVE ) break;    // ignore pause while playTone

            // pausing
            haltPlayback();   // shut down device & update timestamps
            pSt.state = pbPaused;
            ledFg( TB_Config->fgPlayPaused ); // blink green: while paused
            // subsequent call to audPauseResumeAudio() will start playing at pSt.msPlayed msec
            pct = audPlayPct();
            pSt.nPauses++;
            dbgEvt( TB_audPause, pct, pSt.msPlayed, pSt.nPlayed, 0 );
            dbgLog( "D pausePlay at %d ms \n", pSt.msPlayed );
            logEvtNININI( "plPause", "ms", pSt.msPlayed, "pct", pct, "nS", pSt.nPlayed );
            LOG_AUDIO_PLAY_PAUSE( pSt.msecLength, pSt.msPlayed, 100 * pSt.msPlayed / pSt.msecLength );
            pSt.stats->Pause++;
            break;

        case pbPaused:
            // resuming == restarting at msPlayed
            dbgEvt( TB_audResume, pSt.msPlayed, 0, 0, 0 );
            dbgLog( "D resumePlay at %d ms \n", pSt.msPlayed );
            logEvt( "plResume" );
            LOG_AUDIO_PLAY_RESUME( pSt.msecLength, pSt.msPlayed, 100 * pSt.msPlayed / pSt.msecLength );
            pSt.stats->Resume++;
            if (pSt.audioFormat == kAudioWav) {
                // re-start where we stopped
                setWavPos(pSt.msPlayed);
            } else if (pSt.audioFormat == kAudioMp3) {
                setMp3Pos(pSt.msPlayed);
            }
            break;

        case pbRecording:
            // pausing
            haltRecord();       // shut down device & update timestamps
            audSaveBuffs();     // write all filled writeQueue[], file left open

            dbgEvt( TB_recPause, 0, 0, 0, 0 );
            dbgLog( "8 pauseRec at %d ms \n", pSt.msRecorded );
            ledFg( TB_Config->fgRecordPaused ); // blink red: while paused
            // subsequent call to audPauseResumeAudio() will append new recording
            logEvtNINI( "recPause", "ms", pSt.msRecorded, "nS", pSt.nSaved );
            pSt.stats->RecPause++;
            pSt.state = pbRecPaused;
            break;
        case pbRecPaused:
            // resuming == continue recording
            dbgEvt( TB_recResume, pSt.msRecorded, 0, 0, 0 );
            logEvt( "recResume" );
            dbgLog( "8 resumeRec at %d ms \n", pSt.msRecorded );
            pSt.stats->RecResume++;
            Driver_SAI0.PowerControl( ARM_POWER_FULL );   // power audio back up
            ctrl = ARM_SAI_CONFIGURE_RX |
                            ARM_SAI_MODE_SLAVE |
                            ARM_SAI_ASYNCHRONOUS |
                            ARM_SAI_PROTOCOL_I2S |
                            ARM_SAI_MONO_MODE |
                            ARM_SAI_DATA_SIZE( 16 );
            Driver_SAI0.Control( ctrl, 0, pSt.samplesPerSec );  // set sample rate, init codec clock
            startRecord(); // restart recording
            break;

        default:break;
    }
}


void audSaveBuffs() {                               // called on mediaThread to save full recorded buffers
    dbgEvt( TB_svBuffs, 0, 0, 0, 0 );
    while (pSt.writeQueue[0] != NULL) {    // save and free all filled buffs
        Buffer_t *pB = pSt.writeQueue[0];
        saveBuff( pB );
        for (int i = 0; i < kWriteQueueSize - 1; i++)
            pSt.writeQueue[i]        = pSt.writeQueue[i + 1];
        pSt.writeQueue[kWriteQueueSize - 1] = NULL;
    }
}

/**
 * Ensure that there are kAudioQueueSize filled or filling.
 */
void audLoadBuffs() {
    if (pSt.state != pbPlaying && pSt.state != pbPlayFill && pSt.state != pbPaused) {
      dbgLog("! audLoadBuffs from unexpected state: %d\n", pSt.state);
    }
    for (int i = 0; i < kAudioQueueSize; i++)
        if ( pSt.audioQueue[i] == NULL ) {
            // @TODO: there should never be a non-null buffer following a null buffer.
            pSt.audioQueue[i] = loadBuff();
        }
}

extern bool BootVerboseLog;

void audPlaybackComplete( void ) {                 // shut down after completed playback
    int pct;
    haltPlayback();     // updates tsPause & msPlayed, stops codec, frees audioBuffers
    audPlayDone();      // closes file, shuts down file system.
    if ( pSt.SqrWAVE ) {
        pSt.tunePlayed += pSt.nSamples;
        int msTunePlayed = pSt.tsPause - pSt.tsTuneStart;
        pct = msTunePlayed * 100 / pSt.tuneMsec;
        logEvtNININI( "tuneDn", "ms", pSt.msPlayed, "pct", pct, "nS", pSt.tunePlayed );
    } else {
        ledFg( NULL );        // Turn off foreground LED: no longer playing
        pct = 100; //pSt.msPlayed * 100 / pSt.msecLength;
        // complete, so msPlayed == msecLength
        dbgEvt( TB_audDone, pSt.msPlayed, pct, pSt.msPlayed, pSt.msecLength );
        if ( BootVerboseLog ) logEvtNININI( "playDn", "ms", pSt.msPlayed, "pct", pct, "nS", pSt.nPlayed );
        if ( pSt.playbackTyp == kPlayTypeMessage )
            LOG_AUDIO_PLAY_DONE( pSt.msecLength, pSt.msPlayed, 100 * pSt.msPlayed / pSt.msecLength );
        logPlayMsg();

        // playback completed, report difference between measured & expected elapsed time
        if ( BootVerboseLog ) logEvtNI( "Play_Tim", "dif_ms", pSt.msecLength - pSt.msPlayed );
        pSt.stats->Finish++;
    }
    sendEvent( AudioDone, pct );        // end of file playback-- generate CSM event
}


void audRecordComplete( void ) {                    // last buff recorded, finish saving file
    // redundant call possible-- if mediaplayer audioStop() is called before mediaplayer processes CODEC_RECORD_DONE message from saiEvent
    if ( pSt.state != pbRecStop && pSt.state != pbRecording ) {
        dbgLog( "! audRecComp: state %d \n", pSt.state );
        return;
    }
    freeBuffs();
    audSaveBuffs();     // write all filled writeQueue[]
    tbFclose( pSt.audF );  //int err = fclose( pSt.audF );
    if (pSt.privateFeedback) {
        size_t fSize = fsize(pSt.recordFilename);
        encUfAudioFinalize(fSize);
    }

    pSt.audF = NULL;
    ledFg( NULL );
    dbgLog( "8 RecComp %d errs \n", pSt.ErrCnt );

    dbgEvt( TB_audRecClose, pSt.nSaved, pSt.buffNum, minFreeBuffs, 0 );
    logEvtNINI( "recMsg", "ms", pSt.msRecorded, "nSamp", pSt.nSaved );

    if ( pSt.ErrCnt > 0 ) {
        if ( pSt.LastError == SAI_OutOfBuffs )
            dbgLog( "! %d record errs, out of buffs \n", pSt.ErrCnt );
        else
            dbgLog( "! %d record errs, Lst=0x%x \n", pSt.ErrCnt, pSt.LastError );
        logEvtNINI( "RecErr", "cnt", pSt.ErrCnt, "last", pSt.LastError );
    }
    pSt.state = pbIdle;
}

void audFinalizeRecord( bool keep) {
    // TODO: delete files here.
}

//
// INTERNAL functions

/**
 * Allocates a buffer. If no buffer is available, logs an error.
 */
static Buffer_t *allocBuff( const char * owner ) {                      // get a buffer for use
    Buffer_t * result = allocateBuffer(owner);
    if ( !result )
        errLog( "out of aud buffs" );
    return result;
}


static void startPlayback( void ) {                    // power up codec, preload buffers & start playback
    dbgEvt( TB_stPlay, pSt.nLoaded, 0, 0, 0 );

    Driver_SAI0.PowerControl( ARM_POWER_FULL );   // power up audio
    pSt.monoMode = ( pSt.fmtData.numChannels == 1 );
    pSt.nPerBuff = pSt.monoMode ? kBufferWords / 2 : kBufferWords;   // num source data samples per buffer

    uint32_t ctrl = ARM_SAI_CONFIGURE_TX |
                    ARM_SAI_MODE_SLAVE |
                    ARM_SAI_ASYNCHRONOUS |
                    ARM_SAI_PROTOCOL_I2S |
                    ARM_SAI_DATA_SIZE( 16 );
    Driver_SAI0.Control( ctrl, 0, pSt.samplesPerSec );  // set sample rate, init codec clock, power up speaker and unmute

    pSt.state = pbPlayFill;     // preloading
    audLoadBuffs();         // preload kAudioQueueSize of data -- at curr position ( nLoaded & msPos )

    pSt.state = pbFull;

    if ( !pSt.SqrWAVE ) {    // no LED for playTune -- let CSM decide
        ledFg( TB_Config->fgPlaying );  // Turn ON green LED: audio file playing (except for tunes)
    }
    pSt.state = pbPlaying;
    cdc_SetMute( false );   // unmute

    Driver_SAI0.Send( pSt.audioQueue[0], kBufferWords );   // start first buffer
    Driver_SAI0.Send( pSt.audioQueue[1], kBufferWords );   // & set up next buffer

    pSt.tsResume   = tbRtcStamp();    // 1st buffer has actually started-- start of this playing
    if ( pSt.tsPlay == 0 )
        pSt.tsPlay = pSt.tsResume;    // start of file playback

    // buffer complete for cBuff calls saiEvent, which:
    //   1) Sends Buff2
    //   2) shifts buffs 1,2,..N to 0,1,..N-1
    //   3) signals mediaThread to refill empty slots
}


static void haltPlayback( void ) {                               // shutdown device, free buffs & update timestamps
    int msec;
    if ( pSt.state == pbPlaying || pSt.state == pbDone ) {
        if ( pSt.state == pbPlaying )   // record timestamp before shutdown
            pSt.tsPause = tbRtcStamp();  // if pbDone, was saved by saiEvent

        msec      = ( pSt.tsPause - pSt.tsResume );    // (tsResume == tsPlay, if 1st pause)
        dbgLog( "D Adjusting msPlayed: %d -> %d\n", pSt.msPlayed, pSt.msPlayed + msec );
        pSt.msPlayed += msec;   // update position
        pSt.state = pbPaused;
    }

    cdc_SetMute( true );  // (redundant) mute
    cdc_SpeakerEnable( false ); // turn off speaker
    Driver_SAI0.Control( ARM_SAI_ABORT_SEND, 0, 0 );  // shut down I2S device
    Driver_SAI0.PowerControl( ARM_POWER_OFF );    // power off I2S (& codec & I2C)
    freeBuffs();
}

/**
 * Free any audio buffers in pSt->audioBuffers. If this is done after SAI shutdown no more
 * buffers (should) complete, and no more buffers will be read or enqueued.
 */
static void freeBuffs() {
//    dbgLog( "D freeBuffs, %d free before \n", numBuffersAvailable() );
    for (int i = 0; i < kAudioQueueSize; i++) {    // free any allocated audio buffers
        releaseBuffer( pSt.audioQueue[i] );
        pSt.audioQueue[i] = NULL;
    }
//    dbgLog( "D freeBuffs, %d free after \n", numBuffersAvailable() );
}


/**
 * Write a buffer of recorded data to the recording file. Free the buffer after the
 * write finishes.
 * @param pB The buffer to be written
 */
static void saveBuff(Buffer_t *pB) {                    // save buff to file, then free it
    // collapse to Left channel only
    int nS = kBufferWords / 2;   // num mono samples
    for (int i = 2; i < kBufferWords; i += 2)  // nS*2 stereo samples
        pB[i / 2] = pB[i];  // pB[1]=pB[2], [2]=[4], [3]=[6], ... [(kBufferWords-2)/2]=[kBufferWords-2]

    if (pSt.needHeader) {
        // This overwrites 22 samples with the WAVE header. About 1ms.
        pSt.needHeader = false;
        memcpy(pB, &newWavHeader, sizeof(newWavHeader));
    }

    int nB = fwrite(pB, 1, nS * 2, pSt.audF);   // write nS*2 bytes (1/2 buffer)
    if (nB != nS * 2) 
        tbErr("svBuf write(%d)=>%d", nS * 2, nB);

    pSt.nSaved += nS;     // count of samples saved
    dbgEvt(TB_wrRecBuff, nS, pSt.nSaved, nB, (int) pB);
    if (pSt.privateFeedback) {
        encUfAudioAppend(pB, nS * 2);
    } else {
        releaseBuffer(pB);
    }
}

// preload buffers & start recording
static void startRecord( ) {
    for (int i = 0; i < kAudioQueueSize; i++)     // allocate empty buffers for recording
        pSt.audioQueue[i] = allocBuff("start record");
    minFreeBuffs = min(minFreeBuffs, numBuffersAvailable());

    Driver_SAI0.Receive( pSt.audioQueue[0], kBufferWords );    // set first buffer

    pSt.state        = pbRecording;
    pSt.tsResume     = tbRtcStamp();    // start of this record
    if ( pSt.tsRecord == 0 )
        pSt.tsRecord = pSt.tsResume;   // start of file recording

    ledFg( TB_Config->fgRecording );  // Turn ON red LED: audio file recording
    Driver_SAI0.Receive( pSt.audioQueue[1], kBufferWords );    // set up next buffer & start recording
    dbgEvt( TB_stRecord, (int) pSt.audioQueue[0], (int) pSt.audioQueue[1], 0, 0 );

    // buffer complete ISR calls saiEvent, which:
    //   1) marks Buff0 as Filled
    //   2) shifts buffs 1,2,..N to 0,1,..N-1
    //   2) calls Receive() for Buff1
    //   3) signals mediaThread to save filled buffers
}


static void haltRecord( void ) {                           // ISR callable: stop audio input
    Driver_SAI0.Control( ARM_SAI_ABORT_RECEIVE, 0, 0 ); // shut down I2S device
    Driver_SAI0.PowerControl( ARM_POWER_OFF );          // shut off I2S & I2C devices entirely

    //pSt.state -- changed by caller   pSt.state = pbIdle;   // might get switched back to pbRecPaused
    pSt.tsPause = tbRtcStamp();
    pSt.msRecorded += ( pSt.tsPause - pSt.tsResume );     // (tsResume == tsRecord, if never paused)
    dbgEvt( TB_audRecDn, pSt.msRecorded, 0, 0, 0 );

    ledFg( TB_Config->fgSavingRec );        // Switch foreground LED to saving
}

static void setMp3Pos(int newMs) {
    if (pSt.state != pbPaused)
        tbErr("setMp3Pos not paused");

    freeBuffs();          // release any allocated buffers
    if ( newMs < 0 ) newMs = 0;
    if (newMs > pSt.msecLength) {
        dbgLog( "D Audio seek past EOF (%d of %d); finishing\n", newMs, pSt.msecLength );
        audPlaybackComplete();
        return;
    }
    streamMp3JumpToTime(newMs);

    startPlayback();  // preload & play
}

static void setWavPos( int msec ) {
    if ( pSt.state != pbPaused )
        tbErr( "setWavPos not paused" );

    freeBuffs();          // release any allocated buffers
    if ( msec < 0 ) msec = 0;
    int stSample = msec * pSt.samplesPerSec / 1000;         // sample to start at
    if ( stSample > pSt.nSamples ) {
        printf( "Audio seek past EOF; finishing\n" );
        audPlaybackComplete();
        return;
    }
    pSt.audioEOF = false;   // in case restarting near end

    int fpos = pSt.dataStartPos + pSt.bytesPerSample * stSample;
    printf( "Audio seek to %d (start: %d, BPS: %d, S: %d)\n", fpos, pSt.dataStartPos, pSt.bytesPerSample, stSample );
    fseek( pSt.audF, fpos, SEEK_SET );      // seek wav file to byte pos of stSample

    startPlayback();  // preload & play
}

static void fillSqrBuff( Buffer_t *pB, int nSamp, bool stereo ) { // DEBUG: fill buffer with current square wave
    pSt.sqrSamples -= nSamp;    // decrement SqrWv samples to go

    int      phase = pSt.sqrWvPh;      // start from end of previous
    int      val   = phase > 0 ? pSt.sqrHi : pSt.sqrLo;
    for (int i     = 0; i < nSamp; i++) {
        if ( stereo ) {
            pB[i << 1]         = val;
            pB[( i << 1 ) + 1] = 0;
        } else
            pB[i] = val;
        phase--;
        if ( phase == 0 )
            val = pSt.sqrLo;
        else if ( phase == -pSt.sqrHfLen ) {
            phase = pSt.sqrHfLen;
            val   = pSt.sqrHi;
        }
    }
    pSt.sqrWvPh = phase;    // next starts from here
}

static Buffer_t * fillFromMp3( int *pSamplesReceived ) {
    Buffer_t *pB;
    if (pSt.mp3Eof) {
        *pSamplesReceived = 0;
        pB = allocBuff("empty mp3");
    } else {
        pB = allocateBuffer("decode mp3");
        int bytesRead = streamMp3FillBuffer(pB);
        if (bytesRead > 0) {
            int nSamples = bytesRead / sizeof(int16_t);
            *pSamplesReceived = nSamples;
            transferBuffer(pB, "play mp3");
        } else { // Should be kMp3EOF, but treat anything other than data as EOF
            // The final buffer from the decoder was full (then it got EOF on the mp3 file). This is
            // the first indication we have that the file is finished.
            pSt.mp3Eof = true;
            dbgLog("D Mp3 EOF\n");
            *pSamplesReceived = 0;
            transferBuffer(pB, "unknown mp3");
        }
    }
    return pB;
}

/**
 * TODO: separate along lines of concern. Loading a .wav file should know nothing of playing a tone, and neither
 * should know anything of decoding a .mp3.
 *
 * @return
 */
static Buffer_t *loadBuff(void) {                                  // read next block of audio into a buffer
    if ( pSt.audioEOF )
        return NULL;      // all data & padding finished

    Buffer_t * pB   = NULL;
    int nSamp       = 0;

    if (pSt.audioFormat == kAudioMp3) {
        pB = fillFromMp3(&nSamp);
        if (pB == NULL) return pB;
    } else {
        int len         = pSt.nPerBuff;  // leaves room to duplicate if mono
        if (pSt.SqrWAVE) {     // gen sqWav for tunes
            pB = allocBuff("tone");
            pSt.audioEOF = (len >= pSt.sqrSamples);  // last buffer
            nSamp = pSt.sqrSamples > len ? len : pSt.sqrSamples;
            fillSqrBuff(pB, nSamp, false);
        } else {
            pB = allocBuff("wav");
            nSamp = fread(pB, 1, len * 2, pSt.audF) / 2;    // read up to len samples (1/2 buffer if mono)
            dbgEvt(TB_ldBuff, nSamp, ferror(pSt.audF), pSt.buffNum, 0);
        }
        if (pSt.monoMode) {
            // Duplicate each mono sample as a left + right sample.
            // len==2048:  d[2047] = d[1023], d[2046]=d[1023], d[2045] = d[1022], ... d[2]=d[1], d[1]=d[0]
            nSamp *= 2;
            for (int i = nSamp - 1; i > 0; i--) {
                pB[i] = pB[i / 2];
            }
        }
    }

    if ( nSamp < kBufferWords ) {         // room left in buffer: at EOF, so fill rest with zeros
        bool setEof = nSamp <= kBufferWords - MIN_AUDIO_PAD;
        dbgLog( "D playLast nS=%d %s \n", pSt.nLoaded + nSamp, setEof?"EOF":"" );
        if ( setEof ) {
            pSt.audioEOF = true;              // enough padding, so stop after this
            if (pSt.audF != NULL) {
                tbFclose(pSt.audF);
                pSt.audF = NULL;
                dbgEvt(TB_audClose, nSamp, 0, 0, 0);
                FileSysPower(false);    // power down SDIO after playback reading completes
            }
        }

        for (int i = nSamp; i < kBufferWords; i++)    // clear rest of buffer -- can't change count in DoubleBuffer DMA
            pB[i] = 0;
    }
    pSt.buffNum++;
    pSt.nLoaded += nSamp;     // # loaded
    return pB;
}

// This seems to be another gatekeeper for starting playback
extern bool playWave( const char *fname ) {          // play WAV file, true if started -- (extern for DebugLoop)
    dbgEvtD( TB_playWv, fname, strlen( fname ));

    int dataChunkSize, dataStartPosition;
    int fLen = 0;
    pSt.audioFormat = kAudioWav;
    pSt.state   = pbLdHdr;

    // Open file, start reading the header(s).
    pSt.audF = tbOpenRead( fname );
    if ( pSt.audF == NULL ) {
        errLog( "open wav failed, %s", fname );
        audStopAudio();
        return false;
    }

    // Independent check of the file size.
    fseek( pSt.audF, 0, SEEK_END );
    fLen = ftell( pSt.audF );
    fseek( pSt.audF, 0, SEEK_SET );

    if (!readWavFormat(pSt.audF, fname, &pSt.fmtData, &dataChunkSize, &dataStartPosition)) {
        audStopAudio();
    }

    int audioFreq = pSt.fmtData.samplesPerSecond;
    if (( audioFreq < SAMPLE_RATE_MIN ) || ( audioFreq > SAMPLE_RATE_MAX )) {
        errLog( "bad audioFreq, %d", audioFreq );
        audStopAudio();
        return false;
    }
    pSt.samplesPerSec  = audioFreq;
    // = 4 bytes per stereo sample -- same as ->BlockAlign
    pSt.bytesPerSample = pSt.fmtData.numChannels * pSt.fmtData.bitsPerSample / 8;
    pSt.nSamples       = dataChunkSize / pSt.bytesPerSample;
    pSt.msecLength     = pSt.nSamples / ( pSt.samplesPerSec / 1000 );   // nSamples*1000 can overflow for large messages
    pSt.dataStartPos   = dataStartPosition;    // remember start of wav data, for setWavPos()
    pSt.state          = pbGotHdr;
    if ( pSt.playbackTyp == kPlayTypeMessage )   // details only for content messages
        LOG_AUDIO_PLAY_WAVE( fname, pSt.msecLength, fLen, pSt.samplesPerSec, pSt.monoMode );

    startPlayback();       // power up codec, preload buffers from current file pos, & start I2S transfers
    return true;
}

bool playMp3( const char * fname ) {
    int fileLength;
    if (!streamMp3Start(fname, &pSt.fmtData, &fileLength)) {
        audStopAudio(); // TODO: needed?
        return false;
    }
    pSt.mp3Eof = false;
    pSt.audioFormat = kAudioMp3;
    pSt.state = pbLdHdr;

    pSt.samplesPerSec = pSt.fmtData.samplesPerSecond;
    // = 4 bytes per stereo sample -- same as ->BlockAlign
    pSt.bytesPerSample = pSt.fmtData.numChannels * pSt.fmtData.bitsPerSample / 8;
    // TODO: hopefully we can get this:
    pSt.msecLength = streamMp3GetTotalDuration();
    if (pSt.msecLength == 0) pSt.msecLength = 1; // avoid zerodiv

    // These don't seem to make any sense for an MP3, but may actually work.
    pSt.nSamples = fileLength / pSt.bytesPerSample; // Is this even close?
    pSt.dataStartPos = 0; // Is this?
    printf("len: %d, msec: %d, nsamp: %d, fn: %s\n", fileLength, pSt.msecLength, pSt.nSamples, fname);

    pSt.state          = pbGotHdr;

    startPlayback();
    return true;
}

extern void saiEvent( uint32_t event ) {                   // called by ISR on buffer complete or error -- chain next, or report error -- also DebugLoop
    // calls: Driver_SAI0.Send .Receive .Control haltRecord releaseBuff  freeBuffs
    //  dbgEvt, osEventFlagsSet, tbTimestamp
    dbgEvt( TB_saiEvt, event, 0, 0, 0 );
    const uint32_t ERR_EVENTS = ~( ARM_SAI_EVENT_SEND_COMPLETE | ARM_SAI_EVENT_RECEIVE_COMPLETE );
    if (( event & ARM_SAI_EVENT_SEND_COMPLETE ) != 0 ) {
        if ( pSt.audioQueue[2] != NULL ) {   // have more data to send
            pSt.nPlayed += pSt.nPerBuff;   // num samples actually completed
            Driver_SAI0.Send( pSt.audioQueue[2], kBufferWords );   // set up next buffer

            dbgEvt( TB_audSent, (int) pSt.audioQueue[2], 0, 0, 0 );
            releaseBuffer( pSt.audioQueue[0] );   // buff 0 completed, so free it
            memmove(&pSt.audioQueue[0], &pSt.audioQueue[1], (kAudioQueueSize-1)*sizeof(pSt.audioQueue[0]));
            pSt.audioQueue[kAudioQueueSize - 1] = NULL;
            // signal mediaThread to call audLoadBuffs() to preload empty buffer slots
            dbgEvt( TB_saiTXDN, 0, 0, 0, 0 );
            osEventFlagsSet( mMediaEventId, CODEC_DATA_SEND_DONE );
        } else if ( pSt.audioEOF ) {
            // done, close up shop
            pSt.state   = pbDone;
            pSt.tsPause = tbRtcStamp();   // timestamp end of playback
            Driver_SAI0.Control( ARM_SAI_ABORT_SEND, 0, 0 );  // shut down I2S device, arg1==0 => Abort
            if (pSt.SqrWAVE) {
                // make sure buffers are freed in multi-tone sequences
                freeBuffs();
            }
            dbgEvt( TB_saiPLYDN, 0, 0, 0, 0 );
            osEventFlagsSet( mMediaEventId, CODEC_PLAYBACK_DONE );  // signal mediaThread for rest
        } else {
                dbgLog( "! Audio underrun\n" );
        }
    }

    if (( event & ARM_SAI_EVENT_RECEIVE_COMPLETE ) != 0 ) {
        if ( pSt.state == pbRecording && pSt.audioQueue[2] != NULL ) { // provide next buffer to device as quickly as possible
            pSt.nRecorded += pSt.samplesPerBuff;
            Driver_SAI0.Receive( pSt.audioQueue[2], kBufferWords ); // next buff ready for reception
            // At this point, audioQueue[0] has been filled and is ready to be written, audioQueue[1] is in
            // process of being filled, and we've just given audioQueue[2] to SAI to be filled next.
        }

        pSt.buffNum++;                    // received 1 more buffer
        Buffer_t *pFilled = pSt.audioQueue[0];   // filled buffer

        // shift receive buffer ptrs down
        memmove(&pSt.audioQueue[0], &pSt.audioQueue[1], (kAudioQueueSize-1)*sizeof(pSt.audioQueue[0]));
        pSt.audioQueue[kAudioQueueSize - 1] = NULL;

        for (int i = 0; i < kWriteQueueSize; i++) {
            // Put pFilled into first empty slot of write queue.
            if (pSt.writeQueue[i] == NULL) {
                pSt.writeQueue[i] = pFilled;
                break;
            }
        }
        dbgEvt( TB_saiRXDN, (int) pFilled, 0, 0, 0 );
        osEventFlagsSet( mMediaEventId, CODEC_DATA_RECEIVE_DONE );   // tell mediaThread to save buffer

        if ( pSt.state == pbRecStop ) {
            haltRecord();   // stop audio input
            osEventFlagsSet( mMediaEventId, CODEC_RECORD_DONE );    // tell mediaThread to finish recording
            freeBuffs();            // free buffers waiting to record -- pSt.audioQueue[*]
        } else {
            pSt.audioQueue[kAudioQueueSize - 1] = allocateBuffer("receive");    // add new empty buff for reception
            minFreeBuffs = min(minFreeBuffs, numBuffersAvailable());
            if ( pSt.audioQueue[kAudioQueueSize - 1] == NULL ) { // out of buffers-- try to shut down
                pSt.state     = pbRecStop;
                pSt.LastError = SAI_OutOfBuffs;
                pSt.ErrCnt++;
            }
        }
    }

    if (( event & ERR_EVENTS ) != 0 ) {
        pSt.LastError = event;
        pSt.ErrCnt++;
    }
}

// end audio
