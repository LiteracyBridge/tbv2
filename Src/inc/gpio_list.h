//
// Created by bill on 3/7/2023.
//

#ifndef TBV2_GPIO_LIST_H
#define TBV2_GPIO_LIST_H

#define GDEF_AF_0 0
#define GDEF_AF_4 4
#define GDEF_AF_5 5
#define GDEF_AF_6 6
#define GDEF_AF_9 9
#define GDEF_AF_10 10
#define GDEF_AF_12 12


// @formatter:off
#define GPIO_DEFINITION_LIST \
    X( HOUSE,              GPIOA,  0, GDEF_NONE ) \
    X( CIRCLE,            GPIOE,  9, GDEF_NONE ) \
    X( PLUS,              GPIOA,  4, GDEF_NONE ) \
    X( MINUS,             GPIOA,  5, GDEF_NONE ) \
    X( TREE,              GPIOA, 15, GDEF_NONE ) \
    X( LHAND,             GPIOB,  7, GDEF_NONE ) \
    X( RHAND,             GPIOB, 10, GDEF_NONE ) \
    X( BOWL,              GPIOC,  1, GDEF_NONE ) \
    X( STAR,              GPIOE,  8, GDEF_NONE ) \
    X( TABLE,             GPIOE,  3, GDEF_NONE ) \
    X( RED,               GPIOE,  1, GDEF_NONE ) \
    X( GREEN,             GPIOE,  0, GDEF_NONE ) \
    X( BOARD_REV,         GPIOB,  0, GDEF_NONE ) \
    X( ADC_LI_ION,        GPIOA,  2, GDEF_NONE ) \
    X( ADC_PRIMARY,       GPIOA,  3, GDEF_NONE ) \
    X( ADC_THERM,         GPIOC,  2, GDEF_NONE ) \
    X( PWR_FAIL_N,        GPIOE,  2, GDEF_NONE ) \
    X( ADC_ENABLE,        GPIOE, 15, GDEF_NONE ) \
    X( SC_ENABLE,         GPIOD,  1, GDEF_NONE ) \
    X( EN_IOVDD_N,        GPIOE,  4, GDEF_NONE ) \
    X( EN_AVDD_N,         GPIOE,  5, GDEF_NONE ) \
    X( EN_5V,             GPIOD,  4, GDEF_NONE ) \
    X( EN1V8,             GPIOD,  5, GDEF_NONE ) \
    X( 3V3_SW_EN,         GPIOD,  6, GDEF_NONE ) \
    X( EMMC_RSTN,         GPIOE, 10, GDEF_NONE ) \
    X( BAT_STAT1,         GPIOD,  8, GDEF_NONE ) \
    X( BAT_STAT2,         GPIOD,  9, GDEF_NONE ) \
    X( BAT_PG_N,          GPIOD, 10, GDEF_NONE ) \
    X( BAT_CE,            GPIOD,  0, GDEF_NONE ) \
    X( BAT_TE_N,          GPIOD, 13, GDEF_NONE ) \
    X( BOOT1_PDN,         GPIOB,  2, GDEF_NONE ) \
    X( I2C1_SCL,          GPIOB,  8, GDEF_AF_4 ) \
    X( I2C1_SDA,          GPIOB,  9, GDEF_AF_4 ) \
    X( I2S2_SD,           GPIOB, 15, GDEF_AF_5 ) \
    X( I2S2ext_SD,        GPIOB, 14, GDEF_AF_6 ) \
    X( I2S2_WS,           GPIOB, 12, GDEF_AF_5 ) \
    X( I2S2_CK,           GPIOB, 13, GDEF_AF_5 ) \
    X( I2S2_MCK,          GPIOC,  6, GDEF_AF_5 ) \
    X( I2S3_MCK,          GPIOC,  7, GDEF_AF_6 ) \
    X( SPI4_NSS,          GPIOE, 11, GDEF_AF_5 ) \
    X( SPI4_SCK,          GPIOE, 12, GDEF_AF_5 ) \
    X( SPI4_MISO,         GPIOE, 13, GDEF_AF_5 ) \
    X( SPI4_MOSI,         GPIOE, 14, GDEF_AF_5 ) \
    X( USB_DM,            GPIOA, 11, GDEF_AF_10 ) \
    X( USB_DP,            GPIOA, 12, GDEF_AF_10 ) \
    X( USB_ID,            GPIOA, 10, GDEF_AF_10 ) \
    X( USB_VBUS,          GPIOA,  9, GDEF_AF_10 ) \
    X( QSPI_CLK_A,        GPIOD,  3, GDEF_AF_9 ) \
    X( QSPI_CLK_B,        GPIOB,  1, GDEF_AF_9 ) \
    X( MCO2,              GPIOC,  9, GDEF_AF_0 ) \
    X( QSPI_BK1_IO0,      GPIOD, 11, GDEF_AF_9 ) \
    X( QSPI_BK1_IO1,      GPIOD, 12, GDEF_AF_9 ) \
    X( QSPI_BK1_IO2,      GPIOC,  8, GDEF_AF_9 ) \
    X( QSPI_BK1_IO3,      GPIOA,  1, GDEF_AF_9 ) \
    X( QSPI_BK1_NCS,      GPIOB,  6, GDEF_AF_10 ) \
    X( QSPI_BK2_IO0,      GPIOA,  6, GDEF_AF_10 ) \
    X( QSPI_BK2_IO1,      GPIOA,  7, GDEF_AF_10 ) \
    X( QSPI_BK2_IO2,      GPIOC,  4, GDEF_AF_10 ) \
    X( QSPI_BK2_IO3,      GPIOC,  5, GDEF_AF_10 ) \
    X( QSPI_BK2_NCS,      GPIOC, 11, GDEF_AF_9 ) \
    X( SDIO_DAT0,         GPIOB,  4, GDEF_AF_12 ) \
    X( SDIO_DAT1,         GPIOA,  8, GDEF_AF_12 ) \
    X( SDIO_DAT2,         GPIOC, 10, GDEF_AF_12 ) \
    X( SDIO_DAT3,         GPIOB,  5, GDEF_AF_12 ) \
    X( SDIO_CLK,          GPIOC, 12, GDEF_AF_12 ) \
    X( SDIO_CMD,          GPIOD,  2, GDEF_AF_12 ) \
    X( SWDIO,             GPIOA, 13, GDEF_NONE ) \
    X( SWCLK,             GPIOA, 14, GDEF_NONE ) \
    X( SWO,               GPIOB,  3, GDEF_NONE ) \
    X( OSC32_OUT,         GPIOC, 15, GDEF_NONE ) \
    X( OSC32_IN,          GPIOC, 14, GDEF_NONE ) \
    X( OSC_OUT,           GPIOH,  1, GDEF_NONE ) \
    X( OSC_IN,            GPIOH,  0, GDEF_NONE ) \


