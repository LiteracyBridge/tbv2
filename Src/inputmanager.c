// TBookV2  inputmanager.c
//   Apr 2018

#include "tbook.h"
#include "main.h"
#include "inputMgr.h"
#include "log.h"		// logEvt
#include "tb_evr.h"

const int					KEYPAD_EVT 		=	1;
const int					TB_EVT_QSIZE	=	4;

#define count(x) sizeof(x)/sizeof(x[0])

osMessageQueueId_t 				osMsg_TBEvents 	= NULL;     // os message queue id	for TB_Events
osMemoryPoolId_t					TBEvent_pool 	= NULL;		// memory pool for TBEvents 

static osThreadAttr_t 		thread_attr;
static osEventFlagsId_t		osFlag_InpThr;		// osFlag to signal a keyUp event from ISR
static osTimerId_t				keyDownTimer = NULL;
uint16_t									KeypadIMR = 0;					// interrupt mask register bits for keypad (also set in main() for MarcDebug)

// structs defined in tbook.h for Debugging visibility
KeyPadState_t				KSt;				
KeyTestState_t			KTest;		

const char *				keyName[] = { "k0H", "k1C", "k2+", "k3-", "k4T", "k5P", "k6S", "k7t", "k8", "k9", "k10I" };
const char 					keyNm[] = {'H', 'C', '+', '-', 'T', 'L', 'R', 'P', 'S', 'B', 'I' };	// used for Debug ouput
const char *				keyTestGood = "GgGg_";
const char *				keyTestBad = "RrRr_"; 
const char *				keyTestReset = "R8_3 R8_3 R8_3";	// 3 long red pulses
const char *				keyTestSuccess = "G8R_3 G8R_3 G8R_3";	 // 3 long green then red pulses


/*  External interrupts used by the TBook keypad, and corresponding Interrupt Mask Bits
exti0     PA0   gHOME     0001
exti1     PC1   gPOT      0002
exti2   
exti3     PE3   gTABLE -- 0008
exti4     PA4   gPLUS     0010
exti5_9   PA5   gMINUS    0020
exti5_9 
exti5_9   PB7   gLHAND -- 0080
exti5_9   PE8   gSTAR --  0100
exti5_9   PE9   gCIRCLE --0200
exti10_15 PB10  gRHAND -- 0400
exti10_15 
exti10_15 
exti10_15 
exti10_15 
exti10_15 PA15  gTREE     8000
EXTI_IMR                  87BB
*/

