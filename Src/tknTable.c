// TBook Rev2  tknTable.c  --  tokenizer & JSONish parser
//   Gene Ball  Apr2018

#include "tbook.h"					
struct StorageStats {
	short maxHashBucketCnt;
	short tokenCharsAvail;
	short tokenCharsAllocated; 	// N_STR_CHS
	short tokenNodesAvail;
	short tokenNodesAllocated; 	// N_TKN_NDS
	short listNodesAvail;
	short listNodesAllocated; 	// N_LST_NDS
	short listIdsAvail;
	short listIdsAllocated; 	// N_LISTS

} TB_StorageStats;

// *********** static storage sizing constants
//const int 				HASH_SZ 				= 103;						// hashtable size
const int 				MAX_TKN_LEN 		= 100;						// MAX chars in a single token
const int 				MAX_CHS_PER_LN 	= 300;						// MAX chars in a single line
//const int 				MAX_TKNS_PER_LN	= 50;							// MAX tkns per line
//const int 				MAX_ELS_PER_LST = 50;							// MAX vals in gLst, or n:v in gObj
//const int 				N_STR_CHS 			= 3000;						// MAX chars in token strings  		// after control.def(29Apr18): 1413
//const int 				N_TKN_NDS 			= 500;						// MAX tokens in parsed files     	// after control.def(29Apr18): 166
//const int 				N_LST_NDS 			= 800;						// MAX list nodes in parsed files	// after control.def(29Apr18): 381
//const int 				N_LISTS 				= 1000;		  			// MAX # of lists in parsed files	// after control.def(29Apr18): 0
#define N_TKN_NDS 			 500	
#define N_STR_CHS 			2400
#define HASH_SZ 				 103
#define N_LST_NDS 			 800
#define NGRPS 						 8
#define N_LISTS 				1000
#define MAX_ELS_PER_LST	  50
#define MAX_TKNS_PER_LN	  50

// tknTable typedefs
typedef struct 		Tkn {								  						// token hashtable node
	char *		str;
	struct Tkn *link;
	TknID		tknid;
//	short		grp:6;					// idx of grp nd
//	short 		val:10;
} Tkn;
typedef struct 		TknLstNd {					  						// node in a 'TknList' 
	TknID 		Nm;		// tknid of name
	TknID		Val;	// tknid of value, or a TknList
} TknLstNd;

//const int 				NGRPS = 8;					  						// # of elements in enum GrpID
//
/* STATIC tknTable storage 
//		-- pools for strings: sStor[], token nodes: sTknNds[], list nodes: sLstNds[]	-- sizes preallocated from consts
//   	-- string hash buckets:  hTab[], hCnt[]		-- size fixed by hashFn
//		-- token group start indices: grpFirstNd[]	-- for each token group, index of first Tkn node
//		-- list start indices: lstFirstNd[]	-- for each list, index of first TknLstNd 
************* */
static short			nxtNd = -1;					  						// index of first free Tkn in sTknNds
static Tkn  			sTknNds[ N_TKN_NDS ];	  					// token node pool
static short			nxtStr;							  						// index of first free char in sStor
static char				sStor[ N_STR_CHS ];	  						// string storage pool
static Tkn *			hTab[ HASH_SZ ];		  						// array of hashtable chains
static short    	hCnt[ HASH_SZ ];		  						// count of nds in bucket
static short			nxtLstNd = 0;				  						// index of first TknLstNd in sLstNds
static TknLstNd		sLstNds[ N_LST_NDS ];	  					// list node storage pool
static short			nxtTknID;						  						// ID within current Group
static short			currGrpID;					  						// group for new tokens
static short			grpFirstNd[ NGRPS ];	  					// index of 1st token node for each GrpID
static short			nxtLstID = 0;											// ID for next Lst or Obj
static short			lstFirstNd[ N_LISTS ];  					// index of 1st list node for each TknList
static TknID			NULL_TknID;

