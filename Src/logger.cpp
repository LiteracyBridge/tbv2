


// TBook Rev2  logger.c  --  event logging module
//   Gene Ball  Apr2018

#include "tbook.h"
#include "logger.h"
#include "controlmanager.h"
#include "random.h"

static FILE *           logF = NULL;      // file ptr for open log file
static int              totLogCh = 0;
static char             statFileNm[60];   // local storage for computing .stat filepaths
bool                    FirstSysBoot = false;

const osMutexAttr_t     logMutex = { "logF_lock", osMutexRecursive, NULL, 0 };
osMutexId_t             logLock;

// const int      MAX_STATS_CACHED = 10;   // avoid warning:  #3170-D: use of a const variable in a constant expression is nonstandard in C
#define                 MAX_STATS_CACHED 10
static int              nRefs = 0;
static short            lastRef[ MAX_STATS_CACHED ];
static short            touched[ MAX_STATS_CACHED ];
static MsgStats         sStats[ MAX_STATS_CACHED ];
const int               STAT_SIZ = sizeof( MsgStats );
//const char *            TBP[pDEVICE_ID_TXT] = "/system/device_ID.txt";
//const char *            TBP[pFIRMWARE_ID_TXT] = "/system/firmware_ID.txt";
//const char *            TBP[pERASE_NOR_LOG_TXT] = "M0:/system/EraseNorLog.txt";      // flag file to force full erase of NOR flash
//const char *            TBP[pSET_RTC_TXT] = "M0:/system/SetRTC.txt";             // flag file to force setting RTC to last modification time of SetRTC.txt
//const char *            TBP[pDONT_SET_RTC_FNAME] = "dontSetRTC.txt";                // renamed version of SetRTC.txt
//const char *            TBP[pDONT_SET_RTC_TXT = "M0:/system/dontSetRTC.txt";           // full path for fexists
//const char *            TBP[pLAST_RTC_TXT = "M0:/system/lastRTC.txt";           // written at powerdown-- modification date used to reset clock
//const char *            TBP[pTB_LOG_NN_TXT = "M0:/log/tbLog_%d.txt";     // file name of previous log on first boot

const int               MAX_EVT_LEN1 = 32, MAX_EVT_LEN2 = 64;

// Constants for auxillary file names
const char *            auxFilePrefixes[] = {"uf", "msg"};
const char *            auxFileExtensions[] = {".wav", ".txt"};


extern fsTime           lastRTCinLog;
extern bool             BootToUSB;
extern bool             BootDebugLoop;
extern bool             BootVerboseLog;
extern bool             bootToQcTest;
extern bool             BootVerbosePower;
extern bool             BootResetLog;
extern bool             BootFormatFileSys;



/*  //DEBUG ONLY:  debugger accessible log history
TBH_arr         TBH;  

static void     initTBHistory(){
  for ( int i=0; i < MAX_TB_HIST; i++ ){
    TBH[i*2] = tbAlloc( MAX_EVT_LEN1, "log hist" );
    strcpy( TBH[i*2], "---" );
    TBH[i*2+1] = tbAlloc( MAX_EVT_LEN2, "log hist2" );
    TBH[i*2+1][0] = 0;
  }
  Dbg.TBookLog = &TBH;
}
static void     addHist( const char * s1, const char * s2 ){      // DEBUG:  prepend evtMsg to TBHist[] for DEBUG
  if ( TBH[0]==NULL ) 
    initTBHistory();
  
  char * old1 = TBH[ (MAX_TB_HIST-1)*2 ];
  char * old2 = TBH[ (MAX_TB_HIST-1)*2+1 ];
  for ( int i=(MAX_TB_HIST-1); i>0; i-- ){
    TBH[i*2+1] = TBH[(i-1)*2+1];
    TBH[i*2] = TBH[(i-1)*2];
  }
  TBH[0] = old1;
  TBH[1] = old2;
  strcpy( old1, s1 );
  strcpy( old2, s2 );
}
*/

