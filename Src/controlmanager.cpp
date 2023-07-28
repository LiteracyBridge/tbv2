// TBookV2  controlmanager.c
//   Gene Ball  May2018

#include "controlmanager.h"

#include "tbook.h"        // enableMassStorage 
#include "mediaplayer.h"    // audio operations
#include "inputmanager.h"     // osMsg_TBEvents
#include "fileOps.h"

#include "packageData.h"

#include "csm.h"

ControlManager controlManager = ControlManager();

// TBook configuration variables
//extern TBConfig_t           TB_Config;      // defined in tbook_csm.c

// TBook Control State Machine
//extern int                  nCSMstates;     // defined in tbook_csm.c
bool                controlManagerReady = false;

int                 iPkg = 0;     // current package index
Package *         currPkg = NULL; // TBook content package in use

TBook_t             TBook;

osTimerId_t         timers[3];  // ShortIdle, LongIdle, Timer
void setCSMcurrState( short iSt );  // set TBook.iCurrSt & dependent fields
extern bool         BootVerboseLog;

//osEventFlagsId_t    mFileOpSignal;     // osEvent shared with FileOps -- to signal MP3 decode done

// ------------  CSM Action execution
void ControlManager::clearIdle() {       // clear timers started by AudioDone to generate shortIdle & longIdle
    osTimerStop( timers[0] );
    osTimerStop( timers[1] );
}

void ControlManager::assertValidState( int stateIndex ) {
    if ( stateIndex < 0 || stateIndex >= csm->numCStates() ) //nCSMstates )
        tbErr( "invalid state index" );
}

/**
 * Find the next playlist, in the given direction, that has content.
 * @param direction Negative for "previous", otherwise "next".
 */
void ControlManager::changeSubject( const char *change ) {
    while (isspace(*change)) ++change;
    short newSubjectIx = TBook.iSubj;
    short numSubjects  = currPkg->numSubjects();
    // Delta?
    if (*change == '+' || *change == '-') {
        // Yes, delta. Move by one in indicated direction, wrapping as appropriate.
        int delta = (*change == '-') ? -1 : 1; // normalize
        // "subject" === "playlist".
        // Find the next playlist (in the desired iChange) that has content.
        do {
            newSubjectIx += delta;
            if (newSubjectIx < 0)
              newSubjectIx = numSubjects - 1;
            if (newSubjectIx >= numSubjects)
                newSubjectIx = 0;
        } while (newSubjectIx != TBook.iSubj && currPkg->getSubject( newSubjectIx )->numMessages() == 0);
    } else {
        // Absolute. Valid?
        int newIx = atoi(change);
        if (newIx >= 0 && newIx < numSubjects && (currPkg->getSubject( newIx )->numMessages() > 0))
            newSubjectIx = newIx;
    }
    if (newSubjectIx != TBook.iSubj) {
        logEvtNI( "chngSubj", "iSubj", newSubjectIx );
        TBook.iSubj = newSubjectIx;
        TBook.iMsg  = -1; // "next" will yield 0, "prev" -> -2 -> numMessages-1
        resetAudio();
    }
}

/**
 * Find the next message, in the given direction, in the current playlist.
 * @param direction Negative for "previous", otherwise "next".
 */
void ControlManager::changeMessage(const char *change ) {
    // If no subject is selected, do nothing.
    if (TBook.iSubj < 0 || TBook.iSubj >= currPkg->numSubjects()) {
        return;
    }
    // If no messages in subject (how did we get here?), do nothing.
    Subject *subj = currPkg->getSubject( TBook.iSubj );
    short numMessages = subj->numMessages();
    if (numMessages == 0) {
        return;
    }
    while (isspace(*change)) ++change;
    short newMessageIx = TBook.iMsg;
    if (*change == '+' || *change == '-') {
        (*change=='-') ? --newMessageIx : ++newMessageIx;
        if (newMessageIx < 0) {
            newMessageIx = numMessages - 1;
        }
        if (newMessageIx >= numMessages) {
            newMessageIx = 0;
        }
    } else {
        int newIx = atoi(change);
        if (newIx >= 0 && newIx < numMessages)
            newMessageIx = newIx;
    }
    if (newMessageIx != TBook.iMsg) {
        logEvtNI("chgMsg", "iMsg", newMessageIx);
        TBook.iMsg = newMessageIx;
        resetAudio();
    }
}