static void				addGroup( char *grpNm, char *vals1, char *vals2, char *vals3, char *vals4, char *vals5  ); 	// forward
// token & list operations
void 							showTknTable(){										// display stats for tknTable
	struct StorageStats * ss = &TB_StorageStats;
	ss->maxHashBucketCnt = 0;
	for ( short i=0; i<HASH_SZ; i++ ){
		if ( hCnt[ i ] > ss->maxHashBucketCnt )
			ss->maxHashBucketCnt = hCnt[ i ];
	}
	ss->tokenCharsAvail 	= N_STR_CHS - nxtStr;
	ss->tokenCharsAllocated	= N_STR_CHS;
	ss->tokenNodesAvail 	= N_TKN_NDS - nxtNd;
	ss->tokenNodesAllocated	= N_TKN_NDS;
	ss->listNodesAvail 	= N_LST_NDS - nxtLstNd;
	ss->listNodesAllocated	= N_LST_NDS;
	ss->listIdsAvail 		= N_LISTS - nxtLstID;
	ss->listIdsAllocated	= N_LISTS;
	dbgLog( "! showTknTable: tokenCharsAvail=%d/%d\n  tokenNodesAvail=%d/%d\n  listNodesAvail=%d/%d\n  listIdsAvail=%d/%d \n",
		ss->tokenCharsAvail, N_STR_CHS, 
		ss->tokenNodesAvail, N_TKN_NDS, 
		ss->listNodesAvail, N_LST_NDS, 
		ss->listIdsAvail, N_LISTS
	);
}
static TknID			asTknID( GrpID g, short id ){
	TknID res;
	res.flds.grp = g;
	res.flds.val = id;
	return res;
	//return ( (( g & 0x3F ) << 10 ) | ( id & 0x3FF ));
}
static void 			verifyEnum( char *firstEl, short lastEl ){// INTERNAL: verify enum matches tkn group
	TknID fEl = toTkn( firstEl );
	GrpID grp = tknGrp( fEl );
	for( short v=0; v<= lastEl; v++ ){
		TknID tid = asTknID( grp, v );
		char *nm = tknStr( tid );	// convert <grp:v> to string
		TknID tknid = toTkn( nm );			// and get tknid for that string
		if ( tknid.flds.grp != grp ||  tknid.flds.val != v ) 
			tbErr( "group assignments problem" );
	}
}
void 							initTknTable(){										// initialize tknTable-- (self inits on first call to toTkn)
	for ( int i=0; i<HASH_SZ; i++ ){
		hTab[ i ] = NULL;
		hCnt[ i ] = 0;
	}
	nxtNd = 0;
	nxtStr = 0;
	// predefine strings corresponding to enum types:  GrpID, Punct, Events, Actions
	currGrpID = 0;
	nxtTknID = 0;
	NULL_TknID = toTkn( "nullNULL" );	// <g0:0> ==> special nullNULL
	currGrpID = 1;
	nxtTknID = 0;
	toTkn( "gNull" );		// 'gNull' == <g1:0>
	toTkn( "gGroup" );		// 'gGroup' == <g1:1>
	addGroup( "gGroup", 		// Groups:  grp: 1, val: gNull=0,gGroup..gTkn=NGRPS-1  matches enum GrpID
		"gNull gGroup gPunct gEvent gAction gTkn gLst gObj", "", "", "", "" );
	verifyEnum( "gNull", gObj );
	if ( gObj >= NGRPS ) 
		tbErr( "NGRPS too small" );
	
	addGroup( "gPunct", 		// predefine Punct:  grp: 2, val: Comma=0..DQuote  -- matches enum Punct
		"pNull , ; : { } [ ] ( ) ", "", "", "", "" );
	toTkn( "\"" );  // special case to tokenize " and add to gPunct without trying to scan string
	verifyEnum( "pNull", DQuote );
	
	addGroup( "gEvent", 		// predefine Events:  grp: 1, val: Home=0..None  -- matches enum Events
		"eNull Home Circle Plus Minus Tree Lhand Rhand Pot Star Table",
//			eNull=0, 
//			Home, 		Circle,		Plus, 		Minus, 		Tree, 		Lhand, 		Rhand, 		Pot,	 Star,		Table,
		"Home__ Circle__ Plus__ Minus__ Tree__ Lhand__ Rhand__ Pot__ Star__ Table__", 
//			Home__, 	Circle__, 	Plus__, 	Minus__, 	Tree__, 	Lhand__, 	Rhand__, 	Pot__, 	 Star__,	Table__,
		"starHome starCircle starPlus starMinus starTree starLhand starRhand starPot starStar starTable", 
//			starHome,	starCircle, starPlus, 	starMinus, 	starTree, 	starLhand, 	starRhand, 	starPot, starStar,	starTable,
		"AudioDone AudioStart ShortIdle LongIdle LowBattery BattCharging BattCharged FirmwareUpdate Timer", 
//			AudioDone,	AudioStart, ShortIdle,	LongIdle,	LowBattery,	BattCharging, BattCharged,	FirmwareUpdate, Timer, 
		" ChargeFault, LithiumHot, MpuHot, anyKey, eUNDEF" );
//	anyKey, eUNDEF
	verifyEnum( "eNull", eUNDEF );
	
	addGroup( "gAction", 		// predefine Actions:  grp: 3, val: prevSubj=0..next  -- matches enum Actions
		"aNull LED bgLED playSys playSubj pausePlay resumePlay stopPlay volAdj spdAdj posAdj",
//				aNull=0, 	LED,		bgLED,		
//				playSys, 	playSubj,	pausePlay,	resumePlay,		stopPlay,	volAdj,		spdAdj,		posAdj,
		"startRec pauseRec resumeRec finishRec playRec saveRec writeMsg",
//				startRec,	pauseRec,	resumeRec,	finishRec, playRec,	saveRec, writeMsg,
		"goPrevSt saveSt goSavedSt subjAdj msgAdj setTimer resetTimer showCharge",
//				goPrevSt,	saveSt,		goSavedSt,
//				subjAdj, 	msgAdj,		setTimer,	resetTimer, showCharge,
		"startUSB endUSB powerDown sysBoot sysTest", "playNxtPkg changePkg" );
//				startUSB,	endUSB,		powerDown,	sysBoot, sysTest, playNxtPkg, changePkg
	verifyEnum( "aNull", sysBoot );

	addGroup( "gTkn", "tNull", "","","","" );	// predefine one value in group gTkns: subsequent non-predefined strings get allocated as gTkns
}

