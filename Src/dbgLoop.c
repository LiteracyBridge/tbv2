// TBookV2    initialization
//	Apr2018 

#include "tbook.h"
//#include "controlmanager.h"			// runs on initialization thread
#include "mediaPlyr.h"			// thread to watch audio status
//#include "fs_evr.h"					// FileSys components
//#include "fileOps.h"				// decode & encrypt audio files

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
extern int 		DebugMask;
extern void 	debugLoop( bool autoUSB );			// called if boot-MINUS, no file system,  autoUSB => usbMode
extern char * fsDevs[];
extern int    fsNDevs;

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

void CheckRecording(){   // check for dbgLoop ongoing recording
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
void PlayRecCmd( GPIO_ID k ){		// PlayRec mode subcommands -- 
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
void CodecCmd( GPIO_ID k ){			// Codec mode subcommands
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


// end  dbgLoop.c
