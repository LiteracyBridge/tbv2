// audio.c  -- playback & record functions called by mediaplayer

#include "audio.h"
#include "mediaPlyr.h"
#include "controlmanager.h"

extern TBook_t TBook;

#define _SQUARE_WAVE_SIMULATOR

const int WaveHdrBytes 			= 44;					// length of .WAV file header
const int SAMPLE_RATE_MIN 	=  8000;
const int SAMPLE_RATE_MAX 	= 48000;
const int BuffLen 					= 4096;						// in bytes
const int BuffWds						= BuffLen/2;  		// in shorts
const int nPlyBuffs 				= 3;							// num buffers used in playback & record from pSt.Buff[] (must be <= N_AUDIO_BUFFS )
const int nSvBuffs 					= N_AUDIO_BUFFS;	// max buffers used in pSt.SvBuff[] while waiting to save (must be <= N_AUDIO_BUFFS )

const int MIN_AUDIO_PAD			= 37;							// minimum 0-padding samples at end of file, to clear codec pipeline before powering down

const int RECORD_SAMPLE_RATE = 8000;					// AIC3100  7.3.8.2:  ADC, Mono, 8 kHz, DVDD = 1.8 V, AVDD = 3.3 V  AOSR=128,  PRB_R4 (example)
const int RECORD_SEC 				= 30;
const int WAV_DATA_SIZE			= RECORD_SAMPLE_RATE * 2 * RECORD_SEC; 		// 30sec at 8K 16bit samples

typedef struct {
  WavHeader_t   wavHeader;
  RiffChunk_t   fmtHeader;
  WavFmtData_t  fmtData;
  RiffChunk_t   audioHeader;
} WavFileHeader_t;

const int WAV_FILE_SIZE	 		= WAV_DATA_SIZE + sizeof(WavFileHeader_t);


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
static struct { 			// PlaybackFile_t			-- audio state block
  audType_t 						audType;				// file type to playback
  WavFmtData_t          fmtData;        // .wav specific info
  uint32_t 				dataStartPos;   // file pos of data chunk
  FILE * 								audF;						// stream for data

  uint32_t 							samplesPerSec;	// sample frequency
  uint32_t 							bytesPerSample;	// eg. 4 for 16bit stereo
  uint32_t 							nSamples;				// total samples in file
  uint32_t							msecLength;			// length of file in msec

  uint32_t 							tsOpen;					// timestamp at start of fopen
  uint32_t 							tsPlay;					// timestamp at start of curr playback or resume
  uint32_t 							tsPause;				// timestamp at pause
  uint32_t 							tsResume;				// timestamp at resume (or start)
  uint32_t							tsRecord;				// timestamp at recording start

  playback_state_t  		state;					// current (overly detailed) playback state
  playback_type_t                   playbackTyp;
  int32_t playSubj;
  int32_t playMsg;
  uint32_t nPauses;
  uint32_t msFwd;
  uint32_t msBack;

  bool 									monoMode;				// true if data is 1 channel (duplicate on send)
  uint32_t 							nPerBuff;				// samples per buffer (always stereo)
  uint32_t 							LastError;			// last audio error code
  uint32_t 							ErrCnt;					// # of audio errors
  MsgStats *						stats;					// statistics to go to logger

  Buffer_t *						Buff[ N_AUDIO_BUFFS ];			// pointers to playback/record buffers (for double buffering)
  Buffer_t *						SvBuff[ N_AUDIO_BUFFS ];		// pointers to save buffers (waiting to write to file)

  // playback
  int32_t								buffNum;				// idx in file of buffer currently playing
  uint32_t 							nLoaded;				// samples loaded from file so far
  uint32_t 							nPlayed;				// # samples in completed buffers
  uint32_t 							msPlayed;				// elapsed msec playing file
  bool 									audioEOF;				// true if all sample data has been read
//	uint32_t 							msPos;					// msec position in file

  // recording
  uint32_t 							samplesPerBuff;	// number of mono samples in BuffLen (stereo) buffer
  uint32_t 							msRecorded;			// elapsed msec recording file
  uint32_t							nRecorded;			// n samples in filled record buffers
  uint32_t 							nSaved;					// recorded samples sent to file so far

#ifdef _SQUARE_WAVE_SIMULATOR
  // simulated square wave data
  bool 									SqrWAVE;				// T => wavHdr pre-filled to generate square wave
  int32_t								sqrSamples;			// samples still to send
  uint32_t 							sqrHfLen;				// samples per half wave
  int32_t 							sqrWvPh;				// position in wave form: HfLen..0:HI 0..-HfLen: LO
  uint16_t 							sqrHi;					//  = 0x9090;
  uint16_t 							sqrLo;					//  = 0x1010;
  uint32_t 							tuneSamples;		// accumulate total tune samples
  uint32_t 							tunePlayed;			// total samples played
  uint32_t 							tuneMsec;			// accumulate total tune msec
  uint32_t 							tsTuneStart;		// timestamp at start of 1st tune note
  
#endif
} pSt;


const int MxBuffs 					= 10;					// number of audio buffers in audioBuffs[] pool
static Buffer_t 						audio_buffers[ MxBuffs ];
static MsgStats *						sysStats;
const int SAI_OutOfBuffs		= 1 << 8;			// bigger than ARM_SAI_EVENT_FRAME_ERROR
static int nFreeBuffs = 0, cntFreeBuffs = 0, minFreeBuffs = MxBuffs;


// forward decls for internal functions
void 								initBuffs( void );														// create audio buffers
Buffer_t * 					allocBuff( bool frISR );											// get a buffer for use
void 								releaseBuff( Buffer_t *pB );									// release pB (unless NULL)
void 								printAudSt( void );														// DBG: display audio state
Buffer_t * 					loadBuff( void );															// read next block of audio into a buffer
void 								audPlaybackComplete( void );									// shut down after completed playback
void 								audRecordComplete( void );										// shut down recording after stop request
void								setWavPos( int msec );												// set wav file playback position
void 								startPlayback( void );												// preload & play
void								haltPlayback( void );													// just shutdown & update timestamps
bool 								playWave( const char *fname ); 								// play a WAV file
void 								saiEvent( uint32_t event );										// called to handle transfer complete
void 								startRecord( void );													// preload buffers & start recording
void 								haltRecord( void );														// stop audio input
void								saveBuff( Buffer_t * pB );										// write pSt.Buff[*] to file
void								freeBuffs( void );														// release all pSt.Buff[]
void 								testSaveWave( void );
#ifdef _SQUARE_WAVE_SIMULATOR
void 								fillSqrBuff( Buffer_t * pB, int nSamp, bool stereo );
#endif
// EXTERNAL functions


