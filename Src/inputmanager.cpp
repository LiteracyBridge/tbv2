// TBookV2  inputmanager.c
//   Apr 2018

#include "tbook.h"
#include "main.h"
#include "inputmanager.h"
#include "logger.h"   // logEvt
#include "tb_evr.h"

// Define a postfix ++  operator for KEY
KEY& operator++( KEY &k , int ) {
    return k = static_cast<KEY>( static_cast<int>(k) + 1 );
}

const int KEYPAD_EVT   = 1;
const int TB_EVT_QSIZE = 4;
const int TB_KEY_QSIZE = 4;

#define count( x ) sizeof(x)/sizeof(x[0])

osMessageQueueId_t      osMsg_TBEvents = NULL;     // os message queue id  for TB_Events
osMemoryPoolId_t        TBEvent_pool   = NULL;   // memory pool for TBEvents

osMessageQueueId_t      osMsg_KeyTransitions = NULL;     // os message queue id for KeyTransitions
osMemoryPoolId_t        KeyTransition_pool   = NULL;   // memory pool for KeyTransitions

static osThreadAttr_t   thread_attr;
static osEventFlagsId_t osFlag_InpThr;    // osFlag to signal a keyUp event from ISR
static osTimerId_t      keyDownTimer = NULL;
uint16_t                KeypadIMR    = 0;          // interrupt mask register bits for keypad (also set in main() for MarcDebug)

// structs defined in tbook.h for Debugging visibility
KeyPadState_t           KSt;
KeyTestState_t          KTest;

const char *keyName[] = { "k0H", "k1C", "k2+", "k3-", "k4T", "k5P", "k6S", "k7t", "k8", "k9", "k10I" };
const char keyNm[] = { 'H', 'C', '+', '-', 'T', 'L', 'R', 'P', 'S', 't', 'I', '=' };  // used for Debug ouput
const char *keyTestGood    = "GgGg_";
const char *keyTestBad     = "RrRr_";
const char *keyTestReset   = "R8_3 R8_3 R8_3";  // 3 long red pulses
const char *keyTestSuccess = "G8R_3 G8R_3 G8R_3";  // 3 long green then red pulses


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

KeyPadKey_t keydef[10] = {
    // @formatter: off
    // keypad GPIO in order: kHOME, kCIRCLE, KEY::PLUS, KEY::MINUS, KEY::TREE, KEY::LHAND, KEY::POT, KEY::RHAND, KEY::STAR, KEY::TABLE
    // static placeholders, filled in by initializeInterrupts
    //  GPIO_ID    key,    port,    pin,         intq       extiBit   signal    pressed  down   tstamp dntime
    { gHOME,   KEY::HOME,   NULL, 0, WWDG_IRQn, 0, "", 0, false, 0, 0 },
    { gCIRCLE, KEY::CIRCLE, NULL, 0, WWDG_IRQn, 0, "", 0, false, 0, 0 },
    { gPLUS,   KEY::PLUS,   NULL, 0, WWDG_IRQn, 0, "", 0, false, 0, 0 },
    { gMINUS,  KEY::MINUS,  NULL, 0, WWDG_IRQn, 0, "", 0, false, 0, 0 },
    { gTREE,   KEY::TREE,   NULL, 0, WWDG_IRQn, 0, "", 0, false, 0, 0 },
    { gLHAND,  KEY::LHAND,  NULL, 0, WWDG_IRQn, 0, "", 0, false, 0, 0 },
    { gRHAND,  KEY::RHAND,  NULL, 0, WWDG_IRQn, 0, "", 0, false, 0, 0 },
    { gPOT,    KEY::POT,    NULL, 0, WWDG_IRQn, 0, "", 0, false, 0, 0 },
    { gSTAR,   KEY::STAR,   NULL, 0, WWDG_IRQn, 0, "", 0, false, 0, 0 },
    { gTABLE,  KEY::TABLE,  NULL, 0, WWDG_IRQn, 0, "", 0, false, 0, 0 }
    // @formatter: on
};
void handleInterrupt( bool fromThread );   // forward
// 
// inputManager-- Interrupt handling
void disableInputs() {            // disable all keypad GPIO interrupts
    EXTI->IMR &= ~KeypadIMR;
}

