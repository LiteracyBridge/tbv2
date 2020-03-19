#ifndef INPUT_MGR_H
#define INPUT_MGR_H

#include	"tbook.h"

// TalkingBook keypad has 10 keys, each connected to a line of the keypad cable
typedef enum {	kHOME=0, kCIRCLE, kPLUS, kMINUS, kTREE, kLHAND, kRHAND, kPOT, kSTAR, kTABLE, kINVALID  } KEY;

typedef struct	{	// KeyPadKey -- assignments to GPIO pins, in inputManager.cpp
	GPIO_ID				id;			// idx in gpio_def
	KEY						key;		// KEYPAD key connected to this line
	GPIO_TypeDef 	*port;  // specifies GPIO port
	uint16_t 			pin; 		// specifies GPIO pin within port
	IRQn_Type 		intq; 	// specifies INTQ mask
	uint16_t			extiBit; // mask bit == 1 << pin
	char *				signal;	// string name of GPIO
	int 					pressed;	// GPIO input state when pressed

	//  in order to destinguish between SHORT and LONG press,  keep track of the last transition of each keypad line
	bool					down;			// true if key is currently depressed
	uint32_t    	tstamp;		// tstamp of down transition
	uint32_t			dntime;		// num tbTicks it was down

} KeyPadKey_t;

/*typedef struct { 	// KeyState --	to keep track of the state of a keypad GPIO line-- SET/RESET and time of last change
	KEY					key;
	bool				down;			// true if key is depressed
	uint32_t    tstamp;		// tstamp of down transition
	uint32_t		dntime;		// num tbTicks it was down
}	KeyState;
*/

typedef struct {	// TB_Event --  event & downMS for event Q
	Event		 typ;
	uint32_t	 arg;
} TB_Event;

// define TBV2_REV2B for board redesigned in Jan-2020 
#define TBV2_REV2B

//typedef enum { 	// Event -- TBook event types
//			eNull=0, 
//			Home, 		Circle,		Plus, 		Minus, 		Tree, 		Lhand, 		Rhand, 		Pot,	 Star,		Table,
//			Home__, 	Circle__, 	Plus__, 	Minus__, 	Tree__, 	Lhand__, 	Rhand__, 	Pot__, 	 Star__,	Table__,
//			starHome,	starCircle, starPlus, 	starMinus, 	starTree, 	starLhand, 	starRhand, 	starPot, starStar,	starTable,
//			AudioDone,	ShortIdle,	LongIdle,	LowBattery,	BattCharging, BattCharged,	FirmwareUpdate, Timer, anyKey, eUNDEF
//}	Event;

inline bool 	TB_isShort(  TB_Event evt ){	return evt.typ >= Home 			&& evt.typ <= Table;  }
inline bool 	TB_isLong(   TB_Event evt ){	return evt.typ >= Home__ 		&& evt.typ <= Table__;  }
inline bool 	TB_isStar(   TB_Event evt ){	return evt.typ >= starHome 		&& evt.typ <= starTable;  }
inline bool 	TB_isSystem( TB_Event evt ){ 	return evt.typ >= AudioDone 	&& evt.typ <= eUNDEF;  }

inline Event 	toShortEvt( KEY k ){	return (Event)( (int)k + Home );  }
inline Event 	toLongEvt(  KEY k ){	return (Event)( (int)k + Home__ );  }
inline Event 	toStarEvt(  KEY k ){	return (Event)( (int)k + starHome );  }

extern void 	initInputManager( void );
extern void 	keyPadPowerDown( void );						// shut down keypad GPIOs
extern void 	keyPadPowerUp( void );							// re-initialize keypad GPIOs
extern void 	sendEvent( Event, int32_t );

extern 			osMessageQueueId_t 	osMsg_TBEvents;
extern			osMemoryPoolId_t	TBEvent_pool;		// memory pool for TBEvents 

#endif		/* inputMgr.h */

