
// TBookV2  controlmanager.c
//   Gene Ball  May2018

#include "controlmanager.h"

#include "tbook.h"				// enableMassStorage 
#include "mediaPlyr.h"		// audio operations
#include "inputMgr.h"			// osMsg_TBEvents
#include "fileOps.h"			// decodeAudio

extern bool FSysPowerAlways;
extern bool SleepWhenIdle;

static uint32_t 			startPlayingTS;
static uint32_t 			PlayLoopBattCheck = 30*60*1000;		// 30min
void 					controlTest(  ){									// CSM test procedure
	TB_Event *evt;
	osStatus_t status;
	assertValidState(TB_Config.initState );
	TBook.iCurrSt = TB_Config.initState;
	TBook.cSt = TBookCSM[ TBook.iCurrSt ];
	TBook.currStateName = TBook.cSt->nm;	//DEBUG -- update currSt string
  bool playforever = false;
	bool recordingMsg = false;
	MediaState audSt;
	
	dbgLog( "CTest: \n" );
	while (true) {
	  status = osMessageQueueGet( osMsg_TBEvents, &evt, NULL, osWaitForever );  // wait for next TB_Event
		if (status != osOK) 
			tbErr(" EvtQGet error");
		
	  TBook.lastEventName = eventNm( evt->typ );
		dbgLog( "C %s ", eventNm( evt->typ ));
		dbgEvt( TB_csmEvt, evt->typ, 0, 0, 0);
		if (isMassStorageEnabled()){		// if USB MassStorage running: ignore events
			if ( evt->typ==starHome || evt->typ==starCircle )
				USBmode( false );
			else
				dbgLog("starHome to exit USB \n" );
			
		} else if ( recordingMsg ){
			audSt = audGetState();
			switch ( evt->typ ){
				case Table:
				case starTable:
					if ( audSt==Recording )
						stopRecording( );
					break;
				case Star:
					if ( audSt==Recording )
						pauseResume();
					break;
					
				case AudioDone:
				default:
					break;
					
				case Pot:
					if ( audSt==Recording )
						stopRecording( );
					playRecAudio();
					break;
				case Lhand:
					if ( audSt==Recording )
						stopRecording( );
					saveRecAudio( "sv" );
					recordingMsg = false;
					break;
				case Rhand:
					if ( audSt==Recording )
						stopRecording( );
					saveRecAudio( "del" );
					recordingMsg = false;
					break;
				case starHome:
					dbgLog( "going to USB mass-storage mode \n");
					USBmode( true );
					break;
			}
		} else {
			tbSubject * tbS = TBPkg->TBookSubj[ TBook.iSubj ];
			switch (evt->typ){
				case Tree:
					playSysAudio( "welcome" );
					dbgLog( "PlaySys welcome...\n" );
					break; 
				case AudioDone:
				case starPot: 
				case Pot:
					if ( evt->typ==AudioDone && !playforever ) break;
					if ( evt->typ==starPot ){
						playforever = !playforever;
						showBattCharge();
						if ( playforever ){
							startPlayingTS = tbTimeStamp();
							logEvt("PlayLoopStart");
						} else
							logEvt("PlayLoopEnd");
					} 
					if ( tbTimeStamp() - startPlayingTS > PlayLoopBattCheck ){
						showBattCharge();
						startPlayingTS = tbTimeStamp();
					}						
					dbgLog( "Playing msg...\n" );
					TBook.iMsg++;
					if ( TBook.iMsg >= tbS->NMsgs ) TBook.iMsg = 0;
					playSubjAudio( "msg" );
					break; 
				case Circle: 
					TBook.iSubj++;
					if ( TBook.iSubj >= TBPkg->nSubjs  ) TBook.iSubj = 0;
					tbS = TBPkg->TBookSubj[ TBook.iSubj ];
				  TBook.iMsg = 0;
					playSubjAudio( "nm" );
					break;
				case Plus:
					adjVolume( 1 );
					break;
				case Minus:
					adjVolume( -1 );
					break;
				case Lhand:
					audSt = audGetState();
					if ( audSt==Playing ){
						adjPlayPosition( -2 );
						logEvt( "JumpBack2" );
					} 
					break;
				case Rhand:
					if ( audSt==Playing ){
						adjPlayPosition( 2 );
						logEvt( "JumpFwd2" );
					}
					break;
				
				case Star:
					audSt = audGetState();
					if ( audSt==Playing )
						pauseResume();
					else
						showBattCharge();
					break;
					
				case Table:		// Record
					recordingMsg = true;
					startRecAudio( NULL );
					break;
				
				case starRhand:
					powerDownTBook( true );
					osTimerStart( timers[1], TB_Config.longIdleMS );
					playSysAudio( "faster" );
					break;
				case starLhand:
					FSysPowerAlways = true;
					SleepWhenIdle = false;
					break;
				case LongIdle:
					playSysAudio( "slower" );
					saveWriteMsg( "Clean power down" );
				  break;

				
				case starTree:
					playNxtPackage( );		// bump iPkg & plays next package name 
					showPkg();
					break;
				
				case starTable:		// switch packages to iPkg
					changePackage();
					showPkg();
					dbgLog(" iPkg=%d  iSubj=%d  iMsg=%d \n", iPkg, TBook.iSubj, TBook.iMsg );
					break;
				
				case starPlus:	
					decodeAudio();
					//eventTest();
					break;
				
				case starMinus: 
					executeCSM();
					break;
					
				case starCircle:
					saveWriteMsg( "Clean power down" );
					powerDownTBook( false );
				  break;
				
				case starHome:
					dbgLog( "going to USB mass-storage mode \n");
					USBmode( true );
					break;
				
				case FirmwareUpdate:   // pot table
					dbgLog( "rebooting to system bootloader for DFU... \n" );
					ledFg( TB_Config.fgEnterDFU );
					tbDelay_ms( 3300 );  // about to go into DFU
					RebootToDFU(  );
					break;
				default:
					dbgLog( "evt: %d %s \n", evt->typ, eventNm( evt->typ ) );
			}
		}
		osMemoryPoolFree( TBEvent_pool, evt );
	}
}