void 								audInitialize( void ){ 												// allocs buffers, sets up SAI
  pSt.state = pbIdle;
  initBuffs();

  assert(sizeof(newWavHeader) == WaveHdrBytes);

  sysStats = tbAlloc( sizeof( MsgStats ), "sysStats" );
  #ifdef _SQUARE_WAVE_SIMULATOR
  pSt.SqrWAVE = false;			// unless audSquareWav is called
  #endif
  // Initialize I2S & register callback
  //	I2S_TX_DMA_Handler() overides DMA_STM32F10x.c's definition of DMA1_Channel5_Handler
  //    calls I2S_TX_DMA_Complete, which calls saiEvent with DMA events
  Driver_SAI0.Initialize( &saiEvent );
}


#ifdef _SQUARE_WAVE_SIMULATOR
void 								audSquareWav( int nEighths, int hz ){						// preload wavHdr & setup to load buffers with square wave
  pSt.SqrWAVE = true;			// set up to generate 'hz' square wave
  memcpy(&pSt.fmtData, &newWavHeader.fmtData, sizeof(WavFmtData_t));       // header for 30 secs of mono 8K audio
  pSt.fmtData.samplesPerSecond = 16000;                                    // switch to 16k
  pSt.fmtData.bytesPerSecond = pSt.fmtData.samplesPerSecond * 2;      

  pSt.sqrHfLen = pSt.fmtData.samplesPerSecond / (hz*2);		// samples per half wave
  pSt.sqrWvPh = 0;														// start with beginning of LO
  pSt.sqrHi = 0xAAAA;
  pSt.sqrLo = 0x0001;
    
  if ( nEighths==0 ) nEighths = 1;
  pSt.sqrSamples = nEighths * BuffLen/2;		// number of samples to send = 128ms * nEighths == 1/2 buffer at 16khz

  pSt.fmtData.numChannels = 1;
  pSt.fmtData.bitsPerSample = 16;
  //pSt.fmtData->SubChunk2Size =  pSt.sqrSamples * 2; // bytes of data
}
#endif


void								audInitState( void ){													// set up playback State in pSt
  pSt.audType = audUNDEF;
  pSt.playbackTyp = ptUNDEF;
  pSt.playSubj = -1;
  pSt.playMsg = -1;
  pSt.nPauses = 0;
  pSt.msFwd = 0;
  pSt.msBack = 0;
    if ( !pSt.SqrWAVE ){
      // TODO: WAVE_FormatTypeDef
      //memset( pSt.wavHdr, 0x00, WaveHdrBytes );
      memset(&pSt.fmtData, 0, sizeof(WavFmtData_t));
  }
  if ( pSt.state != pbIdle && pSt.state != pbDone ){  // starting new operation before previous completes
      dbgLog( "! audInit: in state %d \n", pSt.state );    
  }
  
  if ( nFreeBuffs != MxBuffs )
    dbgLog( "! audInit: missing buffs %d \n", nFreeBuffs );
  if ( pSt.audF != NULL ){
    dbgLog( "! audInit close audF\n");
    tbCloseFile( pSt.audF );  //int res = fclose( pSt.audF );
    //if ( res!=fsOK ) dbgLog( "! audInit fclose => %d \n", res );
  }
  pSt.audF = NULL;

  pSt.samplesPerSec = 1;		// avoid /0 if never set
  pSt.bytesPerSample = 0; 	// eg. 4 for 16bit stereo
  pSt.nSamples = 0;					// total samples in file
  pSt.msecLength = 0;				// length of file in msec
  pSt.samplesPerBuff = 0;		// num mono samples per stereo buff at curr freq

  pSt.tsOpen = tbTimeStamp();		// before fopen
  pSt.tsPlay = 0;
  pSt.tsPause = 0;
  pSt.tsResume = 0;
  pSt.tsRecord = 0;

  pSt.state 			= pbOpening;					// state at initialization
  pSt.monoMode 		= false;							// if true, mono samples will be duplicated to transmit
  pSt.nPerBuff 		= 0;									// # source samples per buffer

  pSt.LastError 	= 0;									// last audio error code
  pSt.ErrCnt 			= 0;									// # of audio errors
  pSt.stats = sysStats;									// gets overwritten for messages
  memset( pSt.stats, 0, sizeof(MsgStats ));

  for ( int i=0; i<N_AUDIO_BUFFS; i++ ){		// init all slots-- only Buffs[nPlayBuffs] & SvBuffs[nSvBuffs] will be used
    pSt.Buff[i] 	= NULL;
    pSt.SvBuff[i] = NULL;
  }

  // playback
  pSt.buffNum 	= -1;				// idx in file of buffer currently playing
  pSt.nLoaded 	= 0;				// samples loaded from file so far
  pSt.nPlayed 	= 0;				// # samples in completed buffers
  pSt.msPlayed 	= 0;				// elapsed msec playing file
  pSt.audioEOF 	= false;		// true if all sample data has been read

  // recording
  pSt.msRecorded	= 0;			// elapsed msec while recording
  pSt.nRecorded	= 0;				// n samples in filled record buffers
  pSt.nSaved			= 0;			// recorded samples sent to file so far

  // don't initialize SqrWAVE fields
}


MediaState 					audGetState( void ){													// => Ready or Playing or Recording
  switch ( pSt.state ) {
    case pbDone:			// I2S of audio done
    case pbIdle:		return Ready; 		// not playing anything

    case pbOpening:	// calling fopen
    case pbLdHdr:		// fread header
    case pbGotHdr:		// fread hdr done
    case pbFilling:	// fread 1st data
    case pbFull: 		// fread data done
    case pbPlaying: 	// I2S started
    case pbPlayFill:	// fread data under I2S
    case pbLastPlay:	// fread data got 0
    case pbFullPlay:	// fread data after I2S done
    case pbPaused:
        return Playing;

    case pbRecording:	// recording in progress
    case pbWroteHdr:
    case pbRecPaused:
    case pbRecStop:
        return Recording;
  }
  return Ready;
}


