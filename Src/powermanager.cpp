// TBookV2  powermanager.c
//   Apr 2018

#include "csm.h"
#include "tbook.h"
#include "powermanager.h"

//#include "stm32f4xx_hal_dma.h"
//#include "stm32f4xx_hal_adc.h"

void cdc_PowerDown( void );      // ti_aic3100.c -- power down entire codec, requires re-init

extern fsTime           RTC_initTime;         // time from status.txt
extern bool             firstBoot;            // true if 1st run after data loaded


// pwrEventFlags--  PM_NOPWR  PM_PWRCHK   PM_ADCDONE    -- signals from IRQ to powerThread
#define                 PM_NOPWR  1                     // id for osEvent signaling NOPWR interrupt
#define                 PM_PWRCHK 2                     // id for osTimerEvent signaling periodic power check
#define                 PM_ADCDONE  4                   // id for osEvent signaling period power check interrupt
#define                 INITIAL_POWER_CHECK_DELAY 5000  // do initial power check after 5sec

// TODO: It is possible for more than one of these to be true at the same time. Which one(s) win?
enum PwrStat {
    TEMPFAULT = 0, xxx = 1, CHARGING = 2, LOWBATT = 3, CHARGED = 4, xxy = 5, NOLITH = 6, NOUSBPWR = 7
};
static const char *const POWER_STATUS_NAME[]   = { "TempFault", "na-1", "Charging", "LowBatt", "Charged", "na-2",
                                                   "NoLi", "NoUSB" };

// Enum to describe the current state of the battery, based on voltage.
typedef enum BATTERY_CHARGE_LEVEL {
    BL_LOW, BL_MED_LOW, BL_MED_HIGH, BL_HIGH, BL_FULL
}                 BATTERY_CHARGE_LEVEL_T;
// The first value greater than the measured mv level corresponds to BATTERY_CHARGE_LEVEL.
// Note the differing voltages for the replaceable vs rechargeable batteries.
const uint16_t    REPLACEABLE_CUTOFF_LEVELS[]  = { 2200, 2500, 2600, 2900 };
const uint16_t    RECHARGEABLE_CUTOFF_LEVELS[] = { 3630, 3700, 3800, 3900 };
// Gets the charge level from most recent reading of battery voltages.
enum BATTERY_CHARGE_LEVEL latestChargeLevel( void );

// The CSM events corresponding to the battery state that we expose to the CSM.
typedef enum BATTERY_DISPLAY_STATE {
    BDS_CHARGING     = BattCharging,        // Charging, < ~75%
    BDS_CHARGING_75  = BattCharging75,      // Charging, > 75%, < 90%
    BDS_CHARGING_90  = BattMax,             // Charging, > 90% (constant voltage, perhaps?)
    BDS_CHARGED      = BattCharged,         // Charged, no longer charging, but connected to power
    BDS_NOT_CHARGING = BattNotCharging,     // Not charging, not connected to power
    BDS_LOW          = LowBattery,          // Not charging, battern low (< 20% -ish). Charge / replace require soon
    BDS_MINIMUM      = BattMin,             // Not enough power to run.
    BDS_INVALID      = 999                  // Not a valid state.
} BATTERY_DISPLAY_STATE_T;

// TODO: Why would this ever be false?
bool                    PowerChecksEnabled = true;

static osTimerId_t      powerCheckTimer = NULL;
static osEventFlagsId_t pwrEvents       = NULL;
static osThreadAttr_t   pm_thread_attr;

//
//  **************************  ADC: code to read ADC values on gADC_LI_ION & gADC_PRIMARY
static volatile uint32_t adcVoltage;                     // set by ADC ISR
static ADC_TypeDef        *Adc  = (ADC_TypeDef *) ADC1_BASE;
static ADC_Common_TypeDef *AdcC = (ADC_Common_TypeDef *) ADC1_COMMON_BASE;

// ADC channels
const int       ADC_LI_ION_chan  = 2;        // PA2 = ADC1_2   (STM32F412xE/G datasheet pg. 52)
const int       ADC_PRIMARY_chan = 3;        // PA3 = ADC1_3
const int       ADC_VREFINT_chan = 17;       // VREFINT (internal) = ADC1_17 if ADC_CCR.TSVREFE = 1
const int       ADC_VBAT_chan    = 18;       // VBAT = ADC1_18  if ADC_CCR.VBATE = 1
const int       ADC_TEMP_chan    = 18;       // TEMP = ADC1_18  if ADC_CCR.TSVREFE = 1 AND ADC_CCR.VBATE = 0
const int       gADC_THERM_chan  = 12;       // PC2 = ADC_THERM = ADC1_12

typedef struct {
    uint32_t                chkCnt;
    enum PwrStat            Stat;               // stat1 stat2 pgn
    uint32_t                VRefMV;             // should be 1200
    uint32_t                VBatMV;             // RTC Backup CR2032 voltage: 2000..3300
    uint32_t                LiMV;               // liIon voltage: 2000..3900
    uint32_t                PrimaryMV;          // Primary (replaceable) battery: 2000..2800
    uint32_t                MpuTempMV;          // ADC_CHANNEL_TEMPSENSOR in ADC_IN18
    uint32_t                LiThermMV;          // LiIon thermister

    BATTERY_DISPLAY_STATE_T prevBatteryState;   // Battery state exposed via CSM
    enum PwrStat            prvStat;            // previous saved by startPowerCheck()
    bool                    haveUSB;            // USB pwrGood signal
    bool                    hadVBat;            // previous was > BkupPRESENT: saved by startPowerCheck()
    bool                    hadLi;              // previous was > LiPRESENT: saved by startPowerCheck()
    bool                    hadPrimary;         // previous was > ReplPRESENT: saved by startPowerCheck()
    bool                    hadUSB;             // USB power at previous powerCheck
} PwrState;
static PwrState     pS;                     // state of powermanager
extern int          mAudioVolume;           // current audio volume

static void powerThreadProc( void *arg );   // forward
static void initPwrSignals( void );         // forward

static int currentPowerCheckIntervalMS;

bool        RebootOnKeyInt = false;         // tell inputmanager.c to reboot on key interrupt
extern bool BootVerbosePower;
extern bool BootVerboseLog;


