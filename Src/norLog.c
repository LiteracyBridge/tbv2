// TBook Rev2  norLog.c  --  nor Flash logging utilities
//   Gene Ball  Oct2020
/*     
//   operations on TBook Log written to NorFLASH memory  
//  W25Q64JV  TBookRev2 U5    datasheet: https://www.mouser.com/datasheet/2/949/w25q64jv_revj_03272018_plus-1489671.pdf
//  3V 64M-BIT  SERIAL FLASH MEMORY WITH DUAL, QUAD SPI
//
//  The W25Q64JV device is directly handled by W25Q64JV.c 
// �The driver sends commands to the chip over SPI, this file only access a few driver functions:
//   � pNor->Initialize,� pNor->PowerControl, & pNor->GetInfo  -- from initNorLog()
//     pNor->ReadData             -- from NLogReadPage() and saveNorLog()
//   � pNor->ProgramData          -- from NLogWrite()
//   � pNor->EraseSector()        -- from eraseNorFlash()
//
//  
//  the NOR-FLASH is organized as a collection of sequential text log files, each starting on a NOR page boundary
//  the 1st NOR page is reserved as an array of 64 start addresses -- Nor address for logs 0..63
//  erased NOR locations read as all 1's, so a non-zero log start_address means that log has been initialized
//
//-- initNorLog( bool startNewLog )
//    initializes NLg structure,
//      NLg.pNor points to ARMDRIVER Driver_Flash0 (defined by ARM_Driver_Flash_(DRIVER_FLASH_NUM) in W25Q64JV.c)
//      NLg.pI points to key NORFLASH parameters that are defined in W25Q64JV.h 
//     then calls findCurrNLog()
//       which finds current log index, base address, and next empty byte
//       by scanning the page of log start_addresses (in page0): 
//        if s_a[0] is erased, -- entire flash is erased, so initialize Idx=0, Base=page1, Nxt=Base
//        otherwise, Idx & Base are set from the last used address, then findLogNext()
//            sets Nxt to first still erased byte beyond Base  (calls TbErr if entire NOR flash is full)
//        if startNewLog has been requested, index is incremented,
//            Base & Nxt are set to next empty page, and page0 start_address[idx] is written with Base 
//        if currLog==63, startNewLog forces a full erase of the NORflash and starts Log0
//-- appendNorLog( text ) 
//   writes text to NOR flash, starting at NLg.Nxt 
//      splits into two calls to NLogWrite if data crosses into next page
//-- saveNorLog( path ) -- writes contents of current log to specified file path
//-- eraseNorFlash( saveCurrLog ) -- erases entire NOR flash, a page at a time
//       (page at a time, which is slow, because pNor->eraseChip() didn't work)
//       if saveCurrLog was requested, writes the current log before erasing, 
//          then appends the file to log0 afterward
**********************/

#include "tbook.h"

const int NOR_BUFFSZ = 260;   // PGSZ + at least one for /0
struct {        // NOR LOG state
    ARM_DRIVER_FLASH *  pNor;
    ARM_FLASH_INFO   *  pI;
    int32_t             currLogIdx;   // index of this log
    int32_t             logBase;      // address of start of log-- 1st page holds up to 64 start addresses-- so flash is wear leveled
    int32_t             Nxt;          // address of 1st erased byte of Nor flash
    uint32_t            PGSZ;         // CONST bytes per page
    uint32_t            SECTORSZ;     // CONST bytes per sector
    uint8_t             E_VAL;        // CONST errased byte value
    int32_t             MAX_ADDR;     // CONST max address in flash
    int32_t             pgNxt;        // index of 1st erased byte of current pg[]
    int32_t  *          startAddr;   // ptr to pg[] as startAddr[]
    int32_t             currPgAddr;   // address of Nor pg currently in pg[]
    char                pg[NOR_BUFFSZ];
} NLg;
const char *    fgNOR_Erasing = "R5G5!";    // alternate 1/2 sec red & green

//
// NOR-flash LOG
const char *    norTmpFile = "M0:/log/svLog.txt";
const int       BUFFSZ  = 260;   // 256 plus space for null to terminate string
const int       N_SADDR = 64;   // number of startAddresses in page0

int             maxBusyWait      = 0;
bool            norErrInProgress = false;
fsTime          lastRTCinLog;

