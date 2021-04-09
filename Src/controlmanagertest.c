
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