const int padLEN = 22;
char * padStr( char *sv, const char *s ){
	int st = padLEN- strlen( s )-2;
	for ( int i=0; i<st; i++ ) sv[i] = ' ';
	sprintf( &sv[st], "\"%s\"", s );
	return sv;
}
void writeCSM(){			// write current CSM to tbook_csm.c
	char sv[ padLEN+4 ]; 
	FILE * f = tbOpenWrite( "tbook_csm_def.h" );
	const char *aNms[] = {
		"aNull", "LED", "bgLED", "playSys", "playSubj", "pausePlay", "resumePlay", "stopPlay", "volAdj", "spdAdj", "posAdj",
		"startRec", "pauseRec", "resumeRec", "finishRec", "playRec", "saveRec", "writeMsg",
		"goPrevSt", "saveSt", "goSavedSt", "subjAdj", "msgAdj", "setTimer", "resetTimer", "showCharge",
		"startUSB", "endUSB", "powerDown", "sysBoot", "sysTest", "playNxtPkg", "changePkg" 
	};
	
	fprintf( f, "//TBook Control State Machine definition--   tbook_csm.def \n" );
	fprintf( f, "// generated by writeCSM() \n" );
	fprintf( f, "#include \"controlmanager.h\" \n" );
	
	fprintf( f, "TBConfig_t  TB_Config = { \n  " );
	fprintf( f, "%22d,   // %s \n  ", 		TB_Config.default_volume, 	"default_volume" );
	fprintf( f, "%22d,   // %s \n  ", 		TB_Config.default_speed, 		"default_speed" );
	fprintf( f, "%22d,   // %s \n  ", 		TB_Config.powerCheckMS, 		"powerCheckMS" );
	fprintf( f, "%22d,   // %s \n  ", 		TB_Config.shortIdleMS, 			"shortIdleMS" );
	fprintf( f, "%22d,   // %s \n  ", 		TB_Config.longIdleMS, 			"longIdleMS" );
	fprintf( f, "%s,   // %s \n  ", 			padStr(sv, TB_Config.systemAudio), 			"systemAudio" );
	fprintf( f, "%22d,   // %s \n  ", 		TB_Config.minShortPressMS,	"minShortPressMS" );
	fprintf( f, "%22d,   // %s \n  ", 		TB_Config.minLongPressMS, 	"minLongPressMS" );
	fprintf( f, "%22d,   // %s \n  ", 		TB_Config.initState, 				"initState" );
	fprintf( f, "%s,   // %s \n  ", 			padStr(sv, TB_Config.bgPulse ), 				"bgPulse" );
	fprintf( f, "%s,   // %s \n  ", 			padStr(sv, TB_Config.fgPlaying ), 			"fgPlaying" );
	fprintf( f, "%s,   // %s \n  ", 			padStr(sv, TB_Config.fgPlayPaused ), 		"fgPlayPaused" );
	fprintf( f, "%s,   // %s \n  ", 			padStr(sv, TB_Config.fgRecording ), 		"fgRecording" );
	fprintf( f, "%s,   // %s \n  ", 			padStr(sv, TB_Config.fgRecordPaused ), 	"fgRecordPaused" );
	fprintf( f, "%s,   // %s \n  ", 			padStr(sv, TB_Config.fgSavingRec ), 		"fgSavingRec" );
	fprintf( f, "%s,   // %s \n  ", 			padStr(sv, TB_Config.fgSaveRec ), 			"fgSaveRec" );
	fprintf( f, "%s,   // %s \n  ", 			padStr(sv, TB_Config.fgCancelRec ), 		"fgCancelRec" );
	fprintf( f, "%s,   // %s \n  ", 			padStr(sv, TB_Config.fgUSB_MSC ), 			"fgUSB_MSC" );
	fprintf( f, "%s,   // %s \n  ", 			padStr(sv, TB_Config.fgTB_Error ), 			"fgTB_Error" );
	fprintf( f, "%s,   // %s \n  ", 			padStr(sv, TB_Config.fgNoUSBcable ), 		"fgNoUSBcable" );
	fprintf( f, "%s,   // %s \n  ", 			padStr(sv, TB_Config.fgUSBconnect ), 		"fgUSBconnect" );
	fprintf( f, "%s,   // %s \n  ", 			padStr(sv, TB_Config.fgPowerDown ), 		"fgPowerDown" );
	fprintf( f, "%s    // %s \n", 			  padStr(sv, TB_Config.fgEnterDFU ), 			"fgEnterDFU" );
  fprintf( f, "};  // TB_Config \n\n" );
	
	fprintf( f, "int   nPlaySys = %d;  // PlaySys prompts used by CSM \n", nPlaySys );
	
	fprintf( f, "SysAudio_t	SysAudio[] = { \n" );
	for ( int i=0; i<nPlaySys; i++ )
		fprintf( f, "  { \"%s\",   \"%s\" }%c \n", SysAudio[i].sysNm, SysAudio[i].sysPath, i<nPlaySys-1? ',':' ' );
  fprintf( f, "};  // SysAudio \n\n" );

	fprintf( f, "int   nCSMstates = %d;  // TBook state machine definition \n", nCSMstates );
	
	for ( int i=0; i<nCSMstates; i++ ){
		csmState * st = TBookCSM[i];
	  fprintf( f, "csmState %s =  // TBookCMS[%d] \n",   st->nm, i );
		fprintf( f, "  { 0, \"%s\", { \n    ", st->nm );
		for ( Event j=eNull; j<eUNDEF; j++ )
			fprintf( f, "%2d, %s", st->evtNxtState[j], j==starHome? "\n    " : "" );
		int nA = st->nActions;
		fprintf( f, " },\n    %d%c\n", nA, nA>0? ',':' ' );
		if ( nA > 0 ){
			fprintf( f, "    { \n" );
			for ( int k = 0; k<nA; k++ ){
				Action act = st->Actions[k].act;
				char *arg = st->Actions[k].arg;
				if (arg==NULL) arg = "";
				fprintf( f, "      { %10s, \"%s\" }%s \n", aNms[ act ], arg, k==nA-1? "":"," );
			}
			fprintf( f, "    }\n" );
		}
		fprintf( f, "  }; \n" );
	}
	fprintf( f, "\ncsmState * TBookCSM[] = { \n" );
	for ( int i=0; i<nCSMstates; i++ ){
		csmState * st = TBookCSM[i];
		fprintf( f, "&%s%s  // TBookCSM[%d]\n", st->nm, i==nCSMstates-1? "":",", i );
	}
	fprintf( f, "};  // end of TBookCSM.c \n" );
	fclose(f);
}

