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
#include "tbook.h"

#include "gpio.h"

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

GPIO_Def_t GPIO::gpio_def[NUM_GPIO_DEFINITIONS];

void GPIO::setPortClockEnabledState(GPIO_TypeDef *GPIOx, bool enabled) {
    if ( enabled ) {
        if ( GPIOx == GPIOA ) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
        else if ( GPIOx == GPIOB ) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
        else if ( GPIOx == GPIOC ) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
        else if ( GPIOx == GPIOD ) RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
        else if ( GPIOx == GPIOE ) RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN;
//#if defined(GPIOF)
//        else if (GPIOx == GPIOF) RCC->AHB1ENR |=  RCC_AHB1ENR_GPIOFEN;
//        else if (GPIOx == GPIOG) RCC->AHB1ENR |=  RCC_AHB1ENR_GPIOGEN;
//#endif
    } else {
        if ( GPIOx == GPIOA ) RCC->AHB1ENR &= ~RCC_AHB1ENR_GPIOAEN;
        else if ( GPIOx == GPIOB ) RCC->AHB1ENR &= ~RCC_AHB1ENR_GPIOBEN;
        else if ( GPIOx == GPIOC ) RCC->AHB1ENR &= ~RCC_AHB1ENR_GPIOCEN;
        else if ( GPIOx == GPIOD ) RCC->AHB1ENR &= ~RCC_AHB1ENR_GPIODEN;
        else if ( GPIOx == GPIOE ) RCC->AHB1ENR &= ~RCC_AHB1ENR_GPIOEEN;
//#if defined(GPIOF)
//        else if (GPIOx == GPIOF) RCC->AHB1ENR &= ~RCC_AHB1ENR_GPIOFEN;
//        else if (GPIOx == GPIOG) RCC->AHB1ENR &= ~RCC_AHB1ENR_GPIOGEN;
//#endif
    }}
bool GPIO::getPortClockEnabledState(GPIO_TypeDef *GPIOx) {
    if ( GPIOx == GPIOA ) { return (( RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN ) != 0U ); }
    else if ( GPIOx == GPIOB ) { return (( RCC->AHB1ENR & RCC_AHB1ENR_GPIOBEN ) != 0U ); }
    else if ( GPIOx == GPIOC ) { return (( RCC->AHB1ENR & RCC_AHB1ENR_GPIOCEN ) != 0U ); }
    else if ( GPIOx == GPIOD ) { return (( RCC->AHB1ENR & RCC_AHB1ENR_GPIODEN ) != 0U ); }
    else if ( GPIOx == GPIOE ) { return (( RCC->AHB1ENR & RCC_AHB1ENR_GPIOEEN ) != 0U ); }
//#if defined(GPIOF)
//    else if (GPIOx == GPIOF) { return ((RCC->AHB1ENR & RCC_AHB1ENR_GPIOFEN) != 0U); }
//    else if (GPIOx == GPIOG) { return ((RCC->AHB1ENR & RCC_AHB1ENR_GPIOGEN) != 0U); }
//#endif
    return false;
}



bool GPIO::configurePin(GPIO_TypeDef *GPIOx, uint32_t bit, GPIO_PORT_MODE mode, GPIO_OUTPUT_TYPE ot, GPIO_PORT_SPEED speed, GPIO_PULLUP_PULLDOWN pupd) {
    //return GPIO_PinConfigure(GPIOx, bit, static_cast<GPIO__MODE>(mode), static_cast<GPIO_OTYPE>(ot), static_cast<GPIO_SPEED>(speed), static_cast<GPIO_PUPD>(pupd));
    if ( bit > 15 ) return false;

    if ( !GPIO::getPortClockEnabledState( GPIOx ) )
        GPIO::setPortClockEnabledState( GPIOx, true );   // Enable GPIOx peripheral clock

    int pos = bit * 2;
    int msk = 0x3 << pos;
    int val = mode << pos;
    GPIOx->MODER = ( GPIOx->MODER & ~msk ) | val;     // set 2-bit field 'bit'

    int output_type_bit = 1 << bit;
    if ( ot == GPIO::OT_PUSH_PULL )
        GPIOx->OTYPER &= ~output_type_bit;    // clear bit 'bit' for push-pull
    else
        GPIOx->OTYPER |= output_type_bit;   // set bit 'bit' for open drain

    val = speed << pos;
    GPIOx->OSPEEDR = ( GPIOx->OSPEEDR & ~msk ) | val;     // set 2-bit field 'num'

    val = pupd << pos;
    GPIOx->PUPDR = ( GPIOx->PUPDR & ~msk ) | val;     // set 2-bit field 'num'

    return true;
}

