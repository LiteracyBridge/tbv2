// JEB  GPIO_STM32F4xx.h  -- built Feb2020 based on GPIO_STM32F10x.h
/* ----------------------------------------------------------------------
 * Copyright (C) 2013 ARM Limited. All rights reserved.
 *
 * $Date:        26. August 2013
 * $Revision:    V1.2
 *
 * Project:      GPIO Driver definitions for ST STM32F10x
 * -------------------------------------------------------------------- */

#ifndef __GPIO_STM32F4XX_H
#define __GPIO_STM32F4XX_H

#include <stdbool.h>
#include "stm32f4xx.h"


#if defined (__CC_ARM)
  #define __FORCE_INLINE  static __forceinline
#else
  #define __FORCE_INLINE  __STATIC_INLINE
#endif
// JEB -- 
//    stm32f412vx.h  defines GPIO_TypeDef configuration registers very different from stm32f10x.h



	
/// GPIO Pin identifier
typedef struct _GPIO_PIN_ID {
  GPIO_TypeDef *port;				//JEB references to config fields of GPIO_TypeDef will be broken
  uint8_t       num;
} GPIO_PIN_ID;

/// Port Mode            // JEB  May2022 -- add _ to avoid name conflicts added in stm32f4xx_hal_gpio.h 2.16.0
typedef enum {
  GPIO__MODE_INPUT     = 0x00,             // GPIO is input
  GPIO__MODE_OUT			  = 0x01,             // General purpose output
  GPIO__MODE_AF        = 0x02,             // Alternate Function
  GPIO__MODE_ANALOG    = 0x03              // Analog
} GPIO__MODE;

/// Port Output Type
typedef enum {
  GPIO_OTYP_PUSHPULL  = 0x0,              // GPIO is push/pull
  GPIO_OTYP_OPENDRAIN	= 0x01							// GPIO is Open Drain
} GPIO_OTYPE;

/// Port Speed
typedef enum {
  GPIO_SPD_LOW     		= 0x00,             // GPIO is low speed
  GPIO_SPD_MED			  = 0x01,             // GPIO is medium speed
  GPIO_SPD_FAST       = 0x02,             // GPIO is fast speed
  GPIO_SPD_HIGH    		= 0x03              // GPIO is high speed
} GPIO_SPEED;

/// Port Pull-up/Pull-down
typedef enum {
  GPIO_PUPD_NONE     	= 0x00,             // GPIO has no pull-up or pull-down
  GPIO_PUPD_PUP			  = 0x01,             // GPIO has pull-up
  GPIO_PUPD_PDN       = 0x02,             // GPIO has pull-down
  GPIO_PUPD_RES    		= 0x03              // reserved
} GPIO_PUPD;

/// Alternate function I/O remap
typedef enum {
	AF0 = 0x0,
	AF1 = 0x1,
	AF2 = 0x2,
 	AF3 = 0x3,
	AF4 = 0x4,
	AF5 = 0x5,
	AF6 = 0x6,
 	AF7 = 0x7,
	AF8 = 0x8,
	AF9 = 0x9,
	AF10 = 0xA,
 	AF11 = 0xB,
	AF12 = 0xC,
	AF13 = 0xD,
	AF14 = 0xE,
 	AF15 = 0xF
} AFIO_REMAP;


/**
  \fn          void GPIO_PortClock (GPIO_TypeDef *GPIOx, bool en)
  \brief       Port Clock Control
  \param[in]   GPIOx  Pointer to GPIO peripheral
  \param[in]   enable Enable or disable clock
*/
extern void GPIO_PortClock (GPIO_TypeDef *GPIOx, bool enable);

/**
  \fn          bool GPIO_GetPortClockState (GPIO_TypeDef *GPIOx)
  \brief       Get GPIO port clock state
  \param[in]   GPIOx  Pointer to GPIO peripheral
  \return      true  - enabled
               false - disabled
*/
extern bool GPIO_GetPortClockState (GPIO_TypeDef *GPIOx);


bool GPIO_PinConfigure(GPIO_TypeDef   *GPIOx,  uint32_t  num,		// configure pin 'num' of GPIOx
                       GPIO__MODE mode,  GPIO_OTYPE otyp,  GPIO_SPEED spd,  GPIO_PUPD pupdn );  // to mode, output type, speed, and pullup/down

/**
  \fn          void GPIO_PinWrite (GPIO_TypeDef *GPIOx, uint32_t num, uint32_t val)
  \brief       Write port pin
  \param[in]   GPIOx  Pointer to GPIO peripheral
  \param[in]   num    Port pin number
  \param[in]   val    Port pin value (0 or 1)
*/
__FORCE_INLINE void GPIO_PinWrite (GPIO_TypeDef *GPIOx, uint32_t num, uint32_t val) {
  if (val & 1) {
    GPIOx->BSRR = (1UL << num);         // set
  } else {
    GPIOx->BSRR = (1UL << (num + 16));  // clr
  }
}

/**
  \fn          uint32_t GPIO_PinRead (GPIO_TypeDef *GPIOx, uint32_t num)
  \brief       Read port pin
  \param[in]   GPIOx  Pointer to GPIO peripheral
  \param[in]   num    Port pin number
  \return      pin value (0 or 1)
*/
__FORCE_INLINE uint32_t GPIO_PinRead (GPIO_TypeDef *GPIOx, uint32_t num) {
  return ((GPIOx->IDR >> num) & 1);
}

/**
  \fn          void GPIO_PortWrite (GPIO_TypeDef *GPIOx, uint16_t mask, uint16_t val)
  \brief       Write port pins
  \param[in]   GPIOx  Pointer to GPIO peripheral
  \param[in]   mask   Selected pins
  \param[in]   val    Pin values
*/
__FORCE_INLINE void GPIO_PortWrite (GPIO_TypeDef *GPIOx, uint16_t mask, uint16_t val) {
  GPIOx->ODR = (GPIOx->ODR & ~mask) | val;
}

/**
  \fn          uint16_t GPIO_PortRead (GPIO_TypeDef *GPIOx)
  \brief       Read port pins
  \param[in]   GPIOx  Pointer to GPIO peripheral
  \return      port pin inputs
*/
__FORCE_INLINE uint16_t GPIO_PortRead (GPIO_TypeDef *GPIOx) {
  return (GPIOx->IDR);
}

/**
  \fn          void GPIO_AFConfigure (AFIO_REMAP af_type)
  \brief       Configure alternate functions
  \param[in]   af_type Alternate function remap type
*/
void GPIO_AFConfigure ( GPIO_TypeDef *GPIOx, uint32_t num, AFIO_REMAP af_type );


#endif /* __GPIO_STM32F4XX_H */