void norErr( const char *fmt, ... ) {
    char s[100];
    norErrInProgress = true;

    va_list arg_ptr;
    va_start( arg_ptr, fmt );
    vsprintf( s, fmt, arg_ptr );
    va_end( arg_ptr );

    tbErr( s );
}

void NLogShowStatus() {                           // write event with NorLog status details
    logEvtNINININI( "NorLog", "Idx", NLg.currLogIdx, "Sz", NLg.Nxt - NLg.logBase, "Max",
                    ( NLg.MAX_ADDR - NLg.PGSZ ) / N_SADDR, "Free%", ( NLg.MAX_ADDR - NLg.Nxt ) * 100 / NLg.MAX_ADDR );
}

int NLogIdx() {                                  // => index of log section currently in use
    return NLg.currLogIdx;
}

bool LogSizeOk() {                                     // TRUE if log below 1/64 of NLog
    int logSz    = NLg.Nxt - NLg.logBase;
    int maxLogSz = ( NLg.MAX_ADDR - NLg.PGSZ ) / N_SADDR;  // ~8Mb/64 = ~128Kb
    return ( logSz < maxLogSz );
}

bool isValidChar( uint8_t ch ) {                  // TRUE for text & newline
    if ( ch > 0x7E ) return false;
    if ( ch >= 0x20 ) return true;
    if ( ch == 0x0A ) return true;
    return false;
}

bool NLogReadPage( uint32_t addr ) {              // read NOR page at 'addr' into NLg.pg & return true if consistent
    if ( addr % NLg.PGSZ != 0 ) {
        dbgLog( "! NLog read not at pg bndry: 0x%08x \n", addr );
        addr = addr - ( addr % NLg.PGSZ );
    }
    uint32_t stat = NLg.pNor->ReadData( addr, NLg.pg, NLg.PGSZ );     // read page at addr into NLg.pg
    if ( stat != NLg.PGSZ ) norErr( " pNor read(%d) => %d", addr, stat );
    NLg.currPgAddr = addr;

    //verify log page consistency-- 0-256 non E_VAL's, followed by E_VAL's
    bool foundErased = false;
    NLg.pgNxt = NLg.PGSZ;   // assume full till find an erased loc
    for (int i = 0; i < NLg.PGSZ; i++) {
        uint8_t ch     = NLg.pg[i];
        bool    isEVAL = ( ch == NLg.E_VAL );
        if ( foundErased ) {
            if ( !isEVAL ) {  // all bytes after first E_VAL must be also
                dbgLog( "! NLogRdPg 0x%08x: [%d] erased but [%d]=0x%02x \n", addr, NLg.pgNxt, i, ch );
                return false;
            }
        } else {
            if ( isEVAL ) { // found first erased byte
                foundErased = true;
                NLg.pgNxt = i;
            } else {
                if ( !isValidChar( ch )) {
                    dbgLog( "! NLogRdPg 0x%08x: non-text! [%d]=0x%02x \n", addr, i, ch );
                    return false;
                }
            }
        }
    }
    //  dbgLog( "6 NLgRd %x: pN=%d \n", addr, NLg.pgNxt );
    return true;
}

void waitWhileBusy() {                            // wait until NorFlash driver doesn't report busy
    ARM_FLASH_STATUS flstat = NLg.pNor->GetStatus();
    int              cnt    = 0;
    while (flstat.busy) { // wait till complete
        cnt++;
        if ( cnt > 10000000 ) norErr( "NLog wait too long \n" );
        flstat = NLg.pNor->GetStatus();
    }
    if ( cnt > maxBusyWait )
        maxBusyWait         = cnt;
    if ( flstat.error )
        norErr( " pNor wait: stat.error \n" );
}

