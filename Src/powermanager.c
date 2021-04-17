// TBookV2  powermanager.c
//   Apr 2018

#include "tbook.h"
#include "powerMgr.h"

//#include "stm32f4xx_hal_dma.h"
//#include "stm32f4xx_hal_adc.h"

void 						cdc_PowerDown( void );			// ti_aic3100.c -- power down entire codec, requires re-init

extern fsTime					RTC_initTime;					// time from status.txt 
extern bool						firstBoot;						// true if 1st run after data loaded


// pwrEventFlags--  PM_NOPWR  PM_PWRCHK   PM_ADCDONE		-- signals from IRQ to powerThread
#define 									PM_NOPWR  1											// id for osEvent signaling NOPWR interrupt
#define 									PM_PWRCHK 2											// id for osTimerEvent signaling periodic power check
#define 									PM_ADCDONE  4										// id for osEvent signaling period power check interrupt
#define 									PWR_TIME_OUT 15000							// do initial power check after 15sec

enum PwrStat { TEMPFAULT=0, xxx=1, CHARGING=2, LOWBATT=3, CHARGED=4, xxy=5, NOLITH=6, NOUSBPWR=7 };

static osTimerId_t				pwrCheckTimer = NULL;
static osEventFlagsId_t 	pwrEvents = NULL;
static osThreadAttr_t 		pm_thread_attr;

//
//  **************************  ADC: code to read ADC values on gADC_LI_ION & gADC_PRIMARY
static volatile uint32_t				adcVoltage;											// set by ADC ISR
static ADC_TypeDef *						Adc = (ADC_TypeDef *) ADC1_BASE;
static ADC_Common_TypeDef *			AdcC = (ADC_Common_TypeDef *) ADC1_COMMON_BASE;

// ADC channels
const int ADC_LI_ION_chan 			= 2;   			// PA2 = ADC1_2		(STM32F412xE/G datasheet pg. 52)
const int ADC_PRIMARY_chan 			= 3;   			// PA3 = ADC1_3
const int ADC_VREFINT_chan 			= 17;				// VREFINT (internal) = ADC1_17 if ADC_CCR.TSVREFE = 1
const int ADC_VBAT_chan 				= 18;   		// VBAT = ADC1_18  if ADC_CCR.VBATE = 1
const int ADC_TEMP_chan 				= 18;   		// TEMP = ADC1_18  if ADC_CCR.TSVREFE = 1
const int gADC_THERM_chan				= 12;				// PC2 = ADC_THERM = ADC1_12

typedef struct {
	uint32_t							chkCnt;
	enum PwrStat					Stat;								// stat1 stat2 pgn
	uint32_t							VRefMV;							// should be 1200 
	uint32_t							VBatMV;							// Backup CR2032 voltage: 2000..3300
	uint32_t							LiMV;								// liIon voltage: 2000..3900
	uint32_t							PrimaryMV;					// Primary (disposables) battery: 2000..2800
	uint32_t							MpuTempMV;					// ADC_CHANNEL_TEMPSENSOR in ADC_IN18 
	uint32_t							LiThermMV;					// LiIon thermister 
	
	enum PwrStat					prvStat;						// previous saved by startPowerCheck()
	bool									haveUSB; 						// USB pwrGood signal
	bool									hadVBat;						// previous was > 2000: saved by startPowerCheck()
	bool									hadLi;							// previous was > 2000: saved by startPowerCheck()
	bool									hadPrimary;					// previous was > 2000: saved by startPowerCheck()
} PwrState;
static PwrState 					pS;				// state of powermanager

extern void 							ADC_IRQHandler( void );					// override default (weak) ADC_Handler
static void 							powerThreadProc( void *arg );		// forward
static void 							initPwrSignals( void );					// forward

static int								currPwrTimerMS;

bool 											RebootOnKeyInt = false;					// tell inputmanager.c to reboot on key interrupt


void checkPowerTimer(void *arg);                          // forward for timer callback function