void enableInputs( bool fromThread ) {              // check for any unprocessed Interrupt Mask Register from 'imr'

    // I think the following problem occurs when a key change triggers an interrupt, but then the key changes back before
    // the ISR has finished -- so the 2nd transition never triggers an interrupt.
    //
    //BUG CHECK-- sometimes this is called when the keydef[].down state doesn't match the IDR state
    // which can happen if a transitions occurs while interrupts are disabled -- seems more common than one would expect
    // AND new interrupt doesn't happen when re-enabled! 
    for (KEY k = KEY::HOME; k < KEY::INVALID; k++) {   // process key transitions
        //    int idr = keydef[k].port->IDR;    // port Input data register
        bool kdn = GPIO::getLogical( keydef[(int)k].id );  // gets LOGICAL value of port/pin
        if ( kdn != keydef[(int)k].down ) {     // should == current state
            int pendR = EXTI->PR;
            int iw = keydef[(int)k].intq >> 5UL;
            int irq = NVIC->ICPR[iw];
            dbgEvt( TBkeyMismatch, (int)k, ( fromThread << 8 ) + kdn, pendR, irq );
            //if (irq != 0 || pendR!= 0)
            //  dbgLog( "! pR%04x ICPR[%d]%04x \n", pendR, iw, irq );
            dbgLog( "A %s K%c%c %dDn pR%x irq%x \n", fromThread ? "*" : "I", keyNm[(int)k], keydef[(int)k].down ? 'd' : 'u',
                    KSt.downCnt, pendR, irq );

            handleInterrupt( true );    // re-invoke, to process missed transition
            return;
        }
    }
    EXTI->IMR |= KeypadIMR;   // re-enable keypad EXTI interrupts
}

/*
 * Disables interrupts for keys, prior to sleeping, to limit the keys that can
 * cause an interrupt. Expected usage is to disable all but House. Before sleeping
 * it is a very good idea to disable the Bowl, because that will silently boot
 * to DFU mode, likely causing confusion.
 *
 * KEYS_MASK keysToRemainEnabled: A mask of KEYS_MASKs for the keys NOT to be
 *     disabled.
 */
void disableKeyInterrupts( KEYS_MASK keysToRemainEnabled ) {
    // iterate over the membrane switch (keypad) keys
    for (KEY k = KEY::HOME; k < KEY::INVALID; k++) {
        if ((( 1 << k ) & keysToRemainEnabled ) == 0 ) {
            int pin    = keydef[k].pin;
            int pinBit = 1 << pin;      // bit mask for EXTI->IMR, RTSR, FTSR
            EXTI->RTSR &= ~pinBit;      // Enable a rising trigger
            EXTI->FTSR &= ~pinBit;      // Enable a falling trigger
            EXTI->IMR &= ~pinBit;      // Configure the interrupt mask
            KeypadIMR &= ~pinBit;
        }
    }
    EXTI->IMR &= ~KeypadIMR;    // disable keypad EXTI interrupts for the specified keys
}

extern bool RebootOnKeyInt;   // from powermanager -- reboot if interrupt

void sendKeyTran( KEY k, uint32_t ts, bool down ) {   // send transition message to input thread
    TB_Key *trn = (TB_Key *) osMemoryPoolAlloc( KeyTransition_pool, 0 );
    if ( trn == NULL ) tbErr( "out of KeyTransitions" );
    trn->k      = k;
    trn->down   = down;
    trn->tstamp = ts;

    if ( osMessageQueuePut( osMsg_KeyTransitions, &trn, 0, 0 ) != osOK )  // Priority 0, no wait
        tbErr( "failed to enQ key transition" );
}

void checkKeyTimer( void *arg ) {  // called by OS when keyDownTimer expires ==> key down for minLongPressMS
    osTimerStop( keyDownTimer );  // reset necessary?
    sendKeyTran( KEY::TIMER, tbTimeStamp(), false );  // treat similarly to key up
}

