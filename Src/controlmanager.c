// TBookV2  controlmanager.c
//   Gene Ball  May2018

#include "controlmanager.h"

#include "tbook.h"				// enableMassStorage 
#include "mediaPlyr.h"		// audio operations
#include "inputMgr.h"			// osMsg_TBEvents
#include "fileOps.h"			// decodeAudio

const char *  			bgPulse		 			= "_49G";				// brief green flash every 5 seconds
const char *  			fgPlaying 			= "G!";
const char *  			fgPlayPaused		= "G2_3!";
const char *  			fgRecording			= "R!";
const char *  			fgRecordPaused	= "R2_3!";
const char *				fgSavingRec			= "O!";
const char *				fgSaveRec				= "G3_3G3";
const char *				fgCancelRec			= "R3_3R3";
const char *  			fgUSB_MSC				= "O5o5!";
const char *  			fgTB_Error			= "R8_2R8_2R8_20!";		// repeat: 4.8sec
const char *  			fgNoUSBcable		= "_3R3_3R3_3R3_5!";  // repeat: 2.3sec
const char *				fgUSBconnect		= "G5g5!";
const char *  			fgPowerDown			= "G_3G_3G_9G_3G_9G_3";  // length: 3.5sec
const char *  			fgDFU						= "O3_5O3_3O3_2O3_1O3";  // length: 2.6sec

// TBook configuration variables
extern TBConfig_t 					TB_Config;			// defined in tbook_csm.c

// TBook Control State Machine
extern int 	 								nCSMstates;			// defined in tbook_csm.c
extern csmState *						TBookCSM[ ];		// TBook state machine definition -- defined in tbook_csm.c

// TBook content packages
int										nPackages = 0;								// number of packages found
TBPackage_t *					TBPackage[ MAX_PKGS ];				// package info
int										iPkg = 0;											// package index 
TBPackage_t * 				TBPkg;												// content package in use

TBook_t TBook;

osTimerId_t  	timers[3]; 	// ShortIdle, LongIdle, Timer
void									setCSMcurrState( short iSt );  // set TBook.iCurrSt & dependent fields
	
// ------------  CSM Action execution
static void 					adjSubj( int adj ){								// adjust current Subj # in TBook
	short nS = TBook.iSubj + adj;
	short numS = TBPkg->nSubjs;
	if ( nS < 0 ) 
		nS = numS-1;
	if ( nS >= numS )
		nS = 0;
	logEvtNI( "chngSubj", "iSubj", nS );
	TBook.iSubj = nS;
	TBook.iMsg = -1; // "next" will yield 0, "prev" -> -2 -> numMessages-1
}


static void 					adjMsg( int adj ){								// adjust current Msg # in TBook
	// If no subject is selected, do nothing.
  if (TBook.iSubj < 0 || TBook.iSubj >= TBPkg->nSubjs)
		return;
	short nM = TBook.iMsg + adj; 
	short numM = TBPkg->TBookSubj[ TBook.iSubj ]->NMsgs;
	if ( nM < 0 ) 
		nM = numM-1;
	if ( nM >= numM )
		nM = 0;
	logEvtNI( "changeMsg", "iMsg", nM );
	TBook.iMsg = nM;
}


void 									playSubjAudio( char *arg ){				// play current Subject: arg must be 'nm', 'pr', or 'msg'
	// If no subject is selected, do nothing.
  if (TBook.iSubj < 0 || TBook.iSubj >= TBPkg->nSubjs)
		return;
	tbSubject * tbS = TBPkg->TBookSubj[ TBook.iSubj ];
	char path[MAX_PATH];
	char *nm = NULL;
	MsgStats *stats = NULL;
	resetAudio();
	if ( strcasecmp( arg, "nm" )==0 ){
		nm = tbS->audioName;
		logEvtNSNS( "PlayNm", "Subj", tbS->name, "nm", nm ); 
    LOG_AUDIO_PLAY_SPROMPT(tbS->name, nm);
	} else if ( strcasecmp( arg, "pr" )==0 ){
		nm = tbS->audioPrompt;
		logEvtNSNS( "Play", "Subj", tbS->name, "pr", nm ); 
    LOG_AUDIO_PLAY_LPROMPT(tbS->name, nm);
	} else if ( strcasecmp( arg, "msg" )==0 ){
		nm = tbS->msgAudio[ TBook.iMsg ];
		logEvtNSNI( "Play", "Subj", tbS->name, "iM", TBook.iMsg ); //, "aud", nm ); 
	  LOG_AUDIO_PLAY_MESSAGE(TBook.iSubj, tbS->name, nm);
  	stats = loadStats( tbS->name, TBook.iSubj, TBook.iMsg );	// load stats for message
	}
	buildPath( path, tbS->path, nm, ".wav" ); //".ogg" );
	playAudio( path, stats );
}


