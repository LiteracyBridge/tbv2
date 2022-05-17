//*********************************  main.c
//  target-specific startup file
//     for STM3210E_EVAL board
//
//	Feb2020 

#include "main.h"				// platform specific defines
#include "tbook.h"


//
//********  main
DbgInfo Dbg;			// things to make visible in debugger
//extern uint16_t						KeypadIMR;
//bool NO_OS_DEBUG = true;
/* Stop mode power debugging
void											ResetGPIO( void );			// powermanager.c


extern bool RebootOnKeyInt;

int DbgPwrDn = 0; // DEBUG**********************************
// DbgPwdDn-- power down meanings
// LHAND  1: after CODEC_DATA_TX_DN  3uA
// RHAND  2: after CODEC_PLAYBACK_DN
// CIRCLE 3: after CODEC_PLAYBACK_DN 2nd time
// TREE   4: after checkPower
// STAR   5: after 

extern bool FakeCodec; 
bool FakeCodec = false; 
extern bool NoI2S; 
bool NoI2S = false; 

void MarcDebug(){				// DEBUG test for Marc -- for Stop mode power calcs
	flashInit();			// enable keyboard to decode boot type

//	if ( gGet( gCIRCLE ) ){ FakeCodec = true; NoI2S = true; return; }
//	if ( gGet( gRHAND ) ){ FakeCodec = true; return; }

	if ( gGet( gLHAND ))				DbgPwrDn = 1;
	else if ( gGet( gRHAND ))		DbgPwrDn = 2;
	else if ( gGet( gCIRCLE ))	DbgPwrDn = 3;
	else if ( gGet( gTREE ))		DbgPwrDn = 4;
	else if ( gGet( gSTAR ))		DbgPwrDn = 5;
 
	flashCode( DbgPwrDn );
}
*/
int BootMode; // DEBUG**********************************
char BootKey;
// BootOption-- 
// STAR=1 LHAND=2 MINUS=3  PLUS=4  RHAND=5 CIRCLE=6 HOME=7
// hardware boot functions:
//  TABLE + TREE => hardware forced reboot
//  TABLE + TREE + POT => hardware forced DFU mode

// 'P'  										=> enter USBmode() before starting CSM
// 'M'  										=> call debugLoop() in talking_book() thread
// 'L'                                          => verbose log messages
// no FileSys found 							=> call debugLoop( false ) in talking_book() thread
// if FileSys, but no /system/QC_PASS.txt 	    => run acceptance test
// if QC_PASS.txt, but no package_dat.txt 	    => enter USBmode
// 'C'  										=> force running QC acceptance test
// 'R'                                          => show power check details
// calls to DbgPwrDown( cd ):
//   if cd==BootMode, flash cd on LED & fastPowerDown()

// un-define this, or define as 0 for a release build. Note that uvision has no concept of debug vs release builds.
#define DEBUG 1

// TODO: Put all these global state flags, counters, switches and so forth into a single struct.
// Segregate all the debugging stuff, and put it inside an "#if DEBUG".
// uvision doesn't support debug vs release builds (it's a pretty crappy tool), so we'll need to set it manually.
// Still, it will put us in a better position if we ever do get a real build system.
bool BootToUSB = false;
bool BootDebugLoop = false;
bool BootVerboseLog = false;
bool BootToQCtest = false;
bool BootVerbosePower = false;
bool BootResetLog = false;
bool BootFormatFileSys = false;
uint16_t keysDetectedAtBoot = 0;          // Which keys were detected at boot time.

