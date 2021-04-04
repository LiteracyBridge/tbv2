// TBookV2    initialization
//	Apr2018 

#include "tbook.h"
#include "controlmanager.h"			// runs on initialization thread
#include "mediaPlyr.h"			// thread to watch audio status
#include "fs_evr.h"					// FileSys components
#include "fileOps.h"				// decode & encrypt audio files

const char * 	TBV2_Version 				= "V3.02 of 3-Apr-2021";

//
// Thread stack sizes
const int 		TBOOK_STACK_SIZE 		= 6144;		// init, becomes control manager
const int 		POWER_STACK_SIZE 		= 2048;
const int 		INPUT_STACK_SIZE 		= 1024;
const int 		MEDIA_STACK_SIZE 		= 4096;		// opens in/out files
const int 		FILEOP_STACK_SIZE 	= 6144;		// opens in/out files, mp3 decode
const int 		LED_STACK_SIZE 			= 512;

const int pCSM_VERS				= 0;
const int	pBOOTCNT 				= 1; 
const int	pCSM_DEF 				= 2;
const int	pLOG_TXT 				= 3;
const int	pSTATS_PATH 		= 4;
const int	pMSGS_PATH 			= 5;
const int	pLIST_OF_SUBJS 	= 6;
const int	pPACKAGE_DIR		= 7;
const int pPKG_VERS  		  = 8;
const int pAUDIO = 9;		// DEBUG
const int pLAST = 9;
char * TBP[] = {
		"M0:/system/version.txt",							//	pCSM_VERS				= 0;
		"M0:/system/bootcount.txt",						//	pBOOTCNT 				= 1;
		"M0:/system/control.def",							//	pCSM_DEF 				= 2;
		"M0:/log/tbLog.txt",									//	pLOG_TXT 				= 3;
		"M0:/stats/",													//	pSTATS_PATH 		= 4;
		"M0:/messages/",											//	pMSGS_PATH 			= 5;
		"M0:/package/list_of_subjects.txt",		//	pLIST_OF_SUBJS 	= 6;
		"M0:/package/",												//	pPACKAGE_DIR		= 7;
		"M0:/package/version.txt",						//	pPKG_VERS  		  = 8;
		"M0:/audio.wav"												//	pAUDIO = 9;		// DEBUG
};


//TBOOK error codes
const int 		TB_SUCCESS 						=	0;
const int 		GPIO_ERR_ALREADYREG 	=	1;
const int 		GPIO_ERR_INVALIDPIN 	=	2;

const int 		MEDIA_ALREADY_IN_USE	=	1;

const int 		MALLOC_HEAP_SIZE 			=	20000;
char 					MallocHeap[ 20000 ];    // MALLOC_HEAP_SIZE ];
bool					FileSysOK 						= false;
bool					TBDataOK 							= true;			// false if no TB config found


void setDev( char *fname, const char *dev ){  // replace front of fname with dev
	for (int i=0; i< strlen(dev); i++ )
	  fname[i] = dev[i];
}

static char * fsDevs[] = { "M0:", "M1:", "N0:", "F0:", "F1:", NULL };
static int    fsNDevs = 0;

void copyFile( const char *src, const char * dst ){
	FILE * fsrc = tbOpenRead( src ); 
	FILE * fdst = tbOpenWrite( dst ); 
	if ( fsrc==NULL || fdst==NULL ) return;
	char buf[512];
	while (true){
		int cnt = fread( buf, 1, 512, fsrc );
		if ( cnt==0 )	break;

		fwrite( buf, 1, 512, fdst );
	}
	tbCloseFile( fsrc );	
	tbCloseFile( fdst );	
}
int PlayDBG = 0;
int RecDBG = 0;
void 								saiEvent( uint32_t event );

//char dbgKeySt[6];
int upCnt = 0;
char lstKey,dnKey;