int NLogReadSA() {         // read StartAddress array in page 0 & verify consistency
    //verify NLg. basic consistency
    if ( NLg.pNor != &Driver_Flash0 ) {
        dbgLog( "! bad .pNor = 0x%08x \n", NLg.pNor );
        NLg.pNor = &Driver_Flash0;
    }
    if ( NLg.pI == NULL || NLg.pI->page_size != 256 ) {
        dbgLog( "! bad .pNor = 0x%08x \n", NLg.pNor );
        NLg.pI = NLg.pNor->GetInfo();   // get key NORFLASH parameters (defined in W25Q64JV.h)
    }
    if ( NLg.PGSZ != NLg.pI->page_size ) {
        dbgLog( "! bad .PGSZ = %d \n", NLg.PGSZ );
        NLg.PGSZ = NLg.pI->page_size;
    }
    if ( NLg.SECTORSZ != NLg.pI->sector_size ) {
        dbgLog( "! bad .SECTORSZ = %d \n", NLg.SECTORSZ );
        NLg.SECTORSZ = NLg.pI->sector_size;
    }
    if ( NLg.PGSZ > BUFFSZ ) norErr( "NLog: buff too small" );
    if ( NLg.E_VAL != 0xFF ) {
        dbgLog( "! bad .E_VAL = %d \n", NLg.E_VAL );
        NLg.E_VAL = NLg.pI->erased_value;
    }
    if ( NLg.MAX_ADDR != ( NLg.pI->sector_count * NLg.pI->sector_size ) - 1 ) {
        dbgLog( "! NLg.MAX_ADDR = 0x%08x \n", NLg.pNor );
        NLg.MAX_ADDR = ( NLg.pI->sector_count * NLg.pI->sector_size ) - 1;
    }

    uint32_t stat = NLg.pNor->ReadData( 0, NLg.pg,
                                        NLg.PGSZ );      // read NOR page 0 into NLg.pg -- check consistency & return idx of 1st unused or -1 if invalid, -2 if full
    if ( stat != NLg.PGSZ ) norErr( " pNor read0 => %d", stat );

    uint32_t E_VAL32 = ((((( NLg.E_VAL << 8 ) + NLg.E_VAL ) << 8 ) + NLg.E_VAL ) << 8 ) + NLg.E_VAL;
    NLg.currPgAddr = 0;
    NLg.startAddr  = ( int32_t * ) & NLg.pg;
    uint32_t prvSA = NLg.startAddr[0];
    int      fE    = -1;  // firstEmpty -- index of first erased startAddr
    if ( prvSA == E_VAL32 )
        fE = 0;   // newly erased NOR -- [0] unused
    else if ( prvSA != NLg.PGSZ ) // if in use, must = PGSZ
        dbgLog( "! NLogRdSA-- SA[0]=0x%08x \n", NLg.startAddr[0] );

    for (int i = 1; i < N_SADDR; i++) {
        uint32_t SA = NLg.startAddr[i];
        if ( fE >= 0 ) { // found one empty slot, rest must also be empty
            if ( SA != E_VAL32 )
                dbgLog( "! NLogRdSA-- SA[%d]=0x%08x but SA[d]=0x%08x \n", fE, NLg.startAddr[fE], i, NLg.startAddr[i] );
        } else if ( SA == E_VAL32 ) {  // found first empty slot
            fE = i;
        } else {  // next log start-- must be higher & multiple of PGSZ
            if (( SA % NLg.PGSZ ) != 0 || SA <= prvSA || SA >= NLg.MAX_ADDR ) {
                dbgLog( "! NLogRdSA-- SA[%d]=0x%08x but SA[%d]=0x%08x \n", i - 1, NLg.startAddr[i - 1], i,
                        NLg.startAddr[i] );
                FILE *f = tbOpenWrite( "system/badNorPg0.txt" );
                if ( f == NULL )
                    dbgLog( "6 no FS to save bad NLog Pg0 \n" );
                else {
                    fprintf( f, "NLg.pNor=0x%08x  .pI=0x%08x \n", (unsigned int) NLg.pNor, (unsigned int) NLg.pI );
                    fprintf( f, "NLg.currLogIdx=%d  .logBase=%d .Nxt=0x%08x \n", NLg.currLogIdx, NLg.logBase, NLg.Nxt );
                    fprintf( f, "NLg.PGSZ=%d  .SECTORSZ=%d .E_VAL=0x%02x \n", NLg.PGSZ, NLg.SECTORSZ, NLg.E_VAL );
                    fprintf( f, "NLg.MAX_ADDR=0x%08x  .pgNxt=%d .currPgAddr=0x%08x \n", NLg.MAX_ADDR, NLg.pgNxt,
                             NLg.currPgAddr );
                    for (int a = 0; a < N_SADDR; a += 4)
                        fprintf( f, "%02d: 0x%08x  0x%08x  0x%08x  0x%08x \n", a, NLg.startAddr[a],
                                 NLg.startAddr[a + 1], NLg.startAddr[a + 2], NLg.startAddr[a + 3] );
                    fclose( f );
                }
                return -1;
            }
            prvSA = SA;
        }
    }
    if ( fE < 0 ) fE = N_SADDR;   // index is full!
    return fE;    // return idx of firstEmpty slot
}