/*
 * Forward declaration for a function. Related functions and data:
 * - powerCheckTimerCallback    Function that signals the power check thread to check power. Reschedules itself
 *                              by calling setPowerCheckTimer. Note: may only need to be *re*-scheduled once.
 * - setPowerCheckTimer         Sets the OS timer timeout for powerCheckTimer. Only called from powerCheckTimerCallback.
 * - powerCheckTimer            os timer structure for the power check timer. Created and initialized in initPowerMgr.
 * - currentPowerCheckIntervalMS The steady-state power check interval. From config file or over-ridden by RIGHT-boot.
 * - INITIAL_POWER_CHECK_DELAY  Delay before the first power check. Lets the system settle a bit.
 * - TB_Config->powerCheckMS    Power check interval from the config file. Can be overridden to 60sec with RIGHT-boot.
 *                              Note that the power manager is initialized long before the config file is read.
 */
void powerCheckTimerCallback( void *arg );                          // forward for timer callback function

// ========================================================
//   POWER initialization  -- create osTimer to call powerCheck, start powerThreadProc()
void initPowerMgr( void ) {           // initialize PowerMgr & start powerThreadProc  pwrEvents = osEventFlagsNew(NULL);
    pwrEvents = osEventFlagsNew( NULL );
    if ( pwrEvents == NULL )
        tbErr( "pwrEvents not alloc'd" );

    osTimerAttr_t timer_attr;
    timer_attr.name      = "PM Timer";
    timer_attr.attr_bits = 0;
    timer_attr.cb_mem    = 0;
    timer_attr.cb_size   = 0;
    powerCheckTimer = osTimerNew( powerCheckTimerCallback, osTimerPeriodic, 0, &timer_attr );
    if ( powerCheckTimer == NULL )
        tbErr( "powerCheckTimer not alloc'd" );

    initPwrSignals();   // setup power pins & NO_PWR interrupt

    pm_thread_attr.name       = "PM Thread";
    pm_thread_attr.stack_size = POWER_STACK_SIZE;
    Dbg.thread[1] = (osRtxThread_t *) osThreadNew( powerThreadProc, 0, &pm_thread_attr ); //&pm_thread_attr );
    if ( Dbg.thread[1] == NULL )
        tbErr( "powerThreadProc not created" );

    currentPowerCheckIntervalMS = INITIAL_POWER_CHECK_DELAY;
    osTimerStart( powerCheckTimer, currentPowerCheckIntervalMS );

    dbgLog( "4 PowerMgr OK \n" );
}

void initPwrSignals( void ) {         // configure power GPIO pins, & EXTI on NOPWR
    // power supply signals
    GPIO::configureOutput( gADC_ENABLE );  // 1 to enable battery voltage levels
    GPIO::setLogical( gADC_ENABLE, 0 );
    GPIO::configureOutput( gSC_ENABLE );   // 1 to enable SuperCap
    GPIO::setLogical( gSC_ENABLE, 0 );

    GPIO::configureADC( gADC_LI_ION );  // rechargable battery voltage level
    GPIO::configureADC( gADC_PRIMARY ); // disposable battery voltage level
    GPIO::configureADC( gADC_THERM );   // thermistor (R54) by LiIon battery

    GPIO::configureInput( gPWR_FAIL_N );  // PD0 -- input power fail signal, with pullup

    GPIO::configureInput( gBAT_PG_N );  // MCP73871 PG_    IN:  0 => power is good  (configured active high)
    GPIO::configureInput( gBAT_STAT1 );  // MCP73871 STAT1  IN:  open collector with pullup
    GPIO::configureInput( gBAT_STAT2 );  // MCP73871 STAT2  IN:  open collector with pullup

    GPIO::configureOutput( gBAT_CE );    // OUT: 1 to enable charging      MCP73871 CE     (powermanager.c)
    GPIO::setLogical( gBAT_CE, 1 );       // always enable?
    GPIO::configureOutput( gBAT_TE_N );  // OUT: 0 to enable safety timer  MCP73871 TE_    (powermanager.c)
    GPIO::setLogical( gBAT_TE_N, 0 );     // disable?

    // enable power & signals to EMMC & SDIO devices
    FileSysPower( true );       // power up eMMC & SD 3V supply

    // Configure audio power control
    GPIO::configureOutput( gEN_5V );       // 1 to supply 5V to codec-- enable AP6714 regulator  -- powers AIC3100 SPKVDD
    GPIO::configureOutput( gEN1V8 );       // 1 to supply 1.8 to codec-- enable TLV74118 regulator -- powers AIC3100 DVDD
    GPIO::configureOutput( gBOOT1_PDN );   // 0 to reset codec -- RESET_N on AIC3100 (boot1_pdn on AK4637)
    GPIO::setLogical( gEN_5V, 0 );          // initially codec SPKVDD unpowered PD4
    GPIO::setLogical( gEN1V8, 0 );          // initially codec DVDD unpowered PD5
    GPIO::setLogical( gBOOT1_PDN, 0 );      // initially codec in reset state  PB2

    GPIO::configureOutput( gEN_IOVDD_N );  // 0 to supply 3V to AIC3100 IOVDD
    GPIO::configureOutput( gEN_AVDD_N );   // 0 to supply 3V to AIC3100 AVDD & HPVDD
    GPIO::setLogical( gEN_IOVDD_N, 1 );     // initially codec IOVDD unpowered  PE4
    GPIO::setLogical( gEN_AVDD_N, 1 );      // initially codec AVDD & HPVDD unpowered PE5

    tbDelay_ms( 5 ); // pwr start up: 3 );    // wait 3 msec to make sure everything is stable
}