void 									playNxtPackage( ){										// play name of next available Package 
	iPkg++;
	if ( iPkg >= nPackages ) iPkg = 0;
	
	TBPackage_t * pkg = TBPackage[ iPkg ];
	logEvtNSNI( "PlayPkg", "Pkg", pkg->packageName, "Idx", iPkg );  
	playAudio( pkg->packageName, NULL );
}

void									playSqrTune( char *notes ){				// play seq of notes as sqrwaves
	playNotes( notes );		// mediaPlayer op -- switch threads
}

void 									showPkg( ){										// debug print Package iPkg
	TBPackage_t * pkg = TBPackage[ iPkg]; 
	printf( "iPkg=%d nm=%s nSubjs=%d \n", pkg->idx, pkg->packageName, pkg->nSubjs );
	printf( " path=%s \n", pkg->path );
	for (int i=0; i<pkg->nSubjs; i++){
		tbSubject *sb = pkg->TBookSubj[ i ];
		printf(" S%d nMsgs=%d pth=%s \n", i, sb->NMsgs, sb->path );
		printf("   nm=%s anm=%s pr=%s \n", sb->name, sb->audioName, sb->audioPrompt );
		for (int j=0; j<sb->NMsgs; j++)
			printf("   M%d %s \n", j, sb->msgAudio[j] );
	}
}


void									changePackage(){											// switch to last played package name
	TBPkg = TBPackage[ iPkg ];
	TBook.iSubj = -1; // makes "next subject" go to the first subject.
	TBook.iMsg = 0;
	logEvtNS( "ChgPkg", "Pkg", TBPkg->packageName );  
	showPkg();
}


void 									playSysAudio( char *arg ){				// play system file 'arg'
//	char path[MAX_PATH];
	resetAudio();
	for (int i=0; i<nPlaySys; i++)
		if ( strcmp( SysAudio[i].sysNm, arg )==0 ){
//	buildPath( path, TB_Config.systemAudio, arg, ".wav" ); //".ogg" );
			playAudio( SysAudio[i].sysPath, NULL );
		//	logEvtNS( "PlaySys", "file", arg );
      logEvtFmt("PlayAudio", "start, system: '%s', file: '%s'", arg, SysAudio[i].sysPath);
      LOG_AUDIO_PLAY_SYSTEM(arg, SysAudio[i].sysPath);
			return;
		}
	tbErr("playSys %s not found", arg);
}


void									startRecAudio( char *arg ){         	// record user message into temporary .wav file
	resetAudio();
	if ( RunQCTest ){
		recordAudio( "/system/qcAudio.wav", NULL );
		return;
	} 
	char path[MAX_PATH];
	tbSubject * tbS = TBPkg->TBookSubj[ TBook.iSubj ];
	int mCnt = 0;
	char * fNm = logMsgName( path, tbS->name, TBook.iSubj, TBook.iMsg, ".wav", &mCnt ); //".ogg" );		// build file path for next audio msg for S<iS>M<iM>
	logEvtNSNINI( "Record", "Subj", tbS->name, "iM", TBook.iMsg, "cnt", mCnt );
	MsgStats *stats = loadStats( tbS->name, TBook.iSubj, TBook.iMsg );	// load stats for message
	recordAudio( fNm, stats );
}


