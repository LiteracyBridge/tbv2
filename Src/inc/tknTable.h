// TBookV2  tknTable.h
//  Apr2018
#ifndef TKN_TABLE_H
#define TKN_TABLE_H

typedef enum {	// GrpID -- tknid groups, corresponding to enum types
			gNull=0, gGroup, gPunct, gEvent, gAction, gTkn, gLst, gObj 
}	GrpID;

typedef enum { 	// Event -- TBook event types
			eNull=0, 
			Home, 		Circle,		Plus, 		Minus, 		Tree, 		Lhand, 		Rhand, 		Pot,	 Star,		Table,  //=10
			Home__, 	Circle__, 	Plus__, 	Minus__, 	Tree__, 	Lhand__, 	Rhand__, 	Pot__, 	 Star__,	Table__,  //=20
			starHome,	starCircle, starPlus, 	starMinus, 	starTree, 	starLhand, 	starRhand, 	starPot, starStar,	starTable, //=30
	    // TODO: move AudioStart to before AudioDone. Requires finding inequality comparisons against AudioDone
			AudioDone,	AudioStart, ShortIdle,	LongIdle,	LowBattery,	BattCharging, BattCharged,	FirmwareUpdate, Timer, //=39
		  ChargeFault, LithiumHot, MpuHot, anyKey, eUNDEF //=44
}	Event;

typedef enum { 	// Punct -- JSONish punctuation tokens
				pNull=0, Comma, Semi, Colon, LBrace, RBrace, LBracket, RBracket, LParen, RParen, DQuote 
}	Punct;

typedef enum { 	// Action -- TBook actions
				aNull=0, 	LED,		bgLED,		
				playSys, 	playSubj,	pausePlay,	resumePlay,		stopPlay,	volAdj,		spdAdj,		posAdj,
				startRec,	pauseRec,	resumeRec,	finishRec, playRec,	saveRec, writeMsg,
				goPrevSt,	saveSt,		goSavedSt,
				subjAdj, 	msgAdj,		setTimer,	resetTimer, showCharge,
				startUSB,	endUSB,		powerDown,	sysBoot, sysTest, playNxtPkg, changePkg
}	Action;

typedef union { 
	struct {
		short val: 12;
		short grp:  4;
	} flds;
	short tknID;
} TknID;
//typedef short 	TknID;												// reference to a token or list
extern void 	initTknTable( void );								// initialize tknTable-- (self inits on first call to toTkn)
extern TknID	toTkn( const char *s );								// => tkn for string, adding if new
extern short	tokenize( TknID *tkns, short tsiz, char *line );	// tokenizes 'line', => # tknIDs added to 'tkns', from 'line' in tkns[]
void 			showTknTable( void );								// display stats for tknTable
extern bool		isLst( TknID tknid );								// => true if tknID is Lst or Obj
extern char *	tknStr( TknID tknid );								// => string name of 'tknid'
extern void		toVStr( char *buff, int bufflen, char**b2, char**b3, TknID tknid );
extern char *	toStr( char *buff, TknID tknid );					// => buff with  "Grp:Val"
extern GrpID	tknGrp( TknID tknid );								// => GrpID of 'tknid' 
extern short	tknVal( TknID tknid );								// => val of 'tknid' 
extern Punct	asPunct( TknID tknid );								// => Punct or pNull
extern Event	asEvent( TknID tknid );								// => Event or eNull
extern Action	asAction( TknID tknid );							// => Action or aNull
extern char *	eventNm( Event e );									// => string for enum Event 
extern char *	actionNm( Action a );								// => string for enum Action 
extern short	lstCnt( TknID tknid );								// => # els in lst 'tknid' 
extern TknID	getLstNm( TknID listObj, short idx );				// => nm of listObj[idx] as TknID 
extern TknID	getLstVal( TknID listObj, short idx );				// => val of listObj[idx] as TknID
extern TknID	getField( TknID listObj, const char *nm );			// => tknid of value for listObj.nm (e.g. list.'0' or obj.'nm')
extern int		getIntFld( TknID listObj, const char *nm, int defVal );	// => int value of field 'nm', or 'defVal'
extern TknID	lookup( TknID listObj, const char *path );			// => tknid of listObj[ 'nm.idx.nm2' ]  e.g. 'config.default_volume' or 'controlStates.stWakeup.Actions.0' 
extern TknID	parseFile( const char *fnm );						// read and parse a JSONish text file

#endif	/* tknTable.h */