// ========================================================
//    POWER shutdown routines
void ResetGPIO( void ) {    // turn off all possible GPIO signals & device clocks
    GPIO_ID  actHi[] = { gBOOT1_PDN, gBAT_CE, gSC_ENABLE, gEN_5V, gEN1V8, g3V3_SW_EN, gADC_ENABLE, gGREEN, gRED, gEMMC_RSTN, gINVALID };
    GPIO_ID  actLo[] = { gBAT_TE_N, gEN_IOVDD_N, gEN_AVDD_N, gSPI4_NSS, gINVALID }; // PD13, PE4, PE5, PE11
    for (int i       = 0; actHi[i] != gINVALID; i++) GPIO::setLogical( actHi[i], 0 );    // only works if configured as output (Mode1)
    for (int i       = 0; actLo[i] != gINVALID; i++) GPIO::setLogical( actLo[i], 1 );    // only works if configured as output (Mode1)

    GPIO_ID  extGPIO[] = {  // reconfig all output GPIO's to reset state
            gI2S2ext_SD, gI2S2_SD, gI2S2_WS, gI2S2_CK,     // PB14,  PB15, PB12, PB13
            gI2S3_MCK, gI2C1_SDA, gI2C1_SCL, gI2S2_MCK,    // PC7,   PB9,  PB8,  PC6
            gUSB_DM, gUSB_DP, gUSB_ID, gUSB_VBUS,    // PA11,  PA12, PA10, PA9
            gMCO2, gBOOT1_PDN, gBAT_CE, gSC_ENABLE,   // PC9,   PB2,  PD0,  PD1
            gBAT_TE_N,                                            // PD13
            gEN_5V, gEN1V8, g3V3_SW_EN, gBAT_TE_N,    // PD4,   PD5,  PD6,  PD13
            gGREEN, gRED, gEN_IOVDD_N, gEN_AVDD_N,   // PE0,   PE1,  PE4,  PE5
            gEMMC_RSTN, gSDIO_CLK, gSDIO_CMD, gADC_ENABLE,  // PE10,  PC12, PD2,  PE15
            gSDIO_DAT0, gSDIO_DAT1, gSDIO_DAT2, gSDIO_DAT3,   // PB4,   PA8,  PC10, PB5
            gPWR_FAIL_N, gBAT_STAT1, gBAT_STAT2, gBAT_PG_N,    // PE2,   PD8,  PD9,  PD10
            gBOARD_REV,   // PB0
            gQSPI_BK1_IO0, gQSPI_BK1_IO1, gQSPI_BK1_IO2, gQSPI_BK1_IO3,  // PD11, PD12, PC8, PA1
            gQSPI_CLK_A, gQSPI_CLK_B, gQSPI_BK1_NCS, gQSPI_BK2_NCS,  // PD3,  PB1,  PB6, PC11
            gQSPI_BK2_IO0, gQSPI_BK2_IO1, gQSPI_BK2_IO2, gQSPI_BK2_IO3,  // PA6,  PA7,  PC4, PC5
            //    gSWDIO,       gSWCLK,     gSWO,                       // PA13,  PA14, PB3  -- JTAG DEBUGGER
            gINVALID };
    for (int i         = 0; extGPIO[i] != gINVALID; i++)
        GPIO::configureADC( extGPIO[i] );   // RESET GPIOs to analog (Mode 3, Pup 0)

    GPIO_ID  spiGPIO[] = {  // reconfig SPI GPIO's to input PU
            gSPI4_SCK, gSPI4_MISO, gSPI4_MOSI, gSPI4_NSS,    // PE12,  PE13, PE14, PE11
            gINVALID };
    for (int i         = 0; spiGPIO[i] != gINVALID; i++)
        GPIO::configureInput( spiGPIO[i] );   // set to Input PU (Mode 0, Pup 1)

    RCC->CR &= ~RCC_CR_PLLI2SON_Msk;  // shut off PLLI2S
    FLASH->ACR         = 0;     // disable Data, Instruction, & Prefetch caches
    SYSCFG->CMPCR      = 0;  // power down I/O compensation cell

    // switch to 16MHz HSI as Sysclock source
    int cnt = 0;
    RCC->CR &= RCC_CR_HSION_Msk;    // enable HSI
    while (( RCC->CR & RCC_CR_HSIRDY ) == 0) cnt++;   // wait till ready
    RCC->CFGR &= ~RCC_CFGR_SW_Msk;    // set RCC.CFGR.SW[1:0] = 00: HSI as system clock

    // now can turn off PLL & HSE
    RCC->CR &= ~RCC_CR_PLLON_Msk; // shut off PLL
    RCC->CR &= ~RCC_CR_HSEON_Msk; // shut off HSE

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

/*void                      DbgPowerDown( int code ){       // STOP TBook if DbgPwrDn==code
  extern int DbgPwrDn;      // defined in main.c

  if (code != DbgPwrDn ) return;
  
  flashCode( DbgPwrDn );
  FileSysPower( false );    // shut down eMMC supply, unconfig gSDIO_*
  cdc_PowerDown();          // turn off codec
  ResetGPIO();
  enableSleep();
} */

void enterStopMode( void ) {                    // put STM32F4 into Stop mode
    FileSysPower( false );    // shut down eMMC supply, unconfig gSDIO_*
    cdc_PowerDown();          // turn off codec

    // disable power Fail interrupt
    GPIO::disableEXTI(gPWR_FAIL_N);

    disableKeyInterrupts( static_cast<KEYS_MASK>(KM_HOME|KM_RHAND) );

    ResetGPIO();

    // Enable interrupt on the power good line, so we can turn on the charging indicator.
    GPIO::configureInput(gBAT_PG_N);
    GPIO::enableEXTI( gBAT_PG_N, false );

    extern uint16_t KeypadIMR;        // inputmanager.c -- keyboard IMR flags

    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;  // set DeepSleep
    PWR->CR |= PWR_CR_MRLVDS;   // main regulator in low voltage when Stop
    PWR->CR |= PWR_CR_LPLVDS;   // low-power regulator in low voltage when Stop
    PWR->CR |= PWR_CR_FPDS;     // flash power-down when Stop
    PWR->CR |= PWR_CR_LPDS;     // low-power regulator when Stop

    for (int i     = 0; i < 3; i++) {      // clear all pending ISR
        uint32_t pend = NVIC->ISPR[i];
        if ( pend != 0 )  // clear any pending interrupts
            NVIC->ICPR[i] = pend;
    }
    int      sleep = osKernelSuspend();    // turn off sysTic
    RebootOnKeyInt = true;
    EXTI->IMR |= KeypadIMR;         // enable keypad EXTI interrupts
    EXTI->FTSR     = 0;    // disable key up transitions -- so only key down will wakeup device

    __WFI();  // sleep till interrupt -- 10 keyboard EXTI's enabled

    NVIC_SystemReset();     // soft reboot
}

void powerDownTBook( void ) {                   // TBook orderly shut down -- copy NLog to disk
    logEvt( "PwrDown" );
    logPowerDown();        // flush & close logs, copy NorLog to eMMC
    LedManager::ledBg( NULL );
    LedManager::ledFg( NULL );

    tbDelay_ms( 5 ); // pwr start up: 3 );        // wait 3 msec to make sure everything is stable
    enterStopMode();
}

// ========================================================
//    ADC routines
void startADC( int chan ) {           // set up ADC for single conversion on 'chan', then start
    AdcC->CCR &= ~ADC_CCR_ADCPRE;   // CCR.ADCPRE = 0 => PClk div 2

    // Reset ADC.CR1      //  0==>  Res=12bit, no watchdogs, no discontinuous,
    Adc->CR1 = 0;         //        no auto inject, watchdog, scanning, or interrupts
    // Reset ADC.CR2                //  no external triggers, injections,
    Adc->CR2 = ADC_CR2_ADON;        //  DMA, or continuous conversion -- only power on

    Adc->SQR1 = 0;                  // SQR1.L = 0 => 1 conversion
    Adc->SQR3 = chan;               // chan is 1st (&only) conversion sequence

    Adc->SMPR1 = 0x0EFFFFFF;        // SampleTime = 480 for channels 10..18
    Adc->SMPR2 = 0x3FFFFFFF;        // SampleTime = 480 for channels 0..9

    Adc->SR &= ~( ADC_SR_EOC | ADC_SR_OVR ); // Clear regular group conversion flag and overrun flag
    Adc->CR1 |= ADC_CR1_EOCIE;                  // Enable end of conversion interrupt for regular group
    Adc->CR2 |= ADC_CR2_SWSTART;                // Start ADC software conversion for regular group
}

short readADC( int chan, int enableBit ) { // readADC channel 'chan' value
    if ( enableBit != 0 )
        AdcC->CCR |= enableBit;                   // enable this channel

    startADC( chan );
    // wait for signal from ADC_IRQ on EOC interrupt
    if ( osEventFlagsWait( pwrEvents, PM_ADCDONE, osFlagsWaitAny, 10000 ) != PM_ADCDONE )
        logEvt( "ADC timed out" );

    Adc->CR1 &= ~ADC_CR1_EOCIE;                 // Disable ADC end of conversion interrupt for regular group
    uint32_t adcRaw = Adc->DR;                  // read converted value (also clears SR.EOC)
    if ( enableBit != 0 )
        AdcC->CCR &= ~enableBit;                  // disable channel again
    return adcRaw;
}

/*
 * ISR for ADC end of conversion interrupts.
 *
 * A function with this name will be called when the ADC conversion finishes.
 */
extern "C" {
void ADC_IRQHandler(void) {
    if ((Adc->SR & ADC_SR_EOC) == 0) tbErr("ADC no EOC"); // Interrupt, but not end of conversion???

    osEventFlagsSet(pwrEvents, PM_ADCDONE);   // wakeup powerThread
    Adc->SR = ~(ADC_SR_STRT | ADC_SR_EOC);      // Clear regular group conversion flag
}
}

static void EnableADC( bool enable ) {       // power up & enable ADC interrupts
    if ( enable ) {
        GPIO::setLogical( gADC_ENABLE, 1 );                 // PE15: power up the external analog sampling circuitry
        NVIC_EnableIRQ( ADC_IRQn );             // ADC_IRQn = 18 = 0x12  ADC1,2,3   NVIC->ISER[ 0x12>>5 ] = (1 << (0x12 & 0x1F));  ISER[0]=0x00040000
        RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;     // enable clock for ADC
        Adc->CR2 |= ADC_CR2_ADON;               // power up ADC
        tbDelay_ms( 1 );  // ADC pwr up
    } else {
        GPIO::setLogical( gADC_ENABLE, 0 );
        NVIC_DisableIRQ( ADC_IRQn );
        //BUG FIX 21-Apr-2021 == must reset ADON, *before* turning off ADC1 clock!
        Adc->CR2 &= ~ADC_CR2_ADON;                // power down ADC
        RCC->APB2ENR &= ~RCC_APB2ENR_ADC1EN;      // disable clock for ADC
    }
}

/*
 * Read the voltage levels and internal temperatures (or, rather, the voltages corresponding to the temperatures).
 */
void readVoltagesAndTemperatures( void ) {
    // Turn on
    EnableADC( true );      // enable AnalogEN power & ADC clock & power, & ADC INTQ

    const int MAXCNT = 4096;   // max 12-bit ADC count
    const int VREF   = 3300;   // mV of VREF input pin

    int VRefIntCnt = readADC( ADC_VREFINT_chan, ADC_CCR_TSVREFE );  // enable RefInt on channel 17 -- typically 1.21V (1.18..1.24V)
    // TODO: Why doesn't VREF measure as VREF?
    pS.VRefMV = VRefIntCnt * VREF / MAXCNT;     // should be 1200

    int MpuTempCnt = readADC( ADC_TEMP_chan, ADC_CCR_TSVREFE ); // ADC_CHANNEL_TEMPSENSOR in ADC_IN18
    pS.MpuTempMV = MpuTempCnt * VREF / MAXCNT;

    // RTC backup CR2032
    int VBatCnt = readADC( ADC_VBAT_chan, ADC_CCR_VBATE );    // enable VBAT on channel 18
    // Because VBAT can be higher than VREF, the reading is of VBAT/4.
    pS.VBatMV = VBatCnt * 4 * VREF / MAXCNT;

    int LiThermCnt = readADC( gADC_THERM_chan, 0 );
    pS.LiThermMV = LiThermCnt * VREF / MAXCNT;

    int LiCnt = readADC( ADC_LI_ION_chan, 0 );
    // The Li-Ion is connected to the ADC input via a voltage divider, VLION/2.
    pS.LiMV = LiCnt * 2 * VREF / MAXCNT;

    // Replaceable battery
    int PrimaryCnt = readADC( ADC_PRIMARY_chan, 0 );
    // The replaceable battery is connected to the ADC input via a voltage divider, VREPL/2.
    pS.PrimaryMV = PrimaryCnt * 2 * VREF / MAXCNT;

    EnableADC( false );
}

/*NOT USED: uint16_t                  readPVD( void ){                // read PVD level 
  RCC->APB1ENR |= RCC_APB1ENR_PWREN;            // start clocking power control 
  
  PWR->CR |= ( 0 << PWR_CR_PLS_Pos );           // set initial PVD threshold to 2.2V
  PWR->CR |= PWR_CR_PVDE;                       // enable Power Voltage Detector
  
  int pvdMV = 2100;             // if < 2200, report 2100 
  for (int i=0; i< 8; i++){
    PWR->CR = (PWR->CR & ~PWR_CR_PLS_Msk) | ( i << PWR_CR_PLS_Pos );
    tbDelay_ms(1);  // waiting for Pwr Voltage Detection
    
    if ( (PWR->CSR & PWR_CSR_PVDO) != 0 ) break;
    pvdMV = 2200 + i*100;
  }
  return pvdMV;
} */

// ========================================================
//    RTC setup
uint8_t Bcd2( int v ) {                  // return v as 8 bits-- bcdTens : bcdUnits
    if ( v > 99 ) v = v % 100;
    int vt          = v / 10, vu = v % 10;
    return ( vt << 4 ) | vu;
}

void setupRTC( fsTime time ) {        // init RTC & set based on fsTime
    const int MAX_DLY = 1000000;
    int       cnt     = 0;
    RCC->APB1ENR |= ( RCC_APB1ENR_PWREN | RCC_APB1ENR_RTCAPBEN );       // start clocking power control & RTC_APB
    PWR->CSR |= PWR_CSR_BRE;                     // enable backup power regulator
    while (( PWR->CSR & PWR_CSR_BRR ) == 0 && ( cnt < 10000 )) cnt++;  //  & wait till ready

    RCC->BDCR |= RCC_BDCR_BDRST;          // backup domain reset
    RCC->BDCR &= ~RCC_BDCR_BDRST;         // & release

    RTC->WPR = 0xCA;
    RTC->WPR = 0x53;    // un write-protect the RTC

    PWR->CR |= PWR_CR_DBP;      // enable access to Backup Domain to configure RTC

    RCC->BDCR |= RCC_BDCR_RTCEN;
    RCC->BDCR |= RCC_BDCR_LSEON;                        // enable 32KHz LSE clock
    cnt = 0;
    while (( RCC->BDCR & RCC_BDCR_LSERDY ) == 0 && ( cnt < MAX_DLY )) cnt++;  // & wait till ready
    if ( cnt == MAX_DLY ) tbErr( "RTC: LSE not ready" );

    // configure RTC  (per RM0402 22.3.5)
    RCC->BDCR |= RCC_BDCR_RTCSEL_0;                   // select LSE as RTC source

    RTC->WPR = 0xCA;
    RTC->WPR = 0x53;    // un write-protect the RTC

    RTC->ISR |= RTC_ISR_INIT;                         // set init mode
    cnt = 0;
    while (( RTC->ISR & RTC_ISR_INITF ) == 0 && ( cnt < MAX_DLY )) cnt++;    // & wait till ready
    if ( cnt == MAX_DLY ) tbErr( "RTC: Initmode not ready" );

    RTC->PRER = ( 256 << RTC_PRER_PREDIV_S_Pos );        // Synchronous prescaler = 256  MUST BE FIRST
    RTC->PRER |= ( 127 << RTC_PRER_PREDIV_A_Pos );                // THEN asynchronous = 127 for 32768Hz => 1Hz

    // using AM/PM, since 24hr updated improperly (day didn't switch)
    int32_t ampm = 0, hr = time.hr;
    // 0 -> 12a, 1..11 -> 1a..11a 12->12p 13..23 -> 1..11p
    if ( hr == 12 ) { ampm = 1; }           // 12 -> 12pm
    else if ( hr > 12 ) { ampm = 1; hr -= 12; }  // 13..23 -> 1..11pm
    uint32_t TR = ( ampm << 22 ) | ( Bcd2( hr ) << 16 ) | ( Bcd2( time.min ) << 8 ) | Bcd2( time.sec );

    // calc weekday from date. Adapted from https://artofmemory.com/blog/how-to-calculate-the-day-of-the-week
    // The RTC needs day of week, though we don't care about it.
    int      yr      = time.year, yy = yr % 100, cen = yr / 100, mon = time.mon, day = time.day;
    bool     leap    = ( yr % 400 == 0 ) || ( yr % 4 == 0 && yr % 100 != 0 );
    uint16_t cenCd[] = { 6, 4, 2 };  // for centuries 20xx, 21xx, 22xx
    uint16_t monCd[] = { 0, 3, 3, 6, 1, 4, 6, 2, 5, 0, 3, 5 };
    uint16_t cCd     = cenCd[cen - 20];
    uint16_t yCd     = ( yy / 4 + yy ) % 7;
    uint16_t mCd     = monCd[mon - 1];
    uint8_t  wkday   = ( yCd + mCd + day + cCd - ( leap && mon < 3 ? 1 : 0 )) % 7;
    if ( wkday == 0 ) wkday = 7;

    uint32_t DR = ( Bcd2( yy ) << 16 ) | ( wkday << 13 ) | ( Bcd2( mon ) << 8 ) | Bcd2( day );

    RTC->CR |= RTC_CR_FMT;      // use AM/PM hour format
    RTC->CR |= RTC_CR_BYPSHAD;  // Bypass shadow registers; read calendar directly. Costs 1 extra "APB cycle" per read.
    RTC->TR     = TR;    // set time of day
    RTC->DR     = DR;    // set date
    RTC->ISR &= ~RTC_ISR_INIT;                        // leave RTC init mode

    PWR->CR &= ~PWR_CR_DBP;                                                    // disable access to Backup Domain

    // read clock until change has taken effect
    fsTime   newtime;
    uint32_t msec;
    cnt = 0;
    while (true) {
        getRTC( &newtime, &msec );
        // Is the date updated?
        if ( newtime.day == time.day && newtime.mon == time.mon && newtime.year == time.year ) {
            // And is the time also updated?
            if ( newtime.hr == time.hr && newtime.min >= time.min ) {
                dbgLog( "! setupRTC: took %d reads\n", cnt );
                return;
            }
        }
        cnt++;
        if ( cnt > MAX_DLY ) {
            tbErr( "RTC didn't set: %d-%d-%d %02d:%02d  \n", newtime.year, newtime.mon, newtime.day, newtime.hr,
                   newtime.min );
            break;
        }
    }
}

// ========================================================
//    periodic power checks --  osTimer calls powerCheckTimerCallback(), which signals powerThread() to call powerCheck()
void setPowerCheckTimer( int timerMs ) { // set msec between powerChecks  osTimerStop( powerCheckTimer );
    osTimerStop( powerCheckTimer );       // OS fails to reset timer->load value if running!
    if ( BootVerbosePower ) timerMs = 10000;  // every 10 seconds
    osTimerStart( powerCheckTimer, timerMs );
}

void powerCheckTimerCallback( void *arg ) {   // timer to signal periodic power status check
    if ( PowerChecksEnabled )
        osEventFlagsSet( pwrEvents, PM_PWRCHK );            // wakeup powerThread for power status check
    if ( currentPowerCheckIntervalMS != TB_Config->powerCheckMS ) {    // update delay (after initial check)
        currentPowerCheckIntervalMS = TB_Config->powerCheckMS;
        setPowerCheckTimer( currentPowerCheckIntervalMS );
    }
}

const int LiMIN         = 3400;     // power down threshold
const int LiLOW         = 3500;     // mV at  ~5% capacity  BattLow event
const int LiMAX         = 4000;     // start constant voltage charging
const int LiPRESENT     = 2800;     // max if not connected

const int ReplLOW       = 2300;     // mV at ~15% capacity (for AA Alkaline)
const int ReplMED       = 2500;     // mV at ~45%
const int ReplHI        = 2700;     // mV at ~75%
const int ReplPRESENT   = 1000;
const int BkupPRESENT   = 2000;
const int HiMpuTemp     = 800;
const int HiLiTemp      = 800;

char RngChar( int lo, int hi, int val ) {   // => '-', '0', ... '9', '!'
    if ( val < lo ) return '-';
    if ( val > hi ) return '!';
    int stp = ( hi - lo ) / 10;
    return (( val - lo ) / stp + '0' );
}

void startPowerCheck( enum PwrStat pstat ) {  // remember previous values-- only report if changed  // save previous values
    pS.prvStat    = pS.Stat;
    pS.hadLi      = pS.LiMV > LiPRESENT;
    pS.hadPrimary = pS.PrimaryMV > ReplPRESENT; // Replaceable battery
    pS.hadVBat    = pS.VBatMV > BkupPRESENT; // RTC backup CR2032
    pS.hadUSB     = ( pS.prvStat & 1 ) == 0;

    pS.chkCnt++;
    pS.Stat    = pstat;
    pS.haveUSB = ( pstat & 1 ) == 0;
}

void reportPowerChanges() {    // compare previous power status with current value-- need to report change? bool changed = pS.chkCnt==1;
    if ( pS.haveUSB != pS.hadUSB ) {
        logEvtNS( "PwrChg", "USB", pS.haveUSB ? "connected" : "removed" );
    }
    if ( pS.prvStat != pS.Stat ) {
        dbgLog( "5 pwrStat %d => %d \n", pS.prvStat, pS.Stat );
    }
    if (( pS.LiMV > LiPRESENT ) != pS.hadLi ) {
        dbgLog( "5 LiIon %d \n", pS.LiMV );
        logEvtNS( "PwrChg", "LiIon", pS.LiMV > LiPRESENT ? "connected" : "removed" );
    }
    // RTC backup CR2032
    if (( pS.VBatMV > BkupPRESENT ) != pS.hadVBat ) {
        dbgLog( "5 BkupBat %d \n", pS.VBatMV );
        logEvtNS( "PwrChg", "Bkup", pS.VBatMV > BkupPRESENT ? "connected" : "removed" );
    }
    // Replaceable battery
    if (( pS.PrimaryMV > ReplPRESENT ) != pS.hadPrimary ) {
        dbgLog( "5 Primary %d \n", pS.PrimaryMV );
        logEvtNS( "PwrChg", "Primary", pS.PrimaryMV > ReplPRESENT ? "connected" : "removed" );
    }
}

void cdc_PowerUp( void );   // extern from ti_aic3100 -- for early init on pwr thread

/*
 * isPowerGood: Is the Li-Ion power system good?
 *
 * Return: true if the MCP73871 Li-Ion charge manager reports power good, false otherwise.
 */
bool isPowerGood( void ) {
    bool PwrGood_N = GPIO::getLogical( gBAT_PG_N );   // MCP73871 PG_:  0 => power is good         MCP73871 PG_    (powermanager.c)
    return PwrGood_N == 0;
}

/*
 * getPowerStatus: gets a simple view of power status.
 *
 * Return one of TEMPFAULT=0, CHARGING=2, LOWBATT=3, CHARGED=4, NOLITH=6, or NOUSBPWR=7
 */
enum PwrStat getPowerStatus( void ) {
    //  check MCP73871: gBAT_PG_N, gBAT_STAT1, gBAT_STAT2
    bool PwrGood_N = GPIO::getLogical( gBAT_PG_N );   // MCP73871 PG_:  0 => power is good               MCP73871 PG_    (powermanager.c)
    bool BatStat1  = GPIO::getLogical( gBAT_STAT1 );  // MCP73871 STAT1
    bool BatStat2  = GPIO::getLogical( gBAT_STAT2 );  // MCP73871 STAT2

    // MCP73871 battery charging -- BatStat1==STAT1  BatStat2==STAT2  BatPwrGood==PG
    // Table 5.1  Status outputs
    //   STAT1 STAT2 PG_
    //      H     H    H       7-- no USB input power
    //      H     H    L       6-- no LiIon battery
    //      H     L    L       4-- charge completed
    //      L     H    H       3-- low battery output
    //      L     H    L       2-- charging
    //      L     L    L       0-- temperature (or timer) fault

    enum PwrStat powerStatus = (enum PwrStat) (( BatStat1 ? 4 : 0 ) + ( BatStat2 ? 2 : 0 ) + ( PwrGood_N ? 1 : 0 ));
    return powerStatus;
}

/*
 * isCharging: Is the Li-Ion battery currently charging?
 *
 * Return true if currently charging, false if charged, no USB, or no Li-ION
 */
bool isCharging( void ) {
    return pS.haveUSB && getPowerStatus() == CHARGING;
}

/*
 * checkPower: checks the current power state and logs the state. Generally only makes a log entry if
 * the power state is different from the last time this was called.
 *
 * verbose: If true, always log, and also log additional information.
 */
void checkPower( bool verbose ) {       // check and report power status
    //  check gPWR_FAIL_N & MCP73871: gBAT_PG_N, gBAT_STAT1, gBAT_STAT2
    bool PwrFail = GPIO::getLogical( gPWR_FAIL_N );  // PE2 -- input power fail signal
    if ( PwrFail == 0 )
        powerDownTBook();

    enum PwrStat pstat = getPowerStatus();
    startPowerCheck( pstat );   // records previous values

    readVoltagesAndTemperatures();

    reportPowerChanges();

    char sUsb = isPowerGood() ? 'U' : 'u';
    char sLi  = RngChar( 3000, 4000, pS.LiMV );      // range from charge='0' to charge='9' to '!' > 4000
    char sPr  = RngChar( 2000, 4000, pS.PrimaryMV );   // Replaceable battery, 2000..2200 = '0', 3800..4000 = '9'
    char sBk  = RngChar( 2000, 4000, pS.VBatMV );        // RTC backup CR2032
    char sMt  = RngChar( 200, 1200, pS.MpuTempMV );   // 200..300 = '0'  1000..1200 = '9'
    char sLt  = RngChar( 200, 1200, pS.LiThermMV );
    char sCh  = ' ';
    if ( pstat != CHARGING ) sLt  = ' ';    // not meaningful unless charging
    if ( pstat == CHARGING ) sCh  = 'c';
    if ( pstat == CHARGED ) sCh   = 'C';
    if ( pstat == TEMPFAULT ) sCh = 'X';

    /*=========== Power Status -- Log message interpretation ================================
    PwrCheck, stat:  'u Lxct Pp Bb Tm Vv Liuuuu'
                      |  |||  |  |  |  |   uuuu = current Li-ion battery reading, mV
                      |  |||  |  |  | Vv   v = current audio volume
                      |  |||  |  | Tm      m = -/0/1/2/.../9/! = MPU temp  (m+2)* 100mV, - if <200mV, + if >1200mV
                      |  |||  | Bb         b = -/0/1/2/.../9/! = RTC Backup batt voltage 2+b*.2V, - if <2.0V, + if >3.9V
                      |  ||| Pp            p = -/0/1/2/.../9/! = Replaceable batt voltage 2+p*.2V, - if <2.0V, + if >3.9V
                      |  ||t               t = -/0/1/2/.../9/! = Lithium thermistor  (t+2)* 100mV, - if <200mV, + if >1200mV
                      |  |c                c =  /c/C/X  c=charging, C=charged, X=temp fault
                      | Lx                 x = -/0/1/2/.../9/! = Lithium voltage 3.xV, - if <3.0V, + if >3.9V
                      u                    u = U/u, U if Usb power is present
    ========================================================================================*/
    static char prvStat[30];
    char        pwrStat[30];
    sprintf( pwrStat, "%c L%c%c%c P%c B%c T%c V%d Li%d", sUsb, sLi, sCh, sLt, sPr, sBk, sMt, mAudioVolume, pS.LiMV );

    // A somewhat hacky way to ignore the last two digits of the voltage reading.
    bool changed = strncmp( prvStat, pwrStat, strlen( pwrStat ) - 2 ) != 0;   // log it, if status line changes
    strcpy( prvStat, pwrStat );

    if ( verbose ) {
        /*=========== Power Status -- Log message interpretation ================================
        PwrCheck, Data: 'Ref:r, Stat:s, Li:l, Repl:r, RTC:t, MpuTmp:m, LiTmp:p, Vol:v'
                             |       |     |       |      |         |        |      Volume setting
                             |       |     |       |      |         |        p      Li Thermistor reading
                             |       |     |       |      |         m               MPU temp reading
                             |       |     |       |      t                         RTC backup voltage
                             |       |     |       r                                Replaceable battery voltage
                             |       |     l                                        Li-ion battery voltage
                             |       s                                              Power status
                             r                                                      Reference voltage
        ========================================================================================*/
        char pwrDat[80];
        sprintf( pwrDat, "Ref:%d, Stat:%s, Li:%d, Repl:%d, RTC:%d, MpuTmp:%d, LiTmp:%d, Vol:%d", pS.VRefMV,
                 POWER_STATUS_NAME[pS.Stat], pS.LiMV, pS.PrimaryMV, pS.VBatMV, pS.MpuTempMV, pS.LiThermMV,
                 mAudioVolume );
        logEvtNS( "PwrChk", "Data", pwrDat );
        changed = true;
    }

    dbgLog( "5 srTB: %d %4d %3d %4d\n", pstat, pS.VRefMV, pS.MpuTempMV, pS.VBatMV );
    dbgLog( "5 LtLP: %3d %4d %4d\n", pS.LiThermMV, pS.LiMV, pS.PrimaryMV );
    if ( pS.MpuTempMV > HiMpuTemp ) {
        logEvtNI( "MpuTemp", "mV", pS.MpuTempMV );
        sendEvent( MpuHot, pS.MpuTempMV );
        changed = true;
    }

    // Assume Li-Ion present, not charging, not low.
    enum BATTERY_DISPLAY_STATE batteryDisplay   = BDS_NOT_CHARGING;
    uint32_t                   batteryParameter = pS.LiMV;

    if ( pS.haveUSB ) {    // charger status is only meaningful if we haveUSB power
        switch (pstat) {
            case CHARGED:           // CHARGING complete
                if ( pS.prevBatteryState != BDS_CHARGED ) {
                    logEvtNI( "Charged", "mV", pS.LiMV );
                }
                batteryDisplay = BDS_CHARGED;
                break;
            case CHARGING:          // started charging
                if ( pS.prevBatteryState != BDS_CHARGING_90 && pS.prevBatteryState != BDS_CHARGING_75 &&
                     pS.prevBatteryState != BDS_CHARGING ) {
                    logEvtNI( "Charging", "mV", pS.LiMV );
                }
                if ( pS.LiMV > LiMAX ) {
                    batteryDisplay = BDS_CHARGING_90;
                } else if ( latestChargeLevel() >= BL_HIGH ) {
                    batteryDisplay = BDS_CHARGING_75;
                } else {
                    batteryDisplay = BDS_CHARGING;
                }
                if ( pS.LiThermMV > HiLiTemp ) {   // lithium thermistor is only active while charging
                    logEvtNI( "LiTemp", "mV", pS.LiThermMV );
                    sendEvent( LithiumHot, pS.LiThermMV );
                    changed = true;
                }
                break;
            case TEMPFAULT:         // LiIon charging fault (temp?)
                logEvtNI( "ChrgeFlt", "LiTherm mV", pS.LiThermMV );
                sendEvent( ChargeFault, pS.LiMV );
                changed = true;
                break;
            case LOWBATT:           // LiIon is low  (no USB)
                //  logEvtNI("BattLow", "mV", pS.LiMV );
                //  if (pwrChg) sendEvent( LowBattery,  pS.LiMV );  // only send LOWBATT once
                break;
            default: break;
        }
    } else {  // no USB,  report if batteries are present, but low
        if ( pS.LiMV > LiPRESENT ) {   // have Lith battery, but no USB power
            if ( pS.LiMV < LiMIN ) { // level that forces power down
                logEvt( "LiBattOut" );
                batteryDisplay = BDS_MINIMUM;
                changed        = true;
            } else if ( pS.LiMV < LiLOW ) {
                logEvtNI( "LiBattLow", "mV", pS.LiMV );
                batteryDisplay = BDS_LOW;
                changed        = true;
            }
        }
        if ( pS.PrimaryMV > ReplPRESENT && pS.PrimaryMV < ReplLOW ) {
            logEvtNI( "ReplBattLow", "mV", pS.PrimaryMV );
            // Still working on replaceables, even if Li-Ion is dead.
            batteryDisplay   = BDS_LOW;
            batteryParameter = pS.PrimaryMV;
            changed          = true;
        }
    }
    // If USB mass storage mode is enabled, charge events are not relevant. Presumably we are charging if connected.
    if ( isMassStorageEnabled()) {
        printf( "Mass storage mode\n" );
        pS.prevBatteryState = BDS_INVALID; // We'll want to update the state when done here.
    } else if ( pS.prevBatteryState != batteryDisplay ) {
        printf( "Battery state changed: %s -> %s\n", CSM::eventName((enum CSM_EVENT) pS.prevBatteryState ),
                CSM::eventName((enum CSM_EVENT) batteryDisplay ));
        pS.prevBatteryState = batteryDisplay;
        changed = true;
        // sendEvent((enum CSM_EVENT) batteryDisplay, batteryParameter );
        if (!pS.haveUSB) {
            LedManager::setChargeIndicator(NULL);
        } else if (getPowerStatus() == CHARGING) {
            LedManager::setChargeIndicator("_12r4R4r4!");
        } else {
            LedManager::setChargeIndicator("_12g4G4g4_12!");
        }
    }

    if ( changed ) {       // display status if anything changed
        showRTC();
        logEvtNS( "PwrCheck", "Stat", pwrStat );
    }
}

/*
 * Do we (or did we) have USB power?
 */
bool haveUSBpower() {       // true if USB plugged in (PwrGood_N = 0)
    // TODO: is this right? Can the values in PowerState be stale?
    if ( pS.chkCnt == 0 ) checkPower( true );
    return pS.haveUSB;
}

/*
 * Based on the last time the voltages were read, what was the level of charge,
 * from low, medium-low, medium-high, high, or full.
 */
enum BATTERY_CHARGE_LEVEL latestChargeLevel() {
    const uint16_t *cutoffs = ( pS.LiMV > LiPRESENT ) ? RECHARGEABLE_CUTOFF_LEVELS : REPLACEABLE_CUTOFF_LEVELS;
    uint16_t       current  = ( pS.LiMV > LiPRESENT ) ? pS.LiMV : pS.PrimaryMV;
    // Find the highest cutoff that is greater than the current reading.
    int            i;
    for (i = 0; i < BL_FULL && current >= cutoffs[i]; i++);
    return (BATTERY_CHARGE_LEVEL_T) i;
}

/*
 * Flash the LED 4 times to show approximate charge. More green means more charge.
 */
void showBattCharge() {     // generate ledFG to signal power state
    char fg[30];
    checkPower( true );
    const char *LED = "RRRRGGGG";
    LED += (int) latestChargeLevel();

    sprintf( fg, "_5%c5_5%c5_5%c5_5%c5_15", LED[0], LED[1], LED[2], LED[3] );
    LedManager::ledFg( fg );
    sprintf( fg, "_%c_%c_%c_%c_", LED[0], LED[1], LED[2], LED[3] );
    logEvtS( "BattLevel", fg );
}


// ========================================================
//    powerFail ISR
extern "C" {
void EXTI2_IRQHandler(void) {          // PWR_FAIL_N interrupt-- PE2==0 => power failure, shut down
    if (GPIO::getLogical(gPWR_FAIL_N) == 0) {
        logEvt("PWRFAIL");
        showRTC();
        enterStopMode();
    }
}
}

// ========================================================
//    POWER thread -- handle signals from  powerCheckTimer & powerFail ISR's
static void powerThreadProc( void *arg ) {   // powerThread -- catches PM_NOPWR from EXTI: NOPWR interrupt
    dbgLog( "4 pwrThr: 0x%x 0x%x \n", &arg, &arg + POWER_STACK_SIZE );
    cdc_PowerUp();    // start codec powering up -- this thread won't suffer from the delay

    while (true) {
        int flg = osEventFlagsWait( pwrEvents, PM_NOPWR | PM_PWRCHK, osFlagsWaitAny, osWaitForever );

        switch (flg) {
            case PM_NOPWR:
                powerDownTBook();     // shutdown-- reboot on wakeup keys
                // never come back  -- just restarts at main()
                break;

            case PM_PWRCHK:
                checkPower( BootVerbosePower );     // get current power status
                break;
        }
    }
}
//end  powermanager.c