char *loadLine( char *line, const char *fpath, fsTime *tm ) {    // => 1st line of 'fpath'
    const fsTime nullTm = { 0, 0, 0, 1, 1, 2020 };  // midnight 1-Jan-2020
    if ( tm != NULL ) *tm = nullTm;
    strcpy( line, "---" );
    FILE *stF = tbOpenReadBinary( fpath );
    if ( stF == NULL ) return line;

    char *txt = fgets( line, 200, stF );
    tbFclose( stF );   //fclose( stF );

    if ( txt == NULL ) return line;

    char *pRet = strchr( txt, '\n' );
    if ( pRet != NULL ) *pRet = 0;

    fsFileInfo fAttr;
    fAttr.fileID = 0;
    fsStatus fStat = ffind( fpath, &fAttr );
    if ( fStat != fsOK ) {
        dbgLog( "! FFind => %d for %s \n", fStat, fpath );
        return txt;
    }
    if ( tm != NULL )
        *tm = fAttr.time;
    return txt;
}

// Read the timestamp of a file.
//   char   *path   - The path to the file for which to get the timestamp
//   fsTime *time   - Where to store the time, if possible.
// return true if the time was obtained, false otherwise.
bool getFileTime( const char *path, fsTime *time ) {
    fsFileInfo fAttr;
    fAttr.fileID = 0;
    fsStatus fStat = ffind( path, &fAttr );
    if ( fStat != fsOK ) {
        dbgLog( "! FFind => %d for %s \n", fStat, path );
        return false;
    }
    if ( time != NULL ) {
        *time = fAttr.time;
        return true;
    }
    return false;
}

/// \brief Create or truncate a file and write data to it. The data is presumed to be a nul ('\0')
/// terminated string. A '\n' character is appended to the end.
/// \param[in] line     Nul terminated string.
/// \param[in] fpath    Name of the file to be created / truncated.
/// \return             nothing
void writeLine( const char *line, const char *fpath ) {
    FILE *stF = tbOpenWriteBinary( fpath );
    if ( stF != NULL ) {
        int nch = fprintf( stF, "%s\n", line );
        tbFclose( stF );        //int err = fclose( stF );
        dbgEvt( TB_wrLnFile, nch, 0, 0, 0 );
    }
}

bool openLog( bool forRead ) {
    fsFileInfo fAttr;
    fAttr.fileID = 0;
    fsStatus fStat = ffind( TBP[pLOG_TXT], &fAttr );
    if ( fStat != fsOK )
        dbgLog( "! Log missing => %d \n", fStat );
    else
        dbgLog( "6 Log sz= %d \n", fAttr.size );

    if ( logF != NULL )
        errLog( "openLog logF=0x%x ! \n", logF );
    logF     = fopen( TBP[pLOG_TXT], forRead ? "r" : "a" );    // append -- unless checking
    totLogCh = 0;
    if ( logF == NULL ) {
        TBDataOK = false;
        errLog( "Log file failed to open: %s", TBP[pLOG_TXT] );
        return false;
    }
    return true;
}

void checkLog() {       //DEBUG: verify tbLog.txt is readable
    if ( !openLog( true )) return;  // err msg from openLog
    int  linecnt = 0, charcnt = 0;
    char line[200];
    while (fgets( line, 200, logF ) != NULL) {
        linecnt++;
        charcnt += strlen( line );
    }
    tbFclose( logF );    //int err = fclose( logF );
    logF = NULL;
    dbgEvt( TB_chkLog, linecnt, charcnt, 0, 0 );
    dbgLog( "6 checkLog: %d lns, %d chs \n", linecnt, charcnt );
}

void closeLog() {
    if ( logF != NULL ) {
        int err1 = fflush( logF );
        tbFclose( logF );    //int err2 = fclose( logF );
        dbgLog( "6 closeLog fflush=%d \n", err1 );
        dbgEvt( TB_wrLogFile, totLogCh, err1, 0, 0 );
    }
    logF = NULL;
    //checkLog(); //DEBUG
}

void saveLastTime( fsTime rtc ) {
    if ( rtc.year < 2022 ) {
        dbgLog( "! save invalid date\n" );
        return;
    }
    if ( !fexists( TBP[pLAST_RTC_TXT] ))
        writeLine( "---", TBP[pLAST_RTC_TXT] );  // make sure it's there
    ftime_set( TBP[pLAST_RTC_TXT], &rtc, &rtc, &rtc );  // set create,access,write times to RTC
}

