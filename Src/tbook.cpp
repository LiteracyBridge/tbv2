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

#define X(name,path) path,
const char *TBP[] = {
        FILENAME_LIST
};
#undef X

//TBOOK error codes
const int TB_SUCCESS          = 0;
const int GPIO_ERR_ALREADYREG = 1;
const int GPIO_ERR_INVALIDPIN = 2;

const int MEDIA_ALREADY_IN_USE = 1;

const int MALLOC_HEAP_SIZE = 20000;
// This doesn't seem to be used *anywhere*. Commenting it out seems to make no difference.
//char      MallocHeap[20000];    // MALLOC_HEAP_SIZE ];
bool      FileSysOK        = false;
bool      TBDataOK         = true;     // false if no TB config found
bool      runQcTest        = false;
bool      PrecompiledCSM   = false;


void setDev( char *fname, const char *dev ) {  // replace front of fname with dev
    for (int i = 0; i < strlen( dev ); i++)
        fname[i] = dev[i];
}

const char *fsDevs[] = { "M0:", "M1:", "N0:", "F0:", "F1:", NULL };
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

extern bool bootToQcTest;
extern bool BootDebugLoop;

//
//  TBook main thread
void talking_book( void *tbAttr ) {    // talking book initialization & control manager thread
    dbgLog( "4 tbookThread: 0x%x 0x%x \n", &tbAttr, &tbAttr - TBOOK_STACK_SIZE );

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
        fsStatus stat = fsMount( const_cast<char*>(fsDevs[i]) );
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
        //for (int i = 0; i <= pLAST; i++) {   // change paths to fsDevs[0]
        //    setDev( TBP[i], fsDevs[0] );
        //}

        if ( bootToQcTest || !fexists( TBP[pQC_PASS_TXT] )) {
            runQcTest = true;
        }

        if ( BootDebugLoop ) {
            // true: straight to USB mode; false: not straight to USB
            debugLoop( false );
        }
    }


    initLogger();

    LedManager::initLedManager();                 //  Setup GPIOs for LEDs

    initInputManager();               //  Initialize keypad handler & thread

    // I think this uses too much memory, and the mp3 decode thread can't launch. Need some other solution.
//  fileOpInit();                    //  decode mp3's

    const char *startUp = "R3_3G3";
    LedManager::ledFg( startUp );

    initControlManager();   // instantiate ControlManager & run on this thread-- doesn't return
}
// end  tbook.c