void handleInterrupt( bool fromThread ) {         // called for external interrupt on specified keypad pin
    /* external interrupts for GPIO pins call one of the EXTIx_IRQHandler's below:
    // cycle through the keypad keys, and clear any set EXTI Pending Bits, and their NVIC pending IRQs
    // then update the key state for each key, and generate any necessary wakeup events to the input handler
    */
    KEY k;
    disableInputs();
    if ( RebootOnKeyInt ) {
        // clear all Int Pending bits. Pending interrupts are cleared by writing 1 to their bit, and pending interrupts
        // are marked with a 1 in their bit, so this clears exactly the pending interrupts.
        EXTI->PR = EXTI->PR;
        NVIC_SystemReset();     // soft reboot
    }

    KSt.eventTS   = tbTimeStamp();                // record TStamp of this interrupt
    KSt.msecSince = KSt.eventTS - KSt.lastTS;   // msec between prev & this
    KSt.lastTS    = KSt.eventTS;                  // for next one
    dbgEvt( TB_keyIRQ, KSt.msecSince, fromThread, 0, 0 );

    int downCnt = 0;
    for (k      = KEY::HOME; k < KEY::INVALID; k++) {   // process key transitions
        if (( EXTI->PR & keydef[k].extiBit ) != 0 ) {  // pending bit for this key set?
            EXTI->PR = keydef[k].extiBit;           // clear pending bit
            NVIC_ClearPendingIRQ( keydef[k].intq ); // and the corresponding interrupt
        }

        bool keydown = GPIO::getLogical( keydef[k].id );
        KSt.keyState[k] = keydown ? keyNm[k] : '_';  //DEBUG
        if ( keydown ) downCnt++;
        if ( keydef[k].down != keydown ) {  // transition occurred
            keydef[k].down = keydown;   // remember new key state
            sendKeyTran( k, KSt.eventTS, keydown );
        }
    }
    KSt.downCnt = downCnt;

    // TABLE TREE     -- hardware forces reset
    // TABLE TREE POT -- reset with Boot0 => DFU

    // POT HOME -- start keytest
    // POT TABLE -- software DFU?
    /*
    if ( keydef[KEY::POT].down ){ // detect keytest sequence
      KSt.keytestKeysDown = keydef[KEY::HOME].down;     // POT HOME => keytest
      KSt.DFUkeysDown = keydef[KEY::TABLE].down;        // POT TABLE => reboot
      if ( KSt.keytestKeysDown || KSt.DFUkeysDown ){
        osEventFlagsSet( osFlag_InpThr, KEYPAD_EVT );   // wakeup for inputThread
        return;
    }
    */
    enableInputs( fromThread );     // no detectedUpKey -- re-enable interrupts
}

//****** TBook_V2_Rev3  EXTI ints 0 Hom, 1 Pot, 3 Tab, 4 Plu, 5-9 Min/LHa/Sta/Cir,  10-15 RHa/Tre
// BOTH:  EXTI0 EXTI3 EXTI9_5 EXTI15_10
extern "C" {
void EXTI0_IRQHandler(void) {
    handleInterrupt(false);
}

void EXTI3_IRQHandler(void) {
    handleInterrupt(false);
}

void EXTI9_5_IRQHandler(void) {
    handleInterrupt(false);
}

void EXTI15_10_IRQHandler(void) {
    handleInterrupt(false);
}

void EXTI1_IRQHandler(void) {
    handleInterrupt(false);
}

void EXTI4_IRQHandler(void) {
    handleInterrupt(false);
}
}
/*
 * Configure interrupts on the STM32 line for a given membrane switch key.
 */