void   								playRecAudio(){                             // play back recorded temp .wav
	playRecording();
}


void									saveRecAudio( char *arg ){              // encrypt user message .wav => .key & .dat
	if ( strcasecmp( arg, "sv" )==0 ){
		saveRecording();
	} else if ( strcasecmp( arg, "del" )==0 ){
		cancelRecording();
	} 
}

void									saveWriteMsg( char *txt ){				// save 'txt' in Msg file
	tbSubject * tbS = TBPkg->TBookSubj[ TBook.iSubj ];
	char path[MAX_PATH];
	int mCnt = 0;
	char * fNm = logMsgName( path, tbS->name, TBook.iSubj, TBook.iMsg, ".txt", &mCnt );		// build file path for next text msg for S<iS>M<iM>
	FILE* outFP = tbOpenWrite( fNm ); //fopen( fNm, "w" );
	int nch = fprintf( outFP, "%s\n", txt );
	tbCloseFile( outFP );  //int err = fclose( outFP ); 
	dbgEvt( TB_wrMsgFile, nch, 0, 0, 0 );
	logEvtNSNININS( "writeMsg", "Subj", tbS->name, "iM", TBook.iMsg, "cnt", mCnt, "msg", txt );
}

void 									QCfilesTest( char * arg ){					// write & re-read system/qc_test.txt -- gen Event FilesSuccess if identical, or FilesFail if not
	char * testNm = "/system/QC_Test.txt";
	char qcmsg[50], qcres[50];
	fsTime ftime;
	strcpy( qcres, "------------------" );
	strcpy( qcmsg, "QC filesTest" );
	
	writeLine( qcmsg, testNm );   
	loadLine( qcres, testNm, &ftime );  // and read it back
	fdelete( testNm, NULL );
	
	char *nl = strchr( qcres, '\n' );
	if (nl != NULL) *nl = 0;		// truncate newline
	if ( strcmp( qcmsg, qcres )== 0 ) 
		sendEvent( FilesSuccess, 0 );
	else
		sendEvent( FilesFail, 0 );
}
void									svQCresult( char * arg ){  					// save 'arg' to system/QC_PASS.txt if starts with "QC_OK", or as QC_FAIL.txt
	const char * passNm = "/system/QC_PASS.txt";
	const char * failNm = "/system/QC_FAIL.txt";
  checkMem();
	
	if ( fexists( passNm )) fdelete( passNm, NULL );
	if ( fexists( failNm )) fdelete( failNm, NULL );
	if ( strncmp( arg, "QC_OK", 5 )==0 )
		writeLine( arg, "/system/QC_PASS.txt" );  
	else 
		writeLine( arg, "/system/QC_FAIL.txt" );   
}

void 									USBmode( bool start ){						// start (or stop) USB storage mode
	if ( start ){
		playSqrTune( "CEGG" );
		logEvt( "enterUSB" );
		logPowerDown();				// flush & shut down logs
		setDbgFlag('F', false);
		enableMassStorage( "M0:", NULL, NULL, NULL );
	} else {	
		disableMassStorage();      //TODO?  just reboot?
		ledFg( "_" );
		playSqrTune( "GECC" );
		logEvt( "exitUSB" );
//	    NVIC_SystemReset();			// soft reboot?
		logPowerUp( false );
		if ( FirstSysBoot )  // should run in background
			decodeAudio();
	} 
}

void assertValidState(int stateIndex) {
	if ( stateIndex < 0 || stateIndex >= nCSMstates )
		tbErr("invalid state index");
}	