void ControlManager::playSysAudio(const char *arg ) {        // play system file 'arg'
    resetAudio();
    char pathBuf[MAX_PATH];

    currPkg->findAudioPath( pathBuf, arg );  // find path based on current Package promptPaths
    clearIdle();
    logEvtFmt("PlaySys", "Sys: %s, fn: %s", arg, pathBuf);
    playAudio( pathBuf, NULL, kPlayTypeSystemPrompt );

//    for (int i = 0; i < csm->numSystemAudio(); i++)
//        if ( strcmp( csm->getSystemAudio( i ), arg ) == 0 ) {
//            currPkg->findAudioPath( pathBuf, arg );  // find path based on current Package promptPaths
//            clearIdle();
//            logEvtFmt("PlaySys", "Sys: %s, fn: %s", arg, pathBuf);
//            playAudio( pathBuf, NULL, kPlayTypeSystemPrompt );
//            return;
//        }
//    tbErr( "playSys %s not found", arg );
}

void ControlManager::playSubjectAudio(const char *arg ) {       // play current Subject: arg must be 'nm', 'pr', or 'msg'
    // If no subject is selected, do nothing.
    if ( TBook.iSubj < 0 || TBook.iSubj >= currPkg->numSubjects())
        return;
    Subject *subj = currPkg->getSubject( TBook.iSubj );
    char path[MAX_PATH];
    AudioFile *audioFile = NULL;
    MsgStats        *stats = NULL;
    PlaybackType_t playbackType;
    resetAudio();
    if ( strcasecmp( arg, "nm" ) == 0 ) {
        // Playlist name
        audioFile     = subj->shortPrompt;
        playbackType = kTypeSubject;
        logEvtFmt("PlayNm", "iS:%d, nm:%s, fn:%s", TBook.iSubj, subj->getName(), audioFile->filename);
    } else if ( strcasecmp( arg, "pr" ) == 0 ) {
        // Playlist invitation / call to action
        audioFile     = subj->invitation;
        playbackType = kPlayTypeInvitation;
        logEvtFmt("PlayInv", "iS:%d, nm:%s, fn:%s", TBook.iSubj, subj->getName(), audioFile->filename);
    } else if ( strcasecmp( arg, "msg" ) == 0 ) {
        // Program content message
        audioFile     = subj->getMessage(TBook.iMsg );
        playbackType = kPlayTypeMessage;
        logEvtFmt( "PlayMsg", "iS:%d, iM:%d, fn:%s", TBook.iSubj, TBook.iMsg, audioFile->filename );
        stats = loadStats( subj->getName(), TBook.iSubj, TBook.iMsg ); // load stats for message
    }
    Deployment::instance->getPathForAudioFile( path, audioFile );
    clearIdle();
    playAudio( path, stats, playbackType );
}

void ControlManager::playSquareTune( const char *notes ) {       // play seq of notes as sqrwaves
    clearIdle();
    playNotes( notes );   // mediaPlayer op -- switch threads
}

void ControlManager::startRecAudio(const char *arg ) {           // record user message into temporary .wav file
    resetAudio();
    if ( RunQCTest ) {
        recordAudio( "/system/qcAudio.wav", NULL );
        return;
    }
    char path[MAX_PATH];
    Subject *subj = currPkg->getSubject( TBook.iSubj );
    int mCnt = makeAuxFileName( path, AUX_FILE_RECORDING );
    logEvtFmt( "Record", "Subj: %s, ipkg: %d, ipl: %d, im: %d, uniq: %d", subj->getName(), iPkg, TBook.iSubj, TBook.iMsg,
               mCnt );
    MsgStats *stats = loadStats( subj->getName(), TBook.iSubj, TBook.iMsg ); // load stats for message
    clearIdle();
    recordAudio( path, stats );
}

void ControlManager::playRecAudio() {                             // play back recorded temp .wav
    clearIdle();
    playRecording();
}

