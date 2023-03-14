#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include "tbook.h"

class LedManager {
public:
    static void initLedManager(void);

    static void ledFg(const char *def);             // install 'def' as foreground pattern
    static void ledBg(const char *def);             // install 'def' as background pattern
    static void setChargeIndicator(const char *def);             // install 'def' as background pattern

    static void showProgress(const char *s, uint32_t stepMs);      // step through LED string, min stepMs msec each
    static void endProgress(void);                                 // finish showing progress

    static void setEnabled(bool enabled);

private:
    enum LED_EVENT {
        ENABLE          = 0x0001,
        DISABLE         = 0x0002,
        SET_BACKGROUND  = 0x0004,
        SET_CHARGING    = 0x0008,
        SET_FOREGROUND  = 0x0010,

        ANY             = 0x001f
    };

    static bool isEnabled;
    static osEventFlagsId_t ledRequestFlags;            // osFlag to signal an LED change to LedThread
    static osEventFlagsId_t ledResponseFlags;           // osFlag to acknowledge an LED change by LedThread
    static osMutexId_t ledRequestMutex;                 // To protect changing the led sequence structures

    static void updateSequence(LED_EVENT updateFlags, const char * sequence, bool forceRepeat);

    static void ledManagerThreadFunc(void *);


};

#endif // LED_MANAGER_H