GPIO_ID keyDown( ){

	// gHOME, gCIRCLE, gPLUS,  gMINUS, gTREE, gLHAND, gRHAND, gPOT, gSTAR, gTABLE 
	GPIO_ID dnK = gINVALID;
	for ( GPIO_ID k = gHOME; k <= gTABLE; k++ ){
		if ( gGet( k ))
			dnK = k;
	}
	// 								gORANGE, gBLUE, gRED,	 gGREEN, gINVALID,	gHOME, gCIRCLE, gPLUS,  gMINUS, gTREE, gLHAND, gRHAND, gPOT,   gSTAR,  gTABLE
//	const char * 		dbgNm[]= { "Or", 	 "Bl",  "Rd",  "Gr",   "In",  	"Hm", 	"Ci",   "Pl",   "Mi",   "Tr",  "Lh",   "Rh",   "Po",   "St",   "Ta" };
	const char	dbgNm[]= { 'O', 'B','R','G','I','H','C','+','-','T','L','R','P','S','t' };
//	sprintf( dbgKeySt, "%c%c%4d", dbgNm[lstKey], dbgNm[dnKey], upCnt );
	dnKey = dbgNm[ dnK ];
	if ( dnK == gINVALID ){
		if ( upCnt < 1000 ) upCnt++;   // cnt since last key down
	} else if ( upCnt == 1000 ){ 		// first key down after all were solidly up
			upCnt = 0;		// reporting down click, reset up counter
			lstKey = dbgNm[ dnK ];
			return dnK;
		}
	return gINVALID;
}
extern int DebugMask;
void						cdc_Init( void );
void 						cdc_PowerDown( void );
void						cdc_RecordEnable( bool enable );
void 						cdc_SpeakerEnable( bool enable );
void		 				cdc_SetMute( bool muted );
void						cdc_SetMasterFreq( int freq );
void						showCdcRegs( bool always, bool nonReset ); //void						showCdcRegs( bool always );
void 						debugTimingRegs( bool );

void tglUSBmode(){
	if ( isMassStorageEnabled() ){
		disableMassStorage();
		gSet( gGREEN, 0 );	gSet( gRED, 0 );
	} else if ( fsNDevs > 0){  // HOME => if have a filesystem but no data -- try USB MSC
		gSet( gGREEN, 1 );	gSet( gRED, 1 );
		for ( int i=fsNDevs; fsDevs[i]!=NULL; i++ ) fsDevs[i] = NULL;
		enableMassStorage( fsDevs[0], fsDevs[1], fsDevs[2], fsDevs[3] );		// just put 1st device on USB
	}	
}
void tglDebugMask( int bit ){
	int d = 1 << (bit-1);
	if ( DebugMask & d )
		DebugMask &= ~d;		// reset
	else
		DebugMask |= d;			// set
	dbgLog("! Tgl %d DbgMsk= 0x%06x \n", bit, DebugMask );
}


int ts_recStart = 0;  // timestamp of recording start
int dbgIdx = 0;				// file index for current value of RecDBG