#if 0
// GPIO_ID  signal                  eg   'PA4'   => GPIOA, pin 4, active high
//          "Pxdd_|cc" where:        e.g. 'PE12_' => GPIOE, pin 12, active low
//             x  = A,B,C,D,E,F,G,H -- GPIO port
//             dd = 0..15  -- GPIO pin #
//             _  = means 'active low', i.e, 0 if pressed
//             cc = 0..15 -- alternate function code
{ gHOUSE,       "PA0"        },   // IN:  SW9_HOUSE,  1 when pressed    configured by inputmanager.c
{ gCIRCLE,      "PE9"        },   // IN:  SW8_CIRCLE, 1 when pressed    configured by inputmanager.c
{ gPLUS,        "PA4"        },   // IN:  SW7_PLUS,   1 when pressed    configured by inputmanager.c
{ gMINUS,        "PA5"        },   // IN:  SW6_MINUS,  1 when pressed    configured by inputmanager.c
{ gTREE,        "PA15"      },   // IN:  SW5_TREE,   1 when pressed    configured by inputmanager.c
{ gLHAND,        "PB7"        },   // IN:  SW4_LEFT,   1 when pressed    configured by inputmanager.c
{ gRHAND,        "PB10"      },   // IN:  SW3_RIGHT,  1 when pressed    configured by inputmanager.c
{ gBOWL,         "PC1"        },   // IN:  SW2_BOWL,    1 when pressed    configured by inputmanager.c
{ gSTAR,         "PE8"        },   // IN:  SW1_STAR,   1 when pressed    configured by inputmanager.c
{ gTABLE,        "PE3"        },   // IN:  SW0_TABLE,  1 when pressed    configured by inputmanager.c
{  gRED,          "PE1"        },   // OUT: 1 to turn Red LED ON       configured by ledmanager.c
{ gGREEN,       "PE0"        },   // OUT: 1 to turn Green LED ON    configured by ledmanager.c

