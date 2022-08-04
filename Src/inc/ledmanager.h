#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include "tbook.h"


extern void   initLedManager(void);

void ledPowerDown(void);

//void      ledOn( LED led);              // old
//void      ledSequence( const char* seq, int cnt );    // old
//void      ledOff( LED led);             // old

extern void     ledFg( const char *def );             // install 'def' as foreground pattern
extern void     ledBg( const char *def );             // install 'def' as background pattern
extern void     ledBgShade( short pctRed, short pctGrn );

#endif // LED_MANAGER_H
