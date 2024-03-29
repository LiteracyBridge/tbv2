#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

#include  "tbook.h"

#define   POWER_UP  1
#define   POWER_DOWN  0

typedef void(*HPE)( int );    // type for ptr to powerEventHandler() 


extern void   initPowerMgr( void );             // initialize PowerMgr & start thread
extern void   enableSleep( void );              // low power mode -- CPU stops till interrupt
extern void   powerDownTBook( void  );          // shut down TBook -- MCU Stop mode
extern void   wakeup( void );
extern void   setPowerCheckTimer( int timerMs );  // set delay in msec between power checks
extern void   setupRTC( fsTime time );          // init RTC to time
extern void   showBattCharge( void );           // generate LedManager::ledFg to signal power state
extern bool   haveUSBpower( void );             // true if USB plugged in (PwrGood_N = 0)
extern bool     isCharging(void);                   // True if Li-Ion currently charging.
#endif  // POWER_MANAGER_H