KeyPadKey_arr 			keydef = {  // keypad GPIO in order: kHOME, kCIRCLE, kPLUS, kMINUS, kTREE, kLHAND, kPOT, kRHAND, kSTAR, kTABLE
// static placeholders, filled in by initializeInterrupts
//	GPIO_ID    key,    port,  	pin,         intq				extiBit		signal		pressed	 down   tstamp dntime
	{  gHOME,		kHOME,	  NULL, 		0,  				WWDG_IRQn,  	0,			"",					0,	 false,	0,	0  	},
	{  gCIRCLE,	kCIRCLE,	NULL, 		0,  				WWDG_IRQn,  	0,			"",					0,	 false,	0,	0	 	}, 
	{  gPLUS,		kPLUS,	  NULL, 		0,  				WWDG_IRQn, 		0,			"",					0,	 false,	0,	0	 	}, 
	{  gMINUS,	kMINUS,		NULL, 		0,  				WWDG_IRQn, 		0,			"",					0,	 false,	0,	0	 	}, 
	{  gTREE, 	kTREE,	  NULL, 		0,  				WWDG_IRQn, 		0,			"",					0,	 false,	0,	0	  }, 
	{  gLHAND, 	kLHAND,		NULL, 		0,  				WWDG_IRQn, 		0,			"",					0,	 false,	0,	0	  }, 
	{  gRHAND, 	kRHAND,		NULL, 		0,  				WWDG_IRQn, 		0,			"",					0,	 false,	0,	0	  }, 
	{  gPOT, 		kPOT,	  	NULL, 		0,  				WWDG_IRQn, 		0,			"",					0,	 false,	0,	0	  }, 
	{  gSTAR,		kSTAR,	  NULL, 		0,  				WWDG_IRQn, 		0,			"",					0,	 false,	0,	0	  }, 
	{  gTABLE, 	kTABLE,		NULL, 		0,  				WWDG_IRQn, 		0,			"",					0,	 false,	0,	0	  }
}; 
void 					handleInterrupt( bool fromThread );		// forward
// 
// inputManager-- Interrupt handling
void 					disableInputs(){						// disable all keypad GPIO interrupts
	EXTI->IMR &= ~KeypadIMR; 
}
void 					enableInputs( bool fromThread ){							// check for any unprocessed Interrupt Mask Register from 'imr'
	
	// I think the following problem occurs when a key change triggers an interrupt, but then the key changes back before 
	// the ISR has finished -- so the 2nd transition never triggers an interrupt.
	//
	//BUG CHECK-- sometimes this is called when the keydef[].down state doesn't match the IDR state, which shouldn't happen
	// e.g. handleInterrupts is called & doesn't set detectedUpKey, with key[x].down==1, but IDR[x]==0
	for ( KEY k = kHOME; k < kINVALID; k++ ){		// process key transitions
		int idr = keydef[k].port->IDR;		// port Input data register
		bool kdn = gGet( keydef[k].id );	// gets LOGICAL value of port/pin
		if ( kdn != keydef[k].down ){			// should == current state
			char *sig = keydef[k].signal; 
			dbgLog( "! %s K%d %s dn%d \n", fromThread?"*":"I", k, sig, keydef[k].down );
			int pendR = EXTI->PR, iw = keydef[k].intq >> 5UL, irq = NVIC->ICPR[iw];
			dbgEvt(TBkeyMismatch, k, (fromThread<<8) + kdn, pendR, irq );
			if (irq != 0 || pendR!= 0) 
				dbgLog( "! pR%04x ICPR[%d]%04x \n", pendR, iw, irq );
			handleInterrupt( true );		// re-invoke, to process missed transition
			return;
		}
	}
	EXTI->IMR |= KeypadIMR;		// re-enable keypad EXTI interrupts
}						   					

bool 					updateKeyState( KEY k ){		// read keypad pin, update keydef[k] if changed, & generate osEvt if UP transition
	bool keydown = gGet( keydef[k].id ); 
	Dbg.keypad[ k ] = keydown? keyNm[k] : '_';	//DEBUG
	
	if ( k==kSTAR ) KSt.starDown = keydown;	// 	keep track for <STAR-other> control sequences
	if ( keydef[k].down != keydown ){	// state has changed 
		keydef[k].down = keydown;
		if ( keydown ){		// keyDown transition -- remember starttime
			if ( KSt.DownKey==kINVALID ){ // first key down
				// ((osRtxTimer_t *) keyDownTimer )-> 
				osTimerStart( keyDownTimer, TB_Config->minLongPressMS );  // interrupts if key down that long
			} else { // multiple keys down-- disable long press timer
				osTimerStop( keyDownTimer );
			}
			keydef[k].tstamp = tbTimeStamp(); 
			dbgEvt( TB_keyDn, k, keydef[k].tstamp, 0, 0 );
		} else {  // keyUp transition
			if ( KSt.LongPressKey == k ){   // already reported long press, so ignore actual up transition
				KSt.LongPressKey = kINVALID;
				return false;
			}
			if ( KSt.DownKey==k ){
				osTimerStop( keyDownTimer );
				KSt.DownKey = kINVALID;
			}
			if ( k==kSTAR && KSt.starAltUsed ){	// no event for Star UP, if it was used for <star-X>
				KSt.starAltUsed = false;
				dbgEvt( TB_keyStUp, k,0, 0, 0 );
				return false;
			}
			keydef[k].dntime = tbTimeStamp() - keydef[k].tstamp; 
			dbgEvt( TB_keyUp, k, keydef[k].dntime, 0, 0 );
			if ( KSt.detectedUpKey == kINVALID ){		// no event detected yet
				KSt.detectedUpKey = k; 
				osEventFlagsSet( osFlag_InpThr, KEYPAD_EVT );		// wakeup for inputThread
				return true;
			} 
		}
	}
	return false;
}

