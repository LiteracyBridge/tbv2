// TBook Rev2  log.h  --  C event logger
//   Gene Ball  Apr2018
#ifndef LOG_H
#define LOG_H
#include "tbook.h"
#include "Driver_Flash.h"
#include "Driver_SPI.h"

extern ARM_DRIVER_FLASH Driver_Flash0;

#define	MAX_SUBJ_NM 	15

typedef struct {	// MsgStats -- stats for iSubj/iMsg
	short		iSubj;
	short 		iMsg;
	char		SubjNm[ MAX_SUBJ_NM ];
	
	short		Start;			// count of StartPlayback's
	short		Finish;
	short		Pause;
	short		Resume;
	short		JumpFwd;
	short		JumpBk;
	short		Left;					// count of abandoned playbacks (slept or went elsewhere)
	short		LeftMinPct;		// min pct played before pause
	short		LeftMaxPct;		// max pct played before pause
	short		LeftSumPct;		// sum of pcts played before pause
	short		RecStart;
	short		RecPause;
	short		RecResume;
	short		RecSave;
	short		RecLeft;		// count of recorded but never saved
} MsgStats;


//
// external functions
extern void			initLogger( void );												// init tbLog file on bootup
extern void			logPowerUp( bool reboot );								// re-init logger after USB or sleeping
extern char *		loadLine( char * line, char * fpath, fsTime *tm );		// => 1st 200 chars of 'fpath' & *tm to lastAccessed
extern void			logPowerDown( void );											// save & shut down logger for USB or sleeping
extern void         writeLine( char * line, const char * fpath );                   // write one line to file
extern void			saveStats( MsgStats *st ); 										// save statistics block to file
extern char *		logMsgName( char *path, const char * sNm, short iSubj, short iMsg, const char *ext, int *pcnt );	// build & => file path for next msg for Msg_<sNm>_S<iS>_M<iM>.<ext>
extern void			flushStats( void );												// save all cached stats files
extern MsgStats *	loadStats( const char *subjNm, short iSubj, short iMsg );	// load statistics snapshot for Subj/Msg
extern void			logEvt( const char *evtID );									// write log entry: 'EVENT, at:    d.ddd'
extern void			logEvtFmt( const char *evtID, const char *fmt, ... );	// write log entry: 'EVENT, at:    d.ddd ...'
extern void			logEvtS( const char *evtID, const char *args );					// write log entry: 'EVENT, at:    d.ddd, ARGS'
extern void			logEvtNI( const char *evtID, const char *nm, int val );			// write log entry: "EVENT, at:    d.ddd, NM: VAL "
extern void			logEvtNINI( const char *evtID, const char *nm, int val, const char *nm2, int val2 );	// write log entry
extern void 		logEvtNININI( const char *evtID, const char *nm,  int val, const char *nm2, int val2, const char *nm3, int val3 );
extern void 		logEvtNINININI( const char *evtID, const char *nm,  int val, const char *nm2, int val2, const char *nm3, int val3, const char*nm4, int val4 );
extern void			logEvtNS( const char *evtID, const char *nm, const char *val );	// write log entry: "EVENT, at:    d.ddd, NM: 'VAL' "
extern void			logEvtNINS( const char *evtID, const char *nm, int val, const char *nm2, const char * val2 );	// write log entry
extern void			logEvtNSNI( const char *evtID, const char *nm, const char *val, const char *nm2, int val2 );	// write log entry
extern void 		logEvtNSNINI(  const char *evtID, const char *nm, const char *val, const char *nm2, int val2, const char *nm3, int val3 );
extern void 		logEvtNSNININS(  const char *evtID, const char *nm, const char *val, const char *nm2, int val2, const char *nm3, int val3, const char *nm4, const char *val4 );
extern void			logEvtNSNINS( const char *evtID, const char *nm, const char *val, const char *nm2, int val2, const char *nm3, const char *val3 );	// write log entry
extern void			logEvtNSNS( const char *evtID, const char *nm, const char *val, const char *nm2, const char *val2 );	// write log entry
extern void			logEvtNSNSNS( const char *evtID, const char *nm, const char *val, const char *nm2, const char *val2, const char *nm3, const char *val3 );	// write log entry