void dateStr( char *s, fsTime dttm ) {
    sprintf( s, "%d-%d-%d %02d:%02d", dttm.year, dttm.mon, dttm.day, dttm.hr, dttm.min );
}

void startNextLog() {         // save current log, switch to next index
    if ( !BootResetLog )   // if Reset-- skip saving a log with problems
        saveNorLog( "" );     // save with default name
    initNorLog( true );
}

bool validDateTime( fsTime *dtTm ) {
    if ( dtTm->year < 2022 ) return false;
    if ( dtTm->mon < 1 || dtTm->mon > 12 ) return false;
    if ( dtTm->day < 1 || dtTm->day > 31 ) return false;

    if ( dtTm->hr > 23 ) return false;
    if ( dtTm->min > 59 ) return false;
    if ( dtTm->sec > 59 ) return false;
    return true;
}

fsTime *laterDtTm( fsTime *tm1, fsTime *tm2 ) {    // returns later datetime of tm2 & tm2
    if ( !validDateTime( tm1 )) return tm2;
    if ( !validDateTime( tm2 )) return tm1;

    // both valid -- use more recent
    if ( tm1->year != tm2->year ) return tm1->year > tm2->year ? tm1 : tm2;
    if ( tm1->mon != tm2->mon ) return tm1->mon > tm2->mon ? tm1 : tm2;
    if ( tm1->day != tm2->day ) return tm1->day > tm2->day ? tm1 : tm2;
    if ( tm1->hr != tm2->hr ) return tm1->hr > tm2->hr ? tm1 : tm2;
    if ( tm1->min != tm2->min ) return tm1->min > tm2->min ? tm1 : tm2;
    if ( tm1->sec != tm2->sec ) return tm1->sec > tm2->sec ? tm1 : tm2;
    return tm1;  // identical
}

