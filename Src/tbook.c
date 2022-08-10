// TBookV2    initialization
//  Apr2018 

#include "tbook.h"
#include "controlmanager.h"     // runs on initialization thread
#include "mediaplayer.h"      // thread to watch audio status
#include "fs_evr.h"         // FileSys components
#include "fileOps.h"        // decode & encrypt audio files

#include "build_version.h"

const char *TBV2_Version = BUILD_VERSION; // like "V3.1 of Tue 11/16/21 11:41:16.18"

//
// Thread stack sizes
const int TBOOK_STACK_SIZE  = 4000;   // init, becomes control manager
const int POWER_STACK_SIZE  = 2048;
const int INPUT_STACK_SIZE  = 512;
const int MEDIA_STACK_SIZE  = 3000;   // opens in/out files
const int FILEOP_STACK_SIZE = 9000;   // opens in/out files, mp3 decode
const int LED_STACK_SIZE    = 400;
// RTX_Config.h  sets OS_TIMER_THREAD_STACK_SIZE = 512;  // used by timer callbacks:  powerCheckTimerCallback, checkKeyTimer, tbTimer

const int pBOOTCNT         = 0;
const int pCSM_DEF         = 1;
const int pLOG_TXT         = 2;
const int pSTATS_PATH      = 3;
const int pRECORDINGS_PATH = 4;
const int pLIST_OF_SUBJS   = 5;
const int pPACKAGE_DIR     = 6;
const int pPKG_VERS        = 7;
const int pQC_PASS         = 8;
const int pERASE_NOR       = 9;
const int pSET_RTC         = 10;
const int pLAST_RTC        = 11;
const int pPKG_DAT         = 12;
const int pAUDIO           = 13;    // DEBUG
const int pLAST            = 12;

char *TBP[] = {            // as static array, so they can be switched to a different device if necessary
        "M0:/system/bootcount.txt",             //    pBOOTCNT         = 0;
        "M0:/system/csm_data.txt",              //    pCSM_DEF         = 1;
        "M0:/log/tbLog.txt",                    //    pLOG_TXT         = 2;
        "M0:/stats/",                           //    pSTATS_PATH      = 3;
        "M0:/recordings/",                      //    pRECORDINGS_PATH = 4;
        "M0:/package/list_of_subjects.txt",     //    pLIST_OF_SUBJS   = 5;
        "M0:/package/",                         //    pPACKAGE_DIR     = 6;
        "M0:/package/version.txt",              //    pPKG_VERS        = 7;
        "M0:/system/QC_PASS.txt",               //    pQC_PASS         = 8;
        "M0:/system/EraseNorLog.txt",           //    pERASE_NOR       = 9;
        "M0:/system/SetRTC.txt",               //    pSET_RTC         =10;
        "M0:/system/lastRTC.txt",              //    pLAST_RTC        =11;
        "M0:/content/packages_data.txt",       //    pPKG_DAT         =12;
        "M0:/audio.wav"                        //    pAUDIO = 13;        // DEBUG
};


//TBOOK error codes
const int TB_SUCCESS          = 0;
const int GPIO_ERR_ALREADYREG = 1;
const int GPIO_ERR_INVALIDPIN = 2;

const int MEDIA_ALREADY_IN_USE = 1;

const int MALLOC_HEAP_SIZE = 20000;
char      MallocHeap[20000];    // MALLOC_HEAP_SIZE ];
bool      FileSysOK        = false;
bool      TBDataOK         = true;     // false if no TB config found
bool      RunQCTest        = false;
bool      PrecompiledCSM   = false;


void setDev( char *fname, const char *dev ) {  // replace front of fname with dev
    for (int i = 0; i < strlen( dev ); i++)
        fname[i] = dev[i];
}

char *fsDevs[] = { "M0:", "M1:", "N0:", "F0:", "F1:", NULL };
int fsNDevs = 0;