// Gets the position in the file on a scale of 0-100
int32_t 						audPlayPct( void ){														// => current playback pct of file
  if ( pSt.state == pbDone ) return 100;	// completed

  if ( pSt.state == pbPlaying ){
    uint32_t now = tbTimeStamp();
    uint32_t played = pSt.msPlayed + (now - pSt.tsResume);	// elapsed since last Play
    return played*100 / pSt.msecLength;
  }
  if ( pSt.state == pbPaused )
    return pSt.msPlayed*100 / pSt.msecLength;
  return 0;
}

// Seek
// TODO: argument in ms, not seconds
void 								audAdjPlayPos( int32_t adj ){									// shift current playback position +/- by 'adj' seconds
  if ( pSt.state==pbPlaying )
    audPauseResumeAudio();

  if ( pSt.audType==audWave ){
    int newMS = pSt.msPlayed + adj*1000;
    if ( adj > 0 ) pSt.msFwd += adj*1000;
    if ( adj < 0 ) pSt.msBack -= adj*1000;
    logEvtNINI( "adjPos", "bySec", adj, "newMs", newMS );
    LOG_AUDIO_PLAY_JUMP(pSt.msecLength, pSt.msPlayed, adj*1000);
    setWavPos( newMS  );
  } else
    tbErr("NYI");
}


void 								audAdjPlaySpeed( int16_t adj ){								// change playback speed by 'adj' * 10%
  logEvtNI( "adjSpd", "byAdj", adj );
}

void 								audPlayAudio( const char* audioFileName, MsgStats *stats, playback_type_t typ ){ // start playing from file
  audInitState();
  pSt.stats = stats;
  stats->Start++;
  pSt.playbackTyp = typ;        // current type of file being played
  if ( typ == ptMsg ){
    pSt.playSubj = TBook.iSubj;
    pSt.playMsg = TBook.iMsg;
  }

  #ifdef _SQUARE_WAVE_SIMULATOR
  pSt.SqrWAVE = false;
  #endif
  if ( strcasecmp( &audioFileName[ strlen( audioFileName )-4 ], ".wav" ) == 0 ){
    pSt.audType = audWave;
    if ( playWave( audioFileName )){
        // Let CSM know that playback has started
        sendEvent( AudioStart, 0 );
    }
  } else
    tbErr("NYI");
}
void								audPlayTone( int i, int freq, int nEighths ){		// play 'i'th note: 'freq' sqr wave tone for 'nEighths' 128 msec buffers
#ifdef _SQUARE_WAVE_SIMULATOR
	if ( i==0 ){		// first note of tune
		pSt.tuneSamples = 0;		// total samples in tune
		pSt.tuneMsec = 0;				// length of tune in msec
		pSt.tunePlayed = 0;			// total samples played
	} else
		pSt.tunePlayed += pSt.nSamples;  // completed previous note
	
    audInitState();
	audSquareWav( nEighths, freq ); // set up to play tone: pSt.SQRwave==true
    pSt.playbackTyp = ptTone;
    pSt.audType = audTONE;  
    pSt.nSamples = pSt.sqrSamples;
    pSt.msecLength = pSt.nSamples*1000 / pSt.samplesPerSec;

    startPlayback();     // power up codec, preload buffers from current file pos, & start I2S transfers

//	playWave( "tone" );  // updates pSt.nSamples & .msecLength
	pSt.tuneSamples += pSt.nSamples;		// total samples in tune
	pSt.tuneMsec += pSt.msecLength;			// length of tune in msec
	if ( i==0 ){
		pSt.tsTuneStart = pSt.tsPlay;
		sendEvent( AudioStart, 0 );		// Let CSM know that playback has started
	}
#endif
}

void logPlayMsg( void ){
    if ( pSt.playbackTyp != ptMsg ) return;
    
    logEvtFmt("MsgDone", "S:%d, M:%d, L_ms:%d, P_ms:%d, nPaus:%d, Fwd_ms:%d, Bk_ms:%d", 
    pSt.playSubj, pSt.playMsg, pSt.msecLength, pSt.msPlayed, pSt.nPauses, pSt.msFwd, pSt.msBack );
}
void								audPlayDone(){																// close, report errs, => idle
  if ( pSt.audF!=NULL ){ // normally closed by loadBuff
    tbCloseFile( pSt.audF );  //int res = fclose( pSt.audF );
    FileSysPower( false );		// power down SDIO after playback
    //if ( res!=fsOK ) dbgLog( "! PlyDn fclose => %d \n", res );
    pSt.audF = NULL;
  }
  if ( pSt.ErrCnt > 0 ){
    dbgLog( "! %d audio Errors, Last=0x%x \n", pSt.ErrCnt, pSt.LastError );
    logEvtNINI( "PlyErr", "cnt", pSt.ErrCnt, "last", pSt.LastError );
    LOG_AUDIO_PLAY_ERRORS(pSt.ErrCnt, pSt.LastError);
  }
  pSt.state = pbIdle;
}


void 								audStopAudio( void ){													// abort any leftover operation
  MediaState st = audGetState();
  if (st == Ready ) return;

  if (st == Recording ){
    logEvtNI( "CutRec", "state", pSt.state );
    if ( pSt.state==pbRecording || pSt.state==pbRecStop ){
      dbgLog( "8 audStop Rec %x\n", pSt.audF );
      haltRecord();		// shut down dev, update timestamps
      audRecordComplete();  // close file, report errors
    }
    freeBuffs();
    pSt.state = pbIdle;
    return;
  }

  if (st == Playing ){
    playback_type_t playtyp = pSt.playbackTyp;   // remember across reset
      
    if (pSt.state==pbPlaying ){
      haltPlayback();			// stop device & update timestamps
      audPlayDone();
    }
    freeBuffs();
    pSt.stats->Left++;		// update stats for interrupted operation
    
    int pct = 100 * pSt.msPlayed / pSt.msecLength;     // already stopped, so calc final pct
    dbgLog( "D audStop play %d \n", pct );
    pSt.stats->LeftSumPct += pct;
    if ( pct < pSt.stats->LeftMinPct ) pSt.stats->LeftMinPct = pct;
    if ( pct > pSt.stats->LeftMaxPct ) pSt.stats->LeftMaxPct = pct;
    logEvtNININI("CutPlay", "ms", pSt.msPlayed, "pct", pct, "nSamp", pSt.nPlayed );
    if ( playtyp==ptMsg )
        LOG_AUDIO_PLAY_STOP( pSt.msecLength, pSt.msPlayed, pct, pSt.playSubj, pSt.playMsg );
    logPlayMsg();
  }
}

