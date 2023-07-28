// TBookV2  ledmanager.c
//   Apr 2018

#include "tbook.h"
#include "ledmanager.h"

const bool swapCOLORS = false;   // custom build for swapped LED keypad?

///////////////////////////////////////////////////////////////////////////////
//    globals

enum LEDcolor {
    LED_BOTH = 0,
    LED_RED = 1,
    LED_GREEN = 2,
    LED_OFF = 3
};

void ledSet(LEDcolor led) {
    if (swapCOLORS)
        led = led == LED_GREEN ? LED_RED : led == LED_RED ? LED_GREEN : led;

    switch (led) {
        case LED_RED:
            GPIO::setLogical(gRED, 1);
            GPIO::setLogical(gGREEN, 0);
            break;
        case LED_GREEN:
            GPIO::setLogical(gRED, 0);
            GPIO::setLogical(gGREEN, 1);
            break;
        case LED_BOTH:
            GPIO::setLogical(gRED, 1);
            GPIO::setLogical(gGREEN, 1);
            break;
        case LED_OFF:
            GPIO::setLogical(gRED, 0);
            GPIO::setLogical(gGREEN, 0);
            break;
    }
}


///////////////////////////////////////////////////////////////////////////////
//    ledShade

class ledShade {
public:
    uint8_t dly[4];   // # msec each of Both,Red,Green,Off
    bool isOff() { return dly[3] == 255 && dly[0] == 0 && dly[1] == 0 && dly[2] == 0; }

    static ledShade asShade(char c);

private:
    static ledShade gShd;
    static ledShade G_Shd;
    static ledShade rShd;
    static ledShade R_Shd;
    static ledShade oShd;
    static ledShade O_Shd;
    static ledShade OffShd;
};

// Both  Red  Green  None
ledShade ledShade::gShd = {0, 0, 1, 8};    // G  on 10%
ledShade ledShade::G_Shd = {0, 0, 4, 6};    // G  on 40%
ledShade ledShade::rShd = {0, 1, 0, 8};    // R  on 10%
ledShade ledShade::R_Shd = {0, 4, 0, 6};    // R  on 40%
ledShade ledShade::oShd = {1, 0, 0, 8};    // RG on 10%
ledShade ledShade::O_Shd = {4, 0, 2, 4};    // RG on 40%  G on 20%
ledShade ledShade::OffShd = {0, 0, 0, 255};  // special marker value-- no multi-plexing required

ledShade ledShade::asShade(char c) {            // R r G g O o Y y __ => ledShade
    switch (c) {
        // @formatter:off
        case 'g': return gShd;
        case 'G': return G_Shd;
        case 'r': return rShd;
        case 'R': return R_Shd;
        case 'o': return oShd;
        case 'O': return O_Shd;
        default:
        case '_': return OffShd;
        // @formatter: on
    }
}

///////////////////////////////////////////////////////////////////////////////
//   LedSequence

#define MAX_SEQ_STEPS 10  //avoid NONSTANDARD C
struct LedSequence {
    const char * definition;
    uint8_t     nSteps;                 // # of shades displayed in sequence
    bool        isRepeating;            // true, if it loops
    int         repeatCnt;
    ledShade    shades[MAX_SEQ_STEPS];  // shades to display
    int         msecs[MAX_SEQ_STEPS];   // duration of each
    // state during display
    uint8_t     nextStep;               // next shade to display
    ledShade    currShade;              // shade currently being displayed
    uint8_t     shadeStep;              // 0=Both, 1=Red, 2=Grn, 3=Off
    int         msRemainingInStep;

    LedSequence() {
        nextStep = 0;
        shadeStep = 0;
        msRemainingInStep = 0;

        nSteps = 0;
        isRepeating = false;
        repeatCnt = 0;
    }

    bool isStepDone() {
        return msRemainingInStep <= 0;
    }
    bool allStepsDone() {   // move to next shade
        return nextStep >= nSteps;
    }
    bool isSequenceFinished() {
        return allStepsDone() && !isRepeating;
    }

    void step() {
        currShade = shades[nextStep];
        shadeStep = 0;
        msRemainingInStep = msecs[nextStep];
        nextStep++;
    }

    int subStep() {
        int subStepDuration = 0;
        LEDcolor color;
        if ( currShade.isOff() ) {
            color = LED_OFF;
            subStepDuration = msRemainingInStep;   // no need to do time multiplexing
        } else {
            while (subStepDuration == 0) {  // get next non-zero duration in this shade
                color = static_cast<LEDcolor>(shadeStep);
                subStepDuration = currShade.dly[shadeStep];
                shadeStep = (shadeStep+1) % 4;
            }
        }
        ledSet( color );
        if ( subStepDuration > msRemainingInStep ) subStepDuration = msRemainingInStep;
        msRemainingInStep -= subStepDuration;
        return subStepDuration;
    }
    
    void reset() {
        nextStep = 0;
        shadeStep = 0;
        repeatCnt++;
    }
    