void CheckRecording(){
	if ( audGetState()==Recording ){ 
		int msec = tbTimeStamp()-ts_recStart;
		if (msec > 8000){
			audRequestRecStop();
			dbgLog(" RecStop\n");
			while ( audGetState() != Ready )
				gSet( gGREEN, 0 );
			gSet( gRED, 0 );
			resetAudio();
		}
	}
}
void PlayRecCmd( GPIO_ID k ){
	MsgStats tstStats;
	char fname[40];

	switch ( k ){
		case gTREE: 	
			if ( audGetState()==Ready ){
				dbgLog(" Tr: play welcome\n");
				gSet( gGREEN, 1 );	gSet( gRED, 0 );
				playWave( "M0:/system/audio/welcome.wav" );
				showCdcRegs( false, true); 		// regs during playback
			}
			break;
			
		case gCIRCLE: 
			if ( audGetState()==Ready ){
				sprintf( fname, "M0:/REC_%d_%d.WAV", RecDBG, dbgIdx );	
				FILE* outFP = tbOpenWriteBinary( fname ); 
				if ( outFP != NULL ){
					resetAudio();			// clean up anything in progress 
					dbgLog( " Cir: record 5sec to: %s  RecDbg=%d\n", fname, RecDBG );
					gSet( gGREEN, 0 );		gSet( gRED, 1 );
					ts_recStart = tbTimeStamp();
					audStartRecording( outFP, &tstStats );
					showCdcRegs( false, true ); 		// regs during record
					dbgIdx++;
				}		
			}
			break;
			
		case gLHAND:		// adjust debug flags mask
			tglDebugMask( RecDBG );
			break;
		case gRHAND:			// USB commands -- have FS and autoUSB or HOME -- enable
			tglUSBmode();
			break;
		
		case gTABLE: 	
			showCdcRegs( true, true );
			break;
		
		default: 						
			break;
	}
}
void setCodecFreq( int idx ){
	int speeds[] = {  0, 8000, 11025, 12000,  16000,  22050,  24000,  32000,  44100,  48000 };
	int freq = speeds[ (idx % 10) ]; 
	cdc_SetMasterFreq( freq ); 
	cdc_SetVolume(5); 
	cdc_SpeakerEnable( true );
	cdc_SetMute( false );
	dbgLog( " Cir: setFreq %d \n", freq );	
	showCdcRegs(true,true);	
}
void CodecCmd( GPIO_ID k ){
	static uint16_t freqCnt;
	static bool pwrOn=false, mutOn=false, spkrOn=false, recOn=false; 
	switch ( k ){
		case gTREE: 	// pwr
			pwrOn = !pwrOn;
			if (pwrOn){
				dbgLog( " Tr: codec pwr up \n");
				Driver_SAI0.PowerControl( ARM_POWER_FULL );		// power up audio
				uint32_t ctrl = ARM_SAI_CONFIGURE_TX | ARM_SAI_MODE_SLAVE  | ARM_SAI_ASYNCHRONOUS | ARM_SAI_PROTOCOL_I2S | ARM_SAI_DATA_SIZE(16);
				Driver_SAI0.Control( ctrl, 0, 8000 );	// set sample rate, init codec clock, power up speaker and unmute
			} else {
				dbgLog( " Tr: codec pwr down \n");
				cdc_PowerDown();		
			}
			break;
			
		case gCIRCLE: 
			freqCnt++;
			setCodecFreq( freqCnt ); 
			break;
			
		case gLHAND: 	
			mutOn = !mutOn;
			dbgLog( " LH: mute %s \n", mutOn? "on":"off" );	
			cdc_SetMute( mutOn ); 	
			break;
		case gPOT: 
			spkrOn = !spkrOn;
			dbgLog( " Pot: spkr enab %s \n", spkrOn? "on":"off" );	
			cdc_SpeakerEnable( spkrOn );
			break;
		case gRHAND: 	
			recOn = !recOn;
			dbgLog( " RH: record enab %s \n", recOn? "on":"off" );	
			cdc_RecordEnable( recOn ); 
			break;
		case gTABLE: 	
			showCdcRegs( true,true ); 
			break;
		default: 		
			break;
	}
}
void debugLoop( bool autoUSB ){			// called if boot-MINUS, no file system,  autoUSB => usbMode
	if ( fsNDevs==0 ) dbgLog( " no storage avail \n" );
	else dbgLog( " no TBook on %s \n", fsDevs[0]  );

	int ledCntr = 0; //, LEDpauseCnt = 0;
//	bool inTestSequence = false;
	RecDBG = 0;			// default recording params
	
	initLogger();			// init Event log
	logEvtNI( "DebugLoop", "NFSys", fsNDevs );
//	const char *spath = "M0:/local/en/;/Prompts/en/;/messages/en/";
//	char fpath[50];
//	char * dlp_nm = findOnPathList( fpath, spath, "debug_loop.wav" );
	
	if ( autoUSB && fsNDevs>0 && audGetState()==Ready )
		tglUSBmode();
	
	dbgLog(" DebugLoop: +/-: Dbg, St:chgMode, Ta: shRegs \n");
	
	const int nMDs = 2;
	char * cmdMd[] = { "Rec/Play: Tr: play, Cir: record, LH: mask, RH: USB", "Codec: Tr: pwr, Cir: freq, LH: mute, Pot: spkrEn, RH: recEn" };
	int iMd = 0;
	while ( true ){		// loop processing debug commands
		CheckRecording();
		
		GPIO_ID k = keyDown();   // key down
		switch ( k ){
			case gINVALID:  break;
			
			case gSTAR:  // switch modes
				iMd++; 
				if ( iMd==nMDs ) iMd=0;	
				dbgLog( " %s \n", cmdMd[iMd] );
				break;
			
			case gPLUS:
				RecDBG++;
				dbgLog( " + DBG: %d \n", RecDBG );
				dbgIdx = 0;
				break;
			
			case gMINUS:
				if (RecDBG>0) RecDBG--;	
				dbgLog( " - DBG: %d \n", RecDBG ); 
				dbgIdx = 0; 
			break;
			
			default:		// other keys depend on mode
				if (iMd==0) {
					PlayRecCmd( k );
				}
				else {
					CodecCmd( k );
				}
		}
		// LED control-- 
		if (audGetState()==Ready && !isMassStorageEnabled() ){		// blink LED while idle
			ledCntr++;
			gSet( gGREEN, ((ledCntr >> 14) & 0x3)== 0 );
			gSet( gRED, 	((ledCntr >> 14) & 0x3)== 2 );		// flash: Red off Green off  
		}
	}
}  // debugLoop

