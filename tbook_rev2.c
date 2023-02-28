// Signal definitions for TBook Rev2

#include "tbook_rev2.h"

// TBook_V2_Rev3  EXTI ints 0 Hom, 1 Pot, 3 Tab, 4 Plu, 5-9 Min/LHa/Sta/Cir, 10-15 RHa/Tre, 2 Pwr_Fail_N
const GPIO_Signal gpioSignals[] = {  	// GPIO signal definitions
	// GPIO_ID  signal  								eg   'PA4'   => GPIOA, pin 4, active high  
	//          "Pxdd_|cc" where:				e.g. 'PE12_' => GPIOE, pin 12, active low
	//             x  = A,B,C,D,E,F,G,H -- GPIO port
	//             dd = 0..15  -- GPIO pin #
	//             _  = means 'active low', i.e, 0 if pressed
	//						 cc = 0..15 -- alternate function code
	{ gHOME,	  		"PA0"				}, 	// IN:  SW9_HOUSE,  1 when pressed    configured by inputmanager.c
	{ gCIRCLE,			"PE9"				}, 	// IN:  SW8_CIRCLE, 1 when pressed		configured by inputmanager.c
	{ gPLUS,	  		"PA4"				}, 	// IN:  SW7_PLUS,   1 when pressed		configured by inputmanager.c	
	{ gMINUS,				"PA5"				}, 	// IN:  SW6_MINUS,  1 when pressed		configured by inputmanager.c	
	{ gTREE,	  		"PA15"			}, 	// IN:  SW5_TREE,   1 when pressed		configured by inputmanager.c
	{ gLHAND,				"PB7"				}, 	// IN:  SW4_LEFT,   1 when pressed		configured by inputmanager.c
	{ gRHAND,				"PB10"			}, 	// IN:  SW3_RIGHT,  1 when pressed		configured by inputmanager.c
	{ gPOT,	  			"PC1"				}, 	// IN:  SW2_POT,    1 when pressed		configured by inputmanager.c
	{ gSTAR,	  		"PE8"				}, 	// IN:  SW1_STAR,   1 when pressed		configured by inputmanager.c
	{ gTABLE,				"PE3"				}, 	// IN:  SW0_TABLE,  1 when pressed		configured by inputmanager.c
	{	gRED,					"PE1"				}, 	// OUT: 1 to turn Red LED ON		 	configured by ledmanager.c
	{ gGREEN, 			"PE0"				}, 	// OUT: 1 to turn Green LED ON		configured by ledmanager.c

	{ gBOARD_REV,		"PB0"				}, 	// IN:  0 if Rev3 board
	{ gADC_LI_ION, 	"PA2"				},	// IN:  analog rechargable battery voltage level: ADC1_channel2 	(powermanager.c)
	{ gADC_PRIMARY,	"PA3"				},	// IN:  analog disposable battery voltage level:	ADC1_channel3 	(powermanager.c)
	{ gADC_THERM,	  "PC2"				},	// IN:  analog 
	{ gPWR_FAIL_N, 	"PE2"			  },	// IN:  0 => power fail signal 										(powermanager.c)
	{ gADC_ENABLE, 	"PE15"			},	// OUT: 1 to enable battery voltage measurement 	(powermanager.c)
	{ gSC_ENABLE, 	"PD1"				},	// OUT: 1 to enable SuperCap			 								(powermanager.c)
	{ gEN_IOVDD_N,	"PE4"				},	// OUT: 0 to power AIC3100 codec rail IOVDD
	{ gEN_AVDD_N,		"PE5"				},	// OUT: 0 to power AIC3100 codec rail AVDD
	{ gEN_5V, 			"PD4"				},	// OUT: 1 to supply 5V to codec		AP6714 EN				(powermanager.c)
	{ gEN1V8, 			"PD5"				},	// OUT: 1 to supply 1.8 to codec  TLV74118 EN			(powermanager.c)	
	{ g3V3_SW_EN, 	"PD6"				},	// OUT: 1 to power SDCard & eMMC
	{ gEMMC_RSTN, 	"PE10"			},	// OUT: 0 to reset eMMC?		MTFC2GMDEA eMMC RSTN	(powermanager.c)	
	
	{ gBAT_STAT1,		"PD8"				},  // IN:  charge_status[0]     			MCP73871 STAT1  (powermanager.c)
	{ gBAT_STAT2,		"PD9"				},  // IN:  charge_status[1]     			MCP73871 STAT2  (powermanager.c)
	{ gBAT_PG_N,		"PD10"			},  // IN:  0 => power is good   			configured as active High to match MCP73871 Table 5.1
	{ gBAT_CE,	  	"PD0"				},  // OUT: 1 to enable charging 			MCP73871 CE 	  (powermanager.c)
	{ gBAT_TE_N,		"PD13"			},  // OUT: 0 to enable safety timer 	MCP73871 TE_	  (powermanager.c)
	{ gBOOT1_PDN, 	"PB2" 			},	// OUT: 0 to power_down codec 	 AK4637 PDN 			(powermanager.c)

	//******* I2C pins must match definitions in RTE_Device.h: ConfigWizard: I2C1
	{ gI2C1_SCL, 		"PB8|4" 		},	// AIC3100 I2C_SCL 								(RTE_Device.h I2C1 altFn=4)
	{ gI2C1_SDA, 		"PB9|4" 		},	// AIC3100 I2C_SDA 								(RTE_Device.h I2C1 altFn=4)

	{ gI2S2_SD,			"PB15|5"		},	// AIC3100 DIN == MOSI 						(RTE_I2SDevice.h I2S0==SPI2 altFn=5)
	{ gI2S2ext_SD,	"PB14|6"		},	// AIC3100 DOUT 									(RTE_I2SDevice.h I2S0==SPI2 altFn=6)
	{ gI2S2_WS,			"PB12|5"		},	// AIC3100 WCLK             			(RTE_I2SDevice.h I2S0==SPI2 altFn=5)
	{ gI2S2_CK,			"PB13|5"		},	// AIC3100 BCL				 						(RTE_I2SDevice.h I2S0==SPI2 altFn=5) 
	{ gI2S2_MCK,		"PC6|5"			},	// AIC3100 MCLK   								(RTE_I2SDevice.h I2S0==SPI2 altFn=5)
	{ gI2S3_MCK,		"PC7|6"			},	// AIC3100 MCLK 	 								(RTE_I2SDevice.h I2S3==SPI2 altFn=6)

	//******* SPI4 pins must match definitions in RTE_Device.h: ConfigWizard: SPI4
	{ gSPI4_NSS, 		"PE11|5"		}, 	// W25Q64JVSS  NOR flash U8 			(RTE_Device.h SPI4 altFn=5)
	{ gSPI4_SCK, 		"PE12|5"		}, 	// W25Q64JVSS  NOR flash U8 			(RTE_Device.h SPI4 altFn=5)
	{ gSPI4_MISO, 	"PE13|5"		}, 	// W25Q64JVSS  NOR flash U8 			(RTE_Device.h SPI4 altFn=5)
	{ gSPI4_MOSI, 	"PE14|5"		}, 	// W25Q64JVSS  NOR flash U8 			(RTE_Device.h SPI4 altFn=5)
	
	//******* USB_VBUS pin must be enabled in RTE_Device.h: ConfigWizard: USB_OTG_FS
	{ gUSB_DM, 			"PA11|10"		},	// std pinout assumed by USBD (altFn=10)
	{ gUSB_DP, 			"PA12|10"		},	// std pinout assumed by USBD (altFn=10)
	{ gUSB_ID, 			"PA10|10"		},	// std pinout assumed by USBD (altFn=10)
	{ gUSB_VBUS, 		"PA9|10"		},	// std pinout assumed by USBD (altFn=10))

	{ gQSPI_CLK_A, 		"PD3|9"		},	// U5 W25Q64JVSS clock for NOR and NAND Flash (altFn=9)
	{ gQSPI_CLK_B, 		"PB1|9"		},	// U4 MT29F4G01 clock for NOR and NAND Flash (altFn=9)

	{ gMCO2,					"PC9|0"		},  // MCO2 to TP for external SystemClock/4
	
	{ gQSPI_BK1_IO0, 	"PD11|9"	},	// W25Q64JVSS  NOR flash  U5 (altFn=9)
	{ gQSPI_BK1_IO1, 	"PD12|9"	},	// W25Q64JVSS  NOR flash  U5 (altFn=9)
	{ gQSPI_BK1_IO2, 	"PC8|9"		},	// W25Q64JVSS  NOR flash  U5 (altFn=9)
	{ gQSPI_BK1_IO3, 	"PA1|9"		},	// W25Q64JVSS  NOR flash  U5 (altFn=9)
	{ gQSPI_BK1_NCS, 	"PB6|10"	},	// W25Q64JVSS  NOR flash  U5 chip sel (altFn=10)
	
	{ gQSPI_BK2_IO0, 	"PA6|10"	},	// MT29F4G01 NAND flash  U4   (altFn=10)
	{ gQSPI_BK2_IO1, 	"PA7|10"	},	// MT29F4G01 NAND flash  U4   (altFn=10) 
	{ gQSPI_BK2_IO2, 	"PC4|10"	},	// MT29F4G01 NAND flash  U4   (altFn=10) 
	{ gQSPI_BK2_IO3, 	"PC5|10"	},	// MT29F4G01 NAND flash  U4   (altFn=10)
	{ gQSPI_BK2_NCS, 	"PC11|9"	},	// MT29F4G01 NAND flash  U4  chip sel (altFn=9)
		
	//******* SDIO pins must match definitions in RTE_Device.h: ConfigWizard: SDIO
	{ gSDIO_DAT0, 		"PB4|12"	},  // SDcard & eMMC SDIO D0  (RTE_Device.h MCI0)  (altFn=12)
	{ gSDIO_DAT1, 		"PA8|12"	},  // SDcard & eMMC SDIO D1  (RTE_Device.h MCI0)  (altFn=12)
	{ gSDIO_DAT2, 		"PC10|12"	},  // SDcard & eMMC SDIO D2  (RTE_Device.h MCI0)  (altFn=12)
	{ gSDIO_DAT3, 		"PB5|12"	},  // SDcard & eMMC SDIO D3  (RTE_Device.h MCI0)  (altFn=12)
	{ gSDIO_CLK, 			"PC12|12"	},  // SDcard & eMMC SDIO CLK (RTE_Device.h MCI0)  (altFn=12)
	{ gSDIO_CMD, 			"PD2|12"	},	// SDcard & eMMC SDIO CMD (RTE_Device.h MCI0) 	  (altFn=12)

	{ gSWDIO, 		"PA13"		},	// bootup function 
	{ gSWCLK, 		"PA14"		},	// bootup function 
	{ gSWO, 			"PB3"			},	// bootup function

	{ gOSC32_OUT,	"PC15"		},	// bootup function
	{ gOSC32_IN,	"PC14"		},	// bootup function
	{ gOSC_OUT,		"PH1"			},	// bootup function
	{ gOSC_IN,		"PH0"			},	// bootup function
	
	{ gINVALID, 	""  			}	// end of list
};
// end tbook_rev2.c