    void parseDefinition(const char * definition, bool forceRepeat);
};

static LedSequence bgSequence, ciSequence, fgSequence, prepSequence;
static LedSequence sequences[3] = {bgSequence, ciSequence, fgSequence};
static int ixSequence = 0;

/*
 * Compile an LED string into a "LedSequence".
 *
 * pDef:     The string to convert. If the string is null or an empty string, simply clears *seq.
 */
void LedSequence::parseDefinition(const char *pDef, bool forceRepeat) {
    nextStep = 0;
    shadeStep = 0;
    msRemainingInStep = 0;

    nSteps = 0;
    isRepeating = false;
    repeatCnt = 0;
    if (!pDef || !*pDef){
        definition = "off";
        return;
    }
    definition = pDef;
    isRepeating = forceRepeat; // may be set true by "!".

    //  convert string of Color char, duration in 1/10ths sec to color[] & msec[] for shades
    //  "R G8" => shade R for 0.1sec, G for 0.8sec, isRepeating=false
    //  "R20 G O _10" => R 2.0sec, G 0.1, O 0.1, off 1.0, isRepeating=false
    //  "R!" =>  R 0.1, isRepeating=true     // stays till changed
    //  R : brighter red
    //  r : dimmer red
    //  G : brighter green
    //  g : dimmer green
    //  O : both (probably brightest, the apparent color varies by device)
    while (*pDef) {
        ledShade shade = ledShade::asShade(*pDef++);
        short tenths = 0;
        if (*pDef == ' ') pDef++;
        while (isdigit(*pDef)) {
            tenths = tenths * 10 + (*pDef++ - '0');
        }
        if (*pDef == ' ') pDef++;
        if (*pDef == '!') {
            pDef++;
            isRepeating = true;
        }
        if (tenths == 0) tenths = 1;
        if ( nSteps < MAX_SEQ_STEPS ) {
            shades[nSteps] = shade;
            msecs[nSteps] = tenths*100;
            nSteps++;
        }
    }
}

///////////////////////////////////////////////////////////////////////////////
//  LedManager

const int               LED_EVT      = 1;

osEventFlagsId_t LedManager::ledRequestFlags;           // osFlag to signal an LED change to LedThread
osEventFlagsId_t LedManager::ledResponseFlags;          // osFlag to signal an LED change to LedThread
osMutexId_t LedManager::ledRequestMutex;                // osFlag to signal an LED change to LedThread
bool LedManager::isEnabled = false;

//
//******** ledThread -- update LEDs according to current sequence

void LedManager::ledManagerThreadFunc( void *arg ) {
    int32_t delay = osWaitForever;
    dbgLog( "4 ledThread: 0x%x 0x%x \n", &arg, &arg - LED_STACK_SIZE );
    while (true) {
        uint32_t event = osEventFlagsWait(ledRequestFlags, LED_EVENT::ANY, osFlagsWaitAny, delay);
        if (event == osFlagsErrorTimeout || event == osFlagsErrorResource) {
            if ( !isEnabled ) {  // just sleep again if not enabled
                delay = osWaitForever;
            } else {
                // Get the current sequence
                LedSequence *sequence = &sequences[ixSequence];
                // Is the current step (shade) done?
                if ( sequence->isStepDone() ) {
                    // Yes. Is the current sequence done?
                    if ( sequence->allStepsDone() ) {
                        // Yes. If it should repeat, do so.
                        if ( sequence->isRepeating ) {
                            sequence->reset();
                        } else {
                            const char *names[3] = {"Bg","Ci","Fg"};
                            int ixWas = ixSequence;
                            // ... else go the the next lowest priority sequence.
                            while (ixSequence > 0 && sequences[--ixSequence].isSequenceFinished());
                            sequence = &sequences[ixSequence];
                            dbgLog("9 led%s => led%s: %s\n", names[ixWas], names[ixSequence], sequence->definition);
                            sequence->reset();
                        }
                    }
                    sequence->step();
                }
                delay = sequence->subStep();
            }
        } else {
            if (event & (ENABLE|DISABLE)) {
                isEnabled = event == ENABLE;
                delay = isEnabled ? 0 : osWaitForever;
            } else if (event & SET_BACKGROUND) {
                sequences[0] = prepSequence;
                ixSequence = max(ixSequence, 0);
                delay = 1;
            } else if (event & SET_CHARGING) {
                sequences[1] = prepSequence;
                ixSequence = max(ixSequence, 1);
                delay = 1;
            } else if (event & SET_FOREGROUND) {
                sequences[2] = prepSequence;
                ixSequence = max(ixSequence, 2);
                delay = 1;
            }
            osEventFlagsSet(ledResponseFlags, event);
        }
    }
}

