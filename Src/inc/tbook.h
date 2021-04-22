// TBookV2   TBook.h 
// Gene Ball   Apr-2018  -- Nov-2019

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef TBook_H
#define TBook_H

#include "main.h"			// define hardware configuration & optional software switches

#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>			// e.g. atoi
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

#if  defined( RTE_Compiler_EventRecorder )
	#include "eventrecorder.h"   
#endif

#include "tknTable.h"			// GrpID, Event, Action
#include "log.h"				// logEvt...
#include "ledMgr.h"				// ledFg, ledBg
#include "powerMgr.h"			// registerPowerEventHandler
#include "inputMgr.h"			// sendEvent


// global utilities
// tbutil.c
extern void				EnableLedMngr( bool enable );  // DEBUG allow disabling LED manager for debugging
#define count(x) sizeof(x)/sizeof(x[0])

extern char 			CPU_ID[20], TB_ID[20], TBookName[20];
extern bool 			NO_OS_DEBUG;					// set in main.c, used in tbook.c
extern bool 			FirstSysBoot;					// defined in logger

extern void 			initIDs( void );
extern void				loadTBookName( void );													// load TBookName with string matching ID from /system/tbook_names.txt
extern void 			GPIO_DefineSignals( const GPIO_Signal def[] );
extern void 			gSet( GPIO_ID gpio, uint8_t on );								// set the state of a GPIO output pin
extern bool			 	gOutVal( GPIO_ID gpio );												// => LOGICAL state of a GPIO OUTput, e.g. True if 'PA0_'.ODR==0 or 'PB3'.ODR==1
extern bool	 			gGet( GPIO_ID gpio );														// => LOGICAL state of a GPIO input pin (TRUE if PA3_==0 or PC2==1)
extern void				flashInit( void );															// init keypad GPIOs for debugging
extern void 			flashLED( const char *s );											// 'GGRR__' in .1sec
extern void 			flashCode( int v );															// e.g. 9 => "RR__GG__GG__RR______"
extern void				gUnconfig( GPIO_ID id );												// revert GPIO to default configuration
extern void				gConfigOut( GPIO_ID led );											// configure GPIO for low speed pushpull output (LEDs, audio_PDN, pwr control)
extern void				gConfigI2S( GPIO_ID id );												// configure GPIO for high speed pushpull in/out (I2S)
extern void				gConfigADC( GPIO_ID led );											// configure GPIO as ANALOG input ( battery voltage levels )
extern void				gConfigIn( GPIO_ID key, bool pulldown );				// configure GPIO as low speed input, either pulldown or pullup (pwr fail, battery indicators)
extern void				gConfigKey( GPIO_ID key );											// configure GPIO as low speed pulldown input ( keys )
extern void 			RebootToDFU( void );														// reboot into SystemMemory -- Device Firmware Update bootloader
extern uint32_t 	AHB_clock;																			// freq in MHz of AHB == CPU == HCLK
extern uint32_t 	APB2_clock;																			// freq in MHz of APB2 (fast) peripheral bus
extern uint32_t 	APB1_clock;																			// freq in MHz of APB1 (slow) peripheral bus
extern void 			setCpuClock( int mHz, int apbshift2, int apbshift1 );													// config & enable PLL & PLLI2S, bus dividers, flash latency & switch HCLK to PLL at 'mHz' MHz 
extern void 			LED_Init( GPIO_ID led );												// ledManager-- for debugging
extern bool 			ledMgrActive;
extern FILE * 		tbOpenRead( const char * nm );									// repower if necessary, & open file
extern FILE * 		tbOpenReadBinary( const char * nm );						// repower if necessary, & open file
extern FILE * 		tbOpenWrite( const char * nm );									// repower if necessary, & open file
extern FILE * 		tbOpenWriteBinary( const char * nm );						// repower if necessary, & open file
extern void				tbCloseFile( FILE * f );												// close file, errLog if error
extern void				tbRenameFile( const char *src, const char *dst ); // rename path to path 