char * 								ledStr( char * s ){			// lookup TBConfig LED sequences
	if ( strcasecmp( s, "bgPulse" )==0 ) 				return TB_Config.bgPulse;					// set by CSM (background while navigating)						default: _49G
	if ( strcasecmp( s, "fgPlaying" )==0 ) 			return TB_Config.fgPlaying;				// set by startPlayback() 					 									default: G!
	if ( strcasecmp( s, "fgPlayPaused" )==0 ) 	return TB_Config.fgPlayPaused;		// set by audPauseResumeAudio() when playback paused  default: G2_3!
	if ( strcasecmp( s, "fgRecording" )==0 ) 		return TB_Config.fgRecording;			// set by startRecord() while recording user feedback default: R!
	if ( strcasecmp( s, "fgRecordPaused" )==0 ) return TB_Config.fgRecordPaused;	// set by audPauseResumeAudio() when recording paused default: R2_3!
	if ( strcasecmp( s, "fgSavingRec" )==0 ) 		return TB_Config.fgSavingRec;			// set by haltRecord while saving recording  					default: O!
	if ( strcasecmp( s, "fgSaveRec" )==0 ) 			return TB_Config.fgSaveRec;				// set by saveRecording() while encrypting recording  default: G3_3G3
	if ( strcasecmp( s, "fgCancelRec" )==0 ) 		return TB_Config.fgCancelRec;			// set by media.cancelRecording()  										default: R3_3R3
	if ( strcasecmp( s, "fgUSB_MSC" )==0 ) 			return TB_Config.fgUSB_MSC;				// set by USBD_MSC0_Init() when USB host connects  		default: O5o5!
	if ( strcasecmp( s, "fgUSBconnect" )==0 ) 	return TB_Config.fgUSBconnect;		// set by enableMassStorage() when starting USB		  	default: G5g5!
	if ( strcasecmp( s, "fgTB_Error" )==0 ) 		return TB_Config.fgTB_Error;			// set by TBErr() to signal software error  					default: R8_2R8_2R8_20!
	if ( strcasecmp( s, "fgNoUSBcable" )==0 ) 	return TB_Config.fgNoUSBcable;		// set by enableMassStorage() if noUSBpower  					default: _3R3_3R3_3R3_5!
	if ( strcasecmp( s, "fgPowerDown" )==0 ) 		return TB_Config.fgPowerDown;			// set??  powerDownTBook() G_3G_3G_9G_3G_9G_3
  return s;
}
void 									setCurrState( short iSt ){				// set iCurrSt & iPrevSt (& DBG strings)
	if ( iSt == TBook.iCurrSt )
		return;
	assertValidState( iSt );
	TBook.iPrevSt = TBook.iCurrSt;
	TBook.prevStateName = TBook.currStateName;					//DEBUG -- update prevSt string
	
	TBook.iCurrSt = iSt;												// now 'in' (executing) state nSt
	TBook.cSt = TBookCSM[ TBook.iCurrSt ];			// state definition for current state

	TBook.currStateName = TBookCSM[ TBook.iCurrSt ]->nm;					//DEBUG -- update currSt string
	//dbgLog( "C %s(%d) => %s(%d) \n",  TBook.prevStateName, TBook.iPrevSt, TBook.currStateName, TBook.iCurrSt );
}