void ControlManager::saveRecAudio(const char *arg ) {              // encrypt user message .wav => .key & .dat
    if ( strcasecmp( arg, "sv" ) == 0 ) {
        saveRecording();
    } else if ( strcasecmp( arg, "del" ) == 0 ) {
        cancelRecording();
    }
}

/**
 * Write the given text to a .txt file in the recordings directory. Filename based on current
 * playlist and message.
 * @param txt The text to be written.
 */
void ControlManager::saveWriteMsg(const char *txt ) {        // save 'txt' in Msg file
    const char *subjName = TBook.iSubj >= 0 ? currPkg->getSubject( TBook.iSubj )->getName() : "-no-subject-";
    char path[MAX_PATH];
    int  mCnt      = makeAuxFileName( path, AUX_FILE_MESSAGE );
    FILE *outFP = tbOpenWrite( path );
    int nch = fprintf( outFP, "%s\n", txt );
    tbFclose( outFP );  //int err = fclose( outFP );
    saveAuxProperties( path );

    dbgEvt( TB_wrMsgFile, nch, 0, 0, 0 );
    logEvtFmt( "writeMsg", "Subj: %s, ipkg: %d, ipl: %d, im: %d, uniq: %d, msg: %s", subjName, iPkg, TBook.iSubj,
               TBook.iMsg, mCnt, txt );
}

void ControlManager::svQCresult(const char *arg ) {           // save 'arg' to system/QC_PASS.txt if starts with "QC_OK", or as QC_FAIL.txt
    const char *passNm = "/system/QC_PASS.txt";
    const char *failNm = "/system/QC_FAIL.txt";
    checkMem();

    if ( fexists( passNm )) fdelete( passNm, NULL );
    if ( fexists( failNm )) fdelete( failNm, NULL );
    if ( strncmp( arg, "QC_OK", 5 ) == 0 )
        writeLine( arg, "/system/QC_PASS.txt" );
    else
        writeLine( arg, "/system/QC_FAIL.txt" );
}

void ControlManager::showPackage() {                   // debug print Package iPkg
    char path[MAX_PATH];
    printf( "iPkg=%d nm=%s nSubjs=%d \n", currPkg->pkgIdx, currPkg->getName(), currPkg->numSubjects());
    for (int i = 0; i < currPkg->numSubjects(); i++) {
        Subject *sb = currPkg->getSubject( i );
        Deployment::instance->getPathForAudioFile( path, sb->shortPrompt );
        printf( " S%d %s nMsgs=%d prompt=%s\n", i, sb->getName() ,sb->numMessages( ), path );
        Deployment::instance->getPathForAudioFile( path, sb->invitation );
        printf( "  invite=%s \n", path );
        for (int j = 0; j < sb->numMessages(); j++) {
            Deployment::instance->getPathForAudioFile( path, sb->getMessage(j ));
            printf( "   M%d %s \n", j, path );
        }
    }
}

void ControlManager::playNextPackage() {                    // play name of next available Package
    iPkg++;
    if ( iPkg >= Deployment::instance->numPackages()) iPkg = 0;

    Package *pkg = Deployment::instance->getPackage( iPkg );
    logEvtFmt("PlayPkg", "Pkg: %s, iPkg: %d", pkg->getName(), iPkg);
    char path[MAX_PATH];
    Deployment::instance->getPathForAudioFile( path, pkg->pkg_prompt );
    clearIdle();
    playAudio( path, NULL, kPlayTypePackagePrompt );
}

void ControlManager::changePackage() {                      // switch to last played package name
    if ( Deployment::instance == NULL || Deployment::instance->packages == NULL )
        errLog( "changePackage() bad Deployment" );
    if ( iPkg < 0 || iPkg >= Deployment::instance->numPackages() )
        errLog( "changePackage() bad iPkg=%d", iPkg );
    currPkg = Deployment::instance->getPackage(iPkg);

    TBook.iSubj = -1; // makes "next subject" go to the first subject.
    TBook.iMsg  = 0;
    logEvtNS( "ChgPkg", "Pkg", currPkg->getName());
    //showPackage();
}