extern void 			FileSysPower( bool enable );										// power up/down eMMC & SD 3V supply
extern fsStatus 	fsMount( char *drv );														// try to finit() & mount()  drv:   finit() code, fmount() code
extern osMutexId_t		logLock;																		// mutex for access to NorFlash

extern void 			talking_book ( void *argument );
extern void 			getRTC( struct _fsTime *fsTime );							// load current RTC into fsTime
extern bool 			showRTC( void );
extern uint32_t 	tbTimeStamp( void );
extern void 			tbDelay_ms( int ms ); 					//  Delay execution for a specified number of milliseconds
extern void *			tbAlloc( int nbytes, const char *msg );
extern bool 			fexists( const char *fname );		// return true if file path exists
extern char *			findOnPathList( char * destpath, const char *search_path, const char * nm );	// store path to 1st 'nm' on 'search_path' in 'destpath'

#include "tb_evr.h"

extern void 			dbgEvt( int id, int a1, int a2, int a3, int a4 );
extern void 			dbgEvtD( int id, const void *d, int len );
extern void 			dbgEvtS( int id, const char *d );

extern void 			usrLog( const char * fmt, ... );
extern bool				dbgEnab( char ch );		// check if debugging for 'ch' is enabled
extern void 			dbgLog( const char * fmt, ... );
extern void 			errLog( const char * fmt, ... );
extern void 			tbErr( const char * fmt, ... );							// report fatal error
extern void				tbShw( const char *s, char **p1, char **p2 );
extern void 			_Error_Handler( char *, int );
extern int 				PlayDBG;			// DEBUG flag for DebugLoop & playWave
extern int 				RecDBG;			// DEBUG flag for DebugLoop & audStartRecord
extern void 			debugLoop( bool autoUSB );			// dbgLoop.c: called if boot-MINUS, no file system,  autoUSB => usbMode

extern void 			chkDevState( char *loc, bool reset );
extern void 			stdout_putchar( char );
extern int				divTst(int lho, int rho); 	// for fault handler testing
extern void 			enableLCD( void );
extern void 			disableLCD( void );
extern void 			printAudSt( void ); // print audio state

extern void 			USBmode( bool start );						// start (or stop) USB storage mode

extern bool				enableMassStorage( char *drv0, char *drv1, char *drv2, char *drv3 );
extern bool				disableMassStorage( void );						// disable USB MSC & return FSys to TBook
extern bool				isMassStorageEnabled( void );					// => true if usb is providing FileSys as MSC

typedef struct	{	// GPIO_Def_t -- define GPIO port/pins/interrupt # for a GPIO_ID
	GPIO_ID					id;		  // GPIO_ID for this line -- from enumeration in main.h
	GPIO_TypeDef *	port;  	// address of GPIO port -- from STM32Fxxxx.h
	uint16_t 				pin; 		// GPIO pin within port
	AFIO_REMAP			altFn;	// alternate function value
	IRQn_Type 			intq; 	// specifies INTQ num  -- from STM32Fxxxx.h
	char *					signal;	// string name of signal, e.g. PA0
	int							active; // gpio value when active: 'PA3_' => 0, 'PC12' => 1
} GPIO_Def_t;

#define MAX_GPIO 100
extern GPIO_Def_t  	gpio_def[ MAX_GPIO ];	

extern    void 		initPrintf( const char *hdr );

//extern    char 		Screen[ ][ dbgScreenChars ];				
	
#define Error_Handler() _Error_Handler( __FILE__, __LINE__ )
#define min(a,b) (((a)<(b))?(a):(b))

#define MAX_DBG_LEN 	300


typedef enum  {	Ready,  Playing,  Recording	} MediaState;

// TBook version string
extern const char * 	TBV2_Version;
extern bool						FileSysOK;
extern bool						TBDataOK;

// Thread stack sizes
extern const int 	TBOOK_STACK_SIZE;
extern const int 	POWER_STACK_SIZE;
extern const int 	INPUT_STACK_SIZE;
extern const int 	MEDIA_STACK_SIZE;		// opens in/out files
extern const int 	FILEOP_STACK_SIZE;		// opens in/out files
extern const int 	CONTROL_STACK_SIZE;		// opens in/out files
extern const int 	LED_STACK_SIZE;