static void 					doAction( Action act, char *arg, int iarg ){	// execute one csmAction
	dbgEvt( TB_csmDoAct, act, arg[0],arg[1],iarg );
	logEvtNSNS( "Action", "nm", actionNm(act), "arg", arg ); //DEBUG
	if (isMassStorageEnabled()){		// if USB MassStorage running: ignore actions referencing files
		switch ( act ){
			case playSys:			
			case playSubj:			
			case startRec:
			case pausePlay:		// pause/play all share
			case resumePlay:		
			case pauseRec:	
			case resumeRec:
			case finishRec:
			case writeMsg:
			case stopPlay:
				return;
			
			default: break;
		}
	}
	switch ( act ){
		case LED:
			ledFg( arg );	
			break;
		case bgLED:
			ledBg( arg );	
			break;
		case playSys:			
			playSysAudio( arg );
			break;
		case playSubj:			
			playSubjAudio( arg );
			break;
		case startRec:
			startRecAudio( arg );
			break;
		case pausePlay:		// pause/play all share
		case resumePlay:		
		case pauseRec:	
		case resumeRec:
			pauseResume();
			break;
		case playRec:
			playRecAudio();
//			logEvt( "playRec" );
			break;
		case saveRec:
			saveRecAudio( arg );
//			logEvtS( "saveRec", arg );
			break;
		case finishRec:
			stopRecording(); 
//			logEvt( "stopRec" );
			break;
		case writeMsg:
			if ( RunQCTest )
				svQCresult( arg );		// saves 'arg' to system/QC_PASS.txt if it starts with "QC_OK", or as QC_FAIL.txt otherwise
			else
				saveWriteMsg( arg );
			break;
		case filesTest:
			QCfilesTest( arg );		// generates event FilesSuccess or FilesFail
			break;
		case stopPlay:
			stopPlayback(); 
//			logEvt( "stopPlay" );
			break;
		case volAdj:			
			adjVolume( iarg );	
			break;
		case spdAdj:			
			adjSpeed( iarg );
			break;
		case posAdj:			
			adjPlayPosition( iarg );
			break;
		case startUSB:
			USBmode( true );
			break;
		case endUSB:
			USBmode( false );
			break; 
		case subjAdj:			
			adjSubj( iarg );	
			break;
		case msgAdj:			
			adjMsg( iarg );		
			break;
		case goPrevSt:
			setCurrState( TBook.iPrevSt );		// return to prevSt without transition
		  // remembers this state as 'prevState' -- does that matter?
			break;
		case saveSt:   		// remember state that preceded this one (for possible future return)
			if ( iarg > 4 ) iarg = 4;
		  assertValidState( TBook.iPrevSt );
			TBook.iSavedSt[ iarg ] = TBook.iPrevSt;
			dbgLog( "C svSt[%d] = %s(%d) \n", iarg, TBook.prevStateName, TBook.iPrevSt );
			break;
		case goSavedSt:
			if ( iarg > 4 ) iarg = 4;
		  assertValidState( TBook.iSavedSt[ iarg ] );
		  setCurrState( TBook.iSavedSt[ iarg ] );		// return to savedSt without transition (updates prevSt)
//			TBook.iNextSt = TBook.iSavedSt[ iarg ];
		  // BE: I'm not sure this is right, but it has the side effect of ending the while loop in changeCSMstate.
		  // In general, when we return to a saved state, I don't think we want to execute the entrance
		  // actions for that state.
//		  TBook.iCurrSt = TBook.iNextSt;
			break;
		case setTimer:
			osTimerStart( timers[2], iarg );
			break;
		case resetTimer:
			osTimerStop( timers[2] );
			break;
		case showCharge:
			showBattCharge();
			break;
		case powerDown:		
			powerDownTBook();
			break;
	  case sysTest:
            playSqrTune("CDEF_**C*D*E*F*_**C*D*E*F*_***C**G**FDEH**G**");
			dbgLog( "writing tbook_csm.c \n" );
			writeCSM(); //controlTest();
			break;
	  case playNxtPkg:
			playNxtPackage();
			break;
	  case changePkg:
			changePackage();
			break;
	  case playTune:
			playSqrTune( arg );
			break;
	  case sysBoot:
			NVIC_SystemReset();			// soft reboot
			break;
		default:				break; 
	}
}

// ------------- interpret TBook CSM 
static void						changeCSMstate( short nSt, short lastEvtTyp ){
	dbgEvt( TB_csmChSt, nSt, 0,0,0 );
	if (nSt==TBook.iCurrSt){  // no-op transition -- (default for events that weren't defined in control.def) 
		//dbgLog( "C %s(%d): %s => %s \n", TBook.cSt->nm, TBook.iCurrSt, eventNm( (Event)lastEvtTyp), TBookCSM[nSt]->nm );
		//logEvtNSNS( "Noop_evt", "state",TBook.cSt->nm, "evt", eventNm( (Event)lastEvtTyp) ); //DEBUG
		return;
	}

	setCurrState( nSt );    // .iPrevSt = .iCurrSt, .iCurrSt = nSt, .cSt=TBookCSM[nSt]
	// now 'in' (executing) state nSt

	/*DEBUG*/  // Update status strings inside TBook -- solely for visibility in the debugger
	for (int e=eNull; e < eUNDEF; e++){  				//DBG: fill in dynamic parts of transition table for current state
		short iState = TBook.cSt->evtNxtState[ e ];//DBG:
		TBook.evtNxtSt[e].nxtSt = iState;
		TBook.evtNxtSt[e].nstStNm = TBookCSM[ iState ]->nm;
	}
	/*END DEBUG*/
	
	int nActs = TBook.cSt->nActions;
	for ( short i=0; i<nActs; i++ ){					// For each action defined on the state...
		Action act = TBook.cSt->Actions[i].act;  // unpack action, and arg
		char * arg = TBook.cSt->Actions[i].arg;
		if (arg==NULL) arg = ""; 								// make sure its a string for digit test
		int iarg = arg[0]=='-' || isdigit( arg[0] )? atoi( arg ) : 0;		// Parse the argument if it looks like it might be numeric.

		doAction( act, arg, iarg );		// And invoke the action.
	  // NB: actions goPrevSt & goSavedSt change TBook.iCurrSt
	}
}