void GPIO::configureAF(GPIO_TypeDef *GPIOx, uint32_t bit, GPIO::GPIO_AF af_type) {
    //GPIO_AFConfigure(GPIOx, bit, af_type);
    uint32_t reg = bit / 8;
    uint32_t idx = bit & 0x7;
    uint32_t position = idx * 4;   // bit position in register
    uint32_t mask = 0xF << position;
    uint32_t value = af_type << position;

    GPIOx->AFR[reg] = ( GPIOx->AFR[reg] & ~mask ) | value;
}

void GPIO::unConfigure(GPIO_ID id) {
    GPIO::configurePin( GPIO::gpio_def[ id ].port, GPIO::gpio_def[ id ].bit, GPIO::PORT_MODE_INPUT, GPIO::OT_PUSH_PULL, GPIO::SPEED_LOW, GPIO::PUPD_NONE );
    GPIO::configureAF ( GPIO::gpio_def[ id ].port, GPIO::gpio_def[ id ].bit, GPIO::AF_NONE );   // reset to AF0
}

void GPIO::configureI2S(GPIO_ID id) {
    GPIO::GPIO_AF af = static_cast<GPIO::GPIO_AF>(GPIO::gpio_def[ id ].altFn);
    GPIO::configurePin( GPIO::gpio_def[ id ].port, GPIO::gpio_def[ id ].bit, GPIO::PORT_MODE_AF, GPIO::OT_PUSH_PULL, GPIO::SPEED_FAST, GPIO::PUPD_NONE );
    GPIO::configureAF ( GPIO::gpio_def[ id ].port, GPIO::gpio_def[ id ].bit, af );   // set AF
}

void GPIO::configureOutput(GPIO_ID id) {
    GPIO::GPIO_AF af = static_cast<GPIO::GPIO_AF>(GPIO::gpio_def[ id ].altFn);
    GPIO::GPIO_PORT_MODE md = af!=0? GPIO::PORT_MODE_AF : GPIO::PORT_MODE_OUTPUT;
    GPIO::configurePin( GPIO::gpio_def[ id ].port, GPIO::gpio_def[ id ].bit, md, GPIO::OT_PUSH_PULL, GPIO::SPEED_LOW, GPIO::PUPD_NONE );
    GPIO::configureAF ( GPIO::gpio_def[ id ].port, GPIO::gpio_def[ id ].bit, af );   // set AF
}

void GPIO::configureInput(GPIO_ID id) {
    GPIO::configurePin( GPIO::gpio_def[ id ].port, GPIO::gpio_def[ id ].bit, GPIO::PORT_MODE_INPUT, GPIO::OT_PUSH_PULL, GPIO::SPEED_LOW, GPIO::PUPD_PUP );
    GPIO::configureAF ( GPIO::gpio_def[ id ].port, GPIO::gpio_def[ id ].bit, GPIO::AF_NONE );
}

void GPIO::configureKey(GPIO_ID id) {
    GPIO::configurePin( GPIO::gpio_def[ id ].port, GPIO::gpio_def[ id ].bit, GPIO::PORT_MODE_INPUT, GPIO::OT_PUSH_PULL, GPIO::SPEED_LOW, GPIO::PUPD_PDOWN);
    GPIO::configureAF ( GPIO::gpio_def[ id ].port, GPIO::gpio_def[ id ].bit, GPIO::AF_NONE );
    GPIO::enableEXTI( id, true );
}

void GPIO::configureADC(GPIO_ID id) {
    GPIO::GPIO_AF af = static_cast<GPIO::GPIO_AF>(GPIO::gpio_def[ id ].altFn);
    GPIO::configurePin( GPIO::gpio_def[ id ].port, GPIO::gpio_def[ id ].bit, GPIO::PORT_MODE_INPUT, GPIO::OT_PUSH_PULL, GPIO::SPEED_LOW, GPIO::PUPD_NONE );
    GPIO::configureAF ( GPIO::gpio_def[ id ].port, GPIO::gpio_def[ id ].bit, af );   //  set AF
}


