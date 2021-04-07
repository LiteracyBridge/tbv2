// **************************************************************************************
//  TBookV2.h
//	Feb2020 Rev2B board
// ********************************* 

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef TBOOKV2_H
#define TBOOKV2_H

#include "main.h"

#include "stm32f412vx.h"
//#define UID_BASE                     0x1FFF7A10U           /*STM32F412 !< Unique device ID register base address */
//	#include "I2C_STM32F4xx.h"			
#include "stm32f4xx.h"
#include "GPIO_STM32F4xx.h"

// select audio codec device being used-- V2_Rev1: AK4637  V2_Rev3: AIC3100
//#define AK4637
#define AIC3100

#ifdef AK4637
// I2C slave address for ak4637	--  shifted <<1 by I2C, so differs from HAL 0x27 which is masked with 0xFFFE
#define AUDIO_I2C_ADDR                    0x12
#endif

#ifdef AIC3100
// I2C slave address for tlv320aic3100 0x18 specified by AIC3100 datasheet pg 20 (matches start byte of 0x30 in eval bd initscript)
#define AUDIO_I2C_ADDR                     0x18
#endif 

// #define USE_LCD
// define NO_PRINTF if user printf (retargetio.c) not included
// #define NO_PRINTF

// define USE_FILESYS if filesystem is included
#define USE_FILESYS

#define KEY_PRESSED  0

#include "gpio_signals.h"		// GPIO_IDs

// Alternate Functions based on STM32F412xE/G datasheet: Table 11 pg.67

extern const GPIO_Signal gpioSignals[]; 	// GPIO signal definitions -- defined by main.c

#endif
// end tbook_rev2.h