void NLogWriteSA( uint32_t idx, uint32_t addr ) { // write pg0: startAddr[ idx ] = addr
    int firstEmpty = NLogReadSA();            // read & verify current Pg0

    if ( idx != firstEmpty )
        dbgLog( "! write stAddr[%d] but not first empty slot fE=%d \n", idx, firstEmpty );
    if ( idx == 0 && addr != NLg.PGSZ )
        dbgLog( "! write stAddr[0], but addr=0x%08x \n", addr );
    else {
        uint32_t prv = idx == 0 ? 0 : NLg.startAddr[idx - 1];
        if ( addr <= prv )
            dbgLog( "! write stAddr[%d]=0x%08x, but stAddr[%d]=0x%08x \n", idx, addr, idx - 1, prv );
        else {  // everything checks out-- write the startAddr
            dbgLog( "6 NLogWrSA: sa[%d] = 0x%08x \n", idx, addr );
            uint32_t stat = NLg.pNor->ProgramData( idx * 4, &addr, 4 );   // write from copy with no E_VALs
            if ( stat != ARM_DRIVER_OK ) norErr( " pNor wr => %d", stat );
            waitWhileBusy();

            NLogReadSA();       // read & verify updated Pg0
        }
    }
}

void NLogWrite( uint32_t addr, const char *data, int len ) {  // write len bytes at 'addr' -- MUST be within one page
    if ( norErrInProgress ) return;

    if ( addr < NLg.PGSZ ) {
        dbgLog( "! write to pg0: 0x%08x  len=%d data=0x%08x \n", addr, len, data );
        __breakpoint( 1 );
        return;   // don't write it
    }
    char     locData[BUFFSZ];
    for (int i    = 0; i < len; i++) {
        if ( data[i] == NLg.E_VAL )
            dbgLog( "! BAD eval data at %d of %len \n", i, len );
        locData[i] = data[i] == NLg.E_VAL ? '.' : data[i];
    }
    int      stpg = addr / NLg.PGSZ, endpg = ( addr + len - 1 ) / NLg.PGSZ;
    if ( stpg != endpg ) norErr( "NLgWr a=%d l=%d", addr, len );

    //dbgLog( "6 NLogWr: %d @ 0x%08x \n", len, addr );
    uint32_t stat = NLg.pNor->ProgramData( addr, locData, len );    // write from copy with no E_VALs
    if ( stat != ARM_DRIVER_OK ) norErr( " pNor wr => %d", stat );
    waitWhileBusy();
}

int nxtBlk( int addr, int blksz ) {              // => 1st multiple of 'blksz' after 'addr'
    return ( addr / blksz + 1 ) * blksz;
}

void scanLogForRTC( uint32_t norAddr ) {
    int32_t logaddr, year, month, date, hour, minute;
    int     cnt = 20;    // scan up to 20 pages
    logaddr = norAddr;
    lastRTCinLog.year = 0;
    while (logaddr > NLg.logBase && cnt > 0) {
        char *rtc = strstr( NLg.pg, "RTC," );
        if ( rtc != NULL && rtc[28] == '.' ) {  // found a "RTC, Dt: 2022-03-09 16:34:45.535"
            int cnt  = sscanf( rtc, "RTC, Dt: %d-%d-%d", &year, &month, &date );
            int cnt2 = sscanf( rtc + 20, "%d:%d", &hour, &minute );
            if ( cnt == 3 && cnt2 == 2 ) {           // successfully parsed RTC date & time
                lastRTCinLog.year = year;
                lastRTCinLog.mon  = month;
                lastRTCinLog.day  = date;
                lastRTCinLog.hr   = hour;
                lastRTCinLog.min  = minute + 1;   // later!
                lastRTCinLog.sec  = 0;
                break;
            }
        }
        logaddr -= NLg.PGSZ;     // check previous page
        NLogReadPage( logaddr );
        cnt--;
    }
    NLogReadPage( norAddr );      // go back to last page
}