bool ControlManager::callScript(const char *scriptName) {
    // Look for script in "csms".
    CSM *nextCsm = NULL;
    for (int i=0; i<csms.size(); ++i) {
        std::pair<const char *, CSM *> p = csms[i];
        if (strcmp(p.first ,scriptName) == 0) {
            nextCsm = p.second;
            break;
        }
    }
    // If script not already loaded, load it.
    if (nextCsm == NULL) {
        static char fname[MAX_PATH];
        currPkg->findScriptPath(fname, scriptName);
        if (!*fname) return false; // not found
        nextCsm = CSM::loadFromFile(fname);
        if (!nextCsm) return false;
        csms.emplace_back(std::pair<const char*, CSM *>(scriptName, nextCsm));
    }
    // Push existing CSM onto save stack.
    csmStack.push_back(csm);
    // Start new CSM.
    csm = nextCsm;
    return true;
}

void ControlManager::exitScript(CSM_EVENT exitEventId) {
    if (csmStack.size() < 1) return; // nothing to exit to
    csm = csmStack.back();
    csmStack.pop_back();
    csm->handleEvent(exitEventId);
}

void ControlManager::USBmode( bool start ) {            // start (or stop) USB storage mode
    fsTime   rtcTm;
    uint32_t msec;
    clearIdle();
    if ( start ) {
        playSquareTune( "G/+G/+" );
        getRTC( &rtcTm, &msec );  // current RTC
        showRTC();          // put current RTC into log
        saveLastTime( rtcTm );

        logEvt( "enterUSB" );
        saveNorLog( "" );
        logPowerDown();       // flush & shut down logs
        setDbgFlag( 'F', false );
        enableMassStorage( "M0:", NULL, NULL, NULL );
    } else {
        disableMassStorage();
        LedManager::ledFg( "_" );
        playSquareTune( "G/+" );
        logEvt( "exitUSB" );
        /*
         * For a very long time (until 21-March-2023) we did the "logPowerUp(false)". Some change around that
         * time caused the TB to crash when returning from USBMode, but ONLY AFTER CREATING SetRTC.txt. No SetRTC.txt,
         * no crash. Or, create SetRTC.txt and then reboot; no crash. So, we reboot here. Whatever the bug is,
         * it still lies dormant.
         */
        NVIC_SystemReset();     // soft reboot?
//        logPowerUp( false );
    }
}

