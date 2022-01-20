// TBookV2  mediaplayer.c		play/record audio
//  Apr2018

#include "mediaPlyr.h"
#include "fileOps.h"

const int 									CODEC_DATA_TX_DN   =	0x01; 			// signal sent by SAI callback when an buffer completes
const int 									CODEC_PLAYBACK_DN	 =  0x02;				// signal from SAI on playback done
const int 									CODEC_DATA_RX_DN   =	0x04; 			// signal sent by SAI callback when a buffer has been filled
const int 									CODEC_RECORD_DN    =	0x08; 			// signal sent by SAI callback when recording stops
const int 									MEDIA_PLAY_EVENT	 =	0x10;
const int 									MEDIA_RECORD_START =	0x20;
const int										MEDIA_ADJ_POS 		 =  0x40;
const int										MEDIA_SET_SPD 		 =  0x80;
const int										MEDIA_SET_VOL 		 =  0x100;
const int										MEDIA_PLAY_RECORD	 =	0x200;
const int										MEDIA_SAVE_RECORD	 =	0x400;
const int										MEDIA_DEL_RECORD	 =	0x800;
const int										MEDIA_PLAY_TUNE		 =  0x1000;

const int 									MEDIA_EVENTS 			 =  0x1FFF;		// mask for all events

static 	osThreadAttr_t 				thread_attr;

osEventFlagsId_t							mMediaEventId;			// for signals to mediaThread

// communication variables shared with mediaThread
volatile int 				        	mAudioVolume		= 7;			// current volume, overwritten from CSM config (setVolume)    (external for powermanager)
static volatile int 					mAudioSpeed			= 5;			// current speed, NYI

const int MAX_NOTE_CNT = 80;
static volatile int						mNoteCnt = 0;
static volatile int						mNxtNote;
static volatile int						mNoteFreq[ MAX_NOTE_CNT ];
static volatile int						mNoteDur [ MAX_NOTE_CNT ];  // duration in buffers (~1/8 sec)

static volatile int 					mAdjPosSec;
static volatile char					mPlaybackFilePath[ MAX_PATH ];
static volatile playback_type_t         mPlayType;              //  sys/pkg/msg/nm/pr
static volatile MsgStats *		mPlaybackStats;
static volatile char 					mRecordFilePath[ MAX_PATH ];
static volatile MsgStats *		mRecordStats;
static volatile MsgStats *    sysStats;						// subst buffer for system files 

static void 	mediaThread( void *arg );						// forward 
	