void logPowerUp( bool reboot ) {                      // re-init logger after reboot, USB or sleeping
    char   line[200];
    char   dt[30];
    fsTime bootDt, rtcDt;

    logF = NULL;
    //checkLog(); //DEBUG
    //if ( !openLog(false) ) return;

    // "M0:/system/bootcount.txt"
    char *boot = loadLine( line, TBP[pBOOTCOUNT_TXT], &bootDt );
    int bootcnt = 1;    // if file not there-- first boot
    if ( boot != NULL ) sscanf( boot, " %d", &bootcnt );
    FirstSysBoot = ( bootcnt == 1 );

    bool StartNewLog = false;

    if ( FirstSysBoot )     // advance log idx if FirstBoot
        StartNewLog = true;

    if ( !LogSizeOk())     // advance log idx automatically if Log has grown too large
        StartNewLog = true;

    if ( BootResetLog )     // also start new log if ResetLog Boot  
        StartNewLog = true;

    if ( StartNewLog ) {    // switch to new log index
        bool eraseNor = fexists( TBP[pERASE_NOR_LOG_TXT] );
        if ( eraseNor )  // if M0:/system/EraseNorLog.txt exists-- erase
            eraseNorFlash( false );     // CLEAR nor & create a new log
        else {     // First Boot starts a new log (unless already done by erase)
            //  sprintf(line, TBP[pTB_LOG_NN_TXT, NLogIdx() );    //TODO: not needed
            startNextLog();
        }
    }

    bool gotRtc = false;
    if ( reboot ) {
        totLogCh = 0;     // tot chars appended
        if ( logF != NULL ) fprintf( logF, "\n" );
        sprintf( line, " %s %s %s %s %s %s ", BootToUSB ? "USB" : "", BootResetLog ? "ResetLog" : "",
                 BootFormatFileSys ? "FormatFilesys" : "", bootToQcTest ? "QCTest" : "",
                 BootVerboseLog ? "VerboseLog" : "", BootVerbosePower ? "VerbosePwr" : "" );
        logEvtNS( "REBOOT --------", "BootKeys", line );
        gotRtc = showRTC();      // show current RTC time, or false if unset
        if ( !gotRtc ) {
            // RTC unset after hard power down (e.g. battery change) (or DFU). Reset from the last time we knew. Certainly
            // the wrong time, possibly by a huge amount, but at least time increases monotonically.

            fsTime jan22 = { 0, 0, 0, 1, 1, 2022 };  // hr min sec, day, mon, year = 2022-01-01 00:00:00 

            if ( !getFileTime( TBP[pLAST_RTC_TXT], &rtcDt ))   // timestamp of lastRTC.txt (from last proper shutdown)
                rtcDt.year = 0; //if invalid  

            int    major, minor;
            fsTime firmwareTime;
            // set fw to date from TBV2_Version  (e.g. V3.1 of 2022-22-04 13:43:36)
            if ( sscanf( TBV2_Version, "V%u.%u of %hu-%hhu-%hhu %hhu:%hhu:%hhu", &major, &minor, &firmwareTime.year,
                         &firmwareTime.mon, &firmwareTime.day, &firmwareTime.hr, &firmwareTime.min, &firmwareTime.sec ) != 8 )
                firmwareTime.year = 0;  // firmware date invalid

            dateStr( dt, rtcDt );
            logEvtNS( "foundRTC", "lastRtc", dt );
            dateStr( dt, firmwareTime );
            logEvtNS( "foundRTC", "firmwareRtc", dt );
            dateStr( dt, lastRTCinLog );
            logEvtNS( "foundRTC", "lastRtcInLog", dt );

            fsTime *latest = &jan22;                        // default if all others invalid
            latest = laterDtTm( latest, &rtcDt );           // use lastRTC timestamp if valid
            latest = laterDtTm( latest, &firmwareTime );           // use firmware date if later than lastRTC
            latest = laterDtTm( latest, &lastRTCinLog );    // lastRTCinLog may have been set by initNorLog -- from last RTC log entry

            // latest -> most recent valid time of jan22, rtcDt, firmwareTime, lastRTCinLog

            /*
            bool useFileTime = haveTime && (rtcDt.year >= lastRTCinLog.year) && (rtcDt.mon >= lastRTCinLog.mon) && (rtcDt.day >= lastRTCinLog.day) &&
                (rtcDt.hr >= lastRTCinLog.hr) && (rtcDt.min >= lastRTCinLog.min) && (rtcDt.sec >= lastRTCinLog.sec); 
            
            if ( useFileTime ){
              dateStr( dt, rtcDt );
              setupRTC( rtcDt );    
            } else {
              dateStr( dt, lastRTCinLog );
              setupRTC( lastRTCinLog );    
            }                
            */
            dateStr( dt, *latest );
            setupRTC( *latest );

            logEvtNS( "resetRTC", "DtTm", dt );
            showRTC();
        }

        char *oldFW = loadLine( line, TBP[pFIRMWARE_ID_TXT], &bootDt );
        bool haveNewFW = strcmp( oldFW, TBV2_Version ) != 0;
        if ( haveNewFW )
            logEvtNS( "TB_V2", "Old_Firmware", oldFW );
        logEvtNS( "TB_V2", "Firmware", TBV2_Version );

        writeLine((char *) TBV2_Version, TBP[pFIRMWARE_ID_TXT] );
        //  logEvtFmt( "BUILT", "on: %s, at: %s", __DATE__, __TIME__);  // date & time LOGGER.C last compiled -- link date?
        logEvtNS( "CPU", "Id", CPU_ID );
        logEvtNS( "TB_ID", "Id", TB_ID );
        if ( !fexists( TBP[pDEVICE_ID_TXT] ))
            writeLine( TB_ID, TBP[pDEVICE_ID_TXT] );
        logEvtNI( "CPU_CLK", "MHz", SystemCoreClock / 1000000 );
        logEvtNINI( "BUS_CLK", "APB2", APB2_clock, "APB1", APB1_clock );
    } else
        logEvt( "RESUME--" );

    logEvtNI( "BOOT", "cnt", bootcnt );
    dbgEvt( TB_bootCnt, bootcnt, 0, 0, 0 );
    NLogShowStatus();


    // IF a file SetRTC.txt exists, use it's timestamp to set the RTC. The timestamp will likely be at least
    // several seconds in the past, but should be "good enough".`
    if ( fexists( TBP[pSET_RTC_TXT] )) {
        bool haveTime = getFileTime( TBP[pSET_RTC_TXT], &rtcDt );
        if ( haveTime && rtcDt.year >= 2022 ) {  // got a valid date/time
            dateStr( dt, rtcDt );
            logEvtNS( "setRTC", "DtTm", dt );
            setupRTC( rtcDt );
            showRTC();
            gotRtc        = true;
            // rename setRTC.txt to dontSetRTC.txt -- leave it as a comment for how this works
            if ( fexists( TBP[pDONT_SET_RTC_TXT] ))
                fdelete( TBP[pDONT_SET_RTC_TXT], NULL );
            uint32_t stat = frename( TBP[pSET_RTC_TXT], TBP[pDONT_SET_RTC_FNAME] );
            if ( stat != fsOK ) {
                errLog( "frename %s to %s => %d \n", TBP[pSET_RTC_TXT], TBP[pDONT_SET_RTC_FNAME], stat );
            }
            saveLastTime( rtcDt );
        }
    }

    //boot complete!  count it
    bootcnt++;
    sprintf( line, " %d", bootcnt );
    writeLine( line, TBP[pBOOTCOUNT_TXT] );
}

