// TBookV2   TBook.h 
// Gene Ball   Apr-2018  -- Nov-2019

#ifndef TBook_H
#define TBook_H

#include "main.h"     // define hardware configuration & optional software switches

#include <ctype.h>
#include <new>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>     // e.g. atoi
#include <stdio.h>
#include <stdarg.h>
#include "assert.h"

#include "Driver_Common.h"
#include "rtx_os.h"
#include "RTE_Components.h"
#include  CMSIS_device_header
#include "cmsis_os2.h"
#include "rtx_os.h"
#include "rl_fs.h"

#include "tbconfig.h"

#ifdef __packed
#undef __packed
#endif
#define __packed

#if  defined( RTE_Compiler_EventRecorder )
#include "eventrecorder.h"
#endif

// from Keil_v5/ARM/PACK/Keil/STM32F4xx_DFP/2.16.0/Release_notes.html
// V1.26.2 / 16-July-2021
// CMSIS updates
// Remove unsupported �GPIOF_BASE� and �GPIOG_BASE� defines from STM32F412Vx device
#define GPIOF_BASE            (AHB1PERIPH_BASE + 0x1400UL)
#define GPIOG_BASE            (AHB1PERIPH_BASE + 0x1800UL)
#define GPIOF               ((GPIO_TypeDef *) GPIOF_BASE)
#define GPIOG               ((GPIO_TypeDef *) GPIOG_BASE)

#include "logger.h"                // logEvt...
#include "ledmanager.h"       // ledFg, ledBg
#include "powermanager.h"      // registerPowerEventHandler
#include "inputmanager.h"            // sendEvent

// global state
extern bool BootToUSB;
extern bool BootDebugLoop;
extern bool BootVerboseLog;
extern bool bootToQcTest;
extern bool BootVerbosePower;
extern bool BootResetLog;
extern bool BootFormatFileSys;

extern bool controlManagerReady;


// global utilities
// tbutil.c
//#define count(x) sizeof(x)/sizeof(x[0])

extern char CPU_ID[20], TB_ID[20];
extern bool NO_OS_DEBUG;          // set in main.c, used in tbook.c
extern bool FirstSysBoot;         // defined in logger
extern bool runQcTest;            // defined in tbook.c -- if no /system/QC_PASS.txt  or Circle boot
extern char BootKey;                        // set in main.c, used in tbook.c
extern bool PowerChecksEnabled;       // set true for normal operation

extern void initIDs(void);

extern void flashInit(void);                              // init keypad GPIOs for debugging
extern void flashLED(const char *s);                      // 'GGRR__' in .1sec
extern void flashCode(int v);                             // e.g. 9 => "RR__GG__GG__RR______"

//extern void       RebootToDFU( void );                            // reboot into SystemMemory -- Device Firmware Update bootloader
extern uint32_t AHB_clock;                                      // freq in MHz of AHB == CPU == HCLK
extern uint32_t APB2_clock;                                     // freq in MHz of APB2 (fast) peripheral bus
extern uint32_t APB1_clock;                                     // freq in MHz of APB1 (slow) peripheral bus
extern void setCpuClock(int mHz, int apbshift2,
                        int apbshift1);                         // config & enable PLL & PLLI2S, bus dividers, flash latency & switch HCLK to PLL at 'mHz' MHz

extern FILE *tbFopen(const char *fname, const char *flags); // repower i fnecessary & open file.
extern FILE *tbOpenRead(const char *nm);                  // repower if necessary, & open file
extern FILE *tbOpenReadBinary(const char *nm);            // repower if necessary, & open file
extern FILE *tbOpenWrite(const char *nm);                 // repower if necessary, & open file
extern FILE *tbOpenWriteBinary(const char *nm);           // repower if necessary, & open file
extern void tbFclose(FILE *f);                        // close file, errLog if error
extern void tbFrename(const char *src, const char *dst); // rename path to path

extern void FileSysPower(bool enable);                    // power up/down eMMC & SD 3V supply
extern fsStatus fsMount(const char *drv);                 // try to finit() & mount()  drv:   finit() code, fmount() code
extern osMutexId_t logLock;                               // mutex for access to NorFlash