void findLogNext() {                              // sets NLg.Nxt to first erased byte in current log
    uint32_t norAddr = NLg.logBase;     // start reading pages of current log
    uint32_t pgcnt   = 0;

    /*  // linear search reads all used pages of log -- gets slow (>6sec for 5900 pages)
    //  while ( norAddr < NLg.MAX_ADDR ){
    //    NLogReadPage( norAddr );
    //    pgcnt++;
    //    if ( NLg.pg[ NLg.PGSZ-1 ] == NLg.E_VAL )
    //      break;  // page isn't full-- find first erased addr
    //    // page was full, read next
    //    norAddr += NLg.PGSZ;
    //  }
    //  int ms = tbTimeStamp() - stTm;
    //  dbgLog("linear: 0x%x %d in %dms \n", norAddr, pgcnt, ms );
    //
    //  if (norAddr >= NLg.MAX_ADDR )
    //    norErr("NorFlash Full");
    */

    // binary search -- find lowest non-full page between logBase & MAX_ADDR
    uint32_t minpg = NLg.logBase / NLg.PGSZ, maxpg = NLg.MAX_ADDR / NLg.PGSZ, midpg = ( minpg + maxpg ) / 2;
    int      pNx;
    norAddr = midpg * NLg.PGSZ;
    while (minpg <= maxpg) { // ex: 0..41 full:  0..63 R31, 32..63 R47, 32..47 R38, 39..47 R43, 39..43 R41, 42..43 R42, 42..42
        NLogReadPage( norAddr );  // sets NLg.currPgAddr = norAddr
        pNx     = NLg.pgNxt;   // NLg.pgNxt=0 (ERASED pg), =PGSZ (Full pg), or idx of first EVAL in page
        pgcnt++;
        if ( pNx > 0 && pNx < NLg.PGSZ ) // found last non-full page
            break;

        if ( pNx == 0 ) { // page is Erased-- keep searching in minpg..midpg-1
            maxpg = midpg - 1;
        } else {  // page is full, search in midpg+1..maxpg
            minpg = midpg + 1;
        }
        midpg   = ( minpg + maxpg ) / 2;
        norAddr = midpg * NLg.PGSZ;
    }
    scanLogForRTC( norAddr );

    if ( pNx == NLg.PGSZ ) { // last page of log is full, move to next
        norAddr += NLg.PGSZ;
        NLogReadPage( norAddr );
        pgcnt++;
        if ( NLg.pgNxt == NLg.PGSZ ) norErr( "NLog BSearch fail" );
    }
    if ( norAddr >= NLg.MAX_ADDR ) // should have been caught when it was being written
        norErr( "NorFlash Full" );

    // verify that NLg.pgNxt is in [0..PGSZ-1], pg[pgNxt-1] is valid, & pg[pgNxt] is ERASED
    if (( pNx < 0 || pNx >= NLg.PGSZ - 1 ) || ( NLg.pg[pNx] != NLg.E_VAL ) ||
        ( pNx > 0 && !isValidChar( NLg.pg[pNx - 1] ))) {
        dbgLog( "6 bSearch: pgNxt=%d pg[%d]=%02x  pg[%d]=%02x \n", pNx, pNx - 1, pNx > 0 ? NLg.pg[pNx - 1] : 0,
                NLg.pg[pNx] );
    }
    dbgLog( "6 NLog bSearch: 0x%x checked %d \n", norAddr, pgcnt );

    NLg.Nxt = NLg.currPgAddr + NLg.pgNxt;   // 1st still-erased location
    dbgLog( "6 flash: Nxt=%d [%d]=0x%02x \n", NLg.Nxt, NLg.pgNxt, NLg.pg[NLg.pgNxt] );
}