ControlManager::ACTION_DISPOSITION ControlManager::doAction( CSM_ACTION action, const char *arg, int iarg ) {
    void * sp;
    static void *sp_prev = 0;
    sp = &sp;
    ACTION_DISPOSITION result = CONTINUE_ACTIONS;
    dbgEvt( TB_csmDoAct, action, arg[0], arg[1], iarg );
    dbgLog("C doAction %s(%s), sp:%p, prev:%p\n", CSM::actionName(action), arg, sp, sp_prev);
    sp_prev = sp;
    if ( BootVerboseLog ) logEvtNSNS( "Action", "nm", CSM::actionName( action ), "arg", arg ); //DEBUG
    if ( isMassStorageEnabled()) {    // if USB MassStorage running: ignore actions referencing files
        switch (action) {
            case playSys:
            case playSubj:
            case startRec:
            case pausePlay:   // pause/play all share
            case resumePlay:
            case pauseRec:
            case resumeRec:
            case finishRec:
            case writeMsg:
            case stopPlay:
                return result;

            default: break;
        }
    }
    switch (action) {
        case LED:
            LedManager::ledFg( tbConfig.ledStr(arg) );
            break;
        case bgLED:
            LedManager::ledBg( tbConfig.ledStr(arg) );
            break;
        case playSys:
            playSysAudio( arg );
            break;
        case playSubj:
            playSubjectAudio( arg );
            break;
        case startRec:
            startRecAudio( arg );
            break;
        case pausePlay:   // pause/play all share
        case resumePlay:
        case pauseRec:
        case resumeRec:
            pauseResume();
            break;
        case playRec:
            playRecAudio();
            break;
        case saveRec:
            saveRecAudio( arg );
            break;
        case finishRec:
            stopRecording();
            break;
        case writeMsg:
            if ( RunQCTest )
                svQCresult( arg );    // saves 'arg' to system/QC_PASS.txt if it starts with "QC_OK", or as QC_FAIL.txt otherwise
            else
                saveWriteMsg( arg );
            break;
        case filesTest:
            QCfilesTest( arg );   // generates event FilesSuccess or FilesFail
            break;
        case stopPlay:
            stopPlayback();
            //      logEvt( "stopPlay" );
            break;
        case volAdj:
            adjVolume( iarg );
            break;
        case spdAdj:
            adjSpeed( iarg );
            break;
        case posAdj:
            adjPlayPosition( iarg );
            break;
        case startUSB:
            USBmode( true );
            break;
        case endUSB:
            USBmode( false );
            break;
        case subjAdj:
            changeSubject( arg );
            break;
        case msgAdj:
            changeMessage( arg );
            break;
        case goPrevSt:
            csm->goPrevState();
            result = BREAK_ACTIONS;
            break;
        case saveSt:      // remember state that preceded this one (for possible future return)
            if ( iarg > 4 ) iarg = 4;
            dbgLog( "C svSt[%d] = %s(%d) \n", iarg, csm->prevStateName, csm->prevStateIx );
            csm->saveState(iarg);
            break;
        case goSavedSt:
            if ( iarg > 4 ) iarg = 4;
            csm->goSavedState(iarg);
            result = BREAK_ACTIONS;
            break;
        case setTimer:
            osTimerStart( timers[2], iarg );
            break;
        case resetTimer:
            osTimerStop( timers[2] );
            break;
        case showCharge:
            showBattCharge();
            break;
        case powerDown:
            result = BREAK_ACTIONS;
            powerDownTBook();
            break;
        case sysTest:
            playSquareTune( "CDEF_**C*D*E*F*_**C*D*E*F*_***C**G**FDEH**G**" );
            break;
        case playNxtPkg:
            playNextPackage();
            break;
        case changePkg:
            changePackage();
            break;
        case playTune:
            playSquareTune( arg );
            break;
        case sysBoot:
            result = BREAK_ACTIONS;
            NVIC_SystemReset();     // soft reboot
            break;
        case callIf:
            // If no subject is selected, do nothing.
            if (TBook.iSubj >= 0 && TBook.iSubj < currPkg->numSubjects()) {
                Subject *subj = currPkg->getSubject(TBook.iSubj);
                if (subj->script) {
                    result = BREAK_ACTIONS;
                    if (callScript(subj->script)) {
                        CSM::print(csm);
                        csm->start();
                    }
                }
            };
            break;
        case exitCsm:
            {
            result = BREAK_ACTIONS;
            CSM_EVENT eventId = lastEventId;
            if (arg && *arg) {
                eventId = CSM::eventFromName(arg);
            }
            exitScript(eventId);
            }
            break;
        case enterSt:
            {
            result = BREAK_ACTIONS;
            int newIx = csm->stateIxForName(arg);
            csm->enterState(newIx);
            break;
            }
        default: break;
    }
    return result;
}

void ControlManager::updateTimersForEvent(CSM_EVENT eventId) {
    switch (eventId) {
        case AudioDone:
            if ( audioIdle )
                logEvtNS( "error", "msg", "AudioDone without AudioStart" );
            else {
                audioIdle = true;
                osTimerStart( timers[0], tbConfig.shortIdleMS );
                osTimerStart( timers[1], tbConfig.longIdleMS );
            }
            break;
        case ShortIdle:     // events that don't reset idle timers
        case LowBattery:
        case BattCharging:
        case BattCharged:
        case BattMin: // ?? this one
        case BattMax:
        case ChargeFault: // ?? this one
        case LithiumHot: // ?? this one
        case FilesSuccess:
        case FilesFail:
        case BattCharging75:
        case BattNotCharging:
            break;
        case AudioStart:
            audioIdle = false;
            break;
        default:
            clearIdle();
            break;
    }
}

