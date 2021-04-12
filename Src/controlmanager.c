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

// TBook Control State Machine
int 	 								nCSMstates = 0;
csmState *						TBookCSM[ MAX_CSM_STATES ];		// TBook state machine definition

// TBook content packages
int										nPackages = 0;								// number of packages found
TBPackage_t *					TBPackage[ MAX_PKGS ];				// package info
int										iPkg = 0;											// package index 
TBPackage_t * 				TBPkg;												// content package in use

// TBook configuration variales
TBConfig_t 						TB_Config;


/*
struct {							// CSM state variables
	short 		iCurrSt;	// index of current csmState
	char * 		currStateName;	// str nm of current state
	Event		lastEvent;		// str nm of last event
	char *		lastEventName;	// str nm of last event
	char *		evtNms[ eUNDEF ];
	char *		nxtEvtSt[ eUNDEF ];
	
	short		iPrevSt;	// idx of previous state
	short		iNextSt;	// nextSt from state machine table ( can be overridden by goBack, etc )
	short		iSavedSt[5];	// possible saved states
  
	short		volume;
	short		speed;
  
	short		iSubj;		// index of current Subj
	short		iMsg;		// index of current Msg within Subj

	csmState *  cSt;		// -> TBookCSM[ iCurrSt ]
	
}	TBook;
*/
TBook_t TBook;

osTimerId_t  	timers[3]; 	// ShortIdle, LongIdle, Timer
	
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


void 					playSubjAudio( char *arg ){				// play current Subject: arg must be 'nm', 'pr', or 'msg'
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
	} else if ( strcasecmp( arg, "pr" )==0 ){
		nm = tbS->audioPrompt;
		logEvtNSNS( "Play", "Subj", tbS->name, "pr", nm ); 
	} else if ( strcasecmp( arg, "msg" )==0 ){
		nm = tbS->msgAudio[ TBook.iMsg ];
		logEvtNSNI( "Play", "Subj", tbS->name, "iM", TBook.iMsg ); //, "aud", nm ); 
		stats = loadStats( tbS->name, TBook.iSubj, TBook.iMsg );	// load stats for message
	}
	buildPath( path, tbS->path, nm, ".wav" ); //".ogg" );
	playAudio( path, stats );
}


void 					playNxtPackage( ){										// play name of next available Package 
	iPkg++;
	if ( iPkg >= nPackages ) iPkg = 0;
	
	TBPackage_t * pkg = TBPackage[ iPkg ];
	logEvtNSNI( "PlayPkg", "Pkg", pkg->packageName, "Idx", iPkg );  
	playAudio( pkg->packageName, NULL );
}


