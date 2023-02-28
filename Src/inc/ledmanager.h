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

extern void showProgress(const char *s, uint32_t stepMs);      // step through LED string, min stepMs msec each
extern void endProgress(void);                                 // finish showing progress

#endif // LED_MANAGER_H