extern bool 	RebootOnKeyInt;		// from powermanager -- reboot if interrupt
void 					checkKeyTimer( void * arg ){
// called by OS when keyDownTimer expires ==> key has been down more than TB_Config->minLongPressMS
	//osTimerStop( keyDownTimer );			// needed?
	dbgEvt( TB_keyTmr, KSt.DownKey, 0, 0, 0 );
	KSt.detectedUpKey = KSt.DownKey; 	// treat like up after long press
	KSt.LongPressKey = KSt.DownKey;   // remember -- ignore actual up transition
	KSt.DownKey = kINVALID; 
	osEventFlagsSet( osFlag_InpThr, KEYPAD_EVT );		// wakeup for inputThread
}
void 					handleInterrupt( bool fromThread ){					// called for external interrupt on specified keypad pin
/* external interrupts for GPIO pins call one of the EXTIx_IRQHandler's below:
// cycle through the keypad keys, and clear any set EXTI Pending Bits, and their NVIC pending IRQs
// then update the key state for each key, and generate any necessary wakeup events to the input handler
*/
	KEY k;
	disableInputs();
	if (RebootOnKeyInt){
		EXTI->PR = EXTI->PR;		// clear all Int Pending bits (by setting them to 1)
		NVIC_SystemReset();			// soft reboot
	}
	
	KSt.eventTS = tbTimeStamp();								// record TStamp of this interrupt
	KSt.msecSince = KSt.eventTS - KSt.lastTS;		// msec between prev & this
	KSt.lastTS 	= KSt.eventTS; 									// for next one
	dbgEvt( TB_keyIRQ, KSt.msecSince, fromThread, 0, 0 );
	
	for ( k = kHOME; k < kINVALID; k++ ){		// process key transitions
		if ( (EXTI->PR & keydef[ k ].extiBit) != 0 ){  // pending bit for this key set?
			EXTI->PR = keydef[ k ].extiBit;						// clear pending bit
			NVIC_ClearPendingIRQ( keydef[ k ].intq ); // and the corresponding interrupt
		}
		if ( updateKeyState( k ))  
			return; 		// if UP transition sent Event-- return without clearing other interrupts or re-enabling	
	}
	
	// TABLE TREE 		-- hardware forces reset
	// TABLE TREE POT -- reset with Boot0 => DFU
		
	// POT HOME -- start keytest
	// POT TABLE -- software DFU?
	if ( keydef[kPOT].down ){	// detect keytest sequence
		KSt.keytestKeysDown = keydef[kHOME].down;  		// POT HOME => keytest
		KSt.DFUkeysDown = keydef[kTABLE].down;				// POT TABLE => reboot
		if ( KSt.keytestKeysDown || KSt.DFUkeysDown ){
			osEventFlagsSet( osFlag_InpThr, KEYPAD_EVT );		// wakeup for inputThread
			return;
		}
	}
	enableInputs( fromThread );			// no detectedUpKey -- re-enable interrupts
}
// STM3210E_EVAL  EXTI ints 0 Wk, 				3 JDn, 				5-9 JCe/Key, 					10-15 Tam&JRi/JLe/JUp
// TBook_V2_Rev1	EXTI ints 0 Hom, 1 Pot, 3 Tab, 4 Plu, 5-9 Min/LHa/Sta/Cir, 	10-15 RHa/Tre
//****** TBook_V2_Rev3	EXTI ints 0 Hom, 1 Pot, 3 Tab, 4 Plu, 5-9 Min/LHa/Sta/Cir, 	10-15 RHa/Tre
// BOTH:  EXTI0 EXTI3 EXTI9_5 EXTI15_10
void 					EXTI0_IRQHandler(void){  		// call handleInterrupt( )
  handleInterrupt( false );
}
void 					EXTI3_IRQHandler(void){  		// call handleInterrupt( )
  handleInterrupt( false );
}
void 					EXTI9_5_IRQHandler(void){  	// call handleInterrupt( )
	handleInterrupt( false );
}
void 					EXTI15_10_IRQHandler(void){ // call handleInterrupt( )
	handleInterrupt( false );
}
// TBOOK_V2 ONLY:  EXTI1 EXTI4
#if defined ( TBOOK_V2 )
void 					EXTI1_IRQHandler(void){   	// call handleInterrupt( )
  handleInterrupt( false );
}
void 					EXTI4_IRQHandler(void){  		// call handleInterrupt( )
  handleInterrupt( false );
}
#endif
void					configInputKey( KEY k ){		// set up GPIO & external interrupt
		GPIO_PortClock   ( 		keydef[ k ].port, true);
		gConfigKey( keydef[ k ].id ); // low speed pulldown input
    NVIC_ClearPendingIRQ( keydef[ k ].intq );
    NVIC_EnableIRQ( 			keydef[ k ].intq );   //

		int portCode = 0;
		switch ( (int) keydef[k].port ){		// EXTICR code: which port controls EXTI for 'pin'
			case (int)GPIOA:  portCode = 0; break;
			case (int)GPIOB:  portCode = 1; break;
			case (int)GPIOC:  portCode = 2; break;
			case (int)GPIOD:  portCode = 3; break;
			case (int)GPIOE:  portCode = 4; break;
			case (int)GPIOF:  portCode = 5; break;
			case (int)GPIOG:  portCode = 6; break;
#if defined( TBOOK_V2 )
			case (int)GPIOH:  portCode = 7; break;
#endif
		}
		// AFIO->EXTICR[0..3] -- set of 4bit fields for pin#0..15
		int pin = keydef[k].pin;
		int iWd = pin >> 2, iPos = (pin & 0x3), fbit = iPos<<2;
		int msk = (0xF << fbit), val = (portCode << fbit);
		dbgLog( "A K%d %s ICR[%d] m%04x v%04x \n", k, keydef[ k ].signal, iWd, msk, val );
#if defined( STM3210E_EVAL )		
		AFIO->EXTICR[ iWd ] = ( AFIO->EXTICR[ iWd ] & ~msk ) | val;		// replace bits <fbit..fbit+3> with portCode
#endif
#if defined( TBOOK_V2 )		
		SYSCFG->EXTICR[ iWd ] = ( SYSCFG->EXTICR[ iWd ] & ~msk ) | val;		// replace bits <fbit..fbit+3> with portCode
#endif
		
		int pinBit = 1 << pin;			// bit mask for EXTI->IMR, RTSR, FTSR
		EXTI->RTSR |= pinBit; 			// Enable a rising trigger 
		EXTI->FTSR |= pinBit; 			// Enable a falling trigger 
		EXTI->IMR  |= pinBit; 			// Configure the interrupt mask 
		KeypadIMR  |= pinBit;
}
void 					initializeInterrupts(){			// configure each keypad GPIO pin to input, pull, EXTI on rising & falling, freq_low 
	KEY k;
	// using CMSIS GPIO
	for ( k = kHOME; k < kINVALID; k++ ){
		GPIO_ID id = keydef[ k ].id;			// gpio_def idx for this key
		keydef[ k ].port = gpio_def[ id ].port;
		keydef[ k ].pin  = gpio_def[ id ].pin;
		keydef[ k ].intq = gpio_def[ id ].intq;
		keydef[ k ].extiBit = 1 << keydef[ k ].pin;
		keydef[ k ].signal = gpio_def[ id ].signal;
		keydef[ k ].pressed = gpio_def[ id ].active;
		
		if ( keydef[ k ].port != NULL )  // NULL if no line configured for this key
			configInputKey( keydef[ k ].key );
	}
	// load initial state of keypad pins
	for ( k = kHOME; k < kINVALID; k++ ){
		keydef[k].down = gGet( keydef[k].id ); 
		keydef[k].tstamp = tbTimeStamp(); 
	}
	KSt.detectedUpKey = kINVALID;
	KSt.DownKey = kINVALID;
	KSt.LongPressKey = kINVALID;
/*	if ( keydef[kSTAR].down ){		// STAR held down on restart?
		if (keydef[kHOME].down ){   // STAR-HOME => USB
//			RebootToDFU();				// perform reboot into system memory to invoke Device Firmware Update bootloader
		}
		if (keydef[kPLUS].down ){   // STAR-PLUS => jump to system bootloader for DFU
		}
	}
    */


}

