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
static osThreadAttr_t 	tb_attr;																				// has to be static!
int  										main( void ){
	GPIO_DefineSignals( gpioSignals );   // create GPIO_Def_t entries { id, port, pin, intq, signm, pressed } for each GPIO signal in use
//	MarcDebug();
	
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

//	GPIO_DefineSignals( gpioSignals );   // create GPIO_Def_t entries { id, port, pin, intq, signm, pressed } for each GPIO signal in use
	// main.h defines gpioSignals[] for this platform configuration,  e.g.  { gRED, "PF8" } for STM3210E eval board RED LED
	//  used by various initializers to cal GPIOconfigure()
	gConfigI2S( gMCO2 );  // TBookV2B 	{ gMCO2,					"PC9|0"		},  // DEBUG: MCO2 for external SystemClock/4 on PC9

  initPrintf(  TBV2_Version );
  flashInit();		// init Keypad for debugging
	if ( gGet( gPLUS )){  //  PLUS => tbook with no OS -> debugLoop
		flashCode( 10 );
		talking_book( NULL );   // call without OS
	}
	
	initIDs();

	osKernelInitialize();                 // Initialize CMSIS-RTOS
	
	tb_attr.name = (const char*) "talking_book"; 
	tb_attr.stack_size = TBOOK_STACK_SIZE;
	Dbg.thread[0] = (osRtxThread_t *)osThreadNew( talking_book, NULL, &tb_attr );    // Create application main thread
		
	osKernelStart();                      // Start thread execution
	for (;;) {}
}
// end main.c