void											initPowerMgr( void ){						// initialize PowerMgr & start thread
	pwrEvents = osEventFlagsNew(NULL);
	if ( pwrEvents == NULL )
		tbErr( "pwrEvents not alloc'd" );
	
	osTimerAttr_t 	timer_attr;
	timer_attr.name = "PM Timer";
	timer_attr.attr_bits = 0;
	timer_attr.cb_mem = 0;
	timer_attr.cb_size = 0;
	pwrCheckTimer = osTimerNew(	checkPowerTimer, osTimerPeriodic, 0, &timer_attr);
	if ( pwrCheckTimer == NULL ) 
		tbErr( "pwrCheckTimer not alloc'd" );
	
	initPwrSignals();		// setup power pins & NO_PWR interrupt

	pm_thread_attr.name = "PM Thread";	
	pm_thread_attr.stack_size = POWER_STACK_SIZE; 
	Dbg.thread[1] = (osRtxThread_t *)osThreadNew( powerThreadProc, 0, &pm_thread_attr ); //&pm_thread_attr );
	if ( Dbg.thread[1] == NULL ) 
		tbErr( "powerThreadProc not created" );
		
	currPwrTimerMS = PWR_TIME_OUT;
	osTimerStart( pwrCheckTimer, currPwrTimerMS );
	dbgLog( "4 PowerMgr OK \n" );
}
void											setPowerCheckTimer( int timerMs ){
	osTimerStop( pwrCheckTimer );
	osTimerStart( pwrCheckTimer, timerMs );
}
void 											enableSleep( void ){						// low power mode -- (STM32 Stop) CPU stops till interrupt
	//dbgLog( "Stp\n");
	extern uint16_t					KeypadIMR;				// inputmanager.c -- keyboard IMR flags

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
	int sleep = osKernelSuspend();		// turn off sysTic 
	RebootOnKeyInt = true;
	EXTI->IMR |= KeypadIMR;					// enable keypad EXTI interrupts
	
	__WFI();	// sleep till interrupt -- 10 keyboard EXTI's enabled
	
	NVIC_SystemReset();			// soft reboot
}
void 											enableStandby( void ){					// power off-- (STM32 Standby) reboot on wakeup from NRST
	dbgLog( "NdBy\n");
	PWR->CR |= PWR_CR_CWUF;							// clear wakeup flag
	PWR->CR |= PWR_CR_PDDS;							// set power down deepsleep 
	SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;	// set DeepSleep
	
	for (int i=0; i<3; i++){			// clear all pending ISR
		uint32_t pend = NVIC->ISPR[i];
		if ( pend != 0 ) 	// clear any pending interrupts
			NVIC->ICPR[i] = pend;		
	}
	
	__WFI();	// standby till reboot -- shouldn't ever return
	NVIC_SystemReset();			// force a system reset
}
void											ResetGPIO( void ){
	GPIO_ID actHi[] = { 
			gBOOT1_PDN, gBAT_CE, 		gSC_ENABLE, 	gEN_5V, 
			gEN1V8, 		g3V3_SW_EN,	gADC_ENABLE,
			gGREEN, 		gRED, 			gEMMC_RSTN,   gINVALID
	};
	GPIO_ID actLo[] = { 
			gBAT_TE_N, 	gEN_IOVDD_N, gEN_AVDD_N,  gSPI4_NSS,	gINVALID 	// PD13, PE4, PE5, PE11
	};
	for (int i=0; actHi[i]!=gINVALID; i++)  gSet( actHi[i], 0 );		// only works if configured as output (Mode1)
	for (int i=0; actLo[i]!=gINVALID; i++)  gSet( actLo[i], 1 );		// only works if configured as output (Mode1)
	
	GPIO_ID extGPIO[] = {  // reconfig all output GPIO's to reset state
		gI2S2ext_SD,	gI2S2_SD,		gI2S2_WS,			gI2S2_CK,			// PB14, 	PB15, PB12, PB13		
		gI2S3_MCK,		gI2C1_SDA,	gI2C1_SCL,		gI2S2_MCK,		// PC7, 	PB9, 	PB8, 	PC6
		gUSB_DM, 			gUSB_DP, 		gUSB_ID,			gUSB_VBUS,		// PA11,	PA12, PA10, PA9
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
//		gSWDIO,				gSWCLK,			gSWO,												// PA13,  PA14, PB3  -- JTAG DEBUGGER 
		gINVALID	};
	for (int i=0; extGPIO[i]!=gINVALID; i++) 
		gConfigADC( extGPIO[i] );		// RESET GPIOs to analog (Mode 3, Pup 0) 
	
	GPIO_ID spiGPIO[] = {  // reconfig SPI GPIO's to input PU
		gSPI4_SCK, 		gSPI4_MISO, gSPI4_MOSI, 	gSPI4_NSS,		// PE12,	PE13, PE14, PE11
		gINVALID };
	for (int i=0; spiGPIO[i]!=gINVALID; i++) 
		gConfigIn( spiGPIO[i], false );		// set to Input PU (Mode 0, Pup 1) 
	
	RCC->CR &= ~RCC_CR_PLLI2SON_Msk;	// shut off PLLI2S
	FLASH->ACR = 0;			// disable Data, Instruction, & Prefetch caches
	SYSCFG->CMPCR = 0;	// power down I/O compensation cell

	// turn off all the clocks we can
	RCC->AHB1ENR = 0;
	RCC->AHB2ENR = 0;
	RCC->APB1ENR = 0x00000400; // reset value -- RTCAPB=1
	RCC->APB2ENR = 0x00008000; // reset value -- D15=1 (reserved, must be 1) == RCC_APB2ENR_EXTITEN_Msk
	
	RCC->AHB1LPENR = 0; 
	RCC->AHB2LPENR = 0;
	RCC->APB1LPENR = 0;
	RCC->APB2LPENR = 0;	
}
void											powerDownTBook( bool sleep ){					// shut down TBook
//	ledFg( TB_Config.fgPowerDown );
//DEBUG*********************************************************
  sleep = !gGet( gPLUS );   // standby if PLUS held down 
//*********************************************************DEBUG
	ledBg( sleep? "G!":"R!" );
	logEvtNS( "PwrDown", "mode", sleep? "Stop":"Standby" );
	logPowerDown();		// flush & close logs, copy NorLog to eMMC
	ledBg( NULL );
	ledFg( NULL );
	FileSysPower( false );		// shut down eMMC supply, unconfig gSDIO_*
	cdc_PowerDown();					// turn off codec
	
	ResetGPIO();
	
	if ( sleep )
		enableSleep();
	else
		enableStandby();		// shut down
}