{ gBOARD_REV,    "PB0"        },   // IN:  0 if Rev3 board
{ gADC_LI_ION,   "PA2"        },  // IN:  analog rechargable battery voltage level: ADC1_channel2   (powermanager.c)
{ gADC_PRIMARY,  "PA3"        },  // IN:  analog disposable battery voltage level:  ADC1_channel3   (powermanager.c)
{ gADC_THERM,    "PC2"        },  // IN:  analog
{ gPWR_FAIL_N,   "PE2"        },  // IN:  0 => power fail signal                     (powermanager.c)
{ gADC_ENABLE,   "PE15"      },  // OUT: 1 to enable battery voltage measurement   (powermanager.c)
{ gSC_ENABLE,   "PD1"        },  // OUT: 1 to enable SuperCap                       (powermanager.c)
{ gEN_IOVDD_N,  "PE4"        },  // OUT: 0 to power AIC3100 codec rail IOVDD
{ gEN_AVDD_N,    "PE5"        },  // OUT: 0 to power AIC3100 codec rail AVDD
{ gEN_5V,       "PD4"        },  // OUT: 1 to supply 5V to codec    AP6714 EN        (powermanager.c)
{ gEN1V8,       "PD5"        },  // OUT: 1 to supply 1.8 to codec  TLV74118 EN      (powermanager.c)
{ g3V3_SW_EN,   "PD6"        },  // OUT: 1 to power SDCard & eMMC
{ gEMMC_RSTN,   "PE10"      },  // OUT: 0 to reset eMMC?    MTFC2GMDEA eMMC RSTN  (powermanager.c)

{ gBAT_STAT1,    "PD8"        },  // IN:  charge_status[0]           MCP73871 STAT1  (powermanager.c)
{ gBAT_STAT2,    "PD9"        },  // IN:  charge_status[1]           MCP73871 STAT2  (powermanager.c)
{ gBAT_PG_N,    "PD10"      },  // IN:  0 => power is good         configured as active High to match MCP73871 Table 5.1
{ gBAT_CE,      "PD0"        },  // OUT: 1 to enable charging       MCP73871 CE     (powermanager.c)
{ gBAT_TE_N,    "PD13"      },  // OUT: 0 to enable safety timer   MCP73871 TE_    (powermanager.c)
{ gBOOT1_PDN,   "PB2"       },  // OUT: 0 to power_down codec    AK4637 PDN       (powermanager.c)

//******* I2C pins must match definitions in RTE_Device.h: ConfigWizard: I2C1
{ gI2C1_SCL,     "PB8|4"     },  // AIC3100 I2C_SCL                 (RTE_Device.h I2C1 altFn=4)
{ gI2C1_SDA,     "PB9|4"     },  // AIC3100 I2C_SDA                 (RTE_Device.h I2C1 altFn=4)

{ gI2S2_SD,      "PB15|5"    },  // AIC3100 DIN == MOSI             (RTE_I2SDevice.h I2S0==SPI2 altFn=5)
{ gI2S2ext_SD,  "PB14|6"    },  // AIC3100 DOUT                   (RTE_I2SDevice.h I2S0==SPI2 altFn=6)
{ gI2S2_WS,      "PB12|5"    },  // AIC3100 WCLK                   (RTE_I2SDevice.h I2S0==SPI2 altFn=5)
{ gI2S2_CK,      "PB13|5"    },  // AIC3100 BCL                     (RTE_I2SDevice.h I2S0==SPI2 altFn=5)
{ gI2S2_MCK,    "PC6|5"      },  // AIC3100 MCLK                   (RTE_I2SDevice.h I2S0==SPI2 altFn=5)
{ gI2S3_MCK,    "PC7|6"      },  // AIC3100 MCLK                    (RTE_I2SDevice.h I2S3==SPI2 altFn=6)