void configInputKey( KEY k ) {    // set up GPIO & external interrupt
    GPIO::setPortClockEnabledState( keydef[k].port, true );
    GPIO::configureKey( keydef[k].id ); // low speed pulldown input
    NVIC_ClearPendingIRQ( keydef[k].intq );
    NVIC_EnableIRQ( keydef[k].intq );   //

    int portCode = 0;
    GPIO_TypeDef *port = keydef[k].port;
    // @formatter: off
    if (port == GPIOA) portCode=0;
    else if (port == GPIOB) portCode=1;
    else if (port == GPIOC) portCode=2;
    else if (port == GPIOD) portCode=3;
    else if (port == GPIOE) portCode=4;
    else if (port == GPIOF) portCode=5;
    else if (port == GPIOG) portCode=6;
    else if (port == GPIOH) portCode=7;
    // @formatter: on
    
    // AFIO->EXTICR[0..3] -- set of 4bit fields for pin#0..15
    int pin = keydef[k].pin;
    int iWd = pin >> 2, iPos = ( pin & 0x3 ), fbit = iPos << 2;
    int msk = ( 0xF << fbit ), val = ( portCode << fbit );
    //  dbgLog( "A K%d %s ICR[%d] m%04x v%04x \n", k, keydef[ k ].signal, iWd, msk, val );
    SYSCFG->EXTICR[ iWd ] = ( SYSCFG->EXTICR[ iWd ] & ~msk ) | val;   // replace bits <fbit..fbit+3> with portCode

    int pinBit = 1 << pin;      // bit mask for EXTI->IMR, RTSR, FTSR
    EXTI->RTSR |= pinBit;       // Enable a rising trigger 
    EXTI->FTSR |= pinBit;       // Enable a falling trigger 
    EXTI->IMR |= pinBit;       // Configure the interrupt mask
    KeypadIMR |= pinBit;
}

/*
 * Builds the data structures used to manage interrupts
 */
void initializeInterrupts() {     // configure each keypad GPIO pin to input, pull, EXTI on rising & falling, freq_low
    KEY k;
    // using CMSIS GPIO
    // iterate over the membrane switch (keypad) keys
    for (k = KEY::HOME; k < KEY::INVALID; k++) {
        GPIO_ID id = keydef[k].id;      // GPIO::gpio_def idx for this key
        keydef[k].port    = GPIO::gpio_def[id].port;
        keydef[k].pin     = GPIO::gpio_def[id].bit;
        keydef[k].intq    = GPIO::gpio_def[id].intq;
        keydef[k].extiBit = 1 << keydef[k].pin;
        keydef[k].signal  = GPIO::gpio_def[id].signal;
        keydef[k].pressed = GPIO::gpio_def[id].active;

        if ( keydef[k].port != NULL )  // NULL if no line configured for this key
            configInputKey( keydef[k].key );
    }
    // load initial state of keypad pins
    for (k           = KEY::HOME; k < KEY::INVALID; k++) {
        keydef[k].down   = GPIO::getLogical( keydef[k].id );
        keydef[k].tstamp = tbTimeStamp();
    }
    KSt.firstDown    = KEY::INVALID;
    KSt.secondDown   = KEY::INVALID;
    KSt.multipleDown = false;
    KSt.starUsed     = false;

    //  KSt.detectedUpKey = KEY::INVALID;
    //  KSt.DownKey = KEY::INVALID;
    //  KSt.LongPressKey = KEY::INVALID;
    /*  if ( keydef[KEY::STAR].down ){    // STAR held down on restart?
        if (keydef[kHOME].down ){   // STAR-HOME => USB
    //      RebootToDFU();        // perform reboot into system memory to invoke Device Firmware Update bootloader
        }
        if (keydef[kPLUS].down ){   // STAR-PLUS => jump to system bootloader for DFU
        }
      }
        */


}

//
// keypadTest -- 
//     -- green for each new key pressed, red if duplicate, G_G_G when all have been clicked, long press to restart test
/*void          resetKeypadTest(){
  for ( KEY k = KEY::HOME; k < KEY::INVALID; k++ )
    KTest.Status[ k ] = '_';
  KTest.Status[ KEY::INVALID ] = 0;  // so its a string
  KTest.Count = 0;
  KTest.Active = false;
}
void          startKeypadTest(){
  resetKeypadTest();
  dbgEvt( TB_keyTest, 0, 0, 0, 0 );
  dbgLog( "Start Keypad Test \n" );
  KTest.Active = true;
  ledFg( keyTestReset ); // start a new test
}

void          keypadTestKey( KEY evt, int dntime ) {    // verify function of keypad   
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
      KTest.Active = false;     // test complete
    } else 
      ledFg( keyTestGood ); 
  } else {
    dbgEvt( TB_ktRepeat, evt, dntime, 0, 0 );
    ledFg( keyTestBad );
  }
} */