void logPowerDown( void ) {                             // save & shut down logger for USB or sleeping
    flushStats();

    fsTime   rtcTm;
    uint32_t msec;
    showRTC();          // put current RTC into log
    getRTC( &rtcTm, &msec );  // current RTC
    saveLastTime( rtcTm );
    //  if ( !fexists( TBP[pLAST_RTC_TXT )) writeLine( "---", TBP[pLAST_RTC_TXT );  // make sure it's there
    //  ftime_set( TBP[pLAST_RTC_TXT, &rtcTm, &rtcTm, &rtcTm );  // set create,access,write times to RTC

    //  saveNorLog( "" );   // auto log name

    if ( logF == NULL ) return;
    //  logEvtNI( "WrLog", "nCh", totLogCh );
    closeLog();
}

void loggerThread() {
}

void initLogger( void ) {                               // init tbLog file on bootup
    logLock = osMutexNew( &logMutex );
    initNorLog( false );        // init NOR & find current log

    logPowerUp( true );

    //registerPowerEventHandler( handlePowerEvent );
    memset( lastRef, 0, ( sizeof lastRef ));
    memset( touched, 0, ( sizeof touched ));
    dbgLog( "4 Logger initialized \n" );
}

static char *statFNm( const char *nm, short iS, short iM ) {   // INTERNAL: fill statFileNm for  subj 'nm', iSubj, iMsg
    sprintf( statFileNm, "%sP%d_%s_S%d_M%d.stat", TBP[pSTATS_PATH], iPkg, nm, iS, iM );
    return statFileNm;
}


/**
 * Given a pattern of the form "xxxx_*yyyy", return the next unused value.
 * Gaps are ignored, and the next value greater than the highest value found is
 * returned. If no matching files are found, returns 0.
 * The uniquifier starts after the final '_' in the matching filenames. If no
 * '_', the uniquifier will be 0.
 *
 * @param fnPatt Search pattern of filenames for which to search.
 * @return the next uniquifier greater than the largest suffix found, 0 if none found.
 */
int findFilenameUniquifier( const char *fnPatt ) {
    fsFileInfo fInfo;
    fInfo.fileID = 0;
    int nextUniquifier = 0;
    uint32_t mask = 0;
    FileSysPower( true );
    while (ffind( fnPatt, &fInfo ) == fsOK) {
//        printf("found %s\n", fInfo.name);
        // uniqueness counter is digits after rightmost _
        char *pSep = strrchr( fInfo.name, '_' );
        if ( pSep ) {
            int v = atoi( pSep + 1 );
            if (v<32) mask |= (1<<v);
            if ( v >= nextUniquifier ) nextUniquifier = v + 1;
        }
    }
    tbDelay_ms(50);
    printf("uniquifier: %d, mask: %08x\n", nextUniquifier, mask);
    tbDelay_ms(50);
    return nextUniquifier;
}

/**
 * Add '_N' to 'foo.bar' to create unique 'foo_N.bar'
 * NOTE: Multiple threads can return the same value.
 * NOTE: The extension must be less than 30 characters, including the dot.
 *       The buffer must be large enough to add the uniquifier. 4 characters is probably safe.
 * @param buf [in] file name, [out] file name modified to be unique.
 * @param bufSize size of buffer.
 * @return the uniquifier.
 */