void 											initPwrSignals( void ){					// configure power GPIO pins, & EXTI on NOPWR 	
	// power supply signals
	gConfigOut( gADC_ENABLE );	// 1 to enable battery voltage levels
	gSet( gADC_ENABLE, 0 );
	gConfigOut( gSC_ENABLE );		// 1 to enable SuperCap
	gSet( gSC_ENABLE, 0 );
	
	gConfigADC( gADC_LI_ION );	// rechargable battery voltage level
	gConfigADC( gADC_PRIMARY );	// disposable battery voltage level
	gConfigADC( gADC_THERM );	  // thermistor (R54) by LiIon battery
	
	gConfigIn( gPWR_FAIL_N, false );	// PD0 -- input power fail signal, with pullup
	
	gConfigIn( gBAT_PG_N,   false );	// MCP73871 PG_    IN:  0 => power is good  (configured active high)
	gConfigIn( gBAT_STAT1,  false );  // MCP73871 STAT1  IN:  open collector with pullup
	gConfigIn( gBAT_STAT2,  false );  // MCP73871 STAT2  IN:  open collector with pullup

	gConfigOut( gBAT_CE );  	// OUT: 1 to enable charging 			MCP73871 CE 	  (powermanager.c)
	gSet( gBAT_CE, 1 );				// always enable?
	gConfigOut( gBAT_TE_N );  // OUT: 0 to enable safety timer 	MCP73871 TE_	  (powermanager.c)
	gSet( gBAT_TE_N, 0 );			// disable?
	
	// enable power & signals to EMMC & SDIO devices
	FileSysPower( true );				// power up eMMC & SD 3V supply
	
  // Configure audio power control 
	gConfigOut( gEN_5V );				// 1 to supply 5V to codec-- enable AP6714 regulator  -- powers AIC3100 SPKVDD
	gConfigOut( gEN1V8 );				// 1 to supply 1.8 to codec-- enable TLV74118 regulator	-- powers AIC3100 DVDD
  gConfigOut( gBOOT1_PDN );		// 0 to reset codec -- RESET_N on AIC3100 (boot1_pdn on AK4637)
	gSet( gEN_5V, 0 );					// initially codec SPKVDD unpowered PD4
	gSet( gEN1V8, 0 );					// initially codec DVDD unpowered PD5
	gSet( gBOOT1_PDN, 0 );			// initially codec in reset state  PB2
	
#if defined(TBook_V2_Rev3)
	gConfigOut( gEN_IOVDD_N );	// 0 to supply 3V to AIC3100 IOVDD	
	gConfigOut( gEN_AVDD_N );		// 0 to supply 3V to AIC3100 AVDD & HPVDD
	gSet( gEN_IOVDD_N, 1 );			// initially codec IOVDD unpowered  PE4
	gSet( gEN_AVDD_N, 1 );			// initially codec AVDD & HPVDD unpowered PE5
#endif
	
#if defined(TBook_V2_Rev1)
	gConfigOut( gPA_EN );				// 1 to power headphone jack
  gSet( gPA_EN, 0 );					// initially audio external amplifier OFF
#endif

	tbDelay_ms( 5 ); // pwr start up: 3 );		// wait 3 msec to make sure everything is stable
}
void											startADC( int chan ){						// set up ADC for single conversion on 'chan', then start 
	AdcC->CCR &= ~ADC_CCR_ADCPRE;		// CCR.ADCPRE = 0 => PClk div 2

	// Reset ADC.CR1  		//  0==>  Res=12bit, no watchdogs, no discontinuous, 
	Adc->CR1 = 0;  				//        no auto inject, watchdog, scanning, or interrupts

	// Reset ADC.CR2      					//  no external triggers, injections,
	Adc->CR2 = ADC_CR2_ADON;				//  DMA, or continuous conversion -- only power on
	
	Adc->SQR1 = 0;									// SQR1.L = 0 => 1 conversion 
	Adc->SQR3 = chan;								// chan is 1st (&only) conversion sequence
	
	Adc->SMPR1 = 0x0EFFFFFF;				// SampleTime = 480 for channels 10..18
	Adc->SMPR2 = 0x3FFFFFFF;				// SampleTime = 480 for channels 0..9
	
	Adc->SR  &= ~(ADC_SR_EOC | ADC_SR_OVR);	// Clear regular group conversion flag and overrun flag 
	Adc->CR1 |= ADC_CR1_EOCIE;									// Enable end of conversion interrupt for regular group 
  Adc->CR2 |= ADC_CR2_SWSTART; 								// Start ADC software conversion for regular group 
}
short											readADC( int chan, int enableBit ){	// readADC channel 'chan' value 
	if ( enableBit!=0 ) 
		AdcC->CCR |= enableBit;										// enable this channel

  startADC( chan );

	// wait for signal from ADC_IRQ on EOC interrupt
	if ( osEventFlagsWait( pwrEvents, PM_ADCDONE, osFlagsWaitAny, 10000 ) != PM_ADCDONE )
			logEvt( "ADC timed out" );

	Adc->CR1 &= ~ADC_CR1_EOCIE;  								// Disable ADC end of conversion interrupt for regular group 
	
	uint32_t adcRaw = Adc->DR;  								// read converted value (also clears SR.EOC)
	
	if ( enableBit != 0 )
		AdcC->CCR &= ~enableBit;									// disable channel again
		
	return adcRaw;
} 