static short			hashFn( const char *s ){					// INTERNAL: => hash value of tolower(s)
	uint16_t sum = 0;
	for ( int i=0; i<MAX_TKN_LEN; i++ ){
		if ( s[i]==0 ){
			sum += i;
			return sum % HASH_SZ;
		}
		sum += tolower( s[i] );
	}
	tbErr("hashFn string >MAX_TKN_LEN chars" );
	return 0;
}


TknID							toTkn( const char *s ){						// => tkn for string, adding if new
	if ( nxtNd < 0 ) 
		initTknTable();			// self initialize
	short h = hashFn( s );
	Tkn *nd = hTab[ h ];
	while ( nd!=NULL ){
		if ( strcasecmp( s, nd->str )==0 )
			return nd->tknid; //tokID( nd );
		nd = nd->link;
	}
	// not found, add new Nd
	if ( nxtNd >=N_TKN_NDS )  {	showTknTable(); tbErr( "out of Token nodes" ); }

	nd = &sTknNds[ nxtNd ];
	nxtNd++;
	
	short len = strlen( s );
	nd->str = &sStor[ nxtStr ];		// pointer to first free storage char
	nxtStr = nxtStr + len + 1;		// alloc, incl terminator
	if ( nxtStr >= N_STR_CHS ) {	showTknTable(); tbErr( "out of token String storage" ); }
		
	strcpy( nd->str, s );			// copy string into token store
	nd->tknid.flds.grp = currGrpID;
	nd->tknid.flds.val = nxtTknID;
	nxtTknID++;
	
	nd->link = hTab[ h ];			// add to front of hash chain
	hTab[ h ] = nd;
	hCnt[ h ]++;					// inc count in chain
	return nd->tknid; //tokID( nd );
}