//
// keypadTest -- 
//    STAR-TABLE to start -- green for each new key pressed, red if duplicate, G_G_G when all have been clicked, long press to restart test
void 					resetKeypadTest(){
	for ( KEY k = kHOME; k < kINVALID; k++ )
		KTest.Status[ k ] = '_';
	KTest.Status[ kINVALID ] = 0;  // so its a string
	KTest.Count = 0;
	KTest.Active = false;
}
void					startKeypadTest(){
	resetKeypadTest();
	dbgEvt( TB_keyTest, 0, 0, 0, 0 );
	dbgLog( "Start Keypad Test \n" );
	KTest.Active = true;
	ledFg( keyTestReset ); // start a new test
}

void 					keypadTestKey( KEY evt, int dntime ) {		// verify function of keypad   
	bool longPress = dntime > TB_Config->minLongPressMS;
	if ( longPress ){
		dbgEvt( TB_ktReset, evt, dntime, 0, 0 );
		dbgLog("Kpad RESET %dms \n", dntime );
		startKeypadTest();
	} else if ( KTest.Status[ evt ]=='_' ){  // first click of this key
		KTest.Status[ evt ] = keyNm[ evt ];
		KTest.Count++;
		dbgEvt( TB_ktFirst, evt, dntime, 0, 0 );
		dbgLog("T: %d %s %d\n", KTest.Count, KTest.Status, dntime );
		if ( KTest.Count == NUM_KEYPADS ){
			dbgEvt( TB_ktDone, evt, dntime, 0, 0 );
			dbgLog("Keypad test ok! \n" );
			ledFg( keyTestSuccess ); 
			KTest.Active = false;			// test complete
		} else 
			ledFg( keyTestGood ); 
	} else {
		dbgEvt( TB_ktRepeat, evt, dntime, 0, 0 );
		ledFg( keyTestBad );
	}
}
// 
// inputManager-- manager thread
void 					inputThread( void *arg ){			// converts signals from keypad ISR's to events on controlManager queue
	dbgLog( "4 inThr: 0x%x 0x%x \n", &arg, &arg + INPUT_STACK_SIZE );
	Event eTyp;
	while (true){
		uint32_t wkup = osEventFlagsWait( osFlag_InpThr, KEYPAD_EVT, osFlagsWaitAny, osWaitForever );
		dbgEvt( TB_keyWk, wkup, 0, 0, 0 );
		if ( wkup == KEYPAD_EVT ){  
			if ( KSt.keytestKeysDown && !KTest.Active) // start a new test
				startKeypadTest();	
	
			if ( KSt.DFUkeysDown ){
				dbgEvt( TB_keyDFU, 0, 0, 0, 0 );
				sendEvent( FirmwareUpdate, 0 );
			}
			
			if ( KSt.detectedUpKey != kINVALID ){		// keyUp transition on detectedUpKey
				int dntime = keydef[ KSt.detectedUpKey ].dntime;
				if ( dntime < TB_Config->minShortPressMS ){	// ignore (de-bounce) if down less than this 
					dbgEvt( TB_keyBnc, KSt.detectedUpKey, dntime, 0, 0 );
				} else {
					if ( KSt.starDown && KSt.detectedUpKey!=kSTAR ){  //  <STAR-x> xUp transition (ignore duration)
						KSt.starAltUsed = true;			// prevent LONG_PRESS for Star used as Alt
						dbgEvt( TB_keyStar, KSt.detectedUpKey, dntime, 0, 0 );
						eTyp = toStarEvt( KSt.detectedUpKey );
					} else if ( dntime > TB_Config->minLongPressMS ){	// LONGPRESS if down more longer than this
						dbgEvt( TB_keyLong, KSt.detectedUpKey, dntime, 0, 0 );
						eTyp = toLongEvt( KSt.detectedUpKey );
					} else { // short press
						dbgEvt( TB_keyShort, KSt.detectedUpKey, dntime, 0, 0 );
						eTyp = toShortEvt( KSt.detectedUpKey );
					}
					if ( KTest.Active ){
						keypadTestKey( KSt.detectedUpKey, dntime );
					} else {
						dbgLog( "A key: %s\n", eventNm( eTyp ));
						sendEvent( eTyp, dntime );			// add event to queue
					}
				}
			}
		}
		KSt.detectedUpKey = kINVALID;
		handleInterrupt( true );	// re-call INT in case there were other pending keypad transitions
	}
}
// 
//PUBLIC:  inputManager-- API functions
void 					initInputManager( void ){ 			// initializes keypad & starts thread
	initializeInterrupts();
	Dbg.KeyTest = &KTest;
	Dbg.KeyPadStatus = &KSt;
	Dbg.KeyPadDef = &keydef;
	Dbg.TBookConfig = TB_Config;
	resetKeypadTest();
	
	osFlag_InpThr = osEventFlagsNew(NULL);	// os flag ID -- used so ISR can wakeup inputThread
	if ( osFlag_InpThr == NULL)
		tbErr( "osFlag_InpThr alloc failed" );	

	osMsg_TBEvents = osMessageQueueNew( TB_EVT_QSIZE, sizeof(TB_Event), NULL );
	if ( osMsg_TBEvents==NULL ) 
		tbErr( "failed to create TB_Event queue" );

	TBEvent_pool = osMemoryPoolNew( 8, sizeof(TB_Event), NULL );
	if ( TBEvent_pool==NULL ) 
		tbErr( "failed to create TB_Event pool" );
	
	thread_attr.name = "input_thread";
	thread_attr.stack_size = INPUT_STACK_SIZE;
	Dbg.thread[4] = (osRtxThread_t *) osThreadNew( inputThread, NULL, &thread_attr );
	if ( Dbg.thread[4] == NULL )
		tbErr( "inputThread spawn failed" );	

	osTimerAttr_t 	timer_attr;
	timer_attr.name = "Key Timer";
	timer_attr.attr_bits = 0;
	timer_attr.cb_mem = 0;
	timer_attr.cb_size = 0;
	keyDownTimer = osTimerNew(	checkKeyTimer, osTimerOnce, 0, &timer_attr );
	if ( keyDownTimer == NULL ) 
		tbErr( "keyDownTimer not alloc'd" );
	
//	osTimerStart( keyDownTimer, currPwrTimerMS );
//	osTimerStop( keyDownTimer );

	//PowerManager::getInstance()->registerPowerEventHandler( handlePowerEvent );
	//registerPowerEventHandler( handlePowerEvent );	
	dbgLog( "4 InputMgr OK \n" );
}
void 					sendEvent( Event key, int32_t arg ){	// log & send TB_Event to CSM
	if ( key==eNull || key==anyKey || key==eUNDEF || (int)key<0 || (int)key>(int)eUNDEF )
		tbErr( "bad event" );
	if (TBEvent_pool==NULL) return; //DEBUG
	dbgEvt( TB_keyEvt, key, arg, 0, 0 );
	
	TB_Event *evt = (TB_Event *)osMemoryPoolAlloc( TBEvent_pool, osWaitForever );
	evt->typ = key;
	evt->arg = arg;
	if ( osMessageQueuePut( osMsg_TBEvents, &evt, 0, 0 ) != osOK )	// Priority 0, no wait
		tbErr( "failed to enQ tbEvent" );
}
void 					keyPadPowerUp( void ){					// re-initialize keypad GPIOs
	initializeInterrupts();
}
void 					keyPadPowerDown( void ){				// shut down keypad GPIOs
	for (int k = 0; k < count(keydef); k++ ){   
		if ( k != kMINUS && k != kLHAND ){		// leave as WakeUp??
			gUnconfig( keydef[ k ].id );
		}			
	}
}


//end inputmanager.cpp



