//
// Created by bill on 3/6/2023.
//

#ifndef TBV2_GPIO_H
#define TBV2_GPIO_H

#include "gpio_list.h"

/*
 * GPIO definitions. The first #define lists all of the GPIOs that we care about along with their parameters.
 * The parameters are given in the form of #define invocations, like
 *      GPIO_DEFINITION( NAME, GPIOx,  bit#,   flags)
 */

// Create the enumeration from the list.
#define GPIO_DEFINITION(name, port, bit, flags) g ## name,
typedef enum GPIO_ID_t {
    GPIO_DEFINITION_LIST
    NUM_GPIO_DEFINITIONS,
    gINVALID
} GPIO_ID;
#undef GPIO_DEFINITION

//typedef struct {  // GPIO_Signal -- define GPIO port/pins/interrupt #, signal name, & st when pressed for a GPIO_ID
//    GPIO_ID id;      // GPIO_ID connected to this line
//    const char *name;    // "PC7_" => { GPIOC, 7, EXTI15_10_IRQn, "PC7_", 0 }
//} GPIO_Signal;


#define PORT_INDEX_MASK 0x0f
#define BIT_INDEX_MASK 0xf0
#define BIT_INDEX_SHIFT 4
#define ACTIVE_VALUE_MASK 0x100
#define ACTIVE_VALUE_SHIFT 8
#define RESET_FLAGS_MASK 0xfe00
#define RESET_FLAGS_SHIFT 9

#define RESET_NONE       0
#define RESET_TO_INPUT  (0x01 << RESET_FLAGS_SHIFT)
#define RESET_TO_ADC    (0x02 << RESET_FLAGS_SHIFT)
#define RESET_HIGH      (0x04 << RESET_FLAGS_SHIFT)
#define RESET_LOW       (0x08 << RESET_FLAGS_SHIFT)
#define RESET_DISABLE   (0x10 << RESET_FLAGS_SHIFT)
#define RESET_ENABLE    (0x20 << RESET_FLAGS_SHIFT)



#endif //TBV2_GPIO_H