void 								audStartRecording( FILE *outFP, MsgStats *stats ){	// start recording into file
//	EventRecorderEnable( evrEAO, 		EvtFsCore_No,  EvtFsCore_No );
//	EventRecorderEnable( evrEAO, 		EvtFsFAT_No,   EvtFsFAT_No );
//	EventRecorderEnable( evrEAO, 		EvtFsMcSPI_No, EvtFsMcSPI_No );
//	EventRecorderEnable( evrEAOD, 		TBAud_no, TBAud_no );
//	EventRecorderEnable( evrEAOD, 		TBsai_no, TBsai_no ); 	 					//SAI: 	codec & I2S drivers

//	testSaveWave( );		//DEBUG

  minFreeBuffs = MxBuffs;
  audInitState();
  pSt.audType = audWave;

  dbgEvt( TB_recWv, 0,0,0,0 );
  pSt.stats = stats;
  stats->RecStart++;

  pSt.audF = outFP;
  memcpy(&pSt.fmtData, &newWavHeader.fmtData, sizeof(WavFmtData_t));

  int cnt = fwrite( &newWavHeader, 1, WaveHdrBytes, pSt.audF );
  if ( cnt != WaveHdrBytes ) tbErr( ".wav header write failed, wrote cnt=%d", cnt );

  pSt.state = pbWroteHdr;
  int audioFreq = pSt.fmtData.samplesPerSecond;
  if ((audioFreq < SAMPLE_RATE_MIN) || (audioFreq > SAMPLE_RATE_MAX))
    errLog( "bad audioFreq, %d", audioFreq );
  pSt.samplesPerSec = audioFreq;

  Driver_SAI0.PowerControl( ARM_POWER_FULL );		// power up audio
  pSt.monoMode = (pSt.fmtData.numChannels == 1);

  uint32_t ctrl = ARM_SAI_CONFIGURE_RX | ARM_SAI_MODE_SLAVE  | ARM_SAI_ASYNCHRONOUS | ARM_SAI_PROTOCOL_I2S | ARM_SAI_DATA_SIZE(16);
  Driver_SAI0.Control( ctrl, 0, pSt.samplesPerSec );	// set sample rate, init codec clock

  pSt.bytesPerSample = pSt.fmtData.numChannels * pSt.fmtData.bitsPerSample/8;  // = 4 bytes per stereo sample -- same as ->BlockAlign
  pSt.nSamples = 0;
  pSt.msecLength = pSt.nSamples*1000 / pSt.samplesPerSec;
  pSt.samplesPerBuff = BuffWds/2;
  startRecord();
}


void 								audRequestRecStop( void ){										// signal record loop to stop
  if ( pSt.state == pbRecording ){
    //dbgLog( "8 reqRecStp \n");
    pSt.state = pbRecStop;		// handled by SaiEvent on next buffer complete
  } else if ( pSt.state == pbRecPaused ){
    ledFg( NULL );
    audRecordComplete();			// already paused-- just finish up
  }
}


void 								audPauseResumeAudio( void ){									// signal playback to request Pause or Resume
  int pct = 0;
  uint32_t ctrl;
  switch ( pSt.state ){
    case pbPlaying:
      #ifdef _SQUARE_WAVE_SIMULATOR
      if ( pSt.SqrWAVE )  break;    // ignore pause while playTone
      #endif
       if ( pSt.playbackTyp!=ptMsg ) break;     // ignore pause in prompts
    
      // pausing '
      haltPlayback();		// shut down device & update timestamps
      pSt.state = pbPaused;
      ledFg( TB_Config->fgPlayPaused );	// blink green: while paused
        // subsequent call to audPauseResumeAudio() will start playing at pSt.msPlayed msec
      pct = audPlayPct();
      pSt.nPauses++;
      dbgEvt( TB_audPause, pct, pSt.msPlayed, pSt.nPlayed, 0);
      dbgLog( "D pausePlay at %d ms \n", pSt.msPlayed );
      logEvtNININI( "plPause", "ms", pSt.msPlayed, "pct", pct, "nS", pSt.nPlayed  );
      LOG_AUDIO_PLAY_PAUSE(pSt.msecLength, pSt.msPlayed, 100*pSt.msPlayed/pSt.msecLength);
      pSt.stats->Pause++;
      break;

    case pbPaused:
      // resuming == restarting at msPlayed
      dbgEvt( TB_audResume, pSt.msPlayed, 0,0,0);
      dbgLog( "D resumePlay at %d ms \n", pSt.msPlayed );
      logEvt( "plResume" );
      LOG_AUDIO_PLAY_RESUME(pSt.msecLength, pSt.msPlayed, 100*pSt.msPlayed/pSt.msecLength);
      pSt.stats->Resume++;
      if ( pSt.audType==audWave )
        setWavPos( pSt.msPlayed );			// re-start where we stopped
      break;

    case pbRecording:
      // pausing
      haltRecord();				// shut down device & update timestamps
      audSaveBuffs();			// write all filled SvBuff[], file left open

      dbgEvt( TB_recPause, 0,0,0,0);
      dbgLog( "8 pauseRec at %d ms \n", pSt.msRecorded );
      ledFg( TB_Config->fgRecordPaused );	// blink red: while paused
        // subsequent call to audPauseResumeAudio() will append new recording
      logEvtNINI( "recPause", "ms", pSt.msRecorded, "nS", pSt.nSaved );
      pSt.stats->RecPause++;
      pSt.state = pbRecPaused;
      break;
    case pbRecPaused:
      // resuming == continue recording
      dbgEvt( TB_recResume, pSt.msRecorded, 0,0,0);
      logEvt( "recResume" );
      dbgLog( "8 resumeRec at %d ms \n", pSt.msRecorded );
      pSt.stats->RecResume++;
      Driver_SAI0.PowerControl( ARM_POWER_FULL );		// power audio back up
      ctrl = ARM_SAI_CONFIGURE_RX | ARM_SAI_MODE_SLAVE  | ARM_SAI_ASYNCHRONOUS | ARM_SAI_PROTOCOL_I2S | ARM_SAI_DATA_SIZE(16);
      Driver_SAI0.Control( ctrl, 0, pSt.samplesPerSec );	// set sample rate, init codec clock
      startRecord(); // restart recording
      break;

    default:
      break;
  }
}


