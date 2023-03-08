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
#include "gpio.h"

typedef struct { // GPIO_Def_t -- define GPIO port/pins/interrupt # for a GPIO_ID
    GPIO_TypeDef *port;   // address of GPIO port -- from STM32Fxxxx.h
    uint16_t bit;    // GPIO bit within port
    uint16_t altFn;  // alternate function value
    IRQn_Type intq;   // specifies INTQ num  -- from STM32Fxxxx.h
    const char *signal; // string name of signal, e.g. PA0
    int active; // gpio value when active: 'PA3_' => 0, 'PC12' => 1
} GPIO_Def_t;

class GPIO {
public: // TODO: make this private
    static GPIO_Def_t gpio_def[NUM_GPIO_DEFINITIONS];

public:
    static void defineGPIOs(void);

    static bool getLogical(GPIO_ID id) {
        GPIO_TypeDef *port = gpio_def[id].port;
        if ( port == NULL ) return false;   // TODO: is this necessary?
        return GPIO::readPin( port, gpio_def[id].bit ) == gpio_def[id].active;
    }
    static void setLogical(GPIO_ID id, bool on) {
        uint8_t activeLevel = gpio_def[id].active;
        uint8_t newLevel = on ? activeLevel : 1 - activeLevel;    // on => pressed else 1-pressed
        GPIO::writePin( gpio_def[id].port, gpio_def[id].bit, newLevel );
    }

public:
    enum GPIO_PORT_MODE {PORT_MODE_INPUT, PORT_MODE_OUTPUT, PORT_MODE_AF, PORT_MODE_ANALOG};
    enum GPIO_OUTPUT_TYPE {OT_PUSH_PULL, OT_OPEN_DRAIN};
    enum GPIO_PORT_SPEED {SPEED_LOW, SPEED_MEDIUM, SPEED_FAST, SPEED_HIGH};
    enum GPIO_PULLUP_PULLDOWN {PUPD_NONE, PUPD_PUP, PUPD_PDOWN};
    enum GPIO_AF {AF_NONE=0, AF0=0, AF1, AF2, AF3, AF4, AF5, AF6, AF7, AF8, AF9, AF10, AF11, AF12, AF13, AF14, AF15};

    static void setPortClockEnabledState(GPIO_TypeDef *GPIOx, bool enabled);
    static bool getPortClockEnabledState(GPIO_TypeDef *GPIOx);
    static bool configurePin(GPIO_TypeDef *GPIOx, uint32_t bit, GPIO_PORT_MODE mode, GPIO_OUTPUT_TYPE ot, GPIO_PORT_SPEED speed, GPIO_PULLUP_PULLDOWN pupd);
    static void configureAF(GPIO_TypeDef *GPIOx, uint32_t bit, GPIO_AF af_type);

    static void unConfigure(GPIO_ID id);
    static void configureI2S(GPIO_ID id);
    static void configureOutput(GPIO_ID id);
    static void configureInput(GPIO_ID id);
    static void configureKey(GPIO_ID id);
    static void configureADC(GPIO_ID id);


    static void enableEXTI( GPIO_ID id, bool asKey );
    static void disableEXTI( GPIO_ID id );

private:
    static void writePin(GPIO_TypeDef *GPIOx, uint32_t bit, bool val) {
        // set BSRR bit 'bit' to set the value to 1; set bit 'bit + 16' to set the value to 0
        GPIOx->BSRR = (1ul << (bit + (val?0:16) ) );
    }
    static bool readPin(GPIO_TypeDef *GPIOx, uint32_t bit) {
        return GPIOx->IDR & (1u<<bit) ? true : false;
    }

private:
    static void defineGPIO( GPIO_ID gpid, const char * name, GPIO_TypeDef *GPIOx, uint16_t bit, uint32_t flags );

};

#endif /* __GPIO_STM32F4XX_H */