int uniquifyFilename(char *buf, int bufSize) {
    // Find, and save, any extension.
    char ext[30] = "";
    char *pUnique = strrchr(buf, '.');
    if (pUnique) {
        strncpy(ext, pUnique, sizeof(ext));
        ext[sizeof(ext)-1] = '\0';
    } else {
        pUnique = buf + strlen(buf);
    }
    // add "_*" before the extension, then search for any existing files with uniquifiers.
    strcpy(pUnique, "_*");
    int uniquifier = findFilenameUniquifier(buf);
    // Make the input file name unique.
    sprintf(pUnique, "_%d%s", uniquifier, ext);
    return uniquifier;
}

/**
 * Create a filename for an "auxillary" file, associated with the current content message.
 * The filename will be appended with a uniquifier to avoid collisions.
 *
 * There are two kinds of "auxillary" files, ".wav" files, which are audio recordings,
 * and ".txt" files which are created by the control.def script. Every call to this for
 * a given audio file will return a new uniquifier for the file, irrespective of
 * the auxillary file type.
 *
 * @param buf Buffer into which to build the filename. Should be [MAX_PATH]
 * @param auxFileType enum AUX_FILE_TYPE describing what flavor of auxillary file.
 * @return the uniquifier.
 */
int makeAuxFileName( char *buf, enum AUX_FILE_TYPE auxFileType ) {
    char pattern[MAX_PATH];
    sprintf( pattern, "%s%s_pkg%d_pl%d_msg%d_*", TBP[pRECORDINGS_PATH], auxFilePrefixes[auxFileType], iPkg,
             TBook.iSubj, TBook.iMsg );
    int uniquifier = findFilenameUniquifier( pattern );
    sprintf( buf, "%s%s_pkg%d_pl%d_msg%d_%d%s", TBP[pRECORDINGS_PATH], auxFilePrefixes[auxFileType], iPkg, TBook.iSubj,
             TBook.iMsg, uniquifier, auxFileExtensions[auxFileType] );
    return uniquifier;
}

/**
 * Saves properties about the recording to an auxillary file. This file will be filled
 * in further, farther down the processing pipeline.
 */
void saveAuxProperties( char *baseFilename ) {
    char fnBuf[MAX_PATH];
    strcpy( fnBuf, baseFilename );
    char *pSep = strrchr( fnBuf, '.' );
    if ( pSep ) {
        strncpy( pSep, ".properties", sizeof( fnBuf ) - ( pSep - fnBuf ) - 1 );
        fnBuf[sizeof( fnBuf ) - 1] = '\0';
        FILE *outFP                = tbOpenWrite( fnBuf );
        fprintf( outFP, "PACKAGE_NAME:%s\n", currPkg->getName() );
        fprintf( outFP, "PACKAGE_NUM:%d\n", currPkg->pkgIdx );
        if ( TBook.iSubj >= 0 ) {
            fprintf( outFP, "PLAYLIST_NAME:%s\n", currPkg->subjects[TBook.iSubj]->getName() );
            fprintf( outFP, "PLAYLIST_NUM:%d\n", TBook.iSubj );
            if ( TBook.iMsg >= 0 ) {
                // TODO: simplify !!
                fprintf( outFP, "MESSAGE_NAME:%s\n",
                         currPkg->subjects[TBook.iSubj]->messages[TBook.iMsg]->filename );
                fprintf( outFP, "MESSAGE_NUM:%d\n", TBook.iMsg );
            }
        }
        if (controlManager.isSurveyActive()) {
            fprintf(outFP, "SURVEY_UUID=%s\n", random.formatUUID(controlManager.getSurveyUUID()));
        }
        fprintf( outFP, "DEVICE_ID:%s\n", TB_ID );
        tbFclose( outFP );
    }
}

void saveStats( MsgStats *st ) {                        // save statistics block to file
    char *fnm = statFNm( st->SubjNm, st->iSubj, st->iMsg );
    FILE *stF = tbOpenWriteBinary( fnm );
    int  cnt  = fwrite( st, STAT_SIZ, 1, stF );
    tbFclose( stF );   //int err = fclose( stF );
    dbgEvt( TB_wrStatFile, st->iSubj, st->iMsg, cnt, 0 );
}