void 								audSaveBuffs(){																// called on mediaThread to save full recorded buffers
    dbgEvt( TB_svBuffs, 0,0,0,0);
    while ( pSt.SvBuff[0] != NULL ){		// save and free all filled buffs
      Buffer_t * pB = pSt.SvBuff[0];
      saveBuff( pB );
      for ( int i=0; i < nSvBuffs-1; i++)
        pSt.SvBuff[ i ] = pSt.SvBuff[ i+1 ];
      pSt.SvBuff[ nSvBuffs-1 ] = NULL;
    }
}


void								audLoadBuffs(){																// called on mediaThread to preload audio data
    for ( int i=0; i < nPlyBuffs; i++)	// pre-load any empty audio buffers
      if ( pSt.Buff[i]==NULL ){
        pSt.Buff[i] = loadBuff();
      }
}


void 								audPlaybackComplete( void ) {									// shut down after completed playback
  int pct;
  haltPlayback();     // updates tsPause & msPlayed
  audPlayDone();
  #ifdef _SQUARE_WAVE_SIMULATOR
  if ( pSt.SqrWAVE )
      pSt.tunePlayed += pSt.nSamples;
  #endif

  ledFg( NULL );				// Turn off foreground LED: no longer playing
  #ifdef _SQUARE_WAVE_SIMULATOR
  if ( pSt.SqrWAVE ){
	int msTunePlayed = pSt.tsPause - pSt.tsTuneStart;
	pct = msTunePlayed * 100 / pSt.tuneMsec;
	logEvtNININI( "tuneDn", "ms", pSt.msPlayed, "pct", pct, "nS", pSt.tunePlayed );
  } else
  #endif
  {
       pct = 100; //pSt.msPlayed * 100 / pSt.msecLength;
      // complete, so msPlayed == msecLength
      dbgEvt( TB_audDone, pSt.msPlayed, pct, pSt.msPlayed, pSt.msecLength );
      if ( BootKey=='L' ) logEvtNININI( "playDn", "ms", pSt.msPlayed, "pct", pct, "nS", pSt.nPlayed );
      if ( pSt.playbackTyp==ptMsg )
        LOG_AUDIO_PLAY_DONE(pSt.msecLength, pSt.msPlayed, 100*pSt.msPlayed/pSt.msecLength);
      logPlayMsg( );
      
      // playback completed, report difference between measured & expected elapsed time
      if ( BootKey=='L' ) logEvtNI("Play_Tim", "dif_ms", pSt.msecLength - pSt.msPlayed );
  }
  sendEvent( AudioDone, pct );				// end of file playback-- generate CSM event
  pSt.stats->Finish++;
}


void 								audRecordComplete( void ){										// last buff recorded, finish saving file
  // redundant call possible-- if mediaplayer audioStop() is called before mediaplayer processes CODEC_RECORD_DN message from saiEvent
  if (pSt.state != pbRecStop && pSt.state != pbRecording ){
      dbgLog( "! audRecComp: state %d \n", pSt.state );
      return;
  }
  freeBuffs( );
  audSaveBuffs();			// write all filled SvBuff[]
  tbCloseFile( pSt.audF );  //int err = fclose( pSt.audF );
  pSt.audF = NULL;
  ledFg( NULL );
  dbgLog( "8 RecComp %d errs \n", pSt.ErrCnt );

  dbgEvt( TB_audRecClose, pSt.nSaved, pSt.buffNum, minFreeBuffs, 0);
  logEvtNINI( "recMsg", "ms", pSt.msRecorded, "nSamp", pSt.nSaved );

  if ( pSt.ErrCnt > 0 ){
    if (pSt.LastError == SAI_OutOfBuffs )
      dbgLog( "! %d record errs, out of buffs \n", pSt.ErrCnt );
    else
      dbgLog( "! %d record errs, Lst=0x%x \n", pSt.ErrCnt, pSt.LastError );
    logEvtNINI( "RecErr", "cnt", pSt.ErrCnt, "last", pSt.LastError );
  }
  pSt.state = pbIdle;
}


//
// INTERNAL functions
static void 				adjBuffCnt( int adj ){
  nFreeBuffs += adj;
  int cnt = 0;
  for ( int i=0; i<MxBuffs; i++ )
    if ( audio_buffers[i].state == bFree ) cnt++;
  cntFreeBuffs = cnt;
  if ( nFreeBuffs != cntFreeBuffs )
    tbErr("buff mismatch nFr=%d cnt=%d", nFreeBuffs, cntFreeBuffs );
  if ( nFreeBuffs < minFreeBuffs )
    minFreeBuffs = nFreeBuffs;
}


static void 				initBuffs(){																	// create audio buffers
  for ( int i=0; i < MxBuffs; i++ ){
    Buffer_t *pB = &audio_buffers[i];
    pB->state = bFree;
    pB->cntBytes = 0;
    pB->firstSample = 0;
    pB->data = (uint16_t *) tbAlloc( BuffLen, "audio buffer" );
//		for( int j =0; j<BuffWds; j++ ) pB->data[j] = 0x33;
  }
  adjBuffCnt( MxBuffs );
}


static Buffer_t * 	allocBuff( bool frISR ){											// get a buffer for use
  for ( int i=0; i<MxBuffs; i++ ){
    if ( audio_buffers[i].state == bFree ){
      Buffer_t * pB = &audio_buffers[i];
      pB->state = bAlloc;
      adjBuffCnt( -1 );
      dbgEvt( TB_allocBuff, (int)pB, nFreeBuffs, cntFreeBuffs, 0 );
      return pB;
    }
  }
  if ( !frISR )
    errLog("out of aud buffs");
  return NULL;
}


