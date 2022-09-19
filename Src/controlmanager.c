// TBookV2  controlmanager.c
//   Gene Ball  May2018

#include "controlmanager.h"

#include "tbook.h"        // enableMassStorage 
#include "mediaplayer.h"    // audio operations
#include "inputmanager.h"     // osMsg_TBEvents
#include "fileOps.h"      // decodeAudio

#include "packageData.h"

// TBook configuration variables
//extern TBConfig_t           TB_Config;      // defined in tbook_csm.c

// TBook Control State Machine
//extern int                  nCSMstates;     // defined in tbook_csm.c
bool                controlManagerReady = false;


int                 iPkg = 0;     // current package index
Package_t *         currPkg = NULL; // TBook content package in use

TBook_t             TBook;

osTimerId_t         timers[3];  // ShortIdle, LongIdle, Timer
void setCSMcurrState( short iSt );  // set TBook.iCurrSt & dependent fields
extern bool         BootVerboseLog;

osEventFlagsId_t    mFileOpSignal;     // osEvent shared with FileOps -- to signal MP3 decode done
bool                DecodeMP3 = false;  // if set, decode MP3's to Wav before entering CSM


// ------------  CSM Action execution
static void clearIdle() {       // clear timers started by AudioDone to generate shortIdle & longIdle
    osTimerStop( timers[0] );
    osTimerStop( timers[1] );
}

static void adjSubj( int adj ) {               // adjust current Subj # in TBook
    // "subject" means playlist.
    short newSubjectIx   = TBook.iSubj + adj;
    short numSubjects = nSubjs();
    if ( newSubjectIx < 0 )
        newSubjectIx = numSubjects - 1;
    if ( newSubjectIx >= numSubjects )
        newSubjectIx = 0;
    logEvtNI( "chngSubj", "iSubj", newSubjectIx );
    TBook.iSubj = newSubjectIx;
    TBook.iMsg  = -1; // "next" will yield 0, "prev" -> -2 -> numMessages-1
    resetAudio();
}


static void adjMsg( int adj ) {                // adjust current Msg # in TBook
    // If no subject is selected, do nothing.
    if ( TBook.iSubj < 0 || TBook.iSubj >= nSubjs())
        return;
    // adj must be +1 or -1
    short newMessageIx = TBook.iMsg + adj;
    Subject_t *subj = gSubj( TBook.iSubj );
    short numMessages = nMsgs( subj );
    if ( numMessages == 1 )
        numMessages = 0;
    if ( newMessageIx < 0 )
        newMessageIx   = numMessages - 1;
    if ( newMessageIx >= numMessages )
        newMessageIx   = 0;
    logEvtNI( "chgMsg", "iMsg", newMessageIx );
    TBook.iMsg = newMessageIx;
    resetAudio();
}


void playSubjAudio( char *arg ) {       // play current Subject: arg must be 'nm', 'pr', or 'msg'
    // If no subject is selected, do nothing.
    if ( TBook.iSubj < 0 || TBook.iSubj >= nSubjs())
        return;
    Subject_t *subj = gSubj( TBook.iSubj );
    char path[MAX_PATH];
    AudioFile_t *aud = NULL;
    MsgStats        *stats = NULL;
    PlaybackType_t playbackType;
    resetAudio();
    if ( strcasecmp( arg, "nm" ) == 0 ) {
        // Playlist name
        aud     = subj->shortPrompt;
        playbackType = kPlayTypePlaylist;
        logEvtFmt("PlayNm", "iPl: %d, nm: %s, fn: %s", TBook.iSubj, subj->subjName, aud->filename);
    } else if ( strcasecmp( arg, "pr" ) == 0 ) {
        // Playlist invitation / call to action
        aud     = subj->invitation;
        playbackType = kPlayTypeInvitation;
        logEvtFmt("PlayInv", "iPl: %d, nm: %s, fn: %s", TBook.iSubj, subj->subjName, aud->filename);
    } else if ( strcasecmp( arg, "msg" ) == 0 ) {
        // Program content message
        aud     = gMsg( subj, TBook.iMsg );
        playbackType = kPlayTypeMessage;
        logEvtFmt( "PlayMsg", "Msg: %s, iM: %d, fn: %s", subj->subjName, TBook.iMsg, aud->filename );
        stats = loadStats( subj->subjName, TBook.iSubj, TBook.iMsg ); // load stats for message
    }
    getAudioPath( path, aud );
    clearIdle();
    playAudio( path, stats, playbackType );
}