void LedManager::updateSequence( LED_EVENT updateFlags, const char *def, bool forceRepeat ) {
    osMutexAcquire(ledRequestMutex, osWaitForever);

    prepSequence.parseDefinition(def, forceRepeat);

    osEventFlagsSet(ledRequestFlags, updateFlags);
    osEventFlagsWait(ledResponseFlags, LED_EVENT::ANY, osFlagsWaitAny, osWaitForever);

    osMutexRelease(ledRequestMutex);
}

//PUBLIC

/*
 * Sets the "foreground" LED sequence. Displays until cleared.
 *
 * def: A string describing the LEDs to show. See convertSeq.
 */
void LedManager::ledFg( const char *def ) {
    dbgLog("9 ledFg: %s\n", (!def || !*def) ? "off" : def);
    updateSequence(SET_FOREGROUND, def, false);
}

void LedManager::setChargeIndicator( const char *def ) {
    dbgLog("9 ledCi: %s\n", (!def || !*def) ? "off" : def);
    updateSequence(SET_CHARGING, def, true);
}

/*
 * Sets the "background" LED sequence. Until cleared, displays when there is no foreground sequence.
 *
 * def: A string describing the LEDs to show. See convertSeq.
 */
void LedManager::ledBg( const char *def ) {           // install 'def' as background pattern
    dbgLog("9 ledBg: %s\n", (!def || !*def) ? "off" : def);
    updateSequence(SET_BACKGROUND, def, true);
}

/*
 * Enable or disable the LedManager.
 *
 * bool enable: If true, enable the LED manager.
 */
void LedManager::setEnabled(bool enabled) {
    osMutexAcquire(ledRequestMutex, osWaitForever);
    if (enabled != isEnabled) {
        osEventFlagsSet(ledRequestFlags, enabled ? LED_EVENT::ENABLE : LED_EVENT::DISABLE);
        osEventFlagsWait(ledResponseFlags, LED_EVENT::ANY, osFlagsWaitAny, osWaitForever);
    }
    osMutexRelease(ledRequestMutex);
}

uint32_t lastProgress = 0, nextProgressStep = 0;

void LedManager::showProgress( const char *s, uint32_t stepMs ) {      // step through LED string, min stepMs msec each
    uint32_t tm = tbRtcStamp();
    if ( tm < lastProgress + stepMs ) return;  // avoid flashing too fast
    lastProgress = tm;
    setEnabled( false );

    if ( nextProgressStep >= strlen( s )) nextProgressStep = 0;
    char clr = s[nextProgressStep];
    nextProgressStep++;

    switch (clr) {
        case 'G': GPIO::setLogical( gGREEN, 1 ); GPIO::setLogical( gRED, 0 ); break;
        case 'R': GPIO::setLogical( gGREEN, 0 ); GPIO::setLogical( gRED, 1 ); break;
        default: GPIO::setLogical( gGREEN, 0 ); GPIO::setLogical( gRED, 0 ); break;
    }
}

void LedManager::endProgress() {                                      // finish showing progress
    setEnabled( true );
}

void LED_Init( GPIO_ID led ) {    // configure GPIO for output PUSH_PULL slow
    GPIO::configureOutput( led );
    GPIO::setLogical( led, 0 );
}

void handlePowerEvent( int powerEvent ) {
    switch (powerEvent) {
        case POWER_UP:
            //dbgLog("Led Power up \n");
            LED_Init( gGREEN );
            LED_Init( gRED );
            break;
        case POWER_DOWN:
            GPIO::unConfigure( gGREEN );
            GPIO::unConfigure( gRED );
            //dbgLog("Led Power Down \n");
            break;
    }
}

void LedManager::initLedManager() {       // initialize & spawn LED thread
    handlePowerEvent( POWER_UP );
    //  registerPowerEventHandler( handlePowerEvent );

    isEnabled = true;

    ledRequestMutex = osMutexNew(NULL);
    if (ledRequestMutex == NULL) {
        tbErr("ledRequestMutex alloc failed");
    }

    ledRequestFlags = osEventFlagsNew( NULL );  // os flag ID -- used so ledFg & ledBg can wakeup LedThread
    if ( ledRequestFlags == NULL )
        tbErr( "ledRequestFlags alloc failed" );
    ledResponseFlags = osEventFlagsNew( NULL );  // os flag ID -- used so ledFg & ledBg can wakeup LedThread
    if ( ledResponseFlags == NULL )
        tbErr( "ledResponseFlags alloc failed" );

    static osThreadAttr_t threadAttr = {
                .name = "led_thread",
                .priority = osPriorityHigh
        };
    threadAttr.stack_size = LED_STACK_SIZE;
    Dbg.thread[3] = (osRtxThread_t *) osThreadNew( ledManagerThreadFunc, NULL, &threadAttr );
    dbgLog("4 ledThread: %x\n", Dbg.thread[3]);
    if ( Dbg.thread[3] == NULL )
        tbErr( "ledThread spawn failed" );

    ledBg( "_49G!" );  // 0.1sec G flash every 5 sec
    dbgLog( "4 LedMgr OK \n" );
}
// ledmanager.c 