// SD card path definitions  
extern char * TBP[];			// indexed by 0.. pLAST
extern const int	pBOOTCNT; 
extern const int	pCSM_DEF;
extern const int	pLOG_TXT;
extern const int	pSTATS_PATH;
extern const int	pMSGS_PATH;
extern const int	pLIST_OF_SUBJS;
extern const int	pPACKAGE_DIR;
extern const int	pPKG_VERS;
extern const int  pLAST;

//TBOOK error codes
extern const int 	TB_SUCCESS;
extern const int 	GPIO_ERR_ALREADYREG;
extern const int 	GPIO_ERR_INVALIDPIN;
extern const int 	MEDIA_ALREADY_IN_USE;

//  -----------  TBook configuration variables
#define	MAX_PATH  80


typedef struct TBConfig {			// TBConfig
	short 	default_volume;
	int 		powerCheckMS;				// used by powermanager.c
	int			shortIdleMS;
	int			longIdleMS;
	int			minShortPressMS;			// used by inputmanager.c
	int			minLongPressMS;				// used by inputmanager.c
	int			initState;	
	
	char *	systemAudio;				// path to system audio files
	char *	bgPulse;						// LED sequences used in Firmware
	char *	fgPlaying;
	char *	fgPlayPaused;
	char *	fgRecording;
	char *	fgRecordPaused;
	char *	fgSavingRec;
	char *	fgSaveRec;
	char *	fgCancelRec;
	char *	fgUSB_MSC;
	char *	fgTB_Error;
	char *	fgNoUSBcable;
	char *	fgUSBconnect;
	char *	fgPowerDown;
}	TBConfig_t;

extern TBConfig_t 	TB_Config;		// global TBook configuration

// for tbUtil.c
#define	dbgLns  				80
#define	dbgChs  				20
typedef  char DbgScr[dbgLns][dbgChs];
#define	NUM_KEYPADS 		10

typedef  struct {
	int 					eventTS;					// TStamp of most recent input interrupt
	int						lastTS;						// TStamp of previous input interrupt 
	int						msecSince;				// msec between prev & this
	KEY						detectedUpKey;		// shared variable between ISR and thread
	bool					DFUkeysDown;			// TRUE if special DFU keypair down: TABLE + POT
	bool					keytestKeysDown;	// TRUE if special keytest pair down: HOME + POT
	bool					starDown;					// flags to prevent STAR keypress on key-up after STAR-key alt sequence
	bool					starAltUsed;
	char 					keyState[11];
} KeyPadState_t;
typedef KeyPadKey_t 	 KeyPadKey_arr[ NUM_KEYPADS ];		// so Dbg knows array size

typedef struct {
	bool 				Active;
	int			  	Count;
	char		  	Status[ NUM_KEYPADS+1 ];
} KeyTestState_t;

//const int				MAX_TB_HIST 	=	50;  can't define const here-- gets multiply defined
#define 				MAX_TB_HIST 50
typedef char *	TBH_arr[ MAX_TB_HIST*2 ];

typedef struct {			// Dbg -- pointers for easy access to debug info
	bool							loopOpeningM0;

	KeyTestState_t *	KeyTest;			// control block for KeyPad Test
	KeyPadState_t * 	KeyPadStatus;	// current keypad status 
	KeyPadKey_arr * 	KeyPadDef;		// definitions & per/key state of keypad
	char							keypad[11];		// keypad keys as string
	
	osRtxThread_t *		thread[5];		// ptrs to osRtxThread
	TBConfig_t	* 		TBookConfig;	// TalkingBook configuration block
	TBH_arr	*					TBookLog;			// TalkingBook event log
	
//	DbgScr 						Scr;					// dbgLog output 
	
} DbgInfo;
extern  DbgInfo			Dbg;	// in main.c -- visible at start

#endif /* __TBOOK_H__ */