static bool KeypadTestActive = false;

//
// inputThread-- thread to process key transitions & timeouts => TBook Event messages
void inputThread( void *arg ) {     // converts TB_Key msgs from keypad ISR's to TB Event msgs to controlManager queue
    dbgLog( "4 inThr: 0x%x 0x%x \n", &arg, &arg + INPUT_STACK_SIZE );
    TB_Key *transition;
    while (true) {
        transition        = NULL;
        // wait for key transition or timeout
        osStatus_t status = osMessageQueueGet( osMsg_KeyTransitions, &transition, NULL, osWaitForever );
        if ( status != osOK )
            tbErr( "keyTransition QGet error" );
        KEY      k       = transition->k;
        uint32_t ts      = transition->tstamp;
        bool     keydown = transition->down;
        osMemoryPoolFree( KeyTransition_pool, transition );

        dbgLog( "A keyTr: %c%c %d %dDn 1:%c 2:%c %c%c \n", keyNm[k], keydown ? 'v' : '^', ts, KSt.downCnt,
                keyNm[KSt.firstDown], keyNm[KSt.secondDown], KSt.multipleDown ? '_' : 'M', KSt.starUsed ? '_' : 'S' );
        if (( KSt.keyState[KEY::POT] != '_' ) && ( KSt.keyState[KEY::HOME] != '_' )) {
            KeypadTestActive = !KeypadTestActive;
            dbgLog( "A KeypadTestMode %s \n", KeypadTestActive ? "on" : "off" );
        }

        int dntime = 0;
        if ( keydown ) {   //****************** KEY DOWN TRANSITION
            if ( KSt.firstDown == KEY::INVALID ) {
                KSt.firstDown   = k;
                KSt.firstDownTS = ts;
                osTimerStart( keyDownTimer, TB_Config->minLongPressMS );  // start long press timer
            } else if ( KSt.secondDown == KEY::INVALID ) {
                KSt.secondDown   = k;
                KSt.secondDownTS = ts;
                if ( KSt.firstDown == KEY::STAR )
                    osTimerStop( keyDownTimer );  // cancel Star__ timer, to allow StarX events
            } else
                KSt.multipleDown = true;
        } else if ( !KSt.multipleDown ) {
            // ignore everything until all keys are up
            //****************** KEY UP TRANSITION
            if (( k == KEY::STAR ) && KSt.starUsed ) {  // ignore Star up after StarX was sent
                KSt.starUsed  = false;
                KSt.firstDown = KEY::INVALID;
            } else if ( k == KEY::TIMER ) {  // pseudo up for longPress
                if ( KSt.firstDown == KEY::INVALID )
                    dbgLog( "bad timer state\n" );
                else {
                    sendEvent( toLongEvt( KSt.firstDown ), ts - KSt.firstDownTS );      // add event to queue
                    KSt.firstDown = KEY::INVALID;    // so actual key-up will be ignored
                }
            } else if (( k == KSt.firstDown ) &&
                       ( KSt.secondDown == KEY::INVALID )) {   // only one key was down & it came up
                osTimerStop( keyDownTimer );  // cancel long press timer
                dntime = ts - KSt.firstDownTS;
                if ( dntime >= TB_Config->minShortPressMS ) // legal click
                    sendEvent( toShortEvt( KSt.firstDown ), dntime );  // so send TB Event
                KSt.firstDown = KEY::INVALID;   // reset to all keys up
            } else if (( KSt.firstDown == KEY::STAR ) && ( k == KSt.secondDown )) {  // star-K & K came up
                dntime = ts - KSt.secondDownTS;
                if ( dntime >= TB_Config->minShortPressMS ) // legal click
                    sendEvent( toStarEvt( KSt.secondDown ), dntime );  // so send TB star-Event
                KSt.starUsed   = true;          // so star-up will be ignored
                KSt.secondDown = KEY::INVALID;    // back to just STAR down
            }
        }
        if ( KSt.downCnt == 0 ) {  // reset state
            KSt.firstDown    = KEY::INVALID;
            KSt.secondDown   = KEY::INVALID;
            KSt.multipleDown = false;
        }
        // dbgLog( "A %dDn 1K:%c 2K:%c mK:%c sU:%c dnT:%d \n", KSt.downCnt, keyNm[KSt.firstDown], keyNm[KSt.secondDown], KSt.multipleDown? 'T':'f', KSt.starUsed? 'T':'f', dntime );
    } //while
    /*
    //    uint32_t wkup = osEventFlagsWait( osFlag_InpThr, KEYPAD_EVT, osFlagsWaitAny, osWaitForever );
    //    dbgEvt( TB_keyWk, wkup, 0, 0, 0 );
        if ( wkup == KEYPAD_EVT ){
          if ( KSt.keytestKeysDown && !KTest.Active) // start a new test
            startKeypadTest();

          if ( KSt.DFUkeysDown ){
            dbgEvt( TB_keyDFU, 0, 0, 0, 0 );
            sendEvent( FirmwareUpdate, 0 );
          }

          if ( KSt.detectedUpKey != KEY::INVALID ){   // keyUp transition on detectedUpKey
            int dntime = keydef[ KSt.detectedUpKey ].dntime;
            if ( dntime < TB_Config->minShortPressMS ){ // ignore (de-bounce) if down less than this
              dbgEvt( TB_keyBnc, KSt.detectedUpKey, dntime, 0, 0 );
            } else {
              if ( KSt.starDown && KSt.detectedUpKey!=KEY::STAR ){  //  <STAR-x> xUp transition (ignore duration)
                KSt.starAltUsed = true;     // prevent LONG_PRESS for Star used as Alt
                dbgEvt( TB_keyStar, KSt.detectedUpKey, dntime, 0, 0 );
                eTyp = toStarEvt( KSt.detectedUpKey );
              } else if ( dntime > TB_Config->minLongPressMS ){ // LONGPRESS if down more longer than this
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
                sendEvent( eTyp, dntime );      // add event to queue
              }
            }
          }
        }
        KSt.detectedUpKey = KEY::INVALID;
        handleInterrupt( true );  // re-call INT in case there were other pending keypad transitions
      } */
}
/*
void          oldinputThread( void *arg ){      // converts signals from keypad ISR's to events on controlManager queue
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
      
      if ( KSt.detectedUpKey != KEY::INVALID ){   // keyUp transition on detectedUpKey
        int dntime = keydef[ KSt.detectedUpKey ].dntime;
        if ( dntime < TB_Config->minShortPressMS ){ // ignore (de-bounce) if down less than this 
          dbgEvt( TB_keyBnc, KSt.detectedUpKey, dntime, 0, 0 );
        } else {
          if ( KSt.starDown && KSt.detectedUpKey!=KEY::STAR ){  //  <STAR-x> xUp transition (ignore duration)
            KSt.starAltUsed = true;     // prevent LONG_PRESS for Star used as Alt
            dbgEvt( TB_keyStar, KSt.detectedUpKey, dntime, 0, 0 );
            eTyp = toStarEvt( KSt.detectedUpKey );
          } else if ( dntime > TB_Config->minLongPressMS ){ // LONGPRESS if down more longer than this
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
            sendEvent( eTyp, dntime );      // add event to queue
          }
        }
      }
    }
    KSt.detectedUpKey = KEY::INVALID;
    handleInterrupt( true );  // re-call INT in case there were other pending keypad transitions
  }
} */
// 
//PUBLIC:  inputManager-- API functions
void initInputManager( void ) {       // initializes keypad & starts thread
    initializeInterrupts();
    Dbg.KeyTest      = &KTest;
    Dbg.KeyPadStatus = &KSt;
    Dbg.KeyPadDef    = &keydef;
    Dbg.TBookConfig  = TB_Config;
    //  resetKeypadTest();

    osFlag_InpThr = osEventFlagsNew( NULL );  // os flag ID -- used so ISR can wakeup inputThread
    if ( osFlag_InpThr == NULL )
        tbErr( "osFlag_InpThr alloc failed" );

    // set up queue for key transition messages from ISR to inputThread
    osMsg_KeyTransitions = osMessageQueueNew( TB_KEY_QSIZE, sizeof( TB_Key ), NULL );
    if ( osMsg_KeyTransitions == NULL )
        tbErr( "failed to create KeyTransition queue" );

    KeyTransition_pool = osMemoryPoolNew( 8, sizeof( TB_Key ), NULL );
    if ( KeyTransition_pool == NULL )
        tbErr( "failed to create KeyTransition pool" );

    // and set up queue for TBook Events from inputThread to controlManager
    osMsg_TBEvents = osMessageQueueNew( TB_EVT_QSIZE, sizeof( TB_Event ), NULL );
    if ( osMsg_TBEvents == NULL )
        tbErr( "failed to create TB_Event queue" );

    TBEvent_pool = osMemoryPoolNew( 8, sizeof( TB_Event ), NULL );
    if ( TBEvent_pool == NULL )
        tbErr( "failed to create TB_Event pool" );

    thread_attr.name       = "input_thread";
    thread_attr.stack_size = INPUT_STACK_SIZE;
    Dbg.thread[4] = (osRtxThread_t *) osThreadNew( inputThread, NULL, &thread_attr );
    if ( Dbg.thread[4] == NULL )
        tbErr( "inputThread spawn failed" );

    osTimerAttr_t timer_attr;
    timer_attr.name      = "Key Timer";
    timer_attr.attr_bits = 0;
    timer_attr.cb_mem    = 0;
    timer_attr.cb_size   = 0;
    keyDownTimer = osTimerNew( checkKeyTimer, osTimerOnce, 0, &timer_attr );
    if ( keyDownTimer == NULL )
        tbErr( "keyDownTimer not alloc'd" );

    //  osTimerStart( keyDownTimer, currPwrTimerMS );
    //  osTimerStop( keyDownTimer );

    //PowerManager::getInstance()->registerPowerEventHandler( handlePowerEvent );
    //registerPowerEventHandler( handlePowerEvent );
    dbgLog( "4 InputMgr OK \n" );
}