extern void talking_book(void *tbAttr);     // talking book initialization & control manager thread
extern bool getRTC(struct _fsTime *fsTime, uint32_t *pMSec);          // load current RTC into fsTime & *pMSec
extern bool showRTC(void);

extern uint32_t tbTimeStamp(void);  // returns millisecond timestamp based on OS Tic
extern uint32_t tbRtcStamp(void);   // returns millisecond timestamp based on RTC instead of OS Tic

extern void tbDelay_ms(int ms);           //  Delay execution for a specified number of milliseconds
extern void *tbAlloc(int nbytes, const char *msg);

extern void checkMem(void);

extern void showMem(void);

extern int fsize(const char *fname);       // size of file, -1 if not exist
extern bool fexists(const char *fname);

extern char *findOnPathList(char *destpath, const char *search_path,
                            const char *nm);  // store path to 1st 'nm' on 'search_path' in 'destpath'
extern void saveLastTime(fsTime rtc);         // update lastRTC.txt modified time

#include "tb_evr.h"

extern void dbgEvt(int id, int a1, int a2, int a3, int a4);

extern void dbgEvtD(int id, const void *d, int len);

extern void dbgEvtS(int id, const char *d);

extern void usrLog(const char *fmt, ...);

extern bool dbgEnabled(char ch);   // check if debugging for 'ch' is enabled
extern void dbgLog(const char *fmt, ...);

extern void errLog(const char *fmt, ...);

extern void tbErr(const char *fmt, ...);             // report fatal error
extern void tbShw(const char *s, char **p1, char **p2);

extern void _Error_Handler(char *, int);

extern int BootMode;   // defined in main.c -- set by booting with a key:  none=0 STAR=1 LHAND=2 MINUS=3  PLUS=4  RHAND=5 CIRCLE=6 HOUSE=7
extern char BootKey;        // char for BootMode tests:  'S'tar, 'L'hand, 'M'inus, 'P'lus, 'R'hand, 'C'ircle, 'H'ome
extern void debugLoop(bool autoUSB);      // dbgLoop.c: called if boot-MINUS, no file system,  autoUSB => usbMode

extern void chkDevState(char *loc, bool reset);

extern void stdout_putchar(char);

extern int divTst(int lho, int rho);   // for fault handler testing

extern void printAudSt(void); // print audio state
extern void setDbgFlag(char ch, bool enab);

extern void tglDebugFlag(int idx);   // toggle debug flag: "0123456789ABCDEFGHIJK"[idx]

extern void USBmode(bool start);            // start (or stop) USB storage mode

extern bool enableMassStorage(const char *drv0, const char *drv1, const char *drv2, const char *drv3);

extern bool disableMassStorage(void);           // disable USB MSC & return FSys to TBook
extern bool isMassStorageEnabled(void);         // => true if usb is providing FileSys as MSC


extern void initPrintf(const char *hdr);

//extern    char    Screen[ ][ dbgScreenChars ];        

#define Error_Handler() _Error_Handler( __FILE__, __LINE__ )
// TODO: stdlib?
#define min(a, b) (((a)<(b))?(a):(b))
#define max(a, b) (((a)>(b))?(a):(b))

#define MAX_DBG_LEN   300


typedef enum {
    Ready, Playing, Recording
} MediaState;

// TBook version string
extern const char *TBV2_Version;
extern bool FileSysOK;
extern bool TBDataOK;

// Thread stack sizes
extern const int TBOOK_STACK_SIZE;
extern const int POWER_STACK_SIZE;
extern const int INPUT_STACK_SIZE;
extern const int MEDIA_STACK_SIZE;   // opens in/out files
extern const int FILEOP_STACK_SIZE;    // opens in/out files
extern const int CONTROL_STACK_SIZE;   // opens in/out files
extern const int LED_STACK_SIZE;

// SD card path definitions  
extern const char *TBP[];      // indexed by 0.. pLAST

