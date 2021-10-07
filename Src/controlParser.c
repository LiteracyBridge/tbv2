// TBookV2  controlParser.c
//   Gene Ball  May2018

#include "tbook.h"
#include "controlmanager.h"


// ---------------   parse  /system/control.def  &  /package/list_of_subjects.txt  & /package/SUBJ*/messages.txt 
static void		pathCat( char *path, const char * src, const char * src2 ){  // strcat & convert '\' to '/'
	short plen = strlen( path );
	if ( path[plen-1]=='\\' ) path[plen-1] = '/';
	if ( path[plen-1]!='/' ) path[plen++] = '/'; 
	
	short slen = strlen( src );
	for( short i=0; i<slen; i++ )
		path[ plen+i ] = src[i]=='\\'? '/' : src[i];
	path[ plen+slen ] = 0;
	
	if ( src2 != NULL ){
		plen += slen;
		slen = strlen( src2 );
		for( short i=0; i<slen; i++ )
			path[ plen+i ] = src2[i]=='\\'? '/' : src2[i];
		path[ plen+slen ] = 0;
	}
}
static void		appendIf( char * path, const char *suffix ){	// append 'suffix' if not there
	short len = strlen( path );
	short slen = strlen( suffix );
	if ( len<slen || strcasecmp( &path[len-slen], suffix )!=0 )
		strcat( path, suffix );
}
void					buildPath( char *dstpath, const char *dir, const char *nm, const char *ext ){ 		// dstpath[] <= "dir/nm.ext"
	short dirlen = strlen( dir );
	short nmlen = strlen( nm );
	short extlen = strlen( ext );
	
	strcpy( dstpath, "M0:/" );
	bool absNm = ( nm[0]=='/' || strcmp(nm, "M0:")==0 );		// is nm an absolute path?
	
	const char *p = absNm? nm : dir;
	if ( p[2]==':' ) p += 3;
	if ( *p=='/' ) p++;
	pathCat( dstpath, p, NULL );
	if ( !absNm ){
		appendIf( dstpath, "/" );
		pathCat( dstpath, nm, NULL );
	}
	appendIf( dstpath, ext );
	if ( strlen( dstpath ) >= MAX_PATH ) 
		tbErr( "path too long" ); 
}
static char *	allocStr( const char * s ){
	char * pStr = tbAlloc( strlen( s )+1, "parser paths" );
	strcpy( pStr, s );	
	return pStr;
}
static char *	loadPath( TknID tknid ){	// build file path prefix from tkn in path[] => &path
	char path[ MAX_PATH ];
	buildPath( path, tknStr(tknid), "", "" );
	return allocStr( path );
}
char *				getStrFld( TknID listObj, const char *nm, char *defVal ){ // copy string value of field 'nm', if present
	TknID v = getField( listObj, nm );
	if ( v.tknID==0 )
		return defVal;
	char buff[ MAX_PATH ];
	toStr( buff, v );
	return allocStr( buff );
}
/* void					findPackages( void ){						// scan for M0:/package* /  directories
	const char *packageDirPatt = "M0:/package*";		//TODO:  "M0:/packages/packages.list"

	fsFileInfo fInfo;
	fInfo.fileID = 0;
	nPackages = 0;
	int pkgNmLen = 100;
																									//TODO: read directories from packages.list
	while ( ffind( packageDirPatt, &fInfo )==fsOK ){		// find all package directories on device
		char pkgPath[ MAX_PATH ];
		buildPath( pkgPath, "M0:/", fInfo.name, "/" );
		TBPackage[ nPackages ] = readContent( pkgPath, nPackages );
		nPackages++;
		if ( strlen( fInfo.name ) < pkgNmLen ){
			pkgNmLen = strlen( fInfo.name );
			iPkg = nPackages-1;			// shortest is the initial package
		}
	}
	TBPkg = TBPackage[ iPkg ];
}
TBPackage_t * readContent( const char * pkgPath, int pkgIdx ){		// parse list_of_subjects.txt & messages.txt for each Subj => Content	
	char pth[MAX_PATH];
	buildPath( pth, pkgPath, "list_of_subjects", ".txt" );		//TODO: playlists.list
																									//TODO: read package.properties  => audio_path, version, playlists?
	
	FILE *inFile = tbOpenReadBinary( pth ); //fopen( pth, "rb" );
	if ( inFile==NULL )
		tbErr( "list_of_subjects file not found" );
	
	TBPackage_t * Pkg = tbAlloc( sizeof(TBPackage_t), "pkg" );
	Pkg->idx = pkgIdx;
	Pkg->nSubjs = 0;
	Pkg->path = allocStr( pkgPath );								
	//TODO: audio_path=path1;path2;...;pathn
	buildPath( pth, pkgPath, "package", ".wav" );
	Pkg->packageName = allocStr( pth );
	logEvtNINS( "loadPkg", "idx", pkgIdx, "path", pkgPath );
	
	char 		line[200], dt[30];			// up to 200 characters per line
	fsTime tm;
	buildPath( pth, pkgPath, "version", ".txt" );				//TODO: from package.properties
	char *	contentVersion = loadLine( line, pth, &tm );
	sprintf( dt, "%d-%d-%d %d:%d", tm.year, tm.mon, tm.day, tm.hr, tm.min );
	logEvtNSNS( "Package", "dt", dt, "ver", contentVersion );
	
	TknID 	lineTkns[20];
	short 	nLnTkns = 0;		// num tokens in current line
	char	msg_txt_fname[MAX_PATH+15];
	while (true){
		char *s = fgets( line, 200, inFile );
		if (s==NULL) break;
		nLnTkns = tokenize( lineTkns, 20, line );		// get tokens from line
		if ( nLnTkns>0 && asPunct( lineTkns[0] )!=Semi ){	// if not comment, tkn is file path for subj/messages.txt
			tbSubject * sb = tbAlloc( sizeof( tbSubject ), "tbSubjects" ); 
			Pkg->TBookSubj[ Pkg->nSubjs ] = sb;
			Pkg->nSubjs++;

			buildPath( msg_txt_fname, pkgPath, tknStr( lineTkns[0] ), "");
			sb->path = allocStr( msg_txt_fname );
			
			buildPath( msg_txt_fname, sb->path, "messages", ".txt" );
			TknID msgs = parseFile( msg_txt_fname );	// parse JSONish messages.txt
			sb->name = tknStr( getField( msgs, "Subject" ));
			sb->audioName = tknStr( getField( msgs, "audioName" ));
			sb->audioPrompt = tknStr( getField( msgs, "audioPrompt" ));
			TknID msgLst = getField( msgs, "Messages" );
			sb->NMsgs = lstCnt( msgLst );
			if ( sb->NMsgs == 0 || sb->NMsgs > MAX_SUBJ_MSGS ) 
				tbErr( "Invalid msg list" );
			for ( int i=0; i<sb->NMsgs; i++ )
				sb->msgAudio[ i ] = tknStr( getField( msgLst, (char *)i ));			// file paths for each message
			logEvtNSNI( "Subj", "nm", sb->name, "nMsgs", sb->NMsgs );
		}
	}
	tbCloseFile( inFile );  //fclose( inFile );	// done with list_of_subjects.txt
	return Pkg;
}
*/
static short	asStIdx( TknID stNm ){			// => idx of csmState with this 'stNm', or alloc
	for( short i=0; i<nCSMstates; i++ ){
		if ( TBookCSM[i]->nmTknID == stNm.tknID ) 
//		if ( TBookCSM[i]->nmTknID.tknID == stNm.tknID ) 
			return i;
	}
	short stIdx = nCSMstates;
	if ( nCSMstates > MAX_CSM_STATES ) 
		tbErr( "too many CSM states" );
	csmState * cst = tbAlloc( sizeof( csmState ), "csmStates"); 
	cst->nmTknID = stNm.tknID;
	cst->nm = tknStr( stNm );
	cst->nActions = 0;
	for ( int e = (int)eNull; e < (int)eUNDEF; e++ ){ 
		cst->evtNxtState[ (Event)e ] = nCSMstates;		// default: all events stay in this state
	}
	nCSMstates++;
	TBookCSM[ stIdx ] = cst;
	return stIdx;
}
static void 	setAnyKey( void *vcst, TknID nxtstate ){ // set cst->evtNxtState[ all keys ] = nxtstate
	csmState * cst = (csmState *) vcst;
	for ( Event e = Home; e<= starTable;  e = (Event)( (short) e+1) )
		cst->evtNxtState[ e ] = asStIdx( nxtstate );
}
static void 	setCGrp( void *vcst, TknID cgrpLst ){	// set cst->evtNxtState[ x ] for x:nxt in cgrpLst
	csmState * cst = (csmState *) vcst;
	for ( short iE = 0; iE < lstCnt(cgrpLst); iE++ )
		cst->evtNxtState[ asEvent( getLstNm(cgrpLst, iE)) ] = asStIdx( getLstVal(cgrpLst, iE));
}
void 					addPlaySys( char *nm ){
	char path[MAX_PATH];
	buildPath( path, TB_Config.systemAudio, nm, ".wav" ); 
	SysAudio[ nPlaySys ].sysNm = nm;
  SysAudio[ nPlaySys ].sysPath = allocStr( path );
	nPlaySys++;
}
void 					readControlDef( void ){				// parse control.def => Config & TBookCSM[]
	TknID cdef = parseFile( TBP[ pCSM_DEF ] );
	TknID cfg = getField( cdef, "config" );
	
	TB_Config.systemAudio = loadPath( getField( cfg, "systemAudio" ));
	TB_Config.default_volume = getIntFld( cfg, "default_volume", 5 );
	TB_Config.powerCheckMS = getIntFld( cfg, "powerCheckMS", 60000 );	// once/min
	TB_Config.shortIdleMS = getIntFld( cfg, "shortIdleMS", 2000 );		// 2 sec
	TB_Config.longIdleMS = getIntFld( cfg, "longIdleMS", 30000 );		// 30 sec
	
	TB_Config.bgPulse 				= getStrFld( cfg, "bgPulse", 			TB_Config.bgPulse );
	TB_Config.fgPlaying 			= getStrFld( cfg, "fgPlaying",  	TB_Config.fgPlaying );
	TB_Config.fgPlayPaused 		= getStrFld( cfg, "fgPlayPaused", TB_Config.fgPlayPaused );
	TB_Config.fgRecording 		= getStrFld( cfg, "fgRecording", 	TB_Config.fgRecording );
	TB_Config.fgRecordPaused 	= getStrFld( cfg, "fgRecordPaused", TB_Config.fgRecordPaused );
	TB_Config.fgSavingRec 		= getStrFld( cfg, "fgSavingRec", 	TB_Config.fgSavingRec );
	TB_Config.fgSaveRec				= getStrFld( cfg, "fgSaveRec", 		TB_Config.fgSaveRec );
	TB_Config.fgCancelRec			= getStrFld( cfg, "fgCancelRec", 	TB_Config.fgCancelRec );
	TB_Config.fgUSB_MSC 			= getStrFld( cfg, "fgUSB_MSC", 		TB_Config.fgUSB_MSC );
	TB_Config.fgTB_Error 			= getStrFld( cfg, "fgTB_Error", 	TB_Config.fgTB_Error );
	TB_Config.fgNoUSBcable 		= getStrFld( cfg, "fgNoUSBcable", TB_Config.fgNoUSBcable );
	TB_Config.fgUSBconnect 		= getStrFld( cfg, "fgUSBconnect", TB_Config.fgUSBconnect );
	TB_Config.fgPowerDown 		= getStrFld( cfg, "fgPowerDown", 	TB_Config.fgPowerDown );

	TB_Config.minShortPressMS = getIntFld( cfg, "minShortPressMS", 50 );	// extern from inputmanager.c
	TB_Config.minLongPressMS = getIntFld( cfg, "minLongPressMS", 600 );	// extern from inputmanager.c

	TB_Config.initState = asStIdx( getField( cfg, "initState" ));
	
	TknID cgrps = getField( cdef, "CGroups" );
	short nCGrps = lstCnt( cgrps );
	char Nm[MAX_PATH] = { 0 };	//DEBUG
	char Val[MAX_DBG_LEN] = {0}; char* Val2, *Val3;	//DEBUG
	
	TknID cstates =  getField( cdef, "CStates" );
	for ( short iSt=0; iSt < lstCnt( cstates ); iSt++ ){

		TknID csNm = getLstNm( cstates, iSt );
		TknID csDef = getLstVal( cstates, iSt );  	// defn of cstate[iSt]
		toStr( Nm, csNm );  toVStr( Val,0, &Val2, &Val3, csDef ); //DEBUG
		if ( !isLst( csDef )) 
			tbErr( "controlState defn not an object" );
		short stIdx = asStIdx( csNm );
		csmState * cst = TBookCSM[ stIdx ];
		// now fill in cst-> from defn
		TknID actV, argV;
		for ( short stF=0; stF < lstCnt( csDef ); stF++ ){
			TknID nm = getLstNm( csDef, stF );		toStr( Nm, nm );	//DEBUG
			TknID val = getLstVal( csDef, stF );	toVStr( Val,0, &Val2, &Val3, val ); //DEBUG
			if ( nm.tknID==toTkn( "Actions" ).tknID ){
				for ( short iA=0; iA < MAX_ACTIONS; iA++ ) 
					cst->Actions[iA].act = aNull;
				TknID actLst = val;
				cst->nActions = lstCnt( actLst );
				for ( short iA=0; iA < cst->nActions; iA++ ){		// list of tkns, or [ tkn arg ]
					TknID actdefn = getLstVal( actLst, iA );	toStr( Val, actdefn );	//DEBUG
					cst->Actions[ iA ].arg = NULL; // by default
					if ( isLst( actdefn )){ // [ tkn, arg ]
						actV = getLstVal( actdefn, 0 );		toStr( Val, actV );	//DEBUG
						argV = getLstVal( actdefn, 1 );		toStr( Nm, argV );	//DEBUG
						cst->Actions[ iA ].act = asAction( actV );	// Action
						cst->Actions[ iA ].arg = tknStr( argV );	// arg
						if ( cst->Actions[ iA ].act == playSys ) 
							addPlaySys( cst->Actions[ iA ].arg );  // add to list of prompts in use
					} else {
						actV = actdefn;		toStr( Val, actV );	//DEBUG
						cst->Actions[ iA ].act = asAction( actV );	// Action
					}
				}
			} else if ( nm.tknID==toTkn( "CGroups" ).tknID ){
				TknID grpLst = val;			// [ CG1, CG2... ]
				for ( short iG=0; iG < lstCnt( grpLst ); iG++ ){
					TknID cGrp = getLstVal( grpLst, iG );	// nm of CGrp
					TknID gDef = getField( cgrps, tknStr( cGrp ));	// lookup cgrp definition
					setCGrp( cst, gDef );
				}
			} else {	// Evt:St pair
				Event e = asEvent( nm );
				if ( e == anyKey ) 
					setAnyKey( cst, val );
				else
					cst->evtNxtState[ e ] = asStIdx( val );		// nxtState for incoming event 'e'
			}
		}
		//TODO: verify cst->evtNxtState[] are all valid 
	}
	dbgLog( "C CSM.def has %d CGroups & %d states \n", nCGrps, nCSMstates );
}
// controlParser.cpp 