void					initMediaPlayer( void ){						// init mediaPlayer & spawn thread to handle  playback & record requests
	mMediaEventId = osEventFlagsNew(NULL);					// osEvent channel for communication with mediaThread
	if ( mMediaEventId == NULL)
		tbErr( "mMediaEventId alloc failed" );	
	
	sysStats = tbAlloc( sizeof( MsgStats ), "sysStats" );

	mPlaybackFilePath[0] = 0;			
	mRecordFilePath[0] = 0;			
	
	thread_attr.name = "Media";
	thread_attr.stack_size = MEDIA_STACK_SIZE; 
	Dbg.thread[2] = (osRtxThread_t *) osThreadNew( mediaThread, NULL, &thread_attr );
	if ( Dbg.thread[2] == NULL )
		tbErr( "mediaThread spawn failed" );	
	
	//registerPowerEventHandler( handlePowerEvent );
	audInitialize( );		// still on TB thread, during init
}
void					mediaPowerDown( void ){			// not called currently
	audStopAudio();
}
// 
// external methods for  controlManager executeActions
//
//**** Operations signaled to complete on media thread
void 					playAudio( const char * fileName, MsgStats *stats, playback_type_t typ ){ // start playback from fileName 
	strcpy( (char *)mPlaybackFilePath, fileName );
	mPlaybackStats = stats==NULL? sysStats : stats;
    mPlayType = typ;
	osEventFlagsSet( mMediaEventId, MEDIA_PLAY_EVENT );
}
void					playNotes( char *notes ){			// play square tones for 1/4 sec per 'notes' 
	mNoteCnt = 0;
	int nSyms = strlen( notes );
	mNxtNote = 0;
	int i = 0;
	while( i < nSyms ){ 
		int fr = 2, dur = 2;
		switch ( notes[i] ){  // C scale:  C5..C6
				case 'C': case 'c':   fr = 523; break;	// C5
				case 'D': case 'd':   fr = 587; break;	// D5
				case 'E': case 'e':   fr = 659; break;	// E5
				case 'F': case 'f':   fr = 698; break;	// F5
				case 'G': case 'g':   fr = 784; break;	// G5
				case 'A': case 'a':   fr = 880; break;	// A6
				case 'B': case 'b':   fr = 988; break;	// B6
				case 'H': case 'h':   fr = 1047; break; // C6  also could use C+
			default:
				case '_':   					fr = 2; 	break;  // rest
		}
		
		i++;  // next Sym, suck up any modifiers ( . extend, + up oct, - down oct )
		while ( i < nSyms ){
			if 			( notes[i]=='.' ) dur++; 
			else if ( notes[i]=='*' ) dur *= 2;
			else if ( notes[i]=='/' ) dur /= 2;
			else if ( notes[i]=='+' ) fr *= 2;
			else if ( notes[i]=='-' ) fr /= 2;
			else 
				break;
			i++;
		}
		
		mNoteFreq[ mNoteCnt ] = fr;		// add (fr,dur) to note list
		mNoteDur[ mNoteCnt ] = dur;
		mNoteCnt++; 
		if ( mNoteCnt > MAX_NOTE_CNT ) tbErr("playNotes: %d notes");
	}
	osEventFlagsSet( mMediaEventId, MEDIA_PLAY_TUNE );
}
void					recordAudio( const char * fileName, MsgStats *stats ){	// start recording into fileName
	strcpy( (char *)mRecordFilePath, fileName );
	mRecordStats = stats==NULL? sysStats : stats;
	osEventFlagsSet( mMediaEventId, MEDIA_RECORD_START );
}
void					playRecording( ){	// play back just recorded audio
	if ( mRecordFilePath[0] == 0 ) return;
	osEventFlagsSet( mMediaEventId, MEDIA_PLAY_RECORD );	
}
void					saveRecording( ){	// encrypt recorded audio & delete original
	if ( mRecordFilePath[0] == 0 ) return;
	ledFg( TB_Config->fgSaveRec );	
	osEventFlagsSet( mMediaEventId, MEDIA_SAVE_RECORD );	
}
void					cancelRecording(){ // delete recorded message
	if ( mRecordFilePath[0] == 0 ) return;
	ledFg( TB_Config->fgCancelRec );	
	osEventFlagsSet( mMediaEventId, MEDIA_DEL_RECORD );	
}
void 					adjPlayPosition( int sec ){					// skip playback forward/back 'sec' seconds
	dbgEvt( TB_audAdjPos, sec, 0, 0,0);
	mAdjPosSec = sec;
	osEventFlagsSet( mMediaEventId, MEDIA_ADJ_POS );
}
void 					adjSpeed( int adj ){								// adjust playback speed by 'adj'
	mAudioSpeed += adj;
	if ( mAudioSpeed < 0 ) mAudioSpeed = 0;
	if ( mAudioSpeed > 10 ) mAudioSpeed = 10;
	dbgEvt( TB_audAdjSpd, mAudioSpeed, 0, 0,0);
	osEventFlagsSet( mMediaEventId, MEDIA_SET_SPD );
}
void 					adjVolume( int adj ){								// adjust playback volume by 'adj'
	mAudioVolume += adj;
	if ( mAudioVolume > MAX_VOLUME ) mAudioVolume = MAX_VOLUME;
	if ( mAudioVolume < MIN_VOLUME ) mAudioVolume = MIN_VOLUME;
	dbgEvt( TB_audAdjVol, adj, mAudioVolume, 0,0);
	osEventFlagsSet( mMediaEventId, MEDIA_SET_VOL );
}
void 					stopPlayback( void ){								// stop playback
	audStopAudio( );
}
//**** Operations done directly on TB thread
void 					pauseResume( void ){								// pause (or resume) playback or recording
	audPauseResumeAudio( );
}
int						playPosition( void ){								// => current playback position in sec
	return audPlayPct();
}
void					setVolume( int vol ){								// set initial volume
	mAudioVolume = vol;		// remember for next Play
}
MediaState		getStatus( void ){									// => Ready / Playing / Recording
	return audGetState();
}
void					stopRecording( void ){  						// stop recording
	audRequestRecStop( ); 
}
//************************************
// called only on MediaThread
void 					resetAudio(){ 											// stop any playback/recording in progress 
	audStopAudio();
}