void findCurrNLog( bool startNewLog ) {           // read 1st pg (startAddrs) & init NLg for current entry & create next if startNewLog
    int fEmpty     = NLogReadSA();  // reads & checks startAddr[] page
    int lastFull   = fEmpty < 0 ? 0 : fEmpty - 1;
    dbgLog( "6 NLgRdSA => %d, SA[%d]=0x%08x \n", fEmpty, lastFull, NLg.startAddr[lastFull] );

    if ( fEmpty == -1 ) { // inconsistent NOR page 0
        dbgLog( "6 Inconsistent Nor PG0: erase & re-init... \n" );
        eraseNorFlash( false );       // ERASE flash & start new log at beginning
        return;
    }

    if ( lastFull < 0 ) { // entire startAddr page is empty (so entire NorFlash has been erased)
        dbgLog( "6 NLog empty Nor: init log 0 \n" );
        uint32_t pg1 = NLg.PGSZ;    // first log starts at page 1
        NLogWriteSA( 0, pg1 );      // write first entry of startAddr[]
        NLg.logBase = pg1;      // start of log
        NLg.Nxt     = pg1;          // & next empty location
        return;
    }
    NLg.currLogIdx = lastFull;
    NLg.logBase    = NLg.startAddr[lastFull];
    dbgLog( "6 Pg0: Idx=%d logBase=0x%08x \n", NLg.currLogIdx, NLg.logBase );

    findLogNext();          // sets NLg.Nxt to first still-erased byte in current log

    if ( startNewLog ) {    // start a new log at the beginning of the first empty page
        lastFull++;
        if ( lastFull >= N_SADDR ) {
            dbgLog( "6 Full Nor startAddr: erasing... \n" );
            int oldNxt = NLg.Nxt;
            eraseNorFlash( false );       // ERASE flash & start new log at beginning
            logEvtNINI( "ERASE Nor", "logIdx", lastFull, "Nxt", oldNxt );
            return;  // since everything was set by erase
        }
        NLg.currLogIdx = lastFull;
        NLg.logBase    = nxtBlk( NLg.Nxt, NLg.PGSZ );
        NLogWriteSA( lastFull, NLg.logBase );       // write startAddr[ lFull ] to NOR

        NLg.Nxt = NLg.logBase;            // new empty log, Nxt = base
        dbgLog( "6 start Log[%d]: logBase=0x%08x  Nxt=0x%08x \n", NLg.currLogIdx, NLg.logBase, NLg.Nxt );
    }
}

void eraseNorFlash( bool retainCurrentLog ) {            // erase entire chip & re-init with fresh log (or copy of current)
    if ( NLg.pNor == NULL ) return;       // Nor not initialized

    if ( retainCurrentLog )
        saveNorLog( norTmpFile );

    osMutexAcquire( logLock, 0 );     // acquire lock, if not already locked (e.g. filled up during append)

    uint32_t stMS = tbTimeStamp();
    uint32_t cnt  = 0;

    ledFg( fgNOR_Erasing );
    //  bool needToInit = true;
    int           eraseErrCnt = 0;
    for (uint32_t addr        = 0; addr < NLg.MAX_ADDR; addr += NLg.SECTORSZ) {
        int stat = NLg.pNor->EraseSector( addr );
        if ( stat != ARM_DRIVER_OK ) {
            eraseErrCnt++;
            addr -= NLg.SECTORSZ;       // retry this sector
            //norErr(" pNor erase => %d", stat );

        }
        waitWhileBusy();  // non-blocking-- wait till done

        showProgress( "RGR__", 200 );    // eraseNor
        cnt++;
    }
    if ( eraseErrCnt > 0 ) dbgLog( "! eraseNorFlash %d errors \n", eraseErrCnt );
    dbgLog( "! NLog erased %d sectors in %d msec\n", cnt, tbTimeStamp() - stMS );
    endProgress();
    ledFg( NULL );

    findCurrNLog( true );       // creates a new empty currentLog
    osMutexRelease( logLock );      // allow other threads to continue

    //int stat = NLg.pNor->EraseChip( );  //DOESN'T WORK!
    //if ( stat != ARM_DRIVER_OK ) norErr(" pNor erase => %d", stat );

    if (retainCurrentLog) {
        // TODO: What does this mean: "could have some new entries that will be out of order"
        restoreNorLog(norTmpFile);
        fdelete( norTmpFile, NULL );
    }
}

