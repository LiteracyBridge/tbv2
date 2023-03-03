//*************************
//     gpio_signals.h
//

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef GPIO_SIGNALS_H
#define GPIO_SIGNALS_H

// GPIO Signal IDs
//     TalkingBook keypad has 10 keys, each connected to a line of the keypad cable-- these have to be 0..9 to match enum KEY
//    10 is reserved (for an invalid KEY)
//    other IDs are assigned for GPIO pins used in different implementations
typedef enum {      // GPIO_ID -- gpio signal IDs  
    gORANGE, gBLUE,     // LEDs on STM3210E_EVAL & STM32F412_DISCOG
    gRED,               // TB_V2_Rev1:  PE1
    gGREEN,             // TB_V2_Rev1:  PE0    // LEDs on TBook keypad & STM3210E_EVAL & STM32F412_DISCOG
    gINVALID,           // marker for end of definition array
    gHOME,              // TB_V2_Rev1:   PA0
    gCIRCLE,            // TB_V2_Rev1:   PE9
    gPLUS,              // TB_V2_Rev1:   PA4
    gMINUS,             // TB_V2_Rev1:   PA5
    gTREE,              // TB_V2_Rev1:   PA15
    gLHAND,             // TB_V2_Rev1:   PB7
    gRHAND,             // TB_V2_Rev1:   PB10
    gPOT,               // TB_V2_Rev1:   PC1
    gSTAR,              // TB_V2_Rev1:   PE8
    gTABLE,             // TB_V2_Rev1:   PE3
// defs for all the other TB_V2 gpio assignments
    gBOARD_REV,         // TB_V2_Rev1:   N/A      TB_V2_Rev3:  PB0
    gADC_LI_ION,        // TB_V2_Rev1:   PA2
    gADC_PRIMARY,       // TB_V2_Rev1:   PA3
    gPWR_FAIL_N,        // TB_V2_Rev1:   PD0      TB_V2_Rev3:  PE2
    gSC_ENABLE,         // TB_V2_Rev1:   PD1
    gPA_EN,             // TB_V2_Rev1:   PD3
    gEN_5V,             // TB_V2_Rev1:   PD4
    gEN1V8,             // TB_V2_Rev1:   PD5
    g3V3_SW_EN,         // TB_V2_Rev1:   PD6
    gEN_IOVDD_N,        // TB_V2_Rev1:   N/A      TB_V2_Rev3:  PE4
    gEN_AVDD_N,         // TB_V2_Rev1:   N/A      TB_V2_Rev3:  PE5
    gEMMC_RSTN,         // TB_V2_Rev1:   PE10
    gSPI4_NSS,          // TB_V2_Rev1:   PE11
    gSPI4_SCK,          // TB_V2_Rev1:   PE12
    gSPI4_MISO,         // TB_V2_Rev1:   PE13
    gSPI4_MOSI,         // TB_V2_Rev1:   PE14
    gADC_ENABLE,        // TB_V2_Rev1:   PE15
    gEN_1V8,            // TB_V2_Rev1:   PD5
    gUSB_DM,            // TB_V2_Rev1:   PA11
    gUSB_DP,            // TB_V2_Rev1:   PA12
    gUSB_ID,            // TB_V2_Rev1:   PA10
    gUSB_VBUS,          // TB_V2_Rev1:   N/A      TB_V2_Rev3:  PA9
    gI2S2ext_SD,        // TB_V2_Rev1:   PB14
    gI2S2_SD,           // TB_V2_Rev1:   PB15
    gI2S2_WS,           // TB_V2_Rev1:   PB12
    gI2S2_CK,           // TB_V2_Rev1:   PB13
    gI2S2_MCK,          // TB_V2_Rev1:   PC6
    gI2S3_MCK,          // TB_V2_Rev1:   PC7
    gI2C1_SDA,          // TB_V2_Rev1:   PB9    STM3210E_EVAL: PB6
    gI2C1_SCL,          // TB_V2_Rev1:   PB8    STM3210E_EVAL: PB7
    gQSPI_CLK,          // TB_V2_Rev1:   PB1    => U4 & U5
    gQSPI_CLK_A,        //                     TB_V2_Rev3:   PD3 => U5
    gQSPI_CLK_B,        //                     TB_V2_Rev3:   PB1 => U4
    gQSPI_BK1_IO0,      // TB_V2_Rev1:   PC9    TB_V2_Rev3:   PD11
    gMCO2,              // TB_V2_Rev1:   PC9    TB_V2_Rev3:   PC9
    gQSPI_BK1_IO1,      // TB_V2_Rev1:   PC10  TB_V2_Rev3:   PD12
    gQSPI_BK1_IO2,      // TB_V2_Rev1:   PC8
    gQSPI_BK1_IO3,      // TB_V2_Rev1:   PA1
    gQSPI_BK1_NCS,      // TB_V2_Rev1:   PB6
    gQSPI_BK2_IO0,      // TB_V2_Rev1:   PA6
    gQSPI_BK2_IO1,      // TB_V2_Rev1:   PA7
    gQSPI_BK2_IO2,      // TB_V2_Rev1:   PC4
    gQSPI_BK2_IO3,      // TB_V2_Rev1:   PC5
    gQSPI_BK2_NCS,      // TB_V2_Rev1:   PC11
    gSDIO_DAT0,         // TB_V2_Rev1:   PB4
    gSDIO_DAT1,         // TB_V2_Rev1:   PA8
    gSDIO_DAT2,         // TB_V2_Rev1:   PA9    TB_V2_Rev3:  PC10
    gSDIO_DAT3,         // TB_V2_Rev1:   PB5
    gSDIO_CLK,          // TB_V2_Rev1:   PC12
    gSDIO_CMD,          // TB_V2_Rev1:   PD2
    gSWDIO,             // TB_V2_Rev1:   PA13
    gSWCLK,             // TB_V2_Rev1:   PA14
    gSWO,               // TB_V2_Rev1:   PB3
    gNRST,              // TB_V2_Rev1:   NRST
    gBOOT0,             // TB_V2_Rev1:   BOOT0
    gBOOT1_PDN,         // TB_V2_Rev1:   PB2    codec PDN   STM3210E_EVAL: PG11
    gOSC32_OUT,         // TB_V2_Rev1:   PC15
    gOSC32_IN,          // TB_V2_Rev1:   PC14
    gOSC_OUT,           // TB_V2_Rev1:   PH1
    gOSC_IN,            // TB_V2_Rev1:   PH0
    gBAT_STAT1,         // TB_V2_Rev1:   PD8
    gBAT_STAT2,         // TB_V2_Rev1:   PD9
    gBAT_PG_N,          // TB_V2_Rev1:   PD10
    gBAT_CE,            // TB_V2_Rev1:   PD11  TB_V2_Rev3:    PD0
    gBAT_TE_N,          // TB_V2_Rev1:   PD12  TB_V2_Rev3:    PD13
    gADC_THERM,         // TB_V2_Rev1:   PC2

} GPIO_ID;

typedef struct {  // GPIO_Signal -- define GPIO port/pins/interrupt #, signal name, & st when pressed for a GPIO_ID
    GPIO_ID id;      // GPIO_ID connected to this line
    const char *name;    // "PC7_" => { GPIOC, 7, EXTI15_10_IRQn, "PC7_", 0 }
} GPIO_Signal;

#endif