static void								EnableADC( bool enable ){				// power up & enable ADC interrupts
	if ( enable ){
		gSet( gADC_ENABLE, 1 );									// power up the external analog sampling circuitry
		NVIC_EnableIRQ( ADC_IRQn );
		RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;			// enable clock for ADC
		Adc->CR2 |= ADC_CR2_ADON; 							// power up ADC
		tbDelay_ms( 1 );	// ADC pwr up
		} else {
		gSet( gADC_ENABLE, 0 );
		NVIC_DisableIRQ( ADC_IRQn );
		RCC->APB2ENR &= ~RCC_APB2ENR_ADC1EN;			// disable clock for ADC
		Adc->CR2 &= ~ADC_CR2_ADON; 								// power down ADC
	} 
}
void 											ADC_IRQHandler( void ){ 				// ISR for ADC end of conversion interrupts
  if ( (Adc->SR & ADC_SR_EOC)==0 ) tbErr("ADC no EOC");
	
	osEventFlagsSet( pwrEvents, PM_ADCDONE );					// wakeup powerThread
	Adc->SR = ~(ADC_SR_STRT | ADC_SR_EOC);    // Clear regular group conversion flag 
}

uint16_t									readPVD( void ){								// read PVD level 
	RCC->APB1ENR |= RCC_APB1ENR_PWREN;						// start clocking power control 
	
	PWR->CR |= ( 0 << PWR_CR_PLS_Pos );						// set initial PVD threshold to 2.2V
	PWR->CR |= PWR_CR_PVDE;												// enable Power Voltage Detector
	
	int pvdMV = 2100;							// if < 2200, report 2100 
	for (int i=0; i< 8; i++){
		PWR->CR = (PWR->CR & ~PWR_CR_PLS_Msk) | ( i << PWR_CR_PLS_Pos );
		tbDelay_ms(1);  // waiting for Pwr Voltage Detection
		
		if ( (PWR->CSR & PWR_CSR_PVDO) != 0 ) break;
		pvdMV = 2200 + i*100;
	}
	return pvdMV;
}
uint8_t										Bcd2( int v ){									// return v as 8 bits-- bcdTens : bcdUnits
	if ( v > 99 ) v = v % 100;
	int vt = v / 10, vu = v % 10;
	return (vt << 4) | vu;
}
void											setupRTC( fsTime time ){				// init RTC & set based on fsTime
	int cnt = 0;
	RCC->APB1ENR |= ( RCC_APB1ENR_PWREN | RCC_APB1ENR_RTCAPBEN );				// start clocking power control & RTC_APB
	PWR->CSR 	|= PWR_CSR_BRE;											// enable backup power regulator
	while ( (PWR->CSR & PWR_CSR_BRR)==0 && (cnt < 1000)) cnt++;  //  & wait till ready

	RCC->BDCR |= RCC_BDCR_BDRST;   				// backup domain reset
	RCC->BDCR &= ~RCC_BDCR_BDRST;   			// & release

	RTC->WPR = 0xCA;
	RTC->WPR = 0x53;		// un write-protect the RTC
	
	PWR->CR 	|= PWR_CR_DBP;			// enable access to Backup Domain to configure RTC
	
	RCC->BDCR |= RCC_BDCR_RTCEN;
	RCC->BDCR |= RCC_BDCR_LSEON;												// enable 32KHz LSE clock
	cnt = 0;
	while ( (RCC->BDCR & RCC_BDCR_LSERDY)==0 && (cnt < 1000) ) cnt++; 	// & wait till ready

	// configure RTC  (per RM0402 22.3.5)
	RCC->BDCR |= RCC_BDCR_RTCSEL_0;										// select LSE as RTC source

	RTC->WPR = 0xCA;
	RTC->WPR = 0x53;		// un write-protect the RTC

	RTC->ISR |= RTC_ISR_INIT;													// set init mode
	cnt = 0;
	while ( (RTC->ISR & RTC_ISR_INITF)==0 && (cnt < 1000)) cnt++;		// & wait till ready
	
	RTC->PRER  = (256 << RTC_PRER_PREDIV_S_Pos);				// Synchronous prescaler = 256  MUST BE FIRST
	RTC->PRER |= (127 << RTC_PRER_PREDIV_A_Pos);				// THEN asynchronous = 127 for 32768Hz => 1Hz
	
	// try AM/PM to see if days will switch
	int32_t ampm = 0, hr = time.hr;
	if ( hr > 12 ){ ampm = 1; hr -= 12; }
	uint32_t TR = (ampm << 22) | ( Bcd2( hr ) << 16 ) | ( Bcd2( time.min ) << 8 ) | Bcd2( time.sec );
	
	// calc weekday from date
	int yr = time.year, yy = yr % 100, cen = yr / 100, mon = time.mon, day = time.day;
	bool leap = (yr%400==0) || (yr%4==0 && yr%100!=0);
	uint16_t cenCd[] = { 6, 4, 2 };	// for 20, 21, 22
	uint16_t monCd[] = { 0, 3, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5 };
	uint16_t cCd = cenCd[cen-20];
	uint16_t yCd = (yy/4 + yy) % 7;
	uint16_t mCd = monCd[ mon-1 ];
	uint8_t wkday = (yCd+mCd+day+cCd - (leap && mon<3? 1 : 0)) % 7;
	if (wkday==0) wkday = 7;
	
	uint32_t DR = ( Bcd2( yy ) << 16 ) | (wkday << 13 ) | ( Bcd2( mon ) << 8 ) | Bcd2( day );

	RTC->CR 	|= RTC_CR_FMT;			// use AM/PM hour format
	RTC->TR = TR;		// set time of day
	RTC->DR = DR;		// set y
	// RTC->CR.FMT == 0 (24 hour format)
	RTC->ISR 	&= ~RTC_ISR_INIT;												// leave RTC init mode
	
	PWR->CR 	&= ~PWR_CR_DBP;													// disable access to Backup Domain 

}
void											checkPowerTimer( void *arg ){		// timer to signal periodic power status check
	osEventFlagsSet( pwrEvents, PM_PWRCHK );						// wakeup powerThread for power status check
	if ( currPwrTimerMS != TB_Config.powerCheckMS ){		// update delay (after initial check)
		currPwrTimerMS = TB_Config.powerCheckMS;
		setPowerCheckTimer( currPwrTimerMS );
	}
}
char 											RngChar( int lo, int hi, int val ){   // => '-', '0', ... '9', '!' 
	if (val < lo) return '-';
	if (val > hi) return '!';
	int stp = (hi-lo)/10;
	return ( (val-lo)/stp + '0' );
}