static void						tbTimer( void * eNum ){
	switch ((int)eNum){
		case 0:		sendEvent( ShortIdle,  0 );  	break;
		case 1:		sendEvent( LongIdle,  0 );  	break;
		case 2:		sendEvent( Timer,  0 );  		break;
	}
}


void 									executeCSM( void ){								// execute TBook control state machine
	TB_Event *evt;
	osStatus_t status;
	
	TBook.volume = TB_Config.default_volume;
	TBook.iSubj = -1; // makes "next subject" go to the first subject.
	TBook.iMsg = 0;

	// set initialState & do actions
	TBook.iCurrSt = -1;		// so initState (which has been assigned to 0) will be different
	TBook.currStateName = "<start>";
	
	if ( RunQCTest )
		changeCSMstate( TB_Config.qcTestState, 0 );
	else
		changeCSMstate( TB_Config.initState, 0 );
    int asCnt = 0;
	
	while (true){
		evt = NULL;
	    status = osMessageQueueGet( osMsg_TBEvents, &evt, NULL, osWaitForever );  // wait for next TB_Event
		if (status != osOK) 
			tbErr(" EvtQGet error");
		TBook.lastEvent = evt->typ;  // has to be copied, used after 'evt' is freed
		TBook.lastEventName = eventNm( TBook.lastEvent );
		
//		logEvtNSNI( "csmEvent", "typ", TBook.lastEventName, "dnMS", evt->arg );
		switch ( evt->typ ){
			case AudioDone:
				osTimerStart( timers[0], TB_Config.shortIdleMS );
				osTimerStart( timers[1], TB_Config.longIdleMS );
				break;
			case ShortIdle:			// events that don't reset idle timers
			case LowBattery:
			case BattCharging:
			case BattCharged:
			case FilesSuccess:
			case FilesFail:
				break;
			case AudioStart:
				asCnt++;
				break;
			default:
				osTimerStop( timers[0] );
				osTimerStop( timers[1] );
				break;
		}
		short nSt = TBook.cSt->evtNxtState[ TBook.lastEvent ];
		assertValidState( nSt );
		if ( nSt != TBook.iCurrSt ) // state is changing (unless goPrevSt or goSavedSt undoes it)
			logEvtNSNSNS( "csmEvt", "st", TBook.cSt->nm, "evt", TBook.lastEventName, "nSt", TBookCSM[ nSt ]->nm );

		osMemoryPoolFree( TBEvent_pool, evt );
		changeCSMstate( nSt, TBook.lastEvent );	// only changes if different
	}
}


static void 					eventTest(  ){										// report Events until DFU (pot table)
	TB_Event *evt;
	osStatus_t status;

	dbgLog( "EvtTst starPlus\n" );
	while (true) {
	  status = osMessageQueueGet( osMsg_TBEvents, &evt, NULL, osWaitForever );  // wait for next TB_Event
		if (status != osOK) 
			tbErr(" EvtQGet error");

	  TBook.lastEventName = eventNm( evt->typ );
		dbgLog( " %s \n", eventNm( evt->typ ));
		dbgEvt( TB_csmEvt, evt->typ, tbTimeStamp(), 0, 0);
		logEvtNS( "evtTest", "nm",eventNm( evt->typ) );

		if ( evt->typ == Tree ) 
			osTimerStart( timers[0], TB_Config.shortIdleMS );
		if ( evt->typ == Pot ) 
			osTimerStart( timers[1], TB_Config.longIdleMS );
		if ( evt->typ == Table ) 
			osTimerStart( timers[2], 20000 );	// 20 sec

		if ( evt->typ == starPlus ) return;
	}
}