void initNorLog( bool startNewLog ) {             // init driver for W25Q64JV NOR flash
    uint32_t stat;

    // init constants in NLg struct
    NLg.pNor     = &Driver_Flash0;
    NLg.pI       = NLg.pNor->GetInfo();   // get key NORFLASH parameters (defined in W25Q64JV.h)
    NLg.PGSZ     = NLg.pI->page_size;
    NLg.SECTORSZ = NLg.pI->sector_size;       // (4Kb)
    if ( NLg.PGSZ > BUFFSZ ) norErr( "NLog: buff too small" );
    NLg.E_VAL    = NLg.pI->erased_value;
    NLg.MAX_ADDR = ( NLg.pI->sector_count * NLg.pI->sector_size ) - 1;      // 2048 * 4Kb = 8Mb
    NLg.Nxt      = -1;  // mark as NOT fully initialized -- prevent power timer logging during initialization

    stat = NLg.pNor->Initialize( NULL );
    if ( stat != ARM_DRIVER_OK ) norErr( " pNor->Init => %d", stat );

    stat = NLg.pNor->PowerControl( ARM_POWER_FULL );
    if ( stat != ARM_DRIVER_OK ) norErr( " pNor->Pwr => %d", stat );

    findCurrNLog( startNewLog );      // locates (and creates, if startNewLog ) current log-- sets logBase & Nxt

    int logSz = NLg.Nxt - NLg.logBase;
    dbgLog( "6 NorLog: cLogSz=%d NorFilled=%d%% \n", logSz, NLg.Nxt * 100 / NLg.MAX_ADDR );

    /*    lg_thread_attr.name = "Log Erase Thread";
      lg_thread_attr.stack_size = LOG_STACK_SIZE;
      osThreadId_t lg_thread =  osThreadNew( NLogThreadProc, 0, &lg_thread_attr );
      if ( lg_thread == NULL )
        norErr( "logThreadProc not created" );
    */

}

void appendNorLog( const char *s ) {             // append text to Nor flash
    if ( norErrInProgress ) return;
    if ( NLg.pNor == NULL || NLg.Nxt < 0 )
        return;       // Nor not initialized

    int len = strlen( s );

    if ( NLg.Nxt + len > NLg.MAX_ADDR ) {    // NOR flash is full!
        int oldNxt = NLg.Nxt;
        eraseNorFlash( true );        // ERASE flash & reload copy of current log at front
        logEvtNINI( "ERASE Nor", "logIdx", NLg.currLogIdx, "Nxt", oldNxt );
    }

    int nxtPg = nxtBlk( NLg.Nxt, NLg.PGSZ );
    if ( NLg.Nxt + len > nxtPg ) { // crosses page boundary, split into two writes
        int flen = nxtPg - NLg.Nxt;   // bytes on this page
        if ( flen > NLg.PGSZ ) norErr( "append too long" );

        //    dbgLog( "6 app %d @ 0x%08x, %d @ 0x%08x \n", flen, NLg.Nxt, len-flen, NLg.Nxt+flen );
        NLogWrite( NLg.Nxt, s, flen );            // write rest of this page
        NLogWrite( nxtPg, s + flen, len - flen );     // & part on next page

    } else {
        //    dbgLog( "6 app %d @ 0x%08x \n", len, NLg.Nxt );
        NLogWrite( NLg.Nxt, s, len );
    }

    NLg.Nxt += len;
}

void CheckNLog( FILE *f ) {                      // reads & checks Nor Pg0, msgs about any inconsistencies go to file
    int fEmpty = NLogReadSA();  // reads & checks startAddr[] page -- verifies NLog fields

    if ( fEmpty == -1 ) { // inconsistent NOR page 0 reported by ReadSA
        fprintf( f, "CheckNLog: NLogReadSA found inconsistent Pg0-- using in-mem Idx=%d logBase=0x%08x Nxt=0x%08x \n",
                 NLg.currLogIdx, NLg.logBase, NLg.Nxt );
    } else {
        int lastFull = fEmpty - 1;
        if ( NLg.currLogIdx != lastFull ) {
            fprintf( f, "! NLg.currLogIdx = %d not %d \n", NLg.currLogIdx, lastFull );
            NLg.currLogIdx = lastFull;
        }
        if ( NLg.logBase != NLg.startAddr[lastFull] ) {
            fprintf( f, "! NLg.logBase = 0x%08x not 0x%08x \n", NLg.logBase, NLg.startAddr[lastFull] );
            NLg.logBase = NLg.startAddr[lastFull];
        }
    }
}

/**
 * Copy the current NOR log to the given file, or to a default name if nothing specified.
 * @param fpath Optional path to which the log file will be written. MUST be in the 
 *     /log directory.
 */