void flushStats() {                                     // save all cached stats files
    short      cnt = 0;
    for (short i   = 0; i < MAX_STATS_CACHED; i++) {
        if ( touched[i] != 0 ) {
            saveStats( &sStats[i] );
            touched[i] = 0;
            cnt++;
        }
    }
    logEvtNI( "SvStats", "cnt", cnt );
}

MsgStats *loadStats( const char *subjNm, short iSubj, short iMsg ) {   // load statistics snapshot for Subj/Msg
    int oldestIdx = 0;  // becomes idx of block with lowest lastRef
    nRefs++;
    for (short i = 0; i < MAX_STATS_CACHED; i++) {
        if ( lastRef[i] > 0 && sStats[i].iSubj == iSubj && sStats[i].iMsg == iMsg ) {
            lastRef[i] = nRefs; // Most Recently Used
            touched[i] = 1;
            return &sStats[i];
        } else if ( lastRef[i] < lastRef[oldestIdx] )
            oldestIdx = i;
    }
    // not loaded -- load into LRU block
    MsgStats *st = &sStats[oldestIdx];
    if ( touched[oldestIdx] != 0 ) {
        saveStats( &sStats[oldestIdx] );    // save it, if occupied
        touched[oldestIdx] = 0;
    }

    short newStatIdx = oldestIdx;
    char *fnm = statFNm( subjNm, iSubj, iMsg );
    FILE *stF           = tbOpenReadBinary( fnm );
    if ( stF == NULL || fread( st, STAT_SIZ, 1, stF ) != 1 ) {  // file not defined
        if ( stF != NULL ) {
            dbgLog( "! stats S%d, M%d size wrong \n", iSubj, iMsg );
            tbFclose( stF );   //fclose( stF );
        }
        memset( st, 0, STAT_SIZ );    // initialize new block
        st->iSubj = iSubj;
        st->iMsg  = iMsg;
        strncpy( st->SubjNm, subjNm, MAX_SUBJ_NM );
        st->SubjNm[MAX_SUBJ_NM - 1] = '\0';
    } else { // success--
        dbgEvt( TB_LdStatFile, iSubj, iMsg, 0, 0 );
        tbFclose( stF );   //fclose( stF );
    }
    lastRef[newStatIdx] = nRefs;
    touched[newStatIdx] = 1;
    return st;
}

// logEvt formats
void logEvt( const char *evtID ) {                      // write log entry: 'EVENT, at:    d.ddd'
    logEvtS( evtID, "" );
}

void norEvt( const char *s1, const char *s2 ) {
    appendNorLog( s1 );
    if ( s2 != NULL && strlen( s2 ) != 0 ) {
        appendNorLog( ", " );
        appendNorLog( s2 );
    }
    appendNorLog( "\n" );
}

void logEvtS( const char *evtID, const char *args ) {   // write log entry: 'm.ss.s: EVENT, ARGS'
    char     evtBuff[MAX_EVT_LEN2];
    fsTime   tmdt;
    uint32_t msec;
    getRTC( &tmdt, &msec );
    sprintf( evtBuff, "%02d_%02d_%02d.%03d: %8s", tmdt.hr, tmdt.min, tmdt.sec, msec, evtID );
    dbgLog( " %s %s\n", evtBuff, args );

    if (( osKernelGetState() == osKernelRunning ) && osMutexAcquire( logLock, osWaitForever ) != osOK ) {
        dbgLog( "! logLock lost %s \n", evtID );
        return;
    }
    norEvt( evtBuff, args );  // WRITE to NOR Log
    if ( logF != NULL ) {
        int nch = fprintf( logF, "%s, %s\n", evtBuff, args );
        if ( nch < 0 ) {
            // Error writing to log file. Mostly ignore the error and soldier on. NOR is what counts.
            dbgLog( "! LogErr: %d \n", nch );
        } else {
            totLogCh += nch;
        }
        int err = fflush( logF );

        dbgEvt( TB_flshLog, nch, totLogCh, err, 0 );
        if ( err < 0 )
            dbgLog( "! Log flush err %d \n", err );
    }
    if (( osKernelGetState() == osKernelRunning ) && osMutexRelease( logLock ) != osOK ) tbErr( "logLock!" );
}