/*		// AUDIO recording
		if ( st==Recording ){
			if ( !gGet( gCIRCLE )){		// no CIRCLE while recording: stop
				audRequestRecStop();
				while ( audGetState() != Ready )
					gSet( gGREEN, 0 );
				gSet( gRED, 0 );
				resetAudio();
		//		gSet( gGREEN, 1 );
		//		tbDelay_ms(200); //make sure printDebug catches up
		//		audPlayAudio( fname, &tstStats );
		// 		gSet( gGREEN, 0 );
				LEDpauseCnt = 0;
			}
		}	else if ( gGet( gCIRCLE ) && st==Ready ){		// CIRCLE -- record while held down with current RecDBG
			sprintf( fname, "M0:/REC_%d_%d.WAV", RecDBG, dbgIdx );	
			dbgIdx++;
			FILE* outFP = tbOpenWriteBinary( fname ); 
			if ( outFP != NULL ){
				resetAudio();			// clean up anything in progress 
				dbgLog( "8 Rec to: %s \n", fname );
				gSet( gRED, 1 );
				LEDpauseCnt = -1;
				tbDelay_ms(200); //make sure printDebug catches up
				audStartRecording( outFP, &tstStats );
			}		
		} else { 
			if (!gGet( gPLUS ) && !gGet( gMINUS ) && !gGet( gLHAND )){  // PLUS, MINUS & LHAND up -- idle
				idleCnt++;
			} else if ( idleCnt>2000 ){ // was idle for a while, count this click
				idleCnt = 0;
				dbgIdx = 0;			// reset file idx
				if ( gGet(gPLUS) ) RecDBG++;
				if ( gGet(gMINUS) ) RecDBG--;
				if ( RecDBG<0 ) RecDBG = 0;
				dbgLog( "! RecDBG: %d \n", RecDBG );
				if ( gGet( gLHAND )){
					int d = 1 << (RecDBG-1);
					if ( DebugMask & d )
						DebugMask &= ~d;
					else
						DebugMask |= d;
					dbgLog("! DebugMask = 0x%06x \n", DebugMask );
				}
			}
		}
*/
/*    AUDIO playback -- CIR, PL_CIR, MI_CIR, LH_CIR -- play different tones
		if ( st==Playing ){ // controls while playing  PLUS(vol+), MINUS(vol-), TREE(pause/resume), LH adjPos-2), RH adjPos(2)
			curPl = gGet( gPLUS );
			if ( curPl & !prvPl )
				adjVolume( 5 ); 
			prvPl = curPl;
			
			curMi = gGet( gMINUS );
			if ( curMi & !prvMi )
				adjVolume( -5 ); 
			prvMi = curMi;
		
			curTr = gGet(gTREE);
			if (curTr && !prvTr) 
				pauseResume();
			prvTr = curTr;
			
			curLH = gGet(gLHAND);
			if (curLH && !prvLH) 
				adjPlayPosition( -2 );
			prvLH = curLH;
			
			curRH = gGet(gRHAND);
			if (curRH && !prvRH) 
				adjPlayPosition( 2 );
			prvRH = curRH;
		} else if ( gGet( gCIRCLE ) && st==Ready ){		// CIRCLE => 1KHz audio, (PLUS CIRCLE)440Hz=A, MINUS CIR:C, LH CIR:E
			gSet( gRED, 0 );
			gSet( gGREEN, 1 );
			if ( fexists( TBP[pAUDIO] )){
				//PlayDBG: TABLE=x1 POT=x2 PLUS=x4 MINUS=x8 STAR=x10 TREE=x20
				PlayDBG = (gGet( gTABLE )? 1:0) + (gGet( gPOT )? 2:0) + (gGet( gPLUS )? 4:0) + (gGet( gMINUS )? 8:0) + (gGet( gSTAR )? 0x10:0) + (gGet( gTREE )? 0x20:0); 
				dbgEvt( TB_dbgPlay, PlayDBG, 0,0,0 );
				dbgLog( " PlayDBG=0x%x \n", PlayDBG );
				playWave( TBP[pAUDIO] );
			} else {
				int hz = gGet( gPLUS )? 440 : gGet( gMINUS )? 523 : gGet( gLHAND )? 659 : 1000;
				audSquareWav( 5, hz );							// subst file data with square wave for 5sec
				playWave( "SQR.wav" );							// play x seconds of hz square wave
			}
			LEDpauseCnt = -1;
		}
		*/