//bool HaveUsbPower = false, HaveLithium = false, HavePrimary = false, HaveBackup = false, firstCheck = true, pwrChanged;
//enum PwrStat oldState;
void											startPowerCheck( enum PwrStat pstat ){
	// save previous values
	pS.prvStat		= pS.Stat;
	pS.hadLi 			= pS.LiMV > 2000;
	pS.hadPrimary = pS.PrimaryMV > 2000;
	pS.hadVBat		= pS.VBatMV > 2000;

	pS.chkCnt++;
	pS.Stat = pstat;
	pS.haveUSB = (pstat & 1) == 0;
}
bool											powerChanged(){
	bool changed = pS.chkCnt==1;
	if ( pS.prvStat != pS.Stat ){ 
		dbgLog( "5 pwrStat %d => %d \n", pS.prvStat, pS.Stat );
		changed = true;
	}
	if ( (pS.LiMV > 2000) 			!= pS.hadLi ){
		dbgLog( "5 LiIon %d \n", pS.LiMV );
		changed = true;
	}
	if ( (pS.VBatMV > 2000)			!= pS.hadVBat ){
		dbgLog( "5 VBat %d \n", pS.VBatMV );
		changed = true;
	}
	if ( (pS.PrimaryMV > 2000)	!= pS.hadPrimary ){
		dbgLog( "5 Primary %d \n", pS.PrimaryMV );
		changed = true;
	}
	return changed;
}

