

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#include <stdint.h>
#include <stdbool.h>

#define     __IO    volatile             /*!< Defines 'read / write' permissions */

// **************************************************************************************
//      select board pins & options

#include "stm32f412vx.h"
#include "stm32f4xx.h"
#include "GPIO_STM32F4xx.h"           //JEB May2022 enum GPIO_MODE_INPUT etc now conflict with defs in stm32f4xx_hal_gpio.h

// select audio codec device being used-- V2_Rev1: AK4637  V2_Rev3: AIC3100
#define AIC3100

#ifdef AIC3100
// I2C slave address for tlv320aic3100 0x18 specified by AIC3100 datasheet pg 20 (matches start byte of 0x30 in eval bd initscript)
#define AUDIO_I2C_ADDR                     0x18
#endif

// define USE_FILESYS if filesystem is included
//#define USE_FILESYS

#endif /* __MAIN_H */