static void 				startPlayback( void ){										// power up codec, preload buffers & start playback
  dbgEvt( TB_stPlay, pSt.nLoaded, 0,0,0);

  Driver_SAI0.PowerControl( ARM_POWER_FULL );		// power up audio
  pSt.monoMode = (pSt.fmtData.numChannels == 1);
  pSt.nPerBuff = pSt.monoMode? BuffWds/2 : BuffWds;		// num source data samples per buffer

  uint32_t ctrl = ARM_SAI_CONFIGURE_TX | ARM_SAI_MODE_SLAVE  | ARM_SAI_ASYNCHRONOUS | ARM_SAI_PROTOCOL_I2S | ARM_SAI_DATA_SIZE(16);
  Driver_SAI0.Control( ctrl, 0, pSt.samplesPerSec );	// set sample rate, init codec clock, power up speaker and unmute

  pSt.state = pbPlayFill;			// preloading
  audLoadBuffs();					// preload nPlyBuffs of data -- at curr position ( nLoaded & msPos )

  pSt.state = pbFull;

  #ifdef _SQUARE_WAVE_SIMULATOR
  if ( !pSt.SqrWAVE )     // no LED for playTune -- let CSM decide
  #endif
  {
      ledFg( TB_Config->fgPlaying );  // Turn ON green LED: audio file playing (except for tunes)
  }
  pSt.state = pbPlaying;
  cdc_SetMute( false );		// unmute

  Driver_SAI0.Send( pSt.Buff[0]->data, BuffWds );		// start first buffer
  Driver_SAI0.Send( pSt.Buff[1]->data, BuffWds );		// & set up next buffer

  pSt.tsResume = tbTimeStamp();		// 1st buffer has actually started-- start of this playing
  if ( pSt.tsPlay==0 )
    pSt.tsPlay = pSt.tsResume;		// start of file playback

  // buffer complete for cBuff calls saiEvent, which:
  //   1) Sends Buff2
  //   2) shifts buffs 1,2,..N to 0,1,..N-1
  //   3) signals mediaThread to refill empty slots
}


static void					haltPlayback(){																// shutdown device, free buffs & update timestamps
  int msec;
  if ( pSt.state == pbPlaying || pSt.state == pbDone ){
    if ( pSt.state == pbPlaying )   // record timestamp before shutdown
      pSt.tsPause = tbTimeStamp();  // if pbDone, was saved by saiEvent

    msec = (pSt.tsPause - pSt.tsResume);		// (tsResume == tsPlay, if 1st pause)
    pSt.msPlayed += msec;  	// update position
    pSt.state = pbPaused;
  } 

  cdc_SetMute( true );	// (redundant) mute
  cdc_SpeakerEnable( false );	// turn off speaker
  Driver_SAI0.Control( ARM_SAI_ABORT_SEND, 0, 0 );	// shut down I2S device
  Driver_SAI0.PowerControl( ARM_POWER_OFF );		// power off I2S (& codec & I2C)
  freeBuffs();
}


static void 				releaseBuff( Buffer_t * pB ){									// release Buffer (unless NULL)
  if ( pB==NULL ) return;
  pB->state = bFree;
  pB->cntBytes = 0;
  pB->firstSample = 0;
  adjBuffCnt( 1 );
  dbgEvt( TB_relBuff, (int)pB, nFreeBuffs, cntFreeBuffs, 0 );
}


static void					freeBuffs(){																	// free all audio buffs from pSt->Buffs[]
  for (int i=0; i < nPlyBuffs; i++){		// free any allocated audio buffers
    releaseBuff( pSt.Buff[i] );
    pSt.Buff[i] = NULL;
  }
}


#ifdef _SQUARE_WAVE_SIMULATOR
static void 				fillSqrBuff( Buffer_t * pB, int nSamp, bool stereo ){	// DEBUG: fill buffer with current square wave
    pSt.sqrSamples -= nSamp;		// decrement SqrWv samples to go

    int phase = pSt.sqrWvPh;			// start from end of previous
    int val = phase > 0? pSt.sqrHi : pSt.sqrLo;
    for (int i=0; i<nSamp; i++){
      if (stereo) {
        pB->data[i<<1] = val;
        pB->data[(i<<1) + 1 ] = 0;
      } else
        pB->data[i] = val;
      phase--;
      if (phase==0)
        val = pSt.sqrLo;
      else if (phase==-pSt.sqrHfLen){
        phase = pSt.sqrHfLen;
        val = pSt.sqrHi;
      }
    }
    pSt.sqrWvPh = phase;		// next starts from here
}
#endif


static void					saveBuff( Buffer_t * pB ){										// save buff to file, then free it
  // collapse to Left channel only
  int nS = BuffWds/2;	  // num mono samples
  for ( int i = 0; i<BuffWds; i+=2 )	// nS*2 stereo samples
    pB->data[ i >> 1 ] = pB->data[ i ];  // pB[0]=pB[0], [1]=[2], [2]=[4], [3]=[6], ... [(BuffWds-2)/2]=[BuffWds-2]
//	int bNum = pSt.nSaved / nS;
//	if ( bNum < 4 ){
//		bool zero = (pB->data[0]==0) && (pB->data[1]==0);
//		dbgLog( "8 %c%d R%d S%d\n", zero? '!':'D', bNum, pB->cntBytes-pSt.tsRecord, tbTimeStamp()-pSt.tsRecord  );
//	}
  int nB = fwrite( pB->data, 1, nS*2, pSt.audF );		// write nS*2 bytes (1/2 buffer)
  if ( nB != nS*2 ) tbErr("svBuf write(%d)=>%d", nS*2, nB );

  pSt.nSaved += nS;			// cnt of samples saved
  dbgEvt( TB_wrRecBuff, nS, pSt.nSaved, nB, (int)pB);
  releaseBuff( pB );
}


static void 				startRecord( void ){													// preload buffers & start recording
  for (int i=0; i < nPlyBuffs; i++)			// allocate empty buffers for recording
    pSt.Buff[i] = allocBuff(0);

  pSt.Buff[0]->state = bRecording;
  Driver_SAI0.Receive( pSt.Buff[0]->data, BuffWds );		// set first buffer

  pSt.state = pbRecording;
  pSt.tsResume = tbTimeStamp();		// start of this record
  if ( pSt.tsRecord==0 )
   pSt.tsRecord = pSt.tsResume;		// start of file recording

  ledFg( TB_Config->fgRecording ); 	// Turn ON red LED: audio file recording
  pSt.Buff[1]->state = bRecording;
  Driver_SAI0.Receive( pSt.Buff[1]->data, BuffWds );		// set up next buffer & start recording
  dbgEvt( TB_stRecord, (int)pSt.Buff[0], (int)pSt.Buff[1],0,0);

  // buffer complete ISR calls saiEvent, which:
  //   1) marks Buff0 as Filled
  //   2) shifts buffs 1,2,..N to 0,1,..N-1
  //   2) calls Receive() for Buff1
  //   3) signals mediaThread to save filled buffers
}