const int LiLOW = 3400;     // mV at  ~5% capacity
const int LiMED = 3600;			// mV at ~40%
const int LiHI  = 3800;			// mV at ~75%
const int ReplLOW = 2300;   // mV at ~15% capacity (for AA Alkaline)
const int ReplMED = 2500;		// mV at ~45%  
const int ReplHI  = 2700;		// mV at ~75%  
const int HiMpuTemp = 800;
const int HiLiTemp  = 800;

void											cdc_PowerUp( void );	// from ti_aic3100 -- for early init on pwr thread

void 											checkPower( ){				// check and report power status
	//  check gPWR_FAIL_N & MCP73871: gBAT_PG_N, gBAT_STAT1, gBAT_STAT2
	bool PwrFail 			= gGet( gPWR_FAIL_N );	// PD0 -- input power fail signal
  if ( PwrFail && false ) 
		powerDownTBook( false );
	
	//  check MCP73871: gBAT_PG_N, gBAT_STAT1, gBAT_STAT2
	bool PwrGood_N 	  = gGet( gBAT_PG_N );	 // MCP73871 PG_:  0 => power is good   			MCP73871 PG_    (powermanager.c)
	bool BatStat1 		= gGet( gBAT_STAT1 );  // MCP73871 STAT1
	bool BatStat2 		= gGet( gBAT_STAT2 );  // MCP73871 STAT2
	
	// MCP73871 battery charging -- BatStat1==STAT1  BatStat2==STAT2  BatPwrGood==PG
	// Table 5.1  Status outputs
	//   STAT1 STAT2 PG_
	// 		H			H			H		7-- no USB input power
	// 		H			H			L		6-- no LiIon battery
	//		H			L			L		4-- charge completed
	//		L			H			H		3-- low battery output
	//		L			H			L		2-- charging
	// 		L			L			L		0-- temperature (or timer) fault

	enum PwrStat pstat = (enum PwrStat) ((BatStat1? 4:0) + (BatStat2? 2:0) + (PwrGood_N? 1:0));
	startPowerCheck( pstat );
	
	EnableADC( true );			// enable AnalogEN power & ADC clock & power, & ADC INTQ 

	const int MAXCNT 	= 4096;		// max 12-bit ADC count
	const int VREF 		= 3300;		// mV of VREF input pin
	
	int VRefIntCnt = readADC( ADC_VREFINT_chan, ADC_CCR_TSVREFE );	// enable RefInt on channel 17 -- typically 1.21V (1.18..1.24V)
  pS.VRefMV = VRefIntCnt * VREF/MAXCNT;			// should be 1200
	
	int MpuTempCnt = readADC( ADC_TEMP_chan, ADC_CCR_TSVREFE ); // ADC_CHANNEL_TEMPSENSOR in ADC_IN18 
  pS.MpuTempMV = MpuTempCnt * VREF/MAXCNT;			

	int VBatCnt = readADC( ADC_VBAT_chan, ADC_CCR_VBATE ); 		// enable VBAT on channel 18
  pS.VBatMV = VBatCnt * 4 * VREF/MAXCNT;		

	int LiThermCnt = readADC( gADC_THERM_chan, 0 );	
  pS.LiThermMV = LiThermCnt * VREF/MAXCNT;		
	
	int LiCnt = readADC( ADC_LI_ION_chan, 0 );	
  pS.LiMV = LiCnt * 2 * VREF/MAXCNT;			

	int PrimaryCnt = readADC( ADC_PRIMARY_chan, 0 );
  pS.PrimaryMV = PrimaryCnt * 2 * VREF/MAXCNT;	
	
	EnableADC( false );		// turn ADC off
	
	if ( powerChanged() ){	// update pS state & => true if significant change
		char sUsb = PwrGood_N==0? 'U' : 'u';
		char sLi = RngChar( 3000, 4000, pS.LiMV ); 				// range from charge='0' to charge='9' to '!' > 4000
		char sPr = RngChar( 2000, 4000, pS.PrimaryMV ); 	// 2000..2200 = '0', 3800..4000 = '9'
		char sBk = RngChar( 2000, 4000, pS.VBatMV ); 
		char sMt = RngChar(  200, 1200, pS.MpuTempMV ); 	// 200..300 = '0'  1000..1200 = '9'
		char sLt = RngChar(  200, 1200, pS.LiThermMV ); 
		char sCh = ' ';
		if (pstat!=CHARGING)  sLt = ' ';		// not meaningful unless charging
		if (pstat==CHARGING)  sCh = 'c'; 
		if (pstat==CHARGED) 	sCh = 'C';
		if (pstat==TEMPFAULT) sCh = 'X';

		char pwrStat[10];
		sprintf(pwrStat, "%c L%c%c%c P%c B%c T%c", sUsb, sLi,sCh,sLt, sPr, sBk, sMt); 
		logEvtNS( "PwrCheck","Stat",	pwrStat ); 

		dbgLog( "5 srTB: %d %4d %3d %4d\n", pstat, pS.VRefMV, pS.MpuTempMV, pS.VBatMV );
		dbgLog( "5 LtLP: %3d %4d %4d\n", pS.LiThermMV, pS.LiMV, pS.PrimaryMV );
		if ( pS.MpuTempMV > HiMpuTemp ){
			logEvtNI("MpuTemp", "mV", pS.MpuTempMV );
			sendEvent( MpuHot, pS.MpuTempMV );
		}
	  if ( pS.haveUSB ){		// charger status is only meaningful if we haveUSB power
			switch ( pstat ){
				case CHARGED:						// CHARGING complete
					logEvtNI("Charged", "mV", pS.LiMV ); 
					sendEvent( BattCharged, pS.LiMV ); 			
					break; 
				case CHARGING: 					// started charging
					logEvtNI("Charging", "mV", pS.LiMV ); 
					sendEvent( BattCharging, pS.LiMV );	
					if ( pS.LiThermMV > HiLiTemp ){		// lithium thermistor is only active while charging
						logEvtNI("LiTemp", "mV", pS.LiThermMV );
						sendEvent( LithiumHot, pS.LiThermMV );
					}
					break;
				case TEMPFAULT:					// LiIon charging fault (temp?)
					logEvtNI("ChrgeFlt", "LiTherm mV", pS.LiThermMV ); 
					sendEvent( ChargeFault,  pS.LiMV );	
					break;
				case LOWBATT:						// LiIon is low  (no USB)
					logEvtNI("BattLow", "mV", pS.LiMV ); 
					sendEvent( LowBattery,  pS.LiMV );		
					break;
				default: break;
			}
		} else {  // no USB,  report if batteries are present, but low
			if ( pS.PrimaryMV > 1000 && pS.PrimaryMV < ReplLOW ){
				logEvtNI("ReplBattLow", "mV", pS.PrimaryMV ); 
				sendEvent( LowBattery,  pS.PrimaryMV );		
			}
			if ( pS.LiMV > 1000 && pS.LiMV < LiLOW ){
				logEvtNI("LiBattLow", "mV", pS.LiMV ); 
				sendEvent( LowBattery,  pS.LiMV );		
			}
		}
	}
}
bool											haveUSBpower(){				// true if USB plugged in (PwrGood_N = 0)
	if ( pS.chkCnt==0 )		checkPower();
	return pS.haveUSB;
}
void 											showBattCharge(){			// generate ledFG to signal power state
	char fg[20] = {0};
	const char *fmt = "_5%c5_5%c5_5%c5_15";		// 3 .5sec flashes
	char c1, c2, c3;
	
	if ( pS.chkCnt==0 )		checkPower();

	if ( pS.haveUSB )
		logEvt("PwrOnUSB" );
	
	logEvtNI("PwrLiIon", 		"mV", 	pS.LiMV );
	logEvtNI("PwrPrimary", 	"mV", 	pS.PrimaryMV );
	logEvtNI("PwrBackup", 	"mV", 	pS.VBatMV );
	logEvtNI("Therm", 			"Mpu", 	pS.MpuTempMV );

  // if haveUSB power & not NOLITH:  report charging status
	// R R R -- temp fault
	// G O R -- low liIon pre-charging or LiMED
	// G O O -- charging & LiMV < LiHI
	// G O G -- charging & LiMV > LiHI
	// G G G -- charged
  if ( pS.LiMV > 2000 ){
		if ( pS.haveUSB ){ // report Li charge status
			c1 = 'G';
			c2 = 'O';
			logEvtNI("Charger", "Stat", pS.Stat );
			switch ( pS.Stat ){
				case TEMPFAULT:		c1='R'; c2='R', c3='R'; break;	// R R R   - temp fault
				case LOWBATT:			c3='R';	break;									// G O R -- low, pre-charge
				case CHARGING:		
					logEvtNI("Charging", "LiThermMV", pS.LiThermMV );
					c3 = pS.LiMV < LiMED? 'R' : (pS.LiMV < LiHI?'O' : 'G');		// GOR, GOO or GOG
				  break;
				case CHARGED:			c2='G'; c3='G';	break;  				// G G G  -- charged
				
				default: 			// incl NOLITH-- shouldn't get here
					break;
			}
		} else
		// if no USB, but LiMV > 2000 : report LiIon level
		// O O R -- LiMV < LiLOW
		// O O O -- LiMV < LiMED
		// O O G -- LiMV < LiHI
		// O G G -- LiMV > LiHI
		{	
			c1 = 'O'; 
			c2 = pS.LiMV < LiHI? 'O' : 'G';
			c3 = pS.LiMV < LiLOW? 'R' : (pS.LiMV < LiMED? 'O' : 'G');
		}
	} else 
	// if no USB & no Lith:  report primary (replaceable) level
	// R O R -- primary < ReplLOW
	// R O O -- primary < ReplMED
	// R O G -- primary < ReplHI
	// R G G -- primary > ReplHI
	{
		c1 = 'R'; 
		c2 = pS.PrimaryMV < ReplHI? 'O' : 'G';
		c3 = pS.PrimaryMV < ReplLOW? 'R' : (pS.PrimaryMV < ReplMED? 'O' : 'G');
	}
	sprintf( fg, fmt, c1,c2,c3 );
	ledFg( fg );
	logEvtS("battChg", fg );
}