void GPIO::enableEXTI(GPIO_ID id, bool asKey) {
    extern uint16_t KeypadIMR;    // from inputmanager.c

    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN_Msk;     // make sure enabled SysCfg for SYSCFG->EXTICR[]

    // AFIO->EXTICR[0..3] -- set of 4bit fields for bit#0..15
    int port     = (int) GPIO::gpio_def[id].port;  // GPIOA .. GPIOE
    int portCode = ( port - (int) GPIOA ) >> 10;   // GPIOA=0, .. GPIOH=7
    int bit      = GPIO::gpio_def[id].bit;
    int iWd      = bit >> 2, iPos = ( bit & 0x3 ), fbit = iPos << 2;
    int msk      = ( 0xF << fbit ), val = ( portCode << fbit );
    SYSCFG->EXTICR[iWd] = ( SYSCFG->EXTICR[iWd] & ~msk ) | val;   // replace bits <fbit..fbit+3> with portCode

    int pinBit = 1 << bit;      // bit mask for EXTI->IMR, RTSR, FTSR
    if ( asKey ) {  // keys enable both rise & fall
        EXTI->RTSR |= pinBit;       // Enable a rising trigger
        EXTI->FTSR |= pinBit;       // Enable a falling trigger
        //DONT ENABLE--   EXTI->IMR  |= pinBit;       // Configure the interrupt mask
        KeypadIMR |= pinBit;
    } else {  // PWR_FAIL_N --
        EXTI->RTSR |= pinBit;       // Enable a rising trigger
        EXTI->FTSR |= pinBit;       // PWR_FAIL_N -- enable falling trigger only
        EXTI->IMR |= pinBit;       // enable the interrupt
        KeypadIMR |= pinBit;
    }

    NVIC_ClearPendingIRQ( GPIO::gpio_def[id].intq );
    NVIC_EnableIRQ( GPIO::gpio_def[id].intq );   // enable interrupt on key
}

void GPIO::disableEXTI(GPIO_ID id) {
    int bit = gpio_def[id].bit;
    EXTI->IMR &= ~( 1 << bit );
}

void GPIO::defineGPIO( GPIO_ID gpid, const char * name, GPIO_TypeDef *port, uint16_t bit, uint32_t flags ) {
    IRQn_Type intnum = WWDG_IRQn;  // default 0
    uint8_t   altFn  = flags & 0xf;
    int       active = 1;   // default to active high-- 1 when LOGICALLY active

    switch (bit) {
        // @formatter:off
        case 0:   intnum = EXTI0_IRQn;  break;
        case 1:   intnum = EXTI1_IRQn;  break;
        case 2:   intnum = EXTI2_IRQn;  break;
        case 3:   intnum = EXTI3_IRQn;  break;
        case 4:   intnum = EXTI4_IRQn;  break;
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:   intnum = EXTI9_5_IRQn; break;
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:  intnum = EXTI15_10_IRQn; break;
            // @formatter:on
    }
    // TODO: test flag for "ACTIVE_LOW"
//        if ( strchr( signal, '_' ) != NULL )  // contains a _ => active low
//            active = 0; // '_' for active low input

    GPIO::gpio_def[gpid].port   = port;   // mem-mapped address of GPIO port
    GPIO::gpio_def[gpid].bit    = bit;    // bit within port
    GPIO::gpio_def[gpid].altFn  = static_cast<GPIO::GPIO_AF>(altFn); // alternate function index 0..15
    GPIO::gpio_def[gpid].intq   = intnum; // EXTI int num
    GPIO::gpio_def[gpid].active = active; // signal when active (for keys, input state when pressed)
    GPIO::gpio_def[gpid].signal = name;
}

void GPIO::defineGPIOs(void) {
#define GDEF_NONE 0
#define X(NAME, GPIOX, BIT, flags) \
    GPIO::defineGPIO(g##NAME, #NAME, GPIOX, BIT, flags);
    GPIO_DEFINITION_LIST
#undef X

}