static void 				haltRecord( void ){														// ISR callable: stop audio input
  Driver_SAI0.Control( ARM_SAI_ABORT_RECEIVE, 0, 0 );	// shut down I2S device
  Driver_SAI0.PowerControl( ARM_POWER_OFF );					// shut off I2S & I2C devices entirely

  //pSt.state -- changed by caller   pSt.state = pbIdle;   // might get switched back to pbRecPaused
  pSt.tsPause = tbTimeStamp();
  pSt.msRecorded += (pSt.tsPause - pSt.tsResume);  		// (tsResume == tsRecord, if never paused)
  dbgEvt( TB_audRecDn, pSt.msRecorded, 0, 0,0 );

  ledFg( NULL );		// cancel fgRecording
  ledFg( TB_Config->fgSavingRec );				// Switch foreground LED to saving
}


static void					setWavPos( int msec ){
  if ( pSt.state!=pbPaused )
    tbErr("setPos not paused");

  freeBuffs();					// release any allocated buffers
  if ( msec < 0 ) msec = 0;
  int stSample = msec * pSt.samplesPerSec / 1000;					// sample to start at
  if ( stSample > pSt.nSamples ){
    audPlaybackComplete();
    return;
  }
  pSt.audioEOF = false;   // in case restarting near end

  int fpos = pSt.dataStartPos + pSt.bytesPerSample * stSample;
  fseek( pSt.audF, fpos, SEEK_SET );			// seek wav file to byte pos of stSample
  dbgEvt( TB_audSetWPos, msec, pSt.msecLength, pSt.nLoaded, stSample);

  pSt.nLoaded = stSample;		// so loadBuff() will know where it's at

  startPlayback();	// preload & play
}


static Buffer_t * 	loadBuff( ){																	// read next block of audio into a buffer
  if ( pSt.audioEOF )
    return NULL;			// all data & padding finished

  Buffer_t *pB = allocBuff(0);
  pB->firstSample = pSt.nLoaded;

  int nSamp = 0;
  int len = pSt.nPerBuff;  // leaves room to duplicate if mono

  #ifdef _SQUARE_WAVE_SIMULATOR
  if ( pSt.SqrWAVE ) {     // gen sqWav for tunes
    pSt.audioEOF = (len >= pSt.sqrSamples );  // last buffer
    nSamp = pSt.sqrSamples > len? len : pSt.sqrSamples;
    fillSqrBuff( pB, nSamp, false );
  } else
  #endif
  {
    nSamp = fread( pB->data, 1, len*2, pSt.audF )/2;		// read up to len samples (1/2 buffer if mono)
    dbgEvt( TB_ldBuff, nSamp, ferror(pSt.audF), pSt.buffNum, 0 );
  }

  if ( pSt.monoMode ){
    nSamp *= 2;
    for ( int i = nSamp-1; i>0; i-- )	// nSamp*2 stereo samples
      pB->data[i] = pB->data[ i>>1 ];   // len==2048:  d[2047] = d[1023], d[2046]=d[1023], d[2045] = d[1022], ... d[2]=d[1], d[1]=d[0]
  }

  if ( nSamp < BuffWds ){ 				// room left in buffer: at EOF, so fill rest with zeros
    dbgLog( "D plyLast nS=%d \n", pSt.nLoaded + nSamp );
    if ( nSamp <= BuffWds-MIN_AUDIO_PAD ){
      pSt.audioEOF = true;							// enough padding, so stop after this
      tbCloseFile( pSt.audF );  
      pSt.audF = NULL;
      dbgEvt( TB_audClose, nSamp, 0,0,0);
      FileSysPower( false );		// power down SDIO after playback reading completes
    }

    for( int i=nSamp; i<BuffWds; i++ )		// clear rest of buffer -- can't change count in DoubleBuffer DMA
      pB->data[i] = 0;
  }
  pSt.buffNum++;
  pSt.nLoaded += nSamp;			// # loaded
  pB->cntBytes = BuffLen;		// always transmit complete buffer
  pB->state = bFull;
  return pB;
}

// This seems to be another gatekeeper for starting playback
extern bool 				playWave( const char *fname ){ 					// play WAV file, true if started -- (extern for DebugLoop)
    dbgEvtD( TB_playWv, fname, strlen(fname) );

    int dataChunkSize;
    int fLen = 0;
    pSt.audType = audWave;
    pSt.state = pbLdHdr;

    // Open file, start reading the header(s).
    pSt.audF = tbOpenRead( fname ); //fopen( fname, "r" );
    if (pSt.audF==NULL)
      { errLog( "open wav failed, %s", fname ); audStopAudio(); return false; }

    // Independent check of the file size.
    fseek(pSt.audF, 0, SEEK_END);
    fLen = ftell(pSt.audF);
    fseek(pSt.audF, 0, SEEK_SET);

    WavHeader_t wavFileHeader;
    if ( fread( &wavFileHeader, 1, sizeof(WavHeader_t), pSt.audF ) != sizeof(WavHeader_t) )
      { errLog( "read wav header failed, %s", fname ); audStopAudio(); return false; }
    if ( wavFileHeader.riffId != newWavHeader.wavHeader.riffId || wavFileHeader.waveId != newWavHeader.wavHeader.waveId )
      { errLog("bad wav: %s  chunkId=0x%x format=0x%x \n", fname, wavFileHeader.riffId, wavFileHeader.waveId ); audStopAudio(); return false; }

    // Read file chunks until we find "data". When we find "fmt ", load it into pSt.
    while (1) {
      // Read the next chunk header.
      RiffChunk_t riffChunkHeader;
      if (fread(&riffChunkHeader, 1, sizeof(RiffChunk_t), pSt.audF) != sizeof(RiffChunk_t))
        { errLog("read wav file failed, %s", fname);  audStopAudio();  return false; }

      if (riffChunkHeader.chunkId == newWavHeader.audioHeader.chunkId) {
        // Is it "data"? If so, we're done! (And we had better have read "fmt");
        dataChunkSize = riffChunkHeader.chunkSize;
        break;
          
      } else if (riffChunkHeader.chunkId == newWavHeader.fmtHeader.chunkId) {
        // Is it "fmt "? If so, validate and read the format details.
        if (riffChunkHeader.chunkSize != newWavHeader.fmtHeader.chunkSize) {
          errLog("Unexpected fmt size in wav file, %s, expected %d, got %d", fname, riffChunkHeader.chunkSize, newWavHeader.fmtHeader.chunkSize);
          audStopAudio();
          return false;
        }
        if (fread(&pSt.fmtData, 1, sizeof(WavFmtData_t), pSt.audF) != sizeof(WavFmtData_t))
          { errLog("read wav file fmt data failed, %s", fname);  audStopAudio();  return false; }
      } else {
        // Something else, just skip over it.
        fseek(pSt.audF, riffChunkHeader.chunkSize, SEEK_CUR);
      }
    }
    
    int audioFreq = pSt.fmtData.samplesPerSecond;
    if ((audioFreq < SAMPLE_RATE_MIN) || (audioFreq > SAMPLE_RATE_MAX))
        { errLog( "bad audioFreq, %d", audioFreq ); audStopAudio(); return false; }
    pSt.samplesPerSec = audioFreq;
    pSt.bytesPerSample = pSt.fmtData.numChannels * pSt.fmtData.bitsPerSample/8;  // = 4 bytes per stereo sample -- same as ->BlockAlign
    pSt.nSamples = dataChunkSize / pSt.bytesPerSample;
    pSt.msecLength = pSt.nSamples*1000 / pSt.samplesPerSec;
    pSt.dataStartPos = ftell( pSt.audF );    // remember start of wav data, for setWavPos()
    pSt.state = pbGotHdr;
    if ( pSt.playbackTyp==ptMsg )   // details only for content messages
        LOG_AUDIO_PLAY_WAVE(fname, pSt.msecLength, fLen, pSt.samplesPerSec, pSt.monoMode);

    startPlayback();       // power up codec, preload buffers from current file pos, & start I2S transfers
    return true;
}