void playNxtPackage() {                    // play name of next available Package
    iPkg++;
    if ( iPkg >= nPkgs()) iPkg = 0;

    Package_t *pkg = gPkg( iPkg );
    logEvtFmt("PlayPkg", "Pkg: %s, iPkg: %d", pkg->packageName, iPkg);
    char path[MAX_PATH];
    getAudioPath( path, pkg->pkg_prompt );
    clearIdle();
    playAudio( path, NULL, kPlayTypePackagePrompt );
}

void playSqrTune( char *notes ) {       // play seq of notes as sqrwaves
    clearIdle();
    playNotes( notes );   // mediaPlayer op -- switch threads
}

void showPkg() {                   // debug print Package iPkg
    char path[MAX_PATH];
    printf( "iPkg=%d nm=%s nSubjs=%d \n", currPkg->pkgIdx, currPkg->packageName, nSubjs());
    for (int i = 0; i < nSubjs(); i++) {
        Subject_t *sb = gSubj( i );
        getAudioPath( path, sb->shortPrompt );
        printf( " S%d %s nMsgs=%d prompt=%s\n", i, subjNm( sb ), nMsgs( sb ), path );
        getAudioPath( path, sb->invitation );
        printf( "  invite=%s \n", path );
        for (int j = 0; j < nMsgs( sb ); j++) {
            getAudioPath( path, gMsg( sb, j ));
            printf( "   M%d %s \n", j, path );
        }
    }
}


void changePackage() {                      // switch to last played package name
    if ( Deployment == NULL || Deployment->packages == NULL )
        errLog( "changePackage() bad Deployment" );
    if ( iPkg < 0 || iPkg >= Deployment->packages->nPkgs )
        errLog( "changePackage() bad iPkg=%d", iPkg );
    currPkg = Deployment->packages->pkg[iPkg];

    TBook.iSubj = -1; // makes "next subject" go to the first subject.
    TBook.iMsg  = 0;
    logEvtNS( "ChgPkg", "Pkg", pkgNm());
    //showPkg();
}


void playSysAudio( char *arg ) {        // play system file 'arg'
    resetAudio();
    char path[MAX_PATH];

    for (int i = 0; i < nSysAud(); i++)
        if ( strcmp( gSysAud( i ), arg ) == 0 ) {
            findAudioPath( path, currPkg->prompt_paths, arg );  // find path based on current Package promptPaths
            clearIdle();
            logEvtFmt("PlaySys", "Sys: %s, fn: %s", arg, path);
            playAudio( path, NULL, kPlayTypeSystemPrompt );
            return;
        }
    tbErr( "playSys %s not found", arg );
}


void startRecAudio( char *arg ) {           // record user message into temporary .wav file
    resetAudio();
    if ( RunQCTest ) {
        recordAudio( "/system/qcAudio.wav", NULL );
        return;
    }
    char path[MAX_PATH];
    Subject_t *subj = gSubj( TBook.iSubj );
    int mCnt = makeAuxFileName( path, AUX_FILE_RECORDING );
    logEvtFmt( "Record", "Subj: %s, ipkg: %d, ipl: %d, im: %d, uniq: %d", subj->subjName, iPkg, TBook.iSubj, TBook.iMsg,
               mCnt );
    MsgStats *stats = loadStats( subj->subjName, TBook.iSubj, TBook.iMsg ); // load stats for message
    clearIdle();
    recordAudio( path, stats );
}


void playRecAudio() {                             // play back recorded temp .wav
    clearIdle();
    playRecording();
}