void saveNorLog( const char *fpath ) {           // copy curr Nor log into file at path
    if ( NLg.pNor == NULL ) return;       // Nor not initialized

    const char *tempLogPath       = "/log/tbTmpLog.txt";
    const char *goodLogPathFormat = "/log/tbLog_%d.txt";
    const char *badLogPathFormat  = "/log/badLog_%d.txt";

    osMutexAcquire( logLock, 0 );     // acquire lock, if not already locked (e.g. filled up during append)
    dbgLog( "6 saveNorLog %s \n", fpath );
    char     logFilePath[40];
    uint32_t stat, msec, tsStart = tbTimeStamp();

    FILE *f = tbOpenWrite( tempLogPath ); //fopen( tmp, "w" );
    if ( f == NULL )
        return; // try to make it into USBmode anyway 
    //norErr("cpyNor fopen err");

    CheckNLog( f );     // check & correct any consistency issues in NLg or Page0
    bool isLogValid     = true;
    int  totalBytesWritten = 0;
    // See comment below.
//    int  maxLogSz     = ( NLg.MAX_ADDR - NLg.PGSZ ) / N_SADDR;  // 1/64th of capacity
//    int  maxLogCnt    = 0;
    int  numInvalidPages = 0;   // count invalid pages found
    dbgLog( "! saveNorLog: %d bytes to %s\n", NLg.Nxt - NLg.logBase, tempLogPath );
    for (int p = NLg.logBase; p < NLg.Nxt; p += NLg.PGSZ) {   // log always starts at page boundary, is full from logBase..Nxt-1
        stat = NLg.pNor->ReadData( p, NLg.pg, NLg.PGSZ );
        if ( stat != NLg.PGSZ ) fprintf( f, "\n saveNorLog read 0x%08x => %d \n", p, stat );

        int bytesToWrite = NLg.Nxt - p;
        if ( bytesToWrite > NLg.PGSZ ) bytesToWrite = NLg.PGSZ;
        for (int i = 0; i < bytesToWrite; i++) {   // verify validity of data
            if ( !isValidChar( NLg.pg[i] )) {
                numInvalidPages++;
                if ( isLogValid ) // first bad char found
                    fprintf( f, "\n bad Log%d 0x%08x: [%d] = 0x%02x logBase=0x%08x Nxt=0x%08x \n", NLg.currLogIdx, p, i,
                             NLg.pg[i], NLg.logBase, NLg.Nxt );
                isLogValid = false;
            }
        }
        stat       = fwrite( NLg.pg, 1, bytesToWrite, f );
        showProgress( "RG___", 200 );   // saveNorLog
        if ( stat != bytesToWrite )
            fprintf( f, "\n saveNorLog fwrite => %d  totalBytesWritten=%d \n", stat, totalBytesWritten );
        if ( numInvalidPages > 20 ) {
            fprintf( f, "\n >20 invalid pages-- stop copying after %d bytes \n", totalBytesWritten );
            break;
        }
        totalBytesWritten += bytesToWrite;
        // TODO: This seems to count how many times "maxLogCnt" the size of this log is. I don't see how
        //  that is useful, or even interesting, and it is never used.
//        if ( totalBytesWritten / maxLogSz > maxLogCnt ) {
//            maxLogCnt++;
//            dbgLog( "! copying: %d maxLogs\n", maxLogCnt );
//        }
    }
    tbFclose( f );   //fclose( f );  // tbTmpLog
    endProgress();

    if ( !isLogValid ) {  
        // bad log-- save to different filename, no matter what caller requested.
        sprintf( logFilePath, badLogPathFormat, NLg.currLogIdx );  // save invalid log-- no path
        dbgLog( "! inconsistent NorLog-- saving to %s \n", logFilePath );
    } else if (fpath == NULL || strlen(fpath) == 0) {
        // If no file name was provided, generate a name based on the log chunk index.
        sprintf(logFilePath, goodLogPathFormat, NLg.currLogIdx);
    } else {
        // otherwise use the provided name.
        strcpy(logFilePath, fpath);
    }

    tbFrename( tempLogPath, logFilePath );

    msec = tbTimeStamp() - tsStart;
    dbgLog( "6 saveNorLog: %s: %d in %d msec \n", logFilePath, totalBytesWritten, msec );
    osMutexRelease( logLock );  // allow other threads to continue
}

void restoreNorLog( const char *fpath ) {        // copy file into current log
    FILE *f = tbOpenRead( fpath ); //fopen( fpath, "r" );
    if ( f == NULL ) norErr( "rstrNor fopen err" );

    int cnt = fread( NLg.pg, 1, NLg.PGSZ, f );
    while (cnt > 0) {
        NLg.pg[cnt] = 0;          // turn into string-- writes one past PGSZ
        appendNorLog( NLg.pg );   // append string to log
        cnt = fread( NLg.pg, 1, NLg.PGSZ, f );
    }
    tbFclose( f );   //fclose( f );
}
//end logger.c