/*		// TestSequence -- RHAND to enter (RED), STAR to proceed (GREEN)
		
		if ( inTestSequence ){
			if (gGet( gSTAR )){
				gSet( gGREEN, 1 );
				Driver_SAI0.Initialize( &saiEvent );   
				Driver_SAI0.PowerControl( ARM_POWER_FULL );
				uint32_t ctrl = ARM_SAI_CONFIGURE_RX | ARM_SAI_MODE_SLAVE  | ARM_SAI_ASYNCHRONOUS | ARM_SAI_PROTOCOL_I2S | ARM_SAI_DATA_SIZE(16);
				Driver_SAI0.Control( ctrl, 0, 8000 );	// set sample rate, init codec clock
				
				LEDpauseCnt = 100000;  // long GREEN => running clocks
			}
		} else if ( gGet( gRHAND )){			//  RHAND =>  Reboot to Device Firmware Update
			inTestSequence = true;
			gSet( gRED, 1 );
		//	testEncrypt();
		//	RebootToDFU();
			cdc_PowerUp( );			// enable all codec power
			LEDpauseCnt = 100000;	// long RED => power enabled
		}
		*/

//
//  TBook main thread
void talking_book( void *arg ) {
	dbgLog( "4 tbThr: 0x%x 0x%x \n", &arg, &arg + TBOOK_STACK_SIZE );
	
	EventRecorderInitialize( EventRecordNone, 1 );  // start EventRecorder
	EventRecorderEnable( evrE, 			EvtFsCore_No, EvtFsMcSPI_No );  	//FileSys library 
	EventRecorderEnable( evrE, 	 		EvtUsbdCore_No, EvtUsbdEnd_No ); 	//USB library 

	EventRecorderEnable( evrEA,  		TB_no, TB_no );  									//TB:  	Faults, Alloc, DebugLoop
	EventRecorderEnable( evrE, 			TBAud_no, TBAud_no );   					//Aud: 	audio play/record
	EventRecorderEnable( evrE, 			TBsai_no, TBsai_no ); 	 					//SAI: 	codec & I2S drivers
	EventRecorderEnable( evrEA, 		TBCSM_no, TBCSM_no );   					//CSM: 	control state machine
	EventRecorderEnable( evrE, 			TBUSB_no, TBUSB_no );   					//TBUSB: 	usb user callbacks
	EventRecorderEnable( evrE, 			TBUSBDrv_no, TBUSBDrv_no );   		//USBDriver: 	usb driver
	EventRecorderEnable( evrE, 			TBkey_no, TBkey_no );   					//Key: 	keypad input
	
	initPowerMgr();			// set up GPIO signals for controlling & monitoring power -- enables MemCard
	
	// eMMC &/or SDCard are powered up-- check to see if we have usable devices
	EventRecorderEnable( evrEAOD, 			EvtFsCore_No, EvtFsMcSPI_No );  	//FileSys library 
	for (int i=0; fsDevs[i]!=NULL; i++ ){
		fsStatus stat = fsMount( fsDevs[i] );
		if ( stat == fsOK ){	// found a formatted device
			fsDevs[ fsNDevs ] = fsDevs[ i ];
			fsNDevs++;
		} 
		dbgLog( "3 %s%d ", fsDevs[i], stat );
	}
	EventRecorderDisable( evrAOD, 			EvtFsCore_No, EvtFsMcSPI_No );  	//FileSys library 
	dbgLog( "3 NDv=%d \n", fsNDevs );

	if ( osKernelGetState() != osKernelRunning ) // no OS, so straight to debugLoop (no threads)
		debugLoop( fsNDevs > 0 );	

	initMediaPlayer( );
	
	if ( fsNDevs == 0 )
		debugLoop( false );
	else {
		if ( strcmp( fsDevs[0], "M0:" )!=0 ){ 	// not M0: !
			flashCode( 10 );		// R G R G : not M0:
			flashLED( "__" );
		}
		for ( int i = pCSM_VERS; i <= pLAST; i++ ){		// change paths to fsDevs[0]
			setDev( TBP[ i ], fsDevs[0] );
		}
		if ( fsNDevs > 1 ){
			if ( fexists( TBP[pAUDIO] )){	// audio.wav exists on device fsDevs[0] --- copy it to fsDevs[1]
				char dst[40];
				strcpy( dst, TBP[pAUDIO] );
				setDev( dst, fsDevs[1] );
				copyFile( TBP[pAUDIO], dst );
			}
		}

		if ( gGet( gMINUS ) || !fexists( TBP[pCSM_VERS] ))	
			debugLoop( !fexists( TBP[pCSM_VERS] ) );	// if no version.txt -- go straight to USB mode
	}


	initLogger( );

	initLedManager();									//  Setup GPIOs for LEDs
	
	initInputManager();								//  Initialize keypad handler & thread

	initFileOps();										//  decode mp3's 
	
	const char* startUp = "R3_3G3";
	ledFg( startUp );
	
	initControlManager();		// instantiate ControlManager & run on this thread-- doesn't return
}
// end  tbook.c