void saveRecAudio( char *arg ) {              // encrypt user message .wav => .key & .dat
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
void saveWriteMsg( char *txt ) {        // save 'txt' in Msg file
    char *subjName = TBook.iSubj >= 0 ? gSubj( TBook.iSubj )->subjName : "-no-subject-";
    char path[MAX_PATH];
    int  mCnt      = makeAuxFileName( path, AUX_FILE_MESSAGE );
    FILE *outFP = tbOpenWrite( path ); //fopen( fNm, "w" );
    int nch = fprintf( outFP, "%s\n", txt );
    tbFclose( outFP );  //int err = fclose( outFP );
    saveAuxProperties( path );

    dbgEvt( TB_wrMsgFile, nch, 0, 0, 0 );
    logEvtFmt( "writeMsg", "Subj: %s, ipkg: %d, ipl: %d, im: %d, uniq: %d, msg: %s", subjName, iPkg, TBook.iSubj,
               TBook.iMsg, mCnt, txt );
}

void QCfilesTest(
        char *arg ) {          // write & re-read system/qc_test.txt -- gen Event FilesSuccess if identical, or FilesFail if not
    char *testNm = "/system/QC_Test.txt";
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

void svQCresult( char *arg ) {           // save 'arg' to system/QC_PASS.txt if starts with "QC_OK", or as QC_FAIL.txt
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

void USBmode( bool start ) {            // start (or stop) USB storage mode
    fsTime   rtcTm;
    uint32_t msec;
    clearIdle();
    if ( start ) {
        playSqrTune( "G/+G/+" );
        getRTC( &rtcTm, &msec );  // current RTC
        showRTC();          // put current RTC into log
        saveLastTime( rtcTm );

        logEvt( "enterUSB" );
        saveNorLog( "" );
        logPowerDown();       // flush & shut down logs
        setDbgFlag( 'F', false );
        enableMassStorage( "M0:", NULL, NULL, NULL );
    } else {
        disableMassStorage();      //TODO?  just reboot?
        ledFg( "_" );
        playSqrTune( "G/+" );
        logEvt( "exitUSB" );
        //      NVIC_SystemReset();     // soft reboot?
        logPowerUp( false );
    }
}

void assertValidState( int stateIndex ) {
    if ( stateIndex < 0 || stateIndex >= CSM->CsmDef->nCS ) //nCSMstates )
        tbErr( "invalid state index" );
}

char *ledStr( char *s ) {     // lookup TBConfig LED sequences
    // @formatter:off
    if ( strcasecmp( s, "bgPulse" )==0 )        return TB_Config->bgPulse;          // set by CSM (background while navigating)           default: _49G
    if ( strcasecmp( s, "fgPlaying" )==0 )      return TB_Config->fgPlaying;        // set by startPlayback()                             default: G!
    if ( strcasecmp( s, "fgPlayPaused" )==0 )   return TB_Config->fgPlayPaused;     // set by audPauseResumeAudio() when playback paused  default: G2_3!
    if ( strcasecmp( s, "fgRecording" )==0 )    return TB_Config->fgRecording;      // set by startRecord() while recording user feedback default: R!
    if ( strcasecmp( s, "fgRecordPaused" )==0 ) return TB_Config->fgRecordPaused;   // set by audPauseResumeAudio() when recording paused default: R2_3!
    if ( strcasecmp( s, "fgSavingRec" )==0 )    return TB_Config->fgSavingRec;      // set by haltRecord while saving recording           default: O!
    if ( strcasecmp( s, "fgSaveRec" )==0 )      return TB_Config->fgSaveRec;        // set by saveRecording() while encrypting recording  default: G3_3G3
    if ( strcasecmp( s, "fgCancelRec" )==0 )    return TB_Config->fgCancelRec;      // set by media.cancelRecording()                     default: R3_3R3
    if ( strcasecmp( s, "fgUSB_MSC" )==0 )      return TB_Config->fgUSB_MSC;        // set by USBD_MSC0_Init() when USB host connects     default: O5o5!
    if ( strcasecmp( s, "fgUSBconnect" )==0 )   return TB_Config->fgUSBconnect;     // set by enableMassStorage() when starting USB       default: G5g5!
    if ( strcasecmp( s, "fgTB_Error" )==0 )     return TB_Config->fgTB_Error;       // set by TBErr() to signal software error            default: R8_2R8_2R8_20!
    if ( strcasecmp( s, "fgNoUSBcable" )==0 )   return TB_Config->fgNoUSBcable;     // set by enableMassStorage() if noUSBpower           default: _3R3_3R3_3R3_5!
    if ( strcasecmp( s, "fgPowerDown" )==0 )    return TB_Config->fgPowerDown;      // set??  powerDownTBook() G_3G_3G_9G_3G_9G_3
    // @formatter:on
    return s;
}

void setCurrState( short iSt ) {        // set iCurrSt & iPrevSt (& DBG strings)
    if ( iSt == TBook.iCurrSt )
        return;
    assertValidState( iSt );
    TBook.iPrevSt       = TBook.iCurrSt;
    TBook.prevStateName = TBook.currStateName;          //DEBUG -- update prevSt string

    TBook.iCurrSt = iSt;            // now 'in' (executing) state nSt
    TBook.cSt     = gCSt( TBook.iCurrSt );      // state definition for current state

    TBook.currStateName = gStNm( TBook.cSt );   //DEBUG -- update currSt string
}

static void doAction( Action act, char *arg, int iarg ) {  // execute one csmAction
    dbgEvt( TB_csmDoAct, act, arg[0], arg[1], iarg );
    if ( BootVerboseLog ) logEvtNSNS( "Action", "nm", actionNm( act ), "arg", arg ); //DEBUG
    if ( isMassStorageEnabled()) {    // if USB MassStorage running: ignore actions referencing files
        switch (act) {
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
                return;

            default: break;
        }
    }
    switch (act) {
        case LED:
            ledFg( arg );
            break;
        case bgLED:
            ledBg( arg );
            break;
        case playSys:
            playSysAudio( arg );
            break;
        case playSubj:
            playSubjAudio( arg );
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
            //      logEvt( "playRec" );
            break;
        case saveRec:
            saveRecAudio( arg );
            //      logEvtS( "saveRec", arg );
            break;
        case finishRec:
            stopRecording();
            //      logEvt( "stopRec" );
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
            adjSubj( iarg );
            break;
        case msgAdj:
            adjMsg( iarg );
            break;
        case goPrevSt:
            setCurrState( TBook.iPrevSt );    // return to prevSt without transition
            // remembers this state as 'prevState' -- does that matter?
            break;
        case saveSt:      // remember state that preceded this one (for possible future return)
            if ( iarg > 4 ) iarg = 4;
            assertValidState( TBook.iPrevSt );
            TBook.iSavedSt[iarg] = TBook.iPrevSt;
            dbgLog( "C svSt[%d] = %s(%d) \n", iarg, TBook.prevStateName, TBook.iPrevSt );
            break;
        case goSavedSt:
            if ( iarg > 4 ) iarg = 4;
            assertValidState( TBook.iSavedSt[iarg] );
            setCurrState( TBook.iSavedSt[iarg] );   // return to savedSt without transition (updates prevSt)
            //      TBook.iNextSt = TBook.iSavedSt[ iarg ];
            // BE: I'm not sure this is right, but it has the side effect of ending the while loop in changeCSMstate.
            // In general, when we return to a saved state, I don't think we want to execute the entrance
            // actions for that state.
            //      TBook.iCurrSt = TBook.iNextSt;
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
            powerDownTBook();
            break;
        case sysTest:
            playSqrTune( "CDEF_**C*D*E*F*_**C*D*E*F*_***C**G**FDEH**G**" );
            break;
        case playNxtPkg:
            playNxtPackage();
            break;
        case changePkg:
            changePackage();
            break;
        case playTune:
            playSqrTune( arg );
            break;
        case sysBoot:
            NVIC_SystemReset();     // soft reboot
            break;
        default: break;
    }
}

// ------------- interpret TBook CSM 
static void changeCSMstate( short nSt, short lastEvtTyp ) {
    dbgEvt( TB_csmChSt, nSt, 0, 0, 0 );
    if ( nSt == TBook.iCurrSt ) {  // no-op transition -- (default for events that weren't defined in control.def)
        //dbgLog( "C %s(%d): %s => %s \n", TBook.cSt->nm, TBook.iCurrSt, eventNm( (Event)lastEvtTyp), gCSt(nSt)->nm );
        //logEvtNSNS( "Noop_evt", "state",TBook.cSt->nm, "evt", eventNm( (Event)lastEvtTyp) ); //DEBUG
        return;
    }

    setCurrState( nSt );
    // now 'in' (executing) state nSt

    /*DEBUG*/  // Update status strings inside TBook -- solely for visibility in the debugger
    for (int e = eNull; e < eUNDEF; e++) {         //DBG: fill in dynamic parts of transition table for current state
        short iState = TBook.cSt->evtNxtState[e];//DBG:
        TBook.evtNxtSt[e].nxtSt   = iState;
        TBook.evtNxtSt[e].nstStNm = gStNm( gCSt( iState ));
    }
    /*END DEBUG*/

    int        nA = nActs( TBook.cSt );
    for (short i  = 0; i < nA; i++) {         // For each action defined on the state...
        csmAction_t *action = gAct( TBook.cSt, i );
        Action act = action->act;  // unpack action, and arg
        char *arg = action->arg;
        if ( arg == NULL ) arg = "";                // make sure its a string for digit test
        // Parse the argument if it looks like it might be numeric.
        int iarg = arg[0] == '-' || isdigit( arg[0] ) ? atoi( arg ) : 0;

        doAction( act, arg, iarg );   // And invoke the action.
        // NB: actions goPrevSt & goSavedSt change TBook.iCurrSt
    }
}

static void tbTimer( void *eNum ) {
    switch ((int) eNum) {
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
                osTimerStart( timers[1], TB_Config->longIdleMS );
            } else {
                sendEvent( LongIdle, 0 );
            }
            break;
        case 2:
            sendEvent( Timer, 0 );
            break;
    }
}


void executeCSM( void ) {               // execute TBook control state machine
    TB_Event   *evt;
    osStatus_t status;
    controlManagerReady = true;

    TBook.volume = TB_Config->default_volume;
    TBook.iSubj  = -1; // makes "next subject" go to the first subject.
    TBook.iMsg   = 0;

    // set initialState & do actions
    TBook.iCurrSt       = -1;   // so initState (which has been assigned to 0) will be different
    TBook.currStateName = "<start>";

    if ( RunQCTest )
        changeCSMstate( TB_Config->qcTestState, 0 );
    else
        changeCSMstate( TB_Config->initState, 0 );

    bool audioIdle = true;
    while (true) {
        evt    = NULL;
        status = osMessageQueueGet( osMsg_TBEvents, &evt, NULL, osWaitForever );  // wait for next TB_Event
        if ( status != osOK )
            tbErr( " EvtQGet error" );
        TBook.lastEvent     = evt->typ;  // has to be copied, used after 'evt' is freed
        TBook.lastEventName = eventNm( TBook.lastEvent );

        //    logEvtNSNI( "csmEvent", "typ", TBook.lastEventName, "dnMS", evt->arg );
        switch (evt->typ) {
            case AudioDone:
                if ( audioIdle )
                    logEvtNS( "error", "msg", "AudioDone without AudioStart" );
                else {
                    audioIdle = true;
                    osTimerStart( timers[0], TB_Config->shortIdleMS );
                    osTimerStart( timers[1], TB_Config->longIdleMS );
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
        short nSt = TBook.cSt->evtNxtState[TBook.lastEvent];
        assertValidState( nSt );
        if ( nSt != TBook.iCurrSt ) // state is changing (unless goPrevSt or goSavedSt undoes it)
            logEvtNSNS( "csmEvt", "evt", TBook.lastEventName, "nSt", gCSt( nSt )->nm );

        osMemoryPoolFree( TBEvent_pool, evt );
        changeCSMstate( nSt, TBook.lastEvent ); // only changes if different
    }
}


static void eventTest() {                    // report Events until DFU (pot table)
    TB_Event   *evt;
    osStatus_t status;

    dbgLog( "EvtTst starPlus\n" );
    while (true) {
        status = osMessageQueueGet( osMsg_TBEvents, &evt, NULL, osWaitForever );  // wait for next TB_Event
        if ( status != osOK )
            tbErr( " EvtQGet error" );

        TBook.lastEventName = eventNm( evt->typ );
        dbgLog( " %s \n", eventNm( evt->typ ));
        dbgEvt( TB_csmEvt, evt->typ, tbTimeStamp(), 0, 0 );
        logEvtNS( "evtTest", "nm", eventNm( evt->typ ));

        if ( evt->typ == Tree )
            osTimerStart( timers[0], TB_Config->shortIdleMS );
        if ( evt->typ == Pot )
            osTimerStart( timers[1], TB_Config->longIdleMS );
        if ( evt->typ == Table )
            osTimerStart( timers[2], 20000 ); // 20 sec

        if ( evt->typ == starPlus ) return;
    }
}

extern bool BootToUSB;
extern uint32_t Mp3FilesToConvert;

void initControlManager( void ) {       // initialize control manager
    EventRecorderEnable( evrEA, TB_no, TBsai_no );  // TB, TBaud, TBsai
    EventRecorderDisable( evrAOD, EvtFsCore_No, EvtFsMcSPI_No );  //FileSys library
    EventRecorderDisable( evrAOD, EvtUsbdCore_No, EvtUsbdEnd_No );  //USB library

    mFileOpSignal = osEventFlagsNew( NULL );      // osEvent channel for signal when .WAV's are ready
    if ( mFileOpSignal == NULL )
        tbErr( "mFileOpSignal alloc failed" );

    bool onlyQcLoaded = false;
    if ( !loadControlDef()) {   // load csm_data.txt if it's there
        preloadCSM();           // or use the preloaded version for QcTest
        logEvtNS( "TB_CSM", "Ver", CSM->Version );        // log CSM version comment
        onlyQcLoaded = true;
    }

    if (BootToUSB) {
        USBmode(true);
        return;
    }

    if ( CSM != NULL ) {     // have a CSM definition
        ledBg( TB_Config->bgPulse );    // reset background pulse according to TB_Config
        setVolume( TB_Config->default_volume );         // set initial volume

        iPkg = 0;
        if ( RunQCTest ) {
            Deployment = NULL;
        } else {      // don't load package for qcTest
            if ( !loadPackageData() || onlyQcLoaded || BootToUSB ) {
                USBmode( true );    // go to USB unless successfully loaded CSM & packages_data
                return;         // don't do anything else after starting USBmode
            }
        }

        TBook.iSubj = -1; // makes "next subject" go to the first subject.
        TBook.iMsg  = 1;

        // don't init power timer-- it is set to do 1st check at 15sec, then resets from TB_Config
        //setPowerCheckTimer( TB_Config->powerCheckMS );
        if ( RunQCTest )
            PowerChecksEnabled = false;     // disable powerChecks during QC

        if ( DecodeMP3 ) {
            decodeAudio();    // start fileOps thread checking for .mp3 files that haven't been decoded

            // and wait for signal when complete
            uint32_t flags = osEventFlagsWait( mFileOpSignal, FILE_DECODE_DONE, osFlagsWaitAny, osWaitForever );
        }

        for (int   it       = 0; it < 3; it++) {
            timers[it] = osTimerNew( tbTimer, osTimerOnce, (void *) it, NULL );
            if ( timers[it] == NULL )
                tbErr( "timer not alloc'd" );
        }
        for (Event e        = eNull; e < eUNDEF; e++) {  //DBG fill in static parts of debug transition table
            TBook.evtNxtSt[e].evt   = e;
            TBook.evtNxtSt[e].evtNm = eventNm( e );
        }
        TBook.prevStateName = "<start>";
        executeCSM();

    } else if ( FileSysOK ) {   // FS but no CSM, go into USB mode
        logEvt( "NoFS USBmode" );
        USBmode( true );
    } else {  // no FileSystem
        eventTest();
    }
}
// contentmanager.cpp 