void logEvtFmt( const char *evtID, const char *fmt, ... ) {
    va_list args1;
    va_start( args1, fmt );
    va_list args2;
    va_copy( args2, args1 );
    char buf[1 + vsnprintf( NULL, 0, fmt, args1 )];
    va_end( args1 );
    vsnprintf( buf, sizeof buf, fmt, args2 );
    va_end( args2 );

    logEvtS( evtID, buf );
}

void logEvtNS( const char *evtID, const char *nm,
               const char *val ) { // write log entry: "EVENT, at:    d.ddd, NM: 'VAL' "
    char args[300];
    sprintf( args, "%s: '%s'", nm, val );
    logEvtS( evtID, args );
}

void logEvtNSNS( const char *evtID, const char *nm, const char *val, const char *nm2,
                 const char *val2 ) {  // write log entry
    char args[300];
    sprintf( args, "%s: '%s', %s: '%s'", nm, val, nm2, val2 );
    logEvtS( evtID, args );
}

void logEvtNSNSNS( const char *evtID, const char *nm, const char *val, const char *nm2, const char *val2,
                   const char *nm3, const char *val3 ) { // write log entry
    char args[300];
    sprintf( args, "%s: '%s', %s: '%s', %s: '%s'", nm, val, nm2, val2, nm3, val3 );
    logEvtS( evtID, args );
}

void logEvtNSNINI( const char *evtID, const char *nm, const char *val, const char *nm2, int val2, const char *nm3,
                   int val3 ) {
    char args[300];
    sprintf( args, "%s: '%s', %s: %d, %s: %d", nm, val, nm2, val2, nm3, val3 );
    logEvtS( evtID, args );
}

void logEvtNSNININS( const char *evtID, const char *nm, const char *val, const char *nm2, int val2, const char *nm3,
                     int val3, const char *nm4, const char *val4 ) {
    char args[300];
    sprintf( args, "%s: '%s', %s: %d, %s: %d, %s: '%s'", nm, val, nm2, val2, nm3, val3, nm4, val4 );
    logEvtS( evtID, args );
}

void logEvtNININI( const char *evtID, const char *nm, int val, const char *nm2, int val2, const char *nm3, int val3 ) {
    char args[300];
    sprintf( args, "%s: %d, %s: %d, %s: %d", nm, val, nm2, val2, nm3, val3 );
    logEvtS( evtID, args );
}

void logEvtNINININI( const char *evtID, const char *nm, int val, const char *nm2, int val2, const char *nm3, int val3,
                     const char *nm4, int val4 ) {
    char args[300];
    sprintf( args, "%s: %d, %s: %d, %s: %d, %s: %d", nm, val, nm2, val2, nm3, val3, nm4, val4 );
    logEvtS( evtID, args );
}

void logEvtNSNI( const char *evtID, const char *nm, const char *val, const char *nm2, int val2 ) {  // write log entry
    char args[300];
    sprintf( args, "%s: '%s', %s: %d", nm, val, nm2, val2 );
    logEvtS( evtID, args );
}

void logEvtNINS( const char *evtID, const char *nm, int val, const char *nm2, const char *val2 ) { // write log entry
    char args[300];
    sprintf( args, "%s: '%d', %s: %s", nm, val, nm2, val2 );
    logEvtS( evtID, args );
}

void logEvtNSNINS( const char *evtID, const char *nm, const char *val, const char *nm2, int val2, const char *nm3,
                   const char *val3 ) { // write log entry
    char args[300];
    sprintf( args, "%s: '%s', %s: %d, %s: '%s'", nm, val, nm2, val2, nm3, val3 );
    logEvtS( evtID, args );
}

void logEvtNINI( const char *evtID, const char *nm, int val, const char *nm2, int val2 ) {  // write log entry
    char args[300];
    sprintf( args, "%s: %d, %s: %d", nm, val, nm2, val2 );
    logEvtS( evtID, args );
}

void logEvtNI( const char *evtID, const char *nm, int val ) {     // write log entry: "EVENT, at:    d.ddd, NM: VAL "
    char args[300];
    sprintf( args, "%s: %d", nm, val );
    logEvtS( evtID, args );
}
//end logger.c