//******* SPI4 pins must match definitions in RTE_Device.h: ConfigWizard: SPI4
{ gSPI4_NSS,     "PE11|5"    },   // W25Q64JVSS  NOR flash U8       (RTE_Device.h SPI4 altFn=5)
{ gSPI4_SCK,     "PE12|5"    },   // W25Q64JVSS  NOR flash U8       (RTE_Device.h SPI4 altFn=5)
{ gSPI4_MISO,   "PE13|5"    },   // W25Q64JVSS  NOR flash U8       (RTE_Device.h SPI4 altFn=5)
{ gSPI4_MOSI,   "PE14|5"    },   // W25Q64JVSS  NOR flash U8       (RTE_Device.h SPI4 altFn=5)

//******* USB_VBUS pin must be enabled in RTE_Device.h: ConfigWizard: USB_OTG_FS
{ gUSB_DM,       "PA11|10"    },  // std pinout assumed by USBD (altFn=10)
{ gUSB_DP,       "PA12|10"    },  // std pinout assumed by USBD (altFn=10)
{ gUSB_ID,       "PA10|10"    },  // std pinout assumed by USBD (altFn=10)
{ gUSB_VBUS,     "PA9|10"    },  // std pinout assumed by USBD (altFn=10))

{ gQSPI_CLK_A,     "PD3|9"    },  // U5 W25Q64JVSS clock for NOR and NAND Flash (altFn=9)
{ gQSPI_CLK_B,     "PB1|9"    },  // U4 MT29F4G01 clock for NOR and NAND Flash (altFn=9)

{ gMCO2,          "PC9|0"    },  // MCO2 to TP for external SystemClock/4

{ gQSPI_BK1_IO0,   "PD11|9"  },  // W25Q64JVSS  NOR flash  U5 (altFn=9)
{ gQSPI_BK1_IO1,   "PD12|9"  },  // W25Q64JVSS  NOR flash  U5 (altFn=9)
{ gQSPI_BK1_IO2,   "PC8|9"    },  // W25Q64JVSS  NOR flash  U5 (altFn=9)
{ gQSPI_BK1_IO3,   "PA1|9"    },  // W25Q64JVSS  NOR flash  U5 (altFn=9)
{ gQSPI_BK1_NCS,   "PB6|10"  },  // W25Q64JVSS  NOR flash  U5 chip sel (altFn=10)

{ gQSPI_BK2_IO0,   "PA6|10"  },  // MT29F4G01 NAND flash  U4   (altFn=10)
{ gQSPI_BK2_IO1,   "PA7|10"  },  // MT29F4G01 NAND flash  U4   (altFn=10)
{ gQSPI_BK2_IO2,   "PC4|10"  },  // MT29F4G01 NAND flash  U4   (altFn=10)
{ gQSPI_BK2_IO3,   "PC5|10"  },  // MT29F4G01 NAND flash  U4   (altFn=10)
{ gQSPI_BK2_NCS,   "PC11|9"  },  // MT29F4G01 NAND flash  U4  chip sel (altFn=9)

//******* SDIO pins must match definitions in RTE_Device.h: ConfigWizard: SDIO
{ gSDIO_DAT0,     "PB4|12"  },  // SDcard & eMMC SDIO D0  (RTE_Device.h MCI0)  (altFn=12)
{ gSDIO_DAT1,     "PA8|12"  },  // SDcard & eMMC SDIO D1  (RTE_Device.h MCI0)  (altFn=12)
{ gSDIO_DAT2,     "PC10|12"  },  // SDcard & eMMC SDIO D2  (RTE_Device.h MCI0)  (altFn=12)
{ gSDIO_DAT3,     "PB5|12"  },  // SDcard & eMMC SDIO D3  (RTE_Device.h MCI0)  (altFn=12)
{ gSDIO_CLK,       "PC12|12"  },  // SDcard & eMMC SDIO CLK (RTE_Device.h MCI0)  (altFn=12)
{ gSDIO_CMD,       "PD2|12"  },  // SDcard & eMMC SDIO CMD (RTE_Device.h MCI0)     (altFn=12)

{ gSWDIO,     "PA13"    },  // bootup function
{ gSWCLK,     "PA14"    },  // bootup function
{ gSWO,       "PB3"      },  // bootup function

{ gOSC32_OUT,  "PC15"    },  // bootup function
{ gOSC32_IN,  "PC14"    },  // bootup function
{ gOSC_OUT,    "PH1"      },  // bootup function
{ gOSC_IN,    "PH0"      },  // bootup function

#endif

// @formatter:on
#endif //TBV2_GPIO_LIST_H