void setBootMode(){	// use key to to select value for BootMode
    GPIO_ID key_gpios[] = {gHOME, gCIRCLE, gPLUS, gMINUS, gTREE, gLHAND, gRHAND, gPOT, gSTAR, gTABLE};
    flashInit();			// enable keyboard to decode boot type
    for (int i=0; i<=kTABLE; i++) {
        if (gGet(key_gpios[i])) keysDetectedAtBoot |= 1<<i;
    }
    // Nothing other than Tree and/or Table. Whether either or both of Tree or Table are still
    // pressed after a Tree+Table boot is not deterministic.
    bool hwCleanBoot = (keysDetectedAtBoot & ~(1<<kTREE|1<<kTABLE)) == 0;
    // Tree and/or Table + nothing else but Plus, etc.
    bool hwPlusBoot = (keysDetectedAtBoot & ~(1<<kTREE|1<<kTABLE)) == 1<<kPLUS;
    bool hwStarBoot = (keysDetectedAtBoot & ~(1<<kTREE|1<<kTABLE)) == 1<<kSTAR;
#if DEBUG
    bool hwMinusBoot = (keysDetectedAtBoot & ~(1<<kTREE|1<<kTABLE)) == 1<<kMINUS;
#endif
    char prog[20] = "R_G_";

    if ( hwStarBoot ) {
     
        while (true){
            if ( BootToUSB || BootResetLog || BootFormatFileSys )
                sprintf( prog, "_%c_%c_%c__", BootToUSB? 'R':'_', BootResetLog? 'R':'_', BootFormatFileSys? 'R':'_' );
            else if ( BootToQCtest || BootVerboseLog || BootVerbosePower )
                sprintf( prog, "_%c_%c_%c__", BootToQCtest? 'G':'_', BootVerboseLog? 'G':'_', BootVerbosePower? 'G':'_' );
            showProgress( prog, 100 );    // select boot keys
    
            if ( gGet( gPLUS  )) BootToUSB = true;        // boot directly to USB
            if ( gGet( gMINUS )) BootResetLog = true;     // reset Log (skip copy of last log)
            if ( gGet( gLHAND )) BootFormatFileSys = true;  // reformat EMMC filesystem
        
            // debug switches
            if ( gGet( gCIRCLE )) BootToQCtest = true;
            if ( gGet( gPOT    )) BootVerboseLog = true;
            if ( gGet( gRHAND  )) BootVerbosePower = true;
            
            if ( gGet( gTABLE ))  break;
        }
        endProgress();
    } else if (hwPlusBoot) {
      BootToUSB = true;
    }
#if DEBUG
    else if (hwMinusBoot) {
        BootDebugLoop = true;
    }
#endif
}


static osThreadAttr_t 	tb_attr;																				// has to be static!
int  										main( void ){
	// main.h defines gpioSignals[] for this platform configuration,  e.g.  { gRED, "PF8" } for STM3210E eval board RED LED
	//  used by various initializers to call GPIOconfigure()
	GPIO_DefineSignals( gpioSignals );   // create GPIO_Def_t entries { id, port, pin, intq, signm, pressed } for each GPIO signal in use

    setBootMode();
	
	SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk
	 | SCB_SHCSR_BUSFAULTENA_Msk
	 | SCB_SHCSR_MEMFAULTENA_Msk; // enable Usage-/Bus-/MPU Fault
	SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;					// set trap on div by 0
//	SCB->CCR |= SCB_CCR_UNALIGN_TRP_Msk;				// set trap on unaligned access -- CAN'T because RTX TRAPS
//	int c = divTst( 10, 0 );	// TEST fault handler
	
	volatile int CpuMhz = 24, apbsh2 = 0, apbsh1 = 1;
	const int MHZ=1000000;
	setCpuClock( CpuMhz, apbsh2, apbsh1 );						// generate HCLK (CPU_clock) from PLL based on 8MHz HSE  & PLLI2S_Q at 48MHz
    SystemCoreClockUpdate();			// derives clock speed from configured register values-- cross-check calculated value
	if ( SystemCoreClock != CpuMhz * MHZ )
		__breakpoint(0);

//	gConfigI2S( gMCO2 );  // TBookV2B 	{ gMCO2,					"PC9|0"		},  // DEBUG: MCO2 for external SystemClock/4 on PC9

  initPrintf(  TBV2_Version );
	initIDs();

	osKernelInitialize();                 // Initialize CMSIS-RTOS
	
	tb_attr.name = (const char*) "talking_book"; 
	tb_attr.stack_size = TBOOK_STACK_SIZE;
	Dbg.thread[0] = (osRtxThread_t *)osThreadNew( talking_book, NULL, &tb_attr );    // Create application main thread
		
	osKernelStart();                      // Start thread execution
	for (;;) {}
}
// end main.c