void sendEvent( Event key, int32_t arg ) {  // log & send TB_Event to CSM
    if ( key == eNull || key == anyKey || key == eUNDEF || (int) key < 0 || (int) key > (int) eUNDEF )
        tbErr( "bad event" );
    if ( TBEvent_pool == NULL ) return; //DEBUG
    if ( !controlManagerReady ) {
        dbgLog( "enqueue event, csm not ready" );
        return;
    }
    dbgEvt( TB_keyEvt, key, arg, 0, 0 );
    dbgLog( "A Evt: %s %d \n", eventNm( key ), arg );

    if ( KeypadTestActive ) return;

    TB_Event *evt = (TB_Event *) osMemoryPoolAlloc( TBEvent_pool, osWaitForever );
    evt->typ = key;
    evt->arg = arg;
    osStatus_t result;
    // send with Priority 0, no wait
    if (( result = osMessageQueuePut( osMsg_TBEvents, &evt, 0, 0 )) != osOK ) {
        printf( "failed to enQ tbEvent: %d \n", result );
        tbErr( "failed to enQ tbEvent" );
    }
}

//// TODO: UNUSED
//void keyPadPowerUp( void ) {          // re-initialize keypad GPIOs
//    initializeInterrupts();
//}
//
//// TODO: UNUSED
//void keyPadPowerDown( void ) {        // shut down keypad GPIOs
//    for (int k = 0; k < count( keydef ); k++) {
//        if ( k != KEY::MINUS && k != KEY::LHAND ) {    // leave as WakeUp??
//            GPIO::unConfigure( keydef[k].id );
//        }
//    }
//}


//end inputmanager.cpp