void 									initControlManager( void ){				// initialize control manager 	
	EventRecorderEnable(  evrEA, 	  		TB_no, TBsai_no ); 	// TB, TBaud, TBsai  
	EventRecorderDisable( evrAOD, 			EvtFsCore_No,   EvtFsMcSPI_No );  //FileSys library 
	EventRecorderDisable( evrAOD, 	 		EvtUsbdCore_No, EvtUsbdEnd_No ); 	//USB library 
	
	if ( !PrecompiledCSM ){
		TB_Config.default_volume = 5; 		// lower for TB_V2_R3
		TB_Config.powerCheckMS = 10000;				// set by setPowerCheckTimer()
		TB_Config.shortIdleMS = 3000;
		TB_Config.longIdleMS = 11000;
		TB_Config.systemAudio = "M0:/system/audio/";			// path to system audio files
		TB_Config.minShortPressMS = 30;				// used by inputmanager.c
		TB_Config.minLongPressMS = 900;				// used by inputmanager.c
		TB_Config.bgPulse 				= (char *) bgPulse;
		TB_Config.fgPlaying 			= (char *) fgPlaying;
		TB_Config.fgPlayPaused		= (char *) fgPlayPaused;
		TB_Config.fgRecording			= (char *) fgRecording;
		TB_Config.fgRecordPaused	= (char *) fgRecordPaused;
		TB_Config.fgSavingRec			= (char *) fgSavingRec;
		TB_Config.fgSaveRec				= (char *) fgSaveRec;
		TB_Config.fgCancelRec			= (char *) fgCancelRec;
		TB_Config.fgUSB_MSC				= (char *) fgUSB_MSC;
		TB_Config.fgUSBconnect		= (char *) fgUSBconnect;
		TB_Config.fgTB_Error			= (char *) fgTB_Error;
		TB_Config.fgNoUSBcable		= (char *) fgNoUSBcable;
		TB_Config.fgPowerDown			= (char *) fgPowerDown;
		
		initTknTable();
		if ( TBDataOK )		// control.def exists
			readControlDef( );						// reads TB_Config settings
	}

	if ( nCSMstates > 0 ) {    // have a CSM definition
		ledBg( TB_Config.bgPulse );		// reset background pulse according to TB_Config

		iPkg = 0;
		if ( RunQCTest ){
			nPackages = 0;
		} else {			// don't load package for qcTest
			findPackages( );		// sets iPkg & TBPackage to shortest name
		}
		
  	TBook.iSubj = -1; // makes "next subject" go to the first subject.
		TBook.iMsg = 1;
		
		// don't init power timer-- it is set to do 1st check at 15sec, then resets from TB_Config
		//setPowerCheckTimer( TB_Config.powerCheckMS );		
        
		setVolume( TB_Config.default_volume );					// set initial volume
		
		for ( int it=0; it<3; it++ ){
			timers[it] = osTimerNew( tbTimer, osTimerOnce, (void *)it, NULL );
			if ( timers[it] == NULL ) 
				tbErr( "timer not alloc'd" );
		}
		for (Event e=eNull; e < eUNDEF; e++){  //DBG fill in static parts of debug transition table
			TBook.evtNxtSt[e].evt = e;
			TBook.evtNxtSt[e].evtNm = eventNm( e );
		}
		TBook.prevStateName = "<start>";
		executeCSM();	
		
	} else if ( FileSysOK ) {		// FS but no CSM, go into USB mode
		logEvt( "NoFS USBmode" );
		USBmode( true );
	} else {  // no FileSystem
		eventTest();
	}
}
// contentmanager.cpp 
