// JEB  GPIO_STM32F4xx.c  -- built Feb2020 based on GPIO_STM32F10x.c
//  changes in configuration register structure
/* ----------------------------------------------------------------------
 * Copyright (C) 2016 ARM Limited. All rights reserved.
 *
 * $Date:        26. April 2016
 * $Revision:    V1.3
 *
 * Project:      GPIO Driver for ST STM32F10x
 * -------------------------------------------------------------------- */

#include "GPIO_STM32F4xx.h"

/* History:
 *  Version 1.3
 *    Corrected corruption of Serial wire JTAG pins alternate function configuration.
 *  Version 1.2
 *    Added GPIO_GetPortClockState function
 *    GPIO_PinConfigure function enables GPIO peripheral clock, if not already enabled
 *  Version 1.1
 *    GPIO_PortClock and GPIO_PinConfigure functions rewritten
 *    GPIO_STM32F1xx header file cleaned up and simplified
 *    GPIO_AFConfigure function and accompanying definitions added
 *  Version 1.0
 *    Initial release
 */

/* Serial wire JTAG pins alternate function Configuration
 * Can be user defined by C preprocessor:
 *   AFIO_SWJ_FULL, AFIO_SWJ_FULL_NO_NJTRST, AFIO_SWJ_JTAG_NO_SW, AFIO_SWJ_NO_JTAG_NO_SW
 */
#ifndef AFIO_MAPR_SWJ_CFG_VAL
#define AFIO_MAPR_SWJ_CFG_VAL   (AFIO_SWJ_FULL)    // Full SWJ (JTAG-DP + SW-DP)
#endif

/**
  \fn          void GPIO_PortClock (GPIO_TypeDef *GPIOx, bool en)
  \brief       Port Clock Control
  \param[in]   GPIOx  Pointer to GPIO peripheral
  \param[in]   enable Enable or disable clock
*/
void GPIO_PortClock( GPIO_TypeDef *GPIOx, bool enable ) {

    if ( enable ) {
        if ( GPIOx == GPIOA ) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
        else if ( GPIOx == GPIOB ) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
        else if ( GPIOx == GPIOC ) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
        else if ( GPIOx == GPIOD ) RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
        else if ( GPIOx == GPIOE ) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN;
#if defined(GPIOF)
        else if (GPIOx == GPIOF) RCC->AHB1ENR |=  RCC_AHB1ENR_GPIOFEN;
        else if (GPIOx == GPIOG) RCC->AHB1ENR |=  RCC_AHB1ENR_GPIOGEN;
#endif
    } else {
        if ( GPIOx == GPIOA ) RCC->AHB1ENR &= ~RCC_AHB1ENR_GPIOAEN;
        else if ( GPIOx == GPIOB ) RCC->AHB1ENR &= ~RCC_AHB1ENR_GPIOBEN;
        else if ( GPIOx == GPIOC ) RCC->AHB1ENR &= ~RCC_AHB1ENR_GPIOCEN;
        else if ( GPIOx == GPIOD ) RCC->AHB1ENR &= ~RCC_AHB1ENR_GPIODEN;
        else if ( GPIOx == GPIOE ) RCC->AHB1ENR &= ~RCC_AHB1ENR_GPIOEEN;
#if defined(GPIOF)
        else if (GPIOx == GPIOF) RCC->AHB1ENR &= ~RCC_AHB1ENR_GPIOFEN;
        else if (GPIOx == GPIOG) RCC->AHB1ENR &= ~RCC_AHB1ENR_GPIOGEN;
#endif
    }
}

/**
  \fn          bool GPIO_GetPortClockState (GPIO_TypeDef *GPIOx)
  \brief       Get GPIO port clock state
  \param[in]   GPIOx  Pointer to GPIO peripheral
  \return      true  - enabled
               false - disabled
*/
bool GPIO_GetPortClockState( GPIO_TypeDef *GPIOx ) {
    if ( GPIOx == GPIOA ) { return (( RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN ) != 0U ); }
    else if ( GPIOx == GPIOB ) { return (( RCC->AHB1ENR & RCC_AHB1ENR_GPIOBEN ) != 0U ); }
    else if ( GPIOx == GPIOC ) { return (( RCC->AHB1ENR & RCC_AHB1ENR_GPIOCEN ) != 0U ); }
    else if ( GPIOx == GPIOD ) { return (( RCC->AHB1ENR & RCC_AHB1ENR_GPIODEN ) != 0U ); }
    else if ( GPIOx == GPIOE ) { return (( RCC->AHB1ENR & RCC_AHB1ENR_GPIOEEN ) != 0U ); }
#if defined(GPIOF)
    else if (GPIOx == GPIOF) { return ((RCC->AHB1ENR & RCC_AHB1ENR_GPIOFEN) != 0U); }
    else if (GPIOx == GPIOG) { return ((RCC->AHB1ENR & RCC_AHB1ENR_GPIOGEN) != 0U); }
#endif
    return false;
}


/**
  \fn          bool GPIO_PinConfigure (GPIO_TypeDef      *GPIOx,
                                       uint32_t           num,
                                       GPIO_CONF          conf,
                                       GPIO_MODE          mode)
  \brief       Configure port pin
  \param[in]   GPIOx         Pointer to GPIO peripheral
  \param[in]   num           Port pin number
  \param[in]   mode          \ref GPIO_MODE
  \param[in]   conf          \ref GPIO_CONF
  \return      true  - success
               false - error
*/
bool GPIO_PinConfigure( GPIO_TypeDef *GPIOx,
                        uint32_t num,                                   // configure pin 'num' of GPIOx
                        GPIO__MODE mode, GPIO_OTYPE otyp, GPIO_SPEED spd,
                        GPIO_PUPD pupdn ) {  // to mode, output type, speed, and pullup/down
    if ( num > 15 ) return false;

    if ( GPIO_GetPortClockState( GPIOx ) == false )
        GPIO_PortClock( GPIOx, true );   // Enable GPIOx peripheral clock

    int pos = num * 2, msk = 0x3 << pos, val = mode << pos;
    GPIOx->MODER = ( GPIOx->MODER & ~msk ) | val;     // set 2-bit field 'num'

    int bit = 1 << num;
    if ( otyp == 0 )
        GPIOx->OTYPER &= ~bit;    // clear bit 'num'
    else
        GPIOx->OTYPER |= bit;   // set bit 'num'

    val = spd << pos;
    GPIOx->OSPEEDR = ( GPIOx->OSPEEDR & ~msk ) | val;     // set 2-bit field 'num'

    val = pupdn << pos;
    GPIOx->PUPDR = ( GPIOx->PUPDR & ~msk ) | val;     // set 2-bit field 'num'

    return true;
}


/**
  \fn          void GPIO_AFConfigure (AFIO_REMAP af_type)
  \brief       Configure alternate functions
  \param[in]   af_type Alternate function remap type
*/
void GPIO_AFConfigure( GPIO_TypeDef *GPIOx, uint32_t num, AFIO_REMAP af_type ) {    // set Alternate Function of GPIOx pin 'num' to  af_type
    int reg = num / 8;
    int idx = num & 0x7;
    int pos = idx * 4;   // bit position in register
    int msk = 0xF << pos;
    int val = af_type << pos;

    GPIOx->AFR[reg] = ( GPIOx->AFR[reg] & ~msk ) | val;
}