/*   // *************  Handler for PowerFail interrupt
void 											EXTI4_IRQHandler(void){  				// NOPWR ISR
  	if ( __HAL_GPIO_EXTI_GET_IT( NOPWR_pin ) != RESET ){
		__HAL_GPIO_EXTI_CLEAR_IT( NOPWR_pin );  // clear EXTI Pending register
		HAL_NVIC_DisableIRQ( NOPWR_EXTI );		// don't keep getting them

		bool NoPower = HAL_GPIO_ReadPin( NOPWR_port, NOPWR_pin ) == RESET;	// LOW if no power
		if ( NoPower )
			osEventFlagsSet( pwrEvents, PM_NOPWR );		// wakeup powerThread
		else
			HAL_NVIC_EnableIRQ( NOPWR_EXTI );
	}
}
*/


void 											wakeup(){												// resume operation after sleep power-down
}
static void 							powerThreadProc( void *arg ){		// powerThread -- catches PM_NOPWR from EXTI: NOPWR interrupt
	dbgLog( "4 pwrThr: 0x%x 0x%x \n", &arg, &arg + POWER_STACK_SIZE );
	cdc_PowerUp();    // start codec powering up -- this thread won't suffer from the delay
	
	while( true ){
		int flg = osEventFlagsWait( pwrEvents, PM_NOPWR | PM_PWRCHK, osFlagsWaitAny, osWaitForever );	
		
		switch ( flg ){
			case PM_NOPWR:
				logEvt( "Powering Down");
				enableStandby();			// shutdown-- reboot on wakeup keys
				// never come back  -- just restarts at main()
				break;

			case PM_PWRCHK:
				checkPower( );			// get current power status
				break;
		}
	} 
}
//end  powermanager.c
