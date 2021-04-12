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
extern uint16_t						KeypadIMR;
//bool NO_OS_DEBUG = true;

extern bool RebootOnKeyInt;
void MarcDebug(){				// DEBUG test for Marc -- for Stop mode power calcs
	RCC->APB2ENR   |= RCC_APB2ENR_EXTITEN_Msk;  		// turn on EXTI clock
	RCC->APB2LPENR |= RCC_APB2LPENR_EXTITLPEN_Msk;  // also LP
	
	GPIO_ID keysGPIO[] = {  // configure Keys
		gHOME,	gCIRCLE,  gPLUS,	gMINUS,	gTREE,		// PA0 PE9 PA4 PA5 PA15
		gLHAND,	gRHAND,		gPOT,		gSTAR,	gTABLE,		// PB7 PB10 PC1 PE8	PE3	
		gINVALID };
	KeypadIMR = 0;
	for (int i=0; keysGPIO[i]!=gINVALID; i++){
		int gp = keysGPIO[i];
		gConfigKey( keysGPIO[i] );		// set to Input PD (Mode 0, Pup 2) 
		KeypadIMR  |= (1 << gpio_def[ gp ].pin );
	}
	EXTI->IMR &= ~KeypadIMR; // prevent premature INT from gConfigADC( gUSB_ID ) !

	LED_Init( gRED );
	LED_Init( gGREEN );
	
  if ( gGet( gSTAR )==1 ) return;
	gSet( gRED, 1 );
	tbDelay_ms(1000);
	
	GPIO_ID actHi[] = { 
			gBOOT1_PDN, gBAT_CE, 		gSC_ENABLE, 	gEN_5V, 
			gEN1V8, 		g3V3_SW_EN,	gADC_ENABLE,
			gGREEN, 		gRED,				gINVALID
	};
	GPIO_ID actLo[] = { 
			gBAT_TE_N, 	gEN_IOVDD_N, gEN_AVDD_N,	gINVALID 	//
	};
	for (int i=0; actHi[i]!=gINVALID; i++)  gSet( actHi[i], 0 );
	for (int i=0; actLo[i]!=gINVALID; i++)  gSet( actLo[i], 1 );
	
	GPIO_ID extGPIO[] = {  // reconfig all output GPIO's to reset state
		gI2S2ext_SD,	gI2S2_SD,		gI2S2_WS,			gI2S2_CK,			// PB14, 	PB15, PB12, PB13		
		gI2S3_MCK,		gI2C1_SDA,	gI2C1_SCL,		gI2S2_MCK,		// PC7, 	PB9, 	PB8, 	PC6
		gUSB_DM, 			gUSB_DP, 		gUSB_ID,			gUSB_VBUS,		// PA11,	PA12, PA10, PA9
//		gSPI4_SCK, 		gSPI4_MISO, gSPI4_MOSI, 	gSPI4_NSS,		// PE12,	PE13, PE14, PE11
		gMCO2, 				gBOOT1_PDN, gBAT_CE, 			gSC_ENABLE,		// PC9, 	PB2,	PD0,	PD1
		gBAT_TE_N,																						// PD13
		gEN_5V, 			gEN1V8, 		g3V3_SW_EN, 	gBAT_TE_N, 		// PD4,		PD5,	PD6,	PD13
		gGREEN,  			gRED, 			gEN_IOVDD_N,  gEN_AVDD_N,		// PE0,		PE1,	PE4,	PE5
		gEMMC_RSTN,		gSDIO_CLK,	gSDIO_CMD,		gADC_ENABLE,	// PE10, 	PC12, PD2, 	PE15			
		gSDIO_DAT0,		gSDIO_DAT1,	gSDIO_DAT2,		gSDIO_DAT3,		// PB4,		PA8,	PC10,	PB5 
		gPWR_FAIL_N, 	gBAT_STAT1, gBAT_STAT2,		gBAT_PG_N,		// PE2,		PD8,	PD9,	PD10
		gBOARD_REV,		// PB0
		gQSPI_BK1_IO0,	gQSPI_BK1_IO1, gQSPI_BK1_IO2, gQSPI_BK1_IO3,	// PD11, PD12, PC8, PA1
		gQSPI_CLK_A,	  gQSPI_CLK_B,	 gQSPI_BK1_NCS, gQSPI_BK2_NCS,	// PD3,  PB1,  PB6, PC11
		gQSPI_BK2_IO0,	gQSPI_BK2_IO1, gQSPI_BK2_IO2, gQSPI_BK2_IO3,	// PA6,  PA7,  PC4, PC5
//		gSWDIO,				gSWCLK,			gSWO,												// PA13,  PA14, PB3
		gINVALID															 
	};
	for (int i=0; extGPIO[i]!=gINVALID; i++) 
		gConfigADC( extGPIO[i] );		// RESET GPIOs to analog (Mode 3, Pup 0) 
	
	GPIO_ID spiGPIO[] = {  // reconfig SPI GPIO's to input PU
		gSPI4_SCK, 		gSPI4_MISO, gSPI4_MOSI, 	gSPI4_NSS,		// PE12,	PE13, PE14, PE11
		gINVALID };
	for (int i=0; spiGPIO[i]!=gINVALID; i++) 
		gConfigIn( spiGPIO[i], false );		// set to Input PU (Mode 0, Pup 1) 
	
	RCC->CR &= ~RCC_CR_PLLI2SON_Msk;	// shut off PLLI2S
	RCC->AHB1ENR = 0;
	RCC->AHB2ENR = 0;
	RCC->APB1ENR = 0x00000400; // reset value -- RTCAPB=1
	RCC->APB2ENR = 0x00008000; // reset value -- D15=1 (reserved, must be 1) == RCC_APB2ENR_EXTITEN_Pos
	
	RCC->AHB1LPENR = 0; 
	RCC->AHB2LPENR = 0;
	RCC->APB1LPENR = 0;
	RCC->APB2LPENR = 0;

	SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;	// set DeepSleep
	PWR->CR |= PWR_CR_MRLVDS;		// main regulator in low voltage when Stop
	PWR->CR |= PWR_CR_LPLVDS;   // low-power regulator in low voltage when Stop
	PWR->CR |= PWR_CR_FPDS;   	// flash power-down when Stop
	PWR->CR |= PWR_CR_LPDS;   	// low-power regulator when Stop
	
	for (int i=0; i<3; i++){			// clear all pending ISR
		uint32_t pend = NVIC->ISPR[i];
		if ( pend != 0 ) 	// clear any pending interrupts
			NVIC->ICPR[i] = pend;		
	}
	EXTI->IMR |= KeypadIMR;		// enable keypad EXTI interrupts
	RebootOnKeyInt = true;	
	
	__WFI();	// sleep till interrupt -- 10 keyboard EXTI's enabled
	__breakpoint(2);
}

static osThreadAttr_t 	tb_attr;																				// has to be static!
int  										main( void ){
//	GPIO_DefineSignals( gpioSignals );   // create GPIO_Def_t entries { id, port, pin, intq, signm, pressed } for each GPIO signal in use
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

	GPIO_DefineSignals( gpioSignals );   // create GPIO_Def_t entries { id, port, pin, intq, signm, pressed } for each GPIO signal in use
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