extern void 				saiEvent( uint32_t event ){										// called by ISR on buffer complete or error -- chain next, or report error -- also DebugLoop
         // calls: Driver_SAI0.Send .Receive .Control haltRecord releaseBuff  freeBuffs
         //  dbgEvt, osEventFlagsSet, tbTimestamp
  dbgEvt( TB_saiEvt, event, 0,0,0);
  const uint32_t ERR_EVENTS = ~(ARM_SAI_EVENT_SEND_COMPLETE | ARM_SAI_EVENT_RECEIVE_COMPLETE );
  if ( (event & ARM_SAI_EVENT_SEND_COMPLETE) != 0 ){
    if ( pSt.Buff[2] != NULL ){		// have more data to send
      pSt.nPlayed += pSt.nPerBuff;   // num samples actually completed
      Driver_SAI0.Send( pSt.Buff[2]->data, BuffWds );		// set up next buffer

      dbgEvt( TB_audSent, pSt.Buff[2]->firstSample, (int)pSt.Buff[2],0,0);
      releaseBuff( pSt.Buff[0] );		// buff 0 completed, so free it

      for ( int i=0; i < nPlyBuffs-1; i++ )	// shift buffer pointers down
        pSt.Buff[i] = pSt.Buff[i+1];
      pSt.Buff[ nPlyBuffs-1 ] = NULL;
      // signal mediaThread to call audLoadBuffs() to preload empty buffer slots
      dbgEvt( TB_saiTXDN, 0, 0, 0, 0 );
      osEventFlagsSet( mMediaEventId, CODEC_DATA_TX_DN );
    } else if ( pSt.audioEOF ){  // done, close up shop
      pSt.state = pbDone;
      pSt.tsPause = tbTimeStamp();		// timestamp end of playback
      Driver_SAI0.Control( ARM_SAI_ABORT_SEND, 0, 0 );	// shut down I2S device, arg1==0 => Abort
      #ifdef _SQUARE_WAVE_SIMULATOR
	  if ( pSt.SqrWAVE )	freeBuffs();  // make sure buffers are freed in multi-tone sequences
      #endif

      dbgEvt( TB_saiPLYDN, 0, 0, 0, 0 );
      osEventFlagsSet( mMediaEventId, CODEC_PLAYBACK_DN );	// signal mediaThread for rest
    } else {
      dbgLog( "! Audio underrun\n" );
    }
  }

  if ( (event & ARM_SAI_EVENT_RECEIVE_COMPLETE) != 0 ){
    Buffer_t * pB = pSt.Buff[2];		// next record buffer
    if ( pSt.state == pbRecording && pB != NULL ){ // provide next buffer to device as quickly as possible
        pSt.nRecorded += pSt.samplesPerBuff;
        pB->state = bRecording;
        pB->firstSample = pSt.nRecorded+1;
        Driver_SAI0.Receive( pB->data, BuffWds );	// next buff ready for reception
    }

    pSt.buffNum++;										// received 1 more buffer
    Buffer_t * pFull = pSt.Buff[0];		// filled buffer
    pFull->state = bRecorded;
    pFull->cntBytes = tbTimeStamp();  // mark time of completion for DEBUG

    for ( int i=0; i < nPlyBuffs-1; i++ )  // shift receive buffer ptrs down
      pSt.Buff[i] = pSt.Buff[i+1];
    pSt.Buff[ nPlyBuffs-1 ] = NULL;

    for (int i=0; i< nSvBuffs; i++)	// transfer pFull (Buff[0]) to SvBuff[]
      if ( pSt.SvBuff[i]==NULL ){
        pSt.SvBuff[i] = pFull;   // put pFull in empty slot for writing to file
        break;
      }
    dbgEvt( TB_saiRXDN, (int)pFull, 0, 0, 0 );
    osEventFlagsSet( mMediaEventId, CODEC_DATA_RX_DN );		// tell mediaThread to save buffer

    if (pSt.state == pbRecStop ){
      haltRecord();		// stop audio input
      osEventFlagsSet( mMediaEventId, CODEC_RECORD_DN );		// tell mediaThread to finish recording
      freeBuffs();						// free buffers waiting to record -- pSt.Buff[*]
    } else {
      pSt.Buff[ nPlyBuffs-1 ] = allocBuff( true );		// add new empty buff for reception
      if ( pSt.Buff[ nPlyBuffs-1 ]==NULL ){	// out of buffers-- try to shut down
        pSt.state = pbRecStop;
        pSt.LastError = SAI_OutOfBuffs;
        pSt.ErrCnt++;
      }
    }
  }
  if ((event & ERR_EVENTS) != 0) {
    pSt.LastError = event;
    pSt.ErrCnt++;
  }
}


// end audio
