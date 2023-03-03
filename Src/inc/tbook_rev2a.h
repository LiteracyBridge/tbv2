// **************************************************************************************
//  tbook_rev2a.h
//	Sept 2018 Rev2A board
// ********************************* 
	
// define TBOOKREV2A for Sep2018 Rev2B board
// #define TBOOKREV2A	

#if defined( TBOOKREV2A )
	// TalkingBook V2 Rev2A uses STM32F411RC LQFP64 (Low-profile Quad Flat surface mount Package with 64 leads)
	// rev2:  KEY::HOME  PA0  EXTI0 		kCIRCLE PA1  EXTI1 		KEY::PLUS PA2 EXTI2 	KEY::MINUS PA3  EXTI3 		KEY::TREE  PA15 EXTI15_10
	//        KEY::LHAND PB6  EXTI9_5 		KEY::RHAND  PB8  EXTI19_5 	KEY::POT  PB9 EXTI9_5 	KEY::STAR  PB10 EXTI15_10 	KEY::TABLE PB13 EXTI15_10
	//		  RED	 PC13				GREEN	PB14
	// Sep-2018 REV2-proto board -- KEYBOARD CABLE REVERSED, then jumpered to swap RED & HOME, and GREEN & CIRCLE so:
	// rev2:  KEY::HOME  PA0  EXTI0 			kCIRCLE PA1  EXTI1 			KEY::PLUS PB13 EXTI2	 		KEY::MINUS PB10 EXTI15_10		KEY::TREE  PB9  EXTI15_10
	//        KEY::LHAND PB8  EXTI9_5 		KEY::RHAND  PB6  EXTI19_5 	KEY::POT  PA15 EXTI15_10 	KEY::STAR  PA3  EXTI3	 			KEY::TABLE PA2  EXTI12
	//		  	GREEN	 PC13							RED	PB14
	// Sep-22-2018  apparently, KP13_GREEN & KP12_RED, so swapped...
	#define KP12_RED_pin				GPIO_PIN_14		// PB14 -- KP12_RED
	#define KP12_RED_port				GPIOB
	#define KP13_GREEN_pin			GPIO_PIN_13		// PC13 -- KP13_GREEN
	#define KP13_GREEN_port			GPIOC
	#define AMP_EN_pin					GPIO_PIN_2		// PC2 -- AMP_EN digital output			1 turns on the audio amplifier power
	#define AMP_EN_port					GPIOC

	// ********** power-related GPIO assignments
	//   input interrupt
	#define NOPWR_pin							GPIO_PIN_4		// PC4 -- NOPWR  digital input EXTI4	0 when all power sources but SCAP have failed 
	#define NOPWR_port						GPIOC
	#define NOPWR_EXTI						EXTI4_IRQn

	//   digital inputs
	#define USBPWR_pin						GPIO_PIN_5		// PC5 -- USBPWR digital input 			0 when TBook has USB power
	#define USBPWR_port						GPIOC
	#define LICHG_pin							GPIO_PIN_7		// PC7 -- LICHG digital input 			0 when Li-Ion battery is charging
	#define LICHG_port						GPIOC
	// USB patch  30Sep2019  #define T_FAULT_pin						GPIO_PIN_12		// PA12 - T_FAULT digital input 		0 when Li-Ion battery is too hot
	// USB patch  30Sep2019  #define T_FAULT_port					GPIOA
	#define T_FAULT_pin						GPIO_PIN_12		// PC12 - T_FAULT digital input 		0 when Li-Ion battery is too hot
	#define T_FAULT_port					GPIOC

	//   analog inputs
	#define LI_ION_V_pin					GPIO_PIN_0		// PC0 -- LI_ION_V analog input 		0-3.3V when ANALOG_EN is active
	#define LI_ION_V_port					GPIOC
	#define LI_ION_V_channel			ADC_CHANNEL_10
	#define EX_BATV_pin						GPIO_PIN_1
	#define EX_BATV_port					GPIOC			// PC1 -- EX_BATV analog input 			0-3.0V when ANALOG_EN is active
	#define EX_BATV_channel				ADC_CHANNEL_11

	//   digital outputs
	#define ANALOG_EN_pin					GPIO_PIN_3		// PC3 -- ANALOG_EN digital output		1 enables EX_BATV & LI_ION_V
	#define ANALOG_EN_port				GPIOC
	// USB patch  30Sep2019  #define S_CAP_EN_pin					GPIO_PIN_11		// PA11 - S_CAP_EN digital output 		? to charge the super-capacitor 
	// USB patch  30Sep2019  #define S_CAP_EN_port					GPIOA
	#define S_CAP_EN_pin					GPIO_PIN_11		// PC11 - S_CAP_EN digital output 		? to charge the super-capacitor 
	#define S_CAP_EN_port					GPIOC

	// ********** VS8063 audio chip
	#define VS8063_XCS_pin 				GPIO_PIN_4		// PA4
	#define VS8063_XCS_port 			GPIOA
	#define VS8063_XDCS_pin 			GPIO_PIN_0		// PB0
	#define VS8063_XDCS_port 			GPIOB
	#define VS8063_DREQ_pin 			GPIO_PIN_1		// PB1
	#define VS8063_DREQ_port 			GPIOB
	#define VS8063_XRESET_pin 		GPIO_PIN_2		// PB2
	#define VS8063_XRESET_port 		GPIOB
#endif