static Tkn *			asTkn( TknID tknid ){							// INTERNAL: => Tkn* for node 'tknid'
	short grp = tknid.flds.grp;
	short itm = tknid.flds.val;
	if ( grp >= gLst ) return NULL;		// list or invalid
	
	short ndIdx = grpFirstNd[ grp ] + itm;
	return &sTknNds[ ndIdx ];
}
static short			iLstHd( TknID tknid ){						// INTERNAL: => idx (in sLstNds) of header Nd of list 'tknid'
	GrpID grp = tknGrp( tknid );
	short id =  tknid.flds.val; //(tknid & 0xFF);
	if ( grp != gLst && grp != gObj ) // tknid isn't a Lst or Obj
		return -1;
	if ( id >= nxtLstID ) 
		return -1; 				// invalid list ID
	short iHd = lstFirstNd[ id ];	// index of list header node
	return iHd;
}
static TknLstNd *	asTknLst( TknID tknid ){					// INTERNAL: => TknLstNd* for header Nd of list 'tknid'
	short iHd = iLstHd( tknid );
	if ( iHd < 0 ) return NULL;
	return &sLstNds[ iHd ];
}
static TknLstNd *	listEl( TknID listObj, short idx ){	// INTERNAL: => ptr to listNd for listObj[ idx ]
	short iHd = iLstHd( listObj );
	if ( iHd < 0 || idx < 0 || idx >= sLstNds[ iHd ].Val.tknID )  // invalid list or index?
		return NULL;
	return &sLstNds[ iHd+1 + idx ];		// ptr to idx'th list nd
}
char *						tknStr( TknID tknid ){						// => string name of 'tknid'
	Tkn * nd = asTkn( tknid );
	return nd==NULL? "null" : nd->str;
}
GrpID							tknGrp( TknID tknid ){						// => GrpID of 'tknid' 
	short grp = tknid.flds.grp; 
	if ( grp >= NGRPS ) return gNull;		// invalid GrpID
	return (GrpID) grp;
}
static void				catIf( char *buff, int bufflen, char *pfx, char *s ){
	int blen = strlen( buff );
	int slen = strlen( pfx ) + strlen( s );
	if ( bufflen== 0 ) bufflen = MAX_DBG_LEN;
	if ( bufflen-blen > slen+2 ){
		strcat( buff, pfx );
		strcat( buff, s );
	}
}
static void				toVS( char *buff, int bln, int nlevs, TknID tknid ){
	GrpID gid = tknGrp( tknid );
	int cnt;
	char s[10] = {0}, *sep = " ";
	switch ( gid ){
		case gLst:
		case gObj:
			cnt = lstCnt( tknid );
			sprintf( s, "%c%d ", gid==gLst? '[':'{', cnt ); 
			catIf( buff,bln, s, "" );
			if (nlevs>0){
				for ( short i=0; i<cnt; i++ ){
					catIf( buff,bln, sep, "" ); sep = ", ";
					if ( gid==gObj ) catIf( buff,bln, tknStr( getLstNm( tknid, i )), ": " );
					toVS( buff,bln, nlevs-1, getLstVal( tknid, i ));
				}
			}
			catIf( buff,bln, gid==gLst? "]" : "}", "" );
			break;
		case gNull: 
			catIf( buff,bln, "null", "" );
			break;
		default:
			catIf( buff,bln, tknStr( tknid ), "" );
			break;
	}
}
static char *			DBG[8];
void							toVStr( char *buff, int bufflen, char**b2, char**b3, TknID tknid ){
	if ( bufflen== 0 ) bufflen = MAX_DBG_LEN;
	buff[0] = 0;
	toVS( buff, bufflen, 3, tknid );
	tbShw(buff, b2, b3);
	int blen = strlen( buff );
	for ( short i=0; i<8; i++ ) 
		DBG[i] = blen>i*32? &buff[i*32] : "";
	if ( DBG[0] != buff) DBG[0] = buff;  //use it
}
char *						toStr( char *buff, TknID tknid ){	// display tknid in buff
	char *b2, *b3;
	toVStr( buff, MAX_TKN_LEN, &b2, &b3, tknid );
	return buff;
}
bool							isLst( TknID tknid ){							// => true if tknID is Lst or Obj
	GrpID grp = tknGrp( tknid );
	return ( grp==gLst || grp==gObj );
}
short							tknVal( TknID tknid ){						// => val of 'tknid' 
	Tkn * nd = asTkn( tknid );
	return nd->tknid.flds.val;
}
short							lstCnt( TknID tknid ){						// => # els in lst 'tknid' 
    TknLstNd *hd = asTknLst( tknid );
	if ( hd!=NULL )
		return (short) hd->Val.tknID;		// Cnt is Val of header node
	return 0;
}
Punct							asPunct( TknID tknid ){						// => Punct or pNull
	if ( tknGrp( tknid )==gPunct ) 
		return (Punct)tknid.flds.val;
	return pNull;
}
GrpID							asGroup( TknID tknid ){						// => Punct or pNull
	if ( tknGrp( tknid )==gGroup ) 
		return (GrpID)tknid.flds.val; 
	return gNull;
}
Event							asEvent( TknID tknid ){						// => Event or eNull
	if ( tknGrp(tknid)==gEvent ) 
		return (Event)tknVal(tknid);
	return eNull;
}
Action						asAction( TknID tknid ){					// => Action or aNull
	if ( tknGrp(tknid)==gAction ) 
		return (Action)tknVal(tknid);
	return aNull;
}
char *						eventNm( Event event ){								// => string for enum Event 
	TknID id; // = asTknID( gEvent, event );
	id.flds.grp = gEvent;
	id.flds.val = event;
	return tknStr( id );
}
char *						actionNm( Action a ){							// => string for enum Action 
	TknID id; // = asTknID( gAction, a );
	id.flds.grp = gAction;
	id.flds.val = a;
	return tknStr( id );
}
// private static variables used by tokenize
const char *			delimChars = ",;{}[]()\":";
const char *			whiteChars = " \t\r\n";
/* tokenizing rules:
// tokenize:  returns sequence of tknids from a string
//  white space is ignored, except that it terminates a token
//  delimeters  are added as single character tokens
//  enum types GrpID, Punct, Event, Action are predefined, others get allocated as gTkns
// e.g. 'this,is;a test -2oth_er0 "tkn spc" doesn't work' => 
//   [ gTkns.0='this' gPunct.Comma gTkns.1=is' gPunct.Semi gTkns:2='a' gTkns:3='test'
//		gTkns.4="-2oth_er0" gPunct.DQuote gTkns.5='tkn' gTkns.6='spc' gPunct.DQuote gTkns.7="doesn't" gTkns.8='work' ]
//  note that " doesn't affect whitespace as a delimiter
*/
short							tokenize( TknID *tkns, short tsiz, char *line ){	// tokenizes 'line', => # tknIDs added to 'tkns', from 'line' in tkns[]
	short nxtTkn = 0;
	char tkn[ MAX_TKN_LEN ];
	short tknlen = 0;
	
	char *pCh = line;
	while ( true ){
		char ch = *pCh;
		bool isNul = (ch==0);
		bool isWht = (strchr( whiteChars, ch ) != NULL);
		bool isDlm = (strchr( delimChars, ch ) != NULL);
		if (( isWht || isDlm || isNul ) && tknlen > 0 ){	// save token in progress
			tkn[ tknlen ] = 0;
			tkns[ nxtTkn++ ] = toTkn( tkn );
			tknlen = 0;
		}
		if ( isNul ) 	// end of line-- token saved, so return
			return nxtTkn;
		if ( ch=='"' ){ // quoted token
			char * pQ = strchr( pCh+1, '"' );	// find match
			*pQ = 0;
			tknlen = pQ-pCh;
			if ( tknlen > MAX_TKN_LEN ) 
				tbErr( "token too long" );
			strcpy( tkn, pCh+1 );	// copy quoted token to tkn
			pCh = pQ + 1;
		} else {
			if ( isDlm ){	// also add delimiter as token
				tkn[ 0 ] = ch; 
				tkn[ 1 ] = 0;
				tkns[ nxtTkn++ ] = toTkn( tkn );
				tknlen = 0;
			}
			if ( !isWht && !isDlm ){ // token character, append it
				if ( tknlen >= MAX_TKN_LEN )
					tbErr( "token too long" );
				tkn[ tknlen++ ] = ch;
			}
		}
		pCh++;
		if ( pCh-line > MAX_CHS_PER_LN ) break;
	}		
	tbErr( "tokenize: line > MAX_CHS_PER_LN chars" );
	return nxtTkn;
}