void copyFile( const char *src, const char *dst ) {
    FILE *fsrc = tbOpenRead( src );
    FILE *fdst = tbOpenWrite( dst );
    if ( fsrc == NULL || fdst == NULL ) return;
    char buf[512];
    while (true) {
        int cnt = fread( buf, 1, 512, fsrc );
        if ( cnt == 0 ) break;

        fwrite( buf, 1, 512, fdst );
    }
    tbFclose( fsrc );
    tbFclose( fdst );
}

extern bool BootToQCtest;
extern bool BootDebugLoop;

//
//  TBook main thread
void talking_book( void *tbAttr ) {    // talking book initialization & control manager thread

    EventRecorderInitialize( EventRecordNone, 1 );  // start EventRecorder
    EventRecorderEnable( evrE, EvtFsCore_No, EvtFsMcSPI_No );    //FileSys library
    EventRecorderEnable( evrE, EvtUsbdCore_No, EvtUsbdEnd_No );  //USB library

    EventRecorderEnable( evrEA, TB_no, TB_no );                   //TB:   Faults, Alloc, DebugLoop
    EventRecorderEnable( evrE, TBAud_no, TBAud_no );             //Aud:  audio play/record
    EventRecorderEnable( evrE, TBsai_no, TBsai_no );             //SAI:  codec & I2S drivers
    EventRecorderEnable( evrEA, TBCSM_no, TBCSM_no );             //CSM:  control state machine
    EventRecorderEnable( evrE, TBUSB_no, TBUSB_no );             //TBUSB:  usb user callbacks
    EventRecorderEnable( evrE, TBUSBDrv_no, TBUSBDrv_no );       //USBDriver:  usb driver
    EventRecorderEnable( evrE, TBkey_no, TBkey_no );             //Key:  keypad input

    initPowerMgr();     // set up GPIO signals for controlling & monitoring power -- enables MemCard

    // eMMC &/or SDCard are powered up-- check to see if we have usable devices
    EventRecorderEnable( evrEAOD, EvtFsCore_No, EvtFsMcSPI_No );    //FileSys library
    for (int i = 0; fsDevs[i] != NULL; i++) {
        fsStatus stat = fsMount( fsDevs[i] );
        if ( stat == fsOK ) {  // found a formatted device
            fsDevs[fsNDevs] = fsDevs[i];
            fsNDevs++;
        }
        dbgLog( "3 %s%d ", fsDevs[i], stat );
    }
    EventRecorderDisable( evrAOD, EvtFsCore_No, EvtFsMcSPI_No );    //FileSys library
    dbgLog( "3 NDv=%d \n", fsNDevs );

    if ( osKernelGetState() != osKernelRunning ) // no OS, so straight to debugLoop (no threads)
        debugLoop( fsNDevs > 0 );

    initMediaPlayer();

    if ( fsNDevs == 0 )   // no storage device found-- go to debugLoop()
        debugLoop( false );
    else {
        if ( strcmp( fsDevs[0], "M0:" ) != 0 ) {   // not M0: !
            flashCode( 10 );    // R G R G : not M0:
            flashLED( "__" );
        }
        for (int i = 0; i <= pLAST; i++) {   // change paths to fsDevs[0]
            setDev( TBP[i], fsDevs[0] );
        }

        if ( BootToQCtest || !fexists( TBP[pQC_PASS] )) {
            RunQCTest = true;
        }

        if ( BootDebugLoop ) {
            // true: straight to USB mode; false: not straight to USB
            debugLoop( false );
        }
    }


    initLogger();

    initLedManager();                 //  Setup GPIOs for LEDs

    initInputManager();               //  Initialize keypad handler & thread

    // I think this uses too much memory, and the mp3 decode thread can't launch. Need some other solution.
//  initFileOps();                    //  decode mp3's

    const char *startUp = "R3_3G3";
    ledFg( startUp );

    initControlManager();   // instantiate ControlManager & run on this thread-- doesn't return
}
// end  tbook.c