extern void			appendNorLog( const char * s );						// append text to Nor flash
extern void			copyNorLog( const char * fpath );					// copy Nor log into file at path
extern void			restoreNorLog( const char * fpath );			// copy file into current log
extern void			initNorLog( bool startNewLog );						// init driver for W25Q64JV NOR flash
extern void			eraseNorFlash( bool svCurrLog );					// erase entire chip & re-init with fresh log (or copy of current)
extern void			NLogShowStatus( void );
extern int			NLogIdx( void );
extern bool         LogSizeOk( void );                                  // TRUE if log below 1/64 of NLog

// User-level event logging
#define AUDIO_EVENT "PlayAudio"
#define LOG_AUDIO_PLAY_ERRORS(errorCount, lastError) \
          logEvtFmt(AUDIO_EVENT, "evt: errors, count: %d, last: %x")

#define LOG_AUDIO_PLAY_SPROMPT(playlist, filename) \
          logEvtFmt(AUDIO_EVENT, "sprompt: '%s', file: '%s'", playlist, filename)

#define LOG_AUDIO_PLAY_LPROMPT(playlist, filename) \
          if (BootKey=='L') logEvtFmt(AUDIO_EVENT, "lprompt: '%s', file: '%s'", playlist, filename)

#define LOG_AUDIO_PLAY_MESSAGE(position,playlist, filename) \
          if (BootKey=='L') logEvtFmt(AUDIO_EVENT, "message#: %d, playlist: '%s', file: '%s'", position, playlist, filename)

#define LOG_AUDIO_PLAY_SYSTEM(prompt, filename) \
          if (BootKey=='L') logEvtFmt(AUDIO_EVENT, "system: '%s', file: '%s'", prompt, filename)

#define LOG_AUDIO_PLAY_WAVE(filename, lenMs, lenBytes, samplesPerSec, isMono) \
          if (BootKey=='L') logEvtFmt(AUDIO_EVENT, "wave_file: '%s', length_ms: %d, size: %d, samples/sec: %d, isMono: %s", \
              filename, lenMs, lenBytes, samplesPerSec, isMono?"t":"f")

#define LOG_AUDIO_PLAY_STOP(lenMs, playedMs, playedPct, iSubj, iMsg) \
        if (BootKey=='L') logEvtFmt(AUDIO_EVENT, "evt: stop, length_ms: %d, played_ms: %d, completion%%: %d, Subj:%d, Msg:%d", lenMs, playedMs, playedPct, iSubj, iMsg )

#define LOG_AUDIO_PLAY_PAUSE(lenMs, playedMs, playedPct) \
          if (BootKey=='L') logEvtFmt(AUDIO_EVENT, "evt: pause, length_ms: %d, played_ms: %d, completion%%: %d", lenMs, playedMs, playedPct)

#define LOG_AUDIO_PLAY_RESUME(lenMs, playedMs, playedPct) \
          if (BootKey=='L') logEvtFmt(AUDIO_EVENT, "evt: resume, length_ms: %d, played_ms: %d, completion%%: %d", lenMs, playedMs, playedPct)

#define LOG_AUDIO_PLAY_DONE(lenMs, playedMs, playedPct) \
          if (BootKey=='L') logEvtFmt(AUDIO_EVENT, "evt: done, length_ms: %d, played_ms: %d, completion%%: %d", lenMs, playedMs, playedPct)

#define LOG_AUDIO_PLAY_JUMP(lenMs, playedMs, adjustMs ) \
          if (BootKey=='L') logEvtFmt(AUDIO_EVENT, "jump: %s, length_ms: %d, played_ms: %d, skip_ms: %d", adjustMs<0?"back":"ahead", lenMs, playedMs, adjustMs)

#endif  // log.h