static void				addGroup( char *grpNm, char *vals1, char *vals2, char *vals3, char *vals4, char *vals5  ){ 	// INTERNAL: predefine token group 'grpNm' with values from 'grpVals'
	short grpID = tknVal( toTkn( grpNm ));	// grp:1, val: 0..NGRPS-1 
	
	currGrpID = grpID;				// now start allocating IDs in that group
	nxtTknID = 0;					// beginning with xNull = val:0
	grpFirstNd[ grpID ] = nxtNd;	// ndIdx that will be assigned to first value

	if ( strcmp( "gGroup", grpNm ) == 0 ){	// special cases for gGroup assignments, since gNull & gGroup are allocated ahead
		currGrpID = grpID = 1;
		grpFirstNd[ grpID ] = 1;   	// nullNull==sTknNds[0]==<g0:0>, gNull==sTknNds[1]==<g1:0>, gGroup==sTknNds[1]==<g1:1>
		nxtTknID = 2;				// gEvents will ==<g1:2>
	}
	
	TknID tkns[MAX_TKNS_PER_LN];
	short cnt = tokenize( tkns, MAX_TKNS_PER_LN, vals1 );		// allocate tokenIDs for values
	cnt += tokenize( tkns, MAX_TKNS_PER_LN, vals2 );		// allocate tokenIDs for values
	cnt += tokenize( tkns, MAX_TKNS_PER_LN, vals3 );		// allocate tokenIDs for values
	cnt += tokenize( tkns, MAX_TKNS_PER_LN, vals4 );		// allocate tokenIDs for values
	cnt += tokenize( tkns, MAX_TKNS_PER_LN, vals5 );		// allocate tokenIDs for values
}