void ControlManager::executeCSM( void ) {               // execute TBook control state machine
    TB_Event   *event;
    osStatus_t status;
    controlManagerReady = true;

    TBook.iSubj  = -1; // makes "next subject" go to the first subject.
    TBook.iMsg   = 0;

    // set initialState & do actions
    csm->start();
    audioIdle = true;
    while (true) {
        event    = NULL;
        status = osMessageQueueGet( osMsg_TBEvents, &event, NULL, osWaitForever );  // wait for next TB_Event
        if ( status != osOK )
            tbErr( " EvtQGet error" );
        lastEventId = event->typ;
        updateTimersForEvent(lastEventId);
        osMemoryPoolFree( TBEvent_pool, event );
        csm->handleEvent(lastEventId);
    }
}

void ControlManager::QCfilesTest(const char *arg ) {          // write & re-read system/qc_test.txt -- gen Event FilesSuccess if identical, or FilesFail if not
    const char *testNm = "/system/QC_Test.txt";
    char   qcmsg[50], qcres[50];
    fsTime ftime;
    strcpy( qcres, "------------------" );
    strcpy( qcmsg, "QC filesTest" );

    writeLine( qcmsg, testNm );
    loadLine( qcres, testNm, &ftime );  // and read it back
    fdelete( testNm, NULL );

    char *nl = strchr( qcres, '\n' );
    if ( nl != NULL ) *nl = 0;    // truncate newline
    if ( strcmp( qcmsg, qcres ) == 0 )
        sendEvent( FilesSuccess, 0 );
    else
        sendEvent( FilesFail, 0 );
}

/**
 * Timer callback. This is a static member function.
 * @param timerIx The parameter passed to osTimerNew, the timer index.
 */
void ControlManager::tbTimerFunc( void *timerIx ) {
    switch ((int) timerIx) {
        case 0:
            sendEvent( ShortIdle, 0 );
            break;
        case 1:
            // TODO: This feels a little wrong, embedding decisions here based on charging. But both the idle
            // timers and the event processing are managed here, so that's 2 out of 3.
            // We may want to build some sort of "Keep Awake" facility that other sub-systems could use to
            // prevent sleeping. If done well, that would also let us timeout after playing audio, and also
            // sleep immediately when charging is complete.
            // Long idle is for going to sleep. Don't sleep while charging, so we can continue to report status.
            if ( isCharging()) {
                osTimerStart( controlManager.timers[1], tbConfig.longIdleMS );
            } else {
                sendEvent( LongIdle, 0 );
            }
            break;
        case 2:
            sendEvent( Timer, 0 );
            break;
    }
}

void ControlManager::init() {
    for (int timerIx = 0; timerIx < 3; timerIx++) {
        timers[timerIx] = osTimerNew(tbTimerFunc, osTimerOnce, (void *) timerIx, NULL);
        if (timers[timerIx] == NULL)
            tbErr("timer not alloc'd");
    }

    csm = loadControlDef2();

}

void USBmode(bool start) {
    controlManager.USBmode(start);
}

void initControlManager( void ) {       // initialize control manager
    EventRecorderEnable( evrEA, TB_no, TBsai_no );  // TB, TBaud, TBsai
    EventRecorderDisable( evrAOD, EvtFsCore_No, EvtFsMcSPI_No );  //FileSys library
    EventRecorderDisable( evrAOD, EvtUsbdCore_No, EvtUsbdEnd_No );  //USB library

    TBConfig::initConfig();
    controlManager.init();

    if (BootToUSB) {
        USBmode(true);
        return;
    }

    // don't init power timer-- it is set to do 1st check at 15sec, then resets from TB_Config
    //setPowerCheckTimer( tbConfig.powerCheckMS );
    if (RunQCTest) {
        // disable powerChecks during QC
        PowerChecksEnabled = false;
    }

    LedManager::ledBg(tbConfig.bgPulse);    // reset background pulse according to TB_Config
    setVolume(tbConfig.default_volume);         // set initial volume

    iPkg = 0;
    // Don't try load package for qcTest. If not qcTest and can't load packages, go to USB mode.
    if (!RunQCTest && !readPackageData()) {
        USBmode(true);    // go to USB unless successfully loaded CSM & packages_data
        return;         // don't do anything else after starting USBmode
    }

    TBook.iSubj = -1; // makes "next subject" go to the first subject.
    TBook.iMsg  = 1;

    controlManager.executeCSM();


}
// contentmanager.cpp 