int playCnt = 0;  // DEBUG**********************************
/* **************  mediaThread -- start audio play & record operations, handle completion signals
// 		MEDIA_PLAY_EVENT 	=> play mPlaybackFilename
//		CODEC_DATA_TX_DN  => buffer xmt done, call audLoadBuffs outside ISR
//		CODEC_PLAYBACK_DN 	=> finish up, & send AudioDone CSM event
//
//		MEDIA_RECORD_START 	=> record into mRecordFilePath
//		CODEC_DATA_RX_DN		=> buffer filled, call audSaveBuffs outside ISR
//		CODEC_RECORD_DN			=> finish recording & save file
***************/
static void 	mediaThread( void *arg ){						// communicates with audio codec for playback & recording		
	dbgLog( "4 mediaThr: 0x%x 0x%x \n", &arg, &arg + MEDIA_STACK_SIZE );
    bool SetVolumePending = false;
    const int BUF_DN = CODEC_DATA_TX_DN | CODEC_DATA_RX_DN;
    
	while (true){		
		uint32_t flags = osEventFlagsWait( mMediaEventId, MEDIA_EVENTS,  osFlagsWaitAny, osWaitForever );
		
		dbgEvt( TB_mediaEvt, flags, 0,0,0);
        if ( (flags & BUF_DN) !=0 ){      // EITHER Tx or Rx buffer done -- process, then check for pending volume change
            if ( (flags & CODEC_DATA_TX_DN) != 0 )		// buffer transmission complete from SAI_event
                audLoadBuffs();							// preload any empty audio buffers
                
            if ( (flags & CODEC_DATA_RX_DN) != 0 )		// R2) recording buffer complete from SAI_event
                audSaveBuffs();
            
            if ( SetVolumePending ){            // volume change during last buffer, change now (so it won't overlap next buffer ISR)
                cdc_SetVolume( mAudioVolume );
                SetVolumePending = false;
            }
		}
			
		if ( (flags & CODEC_PLAYBACK_DN) != 0 ){		// playback complete
			if ( mNoteCnt==0 ){  											
				audPlaybackComplete();											// 3a)  wav file or complete tune
			} else 
			{																							// 3b)  playNotes
				int fr = mNoteFreq[ mNxtNote ];
				int dur = mNoteDur[ mNxtNote ];
				mNxtNote++;
				audPlayTone( mNxtNote, fr, dur );
				if ( mNxtNote >= mNoteCnt )
					mNoteCnt = 0;			// done with tune next time
			}
		}
		
		if ( (flags & CODEC_RECORD_DN) != 0 ){			// R3) recording complete
			audRecordComplete();
		}

		if ( (flags & MEDIA_PLAY_EVENT) != 0 ){		// request to start playback
			if ( mPlaybackFilePath[0] == 0 ) continue;
			resetAudio();
			audPlayAudio( (const char *)mPlaybackFilePath, (MsgStats *) mPlaybackStats, mPlayType );
		} 
		if ( (flags & MEDIA_PLAY_TUNE) != 0 ){					// 1b) request to start tune-- playNotes()
			resetAudio();
			audPlayTone( 0, mNoteFreq[0], mNoteDur[0] ); 
			mNxtNote = 1;
			if ( mNoteCnt==1 ) mNoteCnt = 0;		// 1 and done
		} 
		
		//******* record wav file:  R1) start, R2) bufferDone, R3) rec done, R4) play rec, R5) save, or R5a) delete
		if ( (flags & MEDIA_RECORD_START) != 0 ){		// R1) request to start recording
			resetAudio();			// clean up anything in progress 
			FILE* outFP = tbOpenWriteBinary( (const char *)mRecordFilePath ); //fopen( (const char *)mRecordFilePath, "wb" );
			if ( outFP != NULL ){
				dbgLog( "8 Rec fnm: %s \n", (char *)mRecordFilePath );
				audStartRecording( outFP, (MsgStats *) mRecordStats );
			} else {
				printf ("Cannot open record file to write\n\r");
			}
		} 
		
		if ( (flags & MEDIA_PLAY_RECORD) != 0 ){	// R4) request to play recording
			resetAudio();
			audPlayAudio( (const char *)mRecordFilePath, (MsgStats *) sysStats, ptRec );
		}
		
		if ( (flags & MEDIA_SAVE_RECORD) != 0 ){	// R5) request to save recording
			copyEncrypted( (char *) mRecordFilePath );
			mRecordFilePath[0] = 0;
		}
		
		if ( (flags & MEDIA_DEL_RECORD) != 0 ){	    // R5a) request to delete recording
			int res = fdelete( (char *) mRecordFilePath, "" );
			mRecordFilePath[0] = 0;
			FileSysPower( false );			// power down SDIO after finished with recording
		}
		
	    //******* set volume
	    if ( (flags & MEDIA_SET_VOL) != 0 ){			// request to set volume
            if ( audGetState() == Ready ){         // no audio active-- do now
                logEvtNI( "setVol", "vol", mAudioVolume );
                cdc_SetVolume( mAudioVolume );
            } else {    // audio active-- postpone till next buffer complete
                logEvtNI( "pndVol", "vol", mAudioVolume );
                SetVolumePending = true;
            }
		}
		
//		if ( (flags & MEDIA_SET_SPD) != 0 )				// request to set speed (NYI)
//			audAdjPlaySpeed( mAudioSpeed );

        //******* set playback position
		if ( (flags & MEDIA_ADJ_POS) != 0 )				// request to adjust playback position
			audAdjPlayPos( mAdjPosSec );
	}
}
//end  mediaplayer.c

