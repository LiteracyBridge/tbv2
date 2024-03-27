#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include "csm.h"
#include  "tbook.h"

// TalkingBook keypad has 10 keys, each connected to a line of the keypad cable
//  plus KEY::INVALID for no key value, & KEY::TIMER for long press timer elapsed
typedef enum ENUM_KEY {
     HOUSE = 0, CIRCLE, PLUS, MINUS, TREE, LHAND, RHAND, BOWL, STAR, TABLE, INVALID, TIMER
} KEY;

#if defined __cplusplus
// Mask bits for keys, so they can be or'd together.
constexpr int operator<<(int v, enum ENUM_KEY s) {
    return v << static_cast<int>(s);
}
#endif

typedef enum {
    KM_HOUSE  = 1 << KEY::HOUSE,
    KM_CIRCLE = 1 << KEY::CIRCLE,
    KM_PLUS   = 1 << KEY::PLUS,
    KM_MINUS  = 1 << KEY::MINUS,
    KM_TREE   = 1 << KEY::TREE,
    KM_LHAND  = 1 << KEY::LHAND,
    KM_RHAND  = 1 << KEY::RHAND,
    KM_BOWL   = 1 << KEY::BOWL,
    KM_STAR   = 1 << KEY::STAR,
    KM_TABLE  = 1 << KEY::TABLE
} KEYS_MASK;

typedef struct { // KeyPadKey -- assignments to GPIO pins, in inputManager.cpp
    GPIO_ID      id;     // idx in gpio_def
    KEY          key;    // KEYPAD key connected to this line
    GPIO_TypeDef *port;  // specifies GPIO port
    uint16_t     pin;    // specifies GPIO pin within port
    IRQn_Type    intq;      // specifies INTQ mask
    uint16_t     extiBit;   // mask bit == 1 << pin
    const char * signal;    // string name of GPIO
    int          pressed;   // GPIO input state when pressed

    //  in order to destinguish between SHORT and LONG press,  keep track of the last transition of each keypad line
    bool     down;     // true if key is currently depressed
    uint32_t tstamp;   // tstamp of down transition
    uint32_t dntime;   // num tbTicks it was down

} KeyPadKey_t;

typedef KeyPadKey_t KeyPadKey_arr[10];   // so Dbg knows array size
extern KeyPadKey_t  keydef[10];

typedef struct {  // TB_Key --  msg for key Q from ISR to InputThread
    KEY      k;         // keyboard key that changed (or KEY::TIMER)
    bool     down;      // down transition
    uint32_t tstamp;    // timestamp
} TB_Key;

typedef struct {  // TB_Event --  event & downMS for event Q
    CSM_EVENT    eventId;
    uint32_t arg;
} TB_Event;

// At one time, these didn't compile with compiler6 -O0 (no optimization); macros below are workaround.
inline bool TB_isShort( TB_Event evt ) { return evt.eventId >= House && evt.eventId <= Table; }
inline bool TB_isLong( TB_Event evt ) { return evt.eventId >= House__ && evt.eventId <= Table__; }
inline bool TB_isStar( TB_Event evt ) { return evt.eventId >= starHouse && evt.eventId <= starTable; }
inline bool TB_isSystem( TB_Event evt ) { return evt.eventId >= AudioDone && evt.eventId <= eUNDEF; }
inline CSM_EVENT toShortEvt( KEY k ) { return (CSM_EVENT) ((int) k + House ); }
inline CSM_EVENT toLongEvt( KEY k ) { return (CSM_EVENT) ((int) k + House__ ); }
inline CSM_EVENT toStarEvt( KEY k ) { return (CSM_EVENT) ((int) k + starHouse ); }

//#define TB_isShort( evt ) ( evt.eventId >= House && evt.eventId <= Table )
//#define TB_isLong( evt ) ( evt.eventId >= House__ && evt.eventId <= Table__ )
//#define TB_isStar( evt ) ( evt.eventId >= starHouse && evt.eventId <= starTable )
//#define TB_isSystem( evt ) ( evt.eventId >= AudioDone && evt.eventId <= eUNDEF )
//#define toShortEvt( k ) ( (CSM_EVENT) ((int) k + House ) )
//#define toLongEvt( k ) ( (CSM_EVENT) ((int) k + House__ ) )
//#define toStarEvt( k ) ( (CSM_EVENT) ((int) k + starHouse ) )


extern void initInputManager( void );
extern void disableKeyInterrupts( KEYS_MASK keysToRemainEnabled );
extern void keyPadPowerDown( void );            // shut down keypad GPIOs
extern void keyPadPowerUp( void );              // re-initialize keypad GPIOs
extern void sendEvent( CSM_EVENT, int32_t );

extern osMessageQueueId_t osMsg_TBEvents;
extern osMemoryPoolId_t   TBEvent_pool;   // memory pool for TBEvents

#endif    //INPUT_MANAGER_H