TknID							getLstNm( TknID listObj, short idx ){				// => nm of listObj[idx] as TknID
	TknLstNd * lnd = listEl( listObj, idx );
	if ( lnd==NULL ) return NULL_TknID;
	return lnd->Nm;
} 
TknID							getLstVal( TknID listObj, short idx ){				// => val of listObj[idx] as TknID
	TknLstNd * lnd = listEl( listObj, idx );
	if ( lnd==NULL ) return NULL_TknID;
	return lnd->Val;
} 
TknID							getField( TknID listObj, const char *nm ){			// => tknid of value for listObj.nm (e.g. list.'0' or obj.'nm')
	char FieldVal[MAX_TKN_LEN];	//DEBUG
	FieldVal[0] = 0; //DEBUG
	GrpID grp = (GrpID) listObj.flds.grp; //tknGrp( listObj );
	TknID fval = NULL_TknID;
	//short id =  listObj.val; //tknVal( listObj );
	if ( grp==gObj ){	// object: look for key=nm
		TknID key = toTkn( nm );
		for ( short i=0; i < lstCnt(listObj); i++ ){
			TknLstNd *pNd = listEl( listObj, i );
			if ( pNd!=NULL && pNd->Nm.tknID==key.tknID ){
				fval = pNd->Val; 
				toStr( FieldVal, fval );	//DEBUG
				return fval;
			}
		}
	} else if ( grp==gLst ){	// [] list: nm should be integer or "number"
		int idx = (int)nm < 100? (int)nm : atoi( nm );
		TknLstNd *pNd = listEl( listObj, idx );
		if ( pNd!=NULL ){
			fval = pNd->Val; 
			toStr( FieldVal, fval );	//DEBUG
			return fval;
		}
	} 
	return NULL_TknID;		// no joy
}
int								getIntFld( TknID listObj, const char *nm, int defVal ){	// => int value of field 'nm', or 'defVal'
	TknID v = getField( listObj, nm );
	if ( v.tknID==0 )
		return defVal;
	else 
		return atoi( tknStr( v ));
}
TknID							lookup( TknID listObj, const char *path ){			// => tknid of listObj[ 'nm.idx.nm2' ]  e.g. 'config.default_volume' or 'controlStates.stWakeup.Actions.0' 
	char part[ 20 ];
	const char *stPart = path;
	TknID cObj = listObj;
	while (true){
		char *pDot = strchr( stPart, '.' );
		if ( pDot == NULL ){ // final segment
			strcpy( part, stPart );
		} else {
			strncpy( part, stPart, pDot-stPart );
		}
		cObj = getField( cObj, part );
		if ( pDot==NULL ) return cObj;
		
		stPart = pDot + 1;
	}
}
/********  JSONish parser
// parseFile( fNm ) --  parses file contents into a list, represented by the returned 'tknid'
//   // starts a comment to end of line, so does <;>
//   parseFile expects a first token of either <[> or <{> 
//   <{>  implies a sequence of <name><:> <val>  separated by (optional) <,>, terminated by <}>
//   <[>  implies a sequence of <val> terminated by <]>
//   <name> can be <str_without_whitespace_or_delim>, or <">str<"> -- no escapes in ""
//   <val> can be <name> 
//       or <{> object-def <}>  
//       or <[> list-def <]>
//       or <nm> <(> <arg> <)>  -- which is parsed as [ <nm> <arg> ]
*************/
// private static variables used by parseFile & descendents
static char 			line[200];			// up to 200 characters per line
static TknID 			lineTkns[MAX_TKNS_PER_LN];		// tokens for current line ( up to 100 )
static short 			nLnTkns = 0;		// num tokens in current line
static TknID 			currTkn; 			// 'gGroups' grp:0, val:0 -- serves as NULL
static short			nxtTknIdx;			// index of next tkn to become curr
static FILE *			inFile;				// open file descriptor for parseFile 
static TknID			cTkn(){									// INTERNAL:  => tknid of curr token in file
	while ( currTkn.tknID==0 ){	// make next token current
		if ( nxtTknIdx >= nLnTkns ){  // out of tokens on line - read a line
			char * s = fgets( line, 200, inFile );
			if ( s==NULL ) return NULL_TknID;	// end of file
	
			char * sl = strchr( line, '/' );		// delete anything from "//" on--  comments
			while ( sl!=0 ){ 
				if ( sl[1]=='/' ){ 
					sl[0] = 0;	// delete // comment
					break;
				}
				sl = strchr( &sl[1], '/' );
			}
			lineTkns[0] = NULL_TknID;
			nLnTkns = tokenize( lineTkns, MAX_TKNS_PER_LN, line );		// get tokens from line
			nxtTknIdx = 0;
		}
		currTkn = lineTkns[ nxtTknIdx ];
		if ( asPunct( currTkn )==Semi ){		// start of comment
			currTkn.tknID = 0;		// go to next line
			nLnTkns = 0;
		}
		nxtTknIdx++;
	}
	return currTkn;
}
TknID 						nTkn(){									// INTERNAL:  accept cTkn, then => new cTkn
char CurrTkn[MAX_TKN_LEN];	//DEBUG
	currTkn.tknID = 0;
	cTkn();
toStr( CurrTkn, currTkn );	//DEBUG
	return currTkn;
}
static TknID			lstID( short lstNdIdx ){				// INTERNAL:  => gLst:lstID or gObj:lstID for list starting with 'lstNdIdx'
	GrpID grp = asGroup( sLstNds[ lstNdIdx ].Nm );
	if ( nxtLstID > N_LISTS ){ showTknTable();  tbErr( "out of list IDs" ); }
	short id = nxtLstID++;
	lstFirstNd[ id ] = lstNdIdx;
	return asTknID( grp, id );	
}
static TknID			parseLstObj( Punct closeBr, short depth, char *path );	// INTERNAL:  fwd decl
static TknID			parseName( ){							// INTERNAL:  parse a name-- tkn | " tkn " -- cTkn is next
	Punct pTkn = asPunct( cTkn());
	TknID nm;
	if ( pTkn==DQuote ){
		nm = nTkn();
		pTkn = asPunct( nTkn());
		if ( pTkn!=DQuote ) 
			tbErr( "Expected matching DQuote after name" );
	} else
		nm = cTkn();
	nTkn();
	return nm;
}
static TknID			parseValue( short depth, char *path ){							// INTERNAL:  parse a value-- tkn | { n:v... } | [ v... ] | tkn( arg )
	// parse a value-- tkn | { n:v... } | [ v... ] | tkn( arg )
	// cTkn at initial tkn, or LBrace or LBracket
	// calls parseLstObj() for {} or [] lists
	// returns with cTkn after }, ], ), or tkn
	char *path2, *path3; tbShw(path, &path2, &path3 );	//DEBUG
	char Value[MAX_DBG_LEN] = {0}; char* Value2, *Value3;	//DEBUG
	char Arg[MAX_TKN_LEN] = {0};	//DEBUG
	Punct pTkn = asPunct( cTkn());
	TknID val, arg;
	val.tknID = 0; arg.tknID = 0;
	if ( pTkn==LBrace ){
		val = parseLstObj( RBrace, depth+1, path );	// value is an { obj }
	} else if ( pTkn==LBracket ){
		val = parseLstObj( RBracket, depth+1, path );	// value is a [ list ]
	} else {
		val = parseName();		// store value as tknid
	}
	toVStr( Value,0, &Value2, &Value3, val ); 
	pTkn = asPunct( cTkn());
	if ( pTkn==LParen ){	// argument-- zero or one token
		pTkn = asPunct( nTkn());
		if ( pTkn!=RParen ){
			arg = cTkn();
			toStr( Arg, arg ); 	//DEBUG
			pTkn = asPunct( nTkn());
			if ( pTkn!=RParen ) 
				tbErr( "expected )" );
		}
		nTkn();		// accept RParen
	}
	TknID result = val;
	if ( arg.tknID!=0 ){	// no arg-- just return value
		// allocate [ val, arg ] and return it
		short fNd = nxtLstNd;
		nxtLstNd += 3;	// alloc 1 as header node, one for val, one for arg
		if ( nxtLstNd >= N_LST_NDS ) 
			tbErr( "Out of list nds" );
		sLstNds[ fNd ].Nm = asTknID( gGroup, gLst );	// gLst
		sLstNds[ fNd ].Val.tknID = 2;			// Val: count of val + arg
		sLstNds[ fNd+1 ].Nm.tknID = 0;
		sLstNds[ fNd+1 ].Val = val;
		sLstNds[ fNd+2 ].Nm.tknID = 0;
		sLstNds[ fNd+2 ].Val = arg;
		result = lstID( fNd );
	}
	switch( depth ){  // DEBUGGER HACK
		case 1:	break;  // (file)
		case 2:	break;
		case 3:	break;  // config: CGroups: CStates:
		case 4:	break;	// CStates.stWakeup:
		case 5:	break;	// CStates.stWakeup.Actions  CStates.stWakeup.CGroups
		case 6:	break;	// 
		case 7:	break;	// 
		default:	break;
	}
	return result;
}
static TknID			parseLstObj( Punct closeBr, short depth, char *path ){			// INTERNAL:  read and parse an object { N:V, ... }
	// called with cTkn==LBrace or LBracket
	// parse list contents & save in nms[] vals[], then allocate listNds at end & return list tknid
	// calls parseValue to parse vals that are {} or [] lists
	TknID nms[MAX_ELS_PER_LST], vals[MAX_ELS_PER_LST];
	short nNmVals = 0;
	short pathLen = strlen( path );  // so it can be restored
	Punct pTkn = asPunct( nTkn());		// move to first token
	char PathPart[MAX_TKN_LEN] = {0};	//DEBUG
	while ( pTkn != closeBr ){
		if ( closeBr==RBrace ){		// parsing Obj, so get <Nm> <:> 
			nms[ nNmVals ] = parseName( );
			pTkn = asPunct( cTkn() );
			if ( pTkn==Colon )  	// check if Colon
				pTkn = asPunct( nTkn());	// and skip it
		} else 
		  nms[ nNmVals ].tknID = 0;		// [] lists have nm==0

		
		// at start of Value 
		TknID nmid = nms[nNmVals];	//DEBUG
		if ( closeBr==RBrace ) sprintf( PathPart, ".%s", tknStr(nmid) ); else sprintf( PathPart, "[%d]", nNmVals );	//DEBUG
		path[pathLen] = 0; strcat( path, PathPart );	//DEBUG
		TknID val = parseValue( depth, path );
		vals[ nNmVals++ ] = val;
		pTkn = asPunct( cTkn());
		if ( pTkn==Comma ) // check if nxt tkn is a comma
			pTkn = asPunct( nTkn());	// and skip it, if it is
		// pTkn should now be closeBr, or nxt Nm or Val
		if ( cTkn().tknID==0 ) 
			tbErr( "EOF without closeBr" );
	}
	nTkn();		// accept closeBr
	// now allocate a contiguous group of LstNodes & fill from nms & vals
	short fNd = nxtLstNd;
	nxtLstNd += nNmVals+1;	// alloc 1 as header node, and one for each nm/val
	if ( nxtLstNd >= N_LST_NDS ) 
		tbErr( "Out of list nds" );
	sLstNds[ fNd ].Nm = asTknID( gGroup, closeBr==RBrace? gObj : gLst );	// Nm: gObj or gLst 
	sLstNds[ fNd ].Val.tknID = nNmVals;	// Val: count of nm:val
	for ( short i=0; i<nNmVals; i++ ){
		sLstNds[ fNd+1 + i ].Nm = nms[ i ];
		sLstNds[ fNd+1 + i ].Val = vals[ i ];
	}
	TknID lst = lstID( fNd );
	path[pathLen] = 0; 
	return lst;
}
TknID 						parseFile( const char *fnm ){						// read and parse a JSONish text file
	char *fnm2, *fnm3; tbShw( fnm, &fnm2, &fnm3 );
	dbgLog( "B Reading %s...\n", fnm );
	inFile = tbOpenReadBinary( fnm ); //fopen( fnm, "rb" );
	if ( inFile==NULL )
		tbErr( "parseFile file not found" );
	char ValPath[300] = {0};	
	TknID tk = cTkn();
	TknID lst = parseValue( 1, ValPath );
	tbCloseFile( inFile );		// fclose( inFile );
	
	char Value[ MAX_TKN_LEN ] = { 0 };
	toStr( Value, lst );
	dbgLog( "C read %s from %s \n", Value, fnm );
	showTknTable();
	return lst;
}
//end  tknTable.c