void 					showPkg( ){										// debug print Package iPkg
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


void						changePackage(){											// switch to last played package name
	TBPkg = TBPackage[ iPkg ];
	TBook.iSubj = -1; // makes "next subject" go to the first subject.
	TBook.iMsg = 0;
	logEvtNS( "ChgPkg", "Pkg", TBPkg->packageName );  
	showPkg();
}


void 					playSysAudio( char *arg ){				// play system file 'arg'
	char path[MAX_PATH];
	resetAudio();
	buildPath( path, TB_Config.systemAudio, arg, ".wav" ); //".ogg" );
	playAudio( path, NULL );
	logEvtNS( "PlaySys", "file", arg );
}


void						startRecAudio( char *arg ){
	resetAudio();
	tbSubject * tbS = TBPkg->TBookSubj[ TBook.iSubj ];
	char path[MAX_PATH];
	int mCnt = 0;
	char * fNm = logMsgName( path, tbS->name, TBook.iSubj, TBook.iMsg, ".wav", &mCnt ); //".ogg" );		// build file path for next audio msg for S<iS>M<iM>
	logEvtNSNINI( "Record", "Subj", tbS->name, "iM", TBook.iMsg, "cnt", mCnt );
	MsgStats *stats = loadStats( tbS->name, TBook.iSubj, TBook.iMsg );	// load stats for message
	recordAudio( fNm, stats );
}


void   				playRecAudio(){
	playRecording();
}


void						saveRecAudio( char *arg ){
	if ( strcasecmp( arg, "sv" )==0 ){
		saveRecording();
	} else if ( strcasecmp( arg, "del" )==0 ){
		cancelRecording();
	} 
}


void						saveWriteMsg( char *txt ){				// save 'txt' in Msg file
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


void 					USBmode( bool start ){						// start (or stop) USB storage mode
	if ( start ){
		logEvt( "enterUSB" );
		logPowerDown();				// flush & shut down logs
		enableMassStorage( "M0:", NULL, NULL, NULL );
	} else {	
		disableMassStorage();
		logEvt( "exitUSB" );
		ledFg( "_" );
		logPowerUp( false );
		if ( FirstSysBoot )  // should run in background
			decodeAudio();
	} 
}


//int						stIdx( int iSt ){
//	if ( iSt < 0 || iSt >= nCSMstates )
//		tbErr("invalid iSt");
//	return iSt;
//}

void 						writeCSM( void );			// write current CSM to tbook_csm.c

void assertValidState(int stateIndex) {
	if ( stateIndex < 0 || stateIndex >= nCSMstates )
		tbErr("invalid state index");
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
			logEvt( "playRec" );
			break;
		case saveRec:
			saveRecAudio( arg );
			logEvtS( "saveRec", arg );
			break;
		case finishRec:
			stopRecording(); 
			logEvt( "stopRec" );
			break;
		case writeMsg:
			saveWriteMsg( arg );
			break;
		case stopPlay:
			stopPlayback(); 
			logEvt( "stopPlay" );
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
			assertValidState(TBook.iPrevSt);
			TBook.iCurrSt = TBook.iNextSt = TBook.iPrevSt;		// return to prevSt without transition
			break;
		case saveSt:
			if ( iarg > 4 ) iarg = 4;
		  assertValidState( TBook.iPrevSt );
			TBook.iSavedSt[ iarg ] = TBook.iPrevSt;
			break;
		case goSavedSt:
			if ( iarg > 4 ) iarg = 4;
		  assertValidState(TBook.iSavedSt[ iarg ]);
			TBook.iNextSt = TBook.iSavedSt[ iarg ];
		  // BE: I'm not sure this is right, but it has the side effect of ending the while loop in changeCSMstate.
		  // In general, when we return to a saved state, I don't think we want to execute the entrance
		  // actions for that state.
		  TBook.iCurrSt = TBook.iNextSt;
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
			ledBg( NULL );								// turn off background heartbeat
			powerDownTBook( true ); //false );
			break;
	  case sysTest:
			dbgLog( "writing tbook_csm.c \n" );
			writeCSM(); //controlTest();
			break;
	  case playNxtPkg:
			playNxtPackage();
			break;
	  case changePkg:
			changePackage();
			break;
	  case sysBoot:
			
			break;
		default:				break; 
	}
}


// ------------- interpret TBook CSM 
static void						changeCSMstate( short nSt, short lastEvtTyp ){
	dbgEvt( TB_csmChSt, nSt, 0,0,0 );
	assertValidState(nSt);
	if (nSt==TBook.iCurrSt) dbgLog( "C %s(%d): %s => %s \n", TBook.cSt->nm, TBook.iCurrSt, eventNm( (Event)lastEvtTyp), TBookCSM[nSt]->nm );
		//logEvtNSNS( "Noop_evt", "state",TBook.cSt->nm, "evt", eventNm( (Event)lastEvtTyp) ); //DEBUG
	
	// We twiddle with nSt and with iCurrSt in various ways. 
	while ( nSt != TBook.iCurrSt ){
		assertValidState(TBook.iCurrSt);
		assertValidState(nSt);
		TBook.iPrevSt = TBook.iCurrSt;
		TBook.iCurrSt = nSt;
		
		// The state definition for the "current" state (it's really the next state).
		csmState *stateDef = TBookCSM[ TBook.iCurrSt ];
		TBook.cSt = stateDef;
		TBook.currStateName = stateDef->nm;	//DEBUG -- update currSt string
		
		// Update status strings inside TBook -- solely for visibility in the debugger
		for ( Event e=eNull; e<eUNDEF; e++ ){	//DEBUG -- update nextSt strings
			// Aren't these names the same each and every time?
			TBook.evtNms[ e ] = eventNm( e );
			// What is the next state (index) for the event in the current state
			short iState = stateDef->evtNxtState[ e ];
			// If we get the event, what is the name of the next state?
			TBook.nxtEvtSt[ e ] = TBookCSM[ iState ]->nm;
		}
		
		// By default the next state is the current state. 
		TBook.iNextSt = TBook.iCurrSt;   // stay here unless something happens
		
		// Build a list of actions, for debugging and logging. Hope the buffer is big enough.
		// The actions are what we do when we enter a state.
		char Actions[200]; Actions[0] = '\0';
		char Act[80];
		int nActs = stateDef->nActions;
		// For each action defined on the state...
		for ( short i=0; i<nActs; i++ ){
			// What is the action, and any arguments.
			Action act = stateDef->Actions[i].act;
			char * arg = stateDef->Actions[i].arg;
			// Next three lines build a string for debugging.
			if (arg==NULL) arg = "";
			sprintf(Act, " %s:%s", actionNm(act), arg );
			strcat(Actions, Act);
			// Parse the argument if it looks like it might be numberic.
			int iarg = arg[0]=='-' || isdigit( arg[0] )? atoi( arg ) : 0;
			// And invoke the action.
			doAction( act, arg, iarg );
		}
		// Log the list of actions.
		// TODO: how is this useful in the log? If we're to do anything with the actions, they should
		// be logged individually.
	//	logEvtNSNSNS( "CSM_st", "ev", eventNm((Event)lastEvtTyp), "nm", stateDef->nm, "act", Actions );
		
		// If one of the actions was goSavedSt, TBook.iNextSt was changed as a side effect of doAction()
		// If one of the actions was goPrevSt, TBook.iCurrSt was also changed as a side effect.
    //   - we set nSt to iNextSt here, if iCurrSt is also set, we will exit the loop that's looking for states.		
		nSt = TBook.iNextSt;		// in case Action set it
	}

}


static void						tbTimer( void * eNum ){
	switch ((int)eNum){
		case 0:		sendEvent( ShortIdle,  0 );  	break;
		case 1:		sendEvent( LongIdle,  0 );  	break;
		case 2:		sendEvent( Timer,  0 );  		break;
	}
}


void 					executeCSM( void ){								// execute TBook control state machine
	TB_Event *evt;
	osStatus_t status;
	
	TBook.volume = TB_Config.default_volume;
	TBook.speed = TB_Config.default_speed;
	TBook.iSubj = -1; // makes "next subject" go to the first subject.
	TBook.iMsg = 0;

	// set initialState & do actions
	TBook.iCurrSt = 1;		// so initState (which has been assigned to 0) will be different
	changeCSMstate( TB_Config.initState, 0 );
	
	while (true){
		evt = NULL;
	    status = osMessageQueueGet( osMsg_TBEvents, &evt, NULL, osWaitForever );  // wait for next TB_Event
		if (status != osOK) 
			tbErr(" EvtQGet error");
		char * eNm = eventNm( evt->typ );
		logEvtNSNI( "csmEvent", "typ", eNm, "dnMS", evt->arg );
		TBook.lastEvent = evt->typ;
		TBook.lastEventName = eNm;
 
		switch ( evt->typ ){
			case AudioDone:
				osTimerStart( timers[0], TB_Config.shortIdleMS );
				osTimerStart( timers[1], TB_Config.longIdleMS );
				break;
			case ShortIdle:			// events that don't reset idle timers
			case LowBattery:
			case BattCharging:
			case BattCharged:
				break;
			default:
				osTimerStop( timers[0] );
				osTimerStop( timers[1] );
				break;
//			case starPlus:  
//				USBmode( true );
//				break;
		}
		short lastEvtTyp = evt->typ;
		assertValidState( TBook.cSt->evtNxtState[ evt->typ ]);
		short nSt = TBook.cSt->evtNxtState[ evt->typ ];
		osMemoryPoolFree( TBEvent_pool, evt );
		changeCSMstate( nSt, lastEvtTyp );	// only changes if different
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
	// init to odd values so changes are visible
	TB_Config.default_volume = 5; 		// lower for TB_V2_R3
	TB_Config.default_speed = 3;
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
	TB_Config.fgEnterDFU			= (char *) fgDFU;

	EventRecorderEnable(  evrEA, 	  		TB_no, TBsai_no ); 	// TB, TBaud, TBsai  
	EventRecorderDisable( evrAOD, 			EvtFsCore_No,   EvtFsMcSPI_No );  //FileSys library 
	EventRecorderDisable( evrAOD, 	 		EvtUsbdCore_No, EvtUsbdEnd_No ); 	//USB library 
	
	initTknTable();
	if ( TBDataOK ) {
		readControlDef( );						// reads TB_Config settings
		ledBg( TB_Config.bgPulse );		// reset background pulse according to TB_Config

		findPackages( );		// sets iPkg & TBPackage to shortest name
		
  	TBook.iSubj = -1; // makes "next subject" go to the first subject.
		TBook.iMsg = 1;
		
		// power timer is set to input configuration by 1st 15sec check
		//setPowerCheckTimer( TB_Config.powerCheckMS );		
		setVolume( TB_Config.default_volume );					// set initial volume
		
		for ( int it=0; it<3; it++ ){
			timers[it] = osTimerNew( tbTimer, osTimerOnce, (void *)it, NULL );
			if ( timers[it] == NULL ) 
				tbErr( "timer not alloc'd" );
		}
		//controlTest();  //DEBUG
		executeCSM();	
		
	} else if ( FileSysOK ) {		// FS but no data, go into USB mode
		logEvt( "NoFS USBmode" );
		USBmode( true );
	} else {  // no FileSystem
		eventTest();
	}
}
// contentmanager.cpp 