//TBOOK error codes
extern const int TB_SUCCESS;
extern const int GPIO_ERR_ALREADYREG;
extern const int GPIO_ERR_INVALIDPIN;
extern const int MEDIA_ALREADY_IN_USE;

//  -----------  TBook configuration variables
#define MAX_PATH  80


//typedef struct {       // TBConfig
//    short default_volume;
//    int powerCheckMS;         // used by powermanager.c
//    int shortIdleMS;
//    int longIdleMS;
//    int minShortPressMS;      // used by inputmanager.c
//    int minLongPressMS;       // used by inputmanager.c
//    int qcTestState;          // first state if running QC acceptance test
//    int initState;
//
//    const char *systemAudio;          // path to system audio files
//    const char *bgPulse;              // LED sequences used in Firmware
//    const char *fgPlaying;
//    const char *fgPlayPaused;
//    const char *fgRecording;
//    const char *fgRecordPaused;
//    const char *fgSavingRec;
//    const char *fgSaveRec;
//    const char *fgCancelRec;
//    const char *fgUSB_MSC;
//    const char *fgTB_Error;
//    const char *fgNoUSBcable;
//    const char *fgUSBconnect;
//    const char *fgPowerDown;
//} TBConfig_t;

//extern TBConfig_t *TB_Config;    // global TBook configuration

// for tbUtil.c
#define dbgLns          80
#define dbgChs          20
typedef char DbgScr[dbgLns][dbgChs];
#define NUM_KEYPADS     10

typedef struct {
    // state updated by keypad ISR in handleInterrupt()
    int eventTS;          // TStamp of most recent input interrupt
    int lastTS;           // TStamp of previous input interrupt
    int msecSince;        // msec between prev & this
    int downCnt;          // # keys currently depressed
    char keyState[11];

    // state updated by inputThread in response to keyTransition msgs
    KEY firstDown;        // KEY::INVALID or 1st key to go down
    int firstDownTS;
    KEY secondDown;       // KEY::INVALID or 2nd key to go down
    int secondDownTS;
    bool multipleDown;     // true if third key goes down-- reset when all are up
    bool starUsed;         // true if STAR used as shift-- Star up is ignored
    /*
      KEY           detectedUpKey;    // shared variable between ISR and thread
      bool          DFUkeysDown;      // TRUE if special DFU keypair down: TABLE + BOWL
      bool          keytestKeysDown;  // TRUE if special keytest pair down: HOUSE + BOWL
      bool          starDown;         // flags to prevent STAR keypress on key-up after STAR-key alt sequence
      bool          starAltUsed;
      KEY           DownKey;          // key currently down
      KEY           LongPressKey;     // key reported as long press -- shared ISR & thread
    */
} KeyPadState_t;

typedef struct {
    bool Active;
    int Count;
    char Status[NUM_KEYPADS + 1];
} KeyTestState_t;

//const int       MAX_TB_HIST   = 50;  can't define const here-- gets multiply defined
#define         MAX_TB_HIST 50
typedef char *TBH_arr[MAX_TB_HIST * 2];

typedef struct {      // Dbg -- pointers for easy access to debug info
    bool loopOpeningM0;

    KeyTestState_t *KeyTest;      // control block for KeyPad Test
    KeyPadState_t *KeyPadStatus; // current keypad status
    KeyPadKey_arr *KeyPadDef;    // definitions & per/key state of keypad
    char keypad[11];   // keypad keys as string

    osRtxThread_t *thread[6];    // ptrs to osRtxThread
    TBConfig *pTBConfig;  // TalkingBook configuration block
    TBH_arr *TBookLog;     // TalkingBook event log

} DbgInfo;
extern DbgInfo Dbg;  // in main.c -- visible at start

// C++ support
extern void* operator new  ( std::size_t count, const char * tag );
extern void* operator new[]  ( std::size_t count, const char * tag );
extern void operator delete  ( void* ptr ) noexcept;
extern void operator delete[] (void* ptr) noexcept;
extern char *allocStr(const char *s, const char *tag);



#endif /* __TBOOK_H__ */

