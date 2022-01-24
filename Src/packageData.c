// TBookV2  packageData.c
//   Gene Ball  Sept2021

#include "tbook.h"
#include "packageData.h"
/********************************************* Symbol Definitions coordinated with CSMcompile  ***********************/
// MUST MATCH:  typedef Action in tknTable.h 
// MUST MATCH:  CsmToken.java -- enum TknAction 
char *  ANms[] = { "aNull",
		"LED", 				"bgLED", 		"playSys", 		
		"playSubj", 		"pausePlay", 	"resumePlay", 
		"stopPlay", 		"volAdj", 		"spdAdj", 
		"posAdj",			"startRec", 	"pauseRec", 
		"resumeRec", 		"finishRec", 	"playRec", 		
		"saveRec", 			"writeMsg",		"goPrevSt", 
		"saveSt", 			"goSavedSt", 	"subjAdj", 		
		"msgAdj", 			"setTimer", 	"resetTimer", 
		"showCharge",		"startUSB", 	"endUSB", 	
		"powerDown", 		"sysBoot", 		"sysTest", 		
		"playNxtPkg", 	    "changePkg",	"playTune",
        "filesTest",
    NULL
};
char *	actionNm( Action a ){ return ANms[ (int)a ]; }

// MUST MATCH:  typedef Event in tknTable.h  
// MUST MATCH:  CsmToken.java -- enum TknEvent 
char *  ENms[] = { 
		"eNull", 
        "Home",     "Circle",     "Plus",     "Minus",     "Tree",     "Lhand",     "Rhand",     "Pot",     "Star",     "Table",     //=10
        "Home__",   "Circle__",   "Plus__",   "Minus__",   "Tree__",   "Lhand__",   "Rhand__",   "Pot__",   "Star__",   "Table__",   //=20
		"starHome", "starCircle", "starPlus", "starMinus", "starTree", "starLhand", "starRhand", "starPot", "starStar", "starTable", //=30
		"AudioStart",             "AudioDone",             "ShortIdle",             "LongIdle",             "LowBattery",            //=35 
        "BattCharging",           "BattCharged",           "FirmwareUpdate",        "Timer",                "ChargeFault",           //=40
        "LithiumHot",             "MpuHot",                "FilesSuccess",          "FilesFail",            "anyKey",                //=45
        "eUNDEF", //=46
    NULL    // end of list marker
};
// MUST MATCH:  typedef Event in tknTable.h
char *  shENms[] = { "eN",
	"Ho", "Ci", "Pl", "Mi", "Tr", "Lh", "Rh", "po", "St", "ta",
	"H_", "C_", "P_", "M_", "T_", "L_", "R_", "p_", "S_", "t_",
	"sH", "sC", "sP", "sM", "sT", "sL", "sR", "sp", "sS", "st",
	"aS", "aD", "sI", "lI", "bL", "bc", "bC", "fU", "Ti", "cF", 
	"LH", "MH", "fS", "fF", "aK", "eU",
    NULL
};
char *	eventNm( Event e )	{ return ENms[ (int)e ];; }
char *	shEvntNm( Event e )	{ return shENms[ (int)e ];; }
//
//
/***************************   read package_data.txt to get structure & audio files for loaded content  ***********************/
Deployment_t *  Deployment = NULL;          // extern cnt & ptrs to info for each loaded content Package
CSM_t *         CSM = NULL;                 // extern ptr to definition of CSM
TBConfig_t *	TB_Config;			        // TBook configuration variables

//
// local shared variables
char            line[202];                  // internal text line buffer shared by all packageData routines
int             errCount;                   // # errors while parsing this file
int             lineNum = 0;
char *          errType;                    // name of file type being loaded, for error messages

// local shared utilities
void *          loadErr( const char * msg ){                    // report & count error in 'errType' file, when parsing 'msg'
    errLog( "%s:L%d %s ", errType, lineNum, msg ); 
    errCount++;
    return NULL;
}
void		    appendIf( char * path, const char *suffix ){	// append 'suffix' if not there
	short len = strlen( path );
	short slen = strlen( suffix );
	if ( len<slen || strcasecmp( &path[len-slen], suffix )!=0 )
		strcat( path, suffix );
}
char *	        allocStr( const char * s, const char * strType ){ // allocate & copy s
	char * pStr = tbAlloc( strlen( s )+1, strType );
	strcpy( pStr, s );	
	return pStr;
}
void            ldDepLn( FILE *inF, const char *typ ){         // load next text line from deployment inF, trim comments & lead/trail whitespace, repeat if empty
    if ( errCount > 0 ) { line[0]='\0'; return; }
    
    while ( fscanf( inF, "%201[^\n]%*[\n]", line )==1  ){
        lineNum++;
        if ( strlen(line) > 200 ) loadErr("Line too long");
        char *cret = strchr( line, '\r' );
        if ( cret!=NULL ) *cret = '\0';
        char *hash = strchr( line, '#' );
        if ( hash!=NULL ) *hash = '\0';   // terminate line at first '#'
        for ( int i=0; i<strlen(line); i++ )
          if ( line[i] != ' ' && line[i] != '\t' ){
              if ( i>0 ) strcpy( &line[0], &line[i] );   // skip leading whitespace
              
              for (int j=strlen(line)-1; j>i; j-- ){
                  if ( line[j] == ' ' || line[j] == '\t' ){
                      line[j] = '\0';  // truncate trailing whitespace 
                  } else {
                      break;   // last non-whitespace char
                  }
              }
              return; // non-empty line -- so return  
          }              
    }
    loadErr( typ );  // no str found (EOF)
}
char *          ldDepStr( FILE *inF, const char *typ ){         // allocate & return next string from inF (ignoring comments & whitespace)
    ldDepLn( inF, typ );                // load next string into line[]
    return allocStr( line, typ );       // allocate & return it
}
int             ldDepInt( FILE *inF, const char *typ ){         // read 'typ' string from inF, alloc & return ptr -- loadErr if fails
    ldDepLn( inF, typ );    // line  gets next line (no comments, or lead/trail white space)  
    int v;
    if ( sscanf( line, "%d", &v )==1  )
        return v;
    
    loadErr( typ );
    return 0;
}

//
// public Deployment access routines
int             nSubjs( void ){                                 // return # of subjects in current package ( currPkg )
  if ( currPkg==NULL || currPkg->subjects==NULL ) 
      errLog( "nSubjs -- bad pkg" );
  return currPkg->subjects->nSubjs;  
}
Subject_t *     gSubj( int iSubj ){                             // return iSubj subject from current package ( currPkg ) 
  if ( currPkg==NULL || currPkg->subjects==NULL ) 
      errLog( "cSubj[%d] -- bad pkg", iSubj );
  if ( iSubj < 0 || iSubj >= currPkg->subjects->nSubjs ) 
      errLog( "cSubj(%d) bad idx", iSubj );
  Subject_t * subj = currPkg->subjects->subj[ iSubj ]; 
  if ( subj==NULL ) 
      errLog( "cSubj(%d) bad subj", iSubj );
  return subj;
}
int             nMsgs( Subject_t * subj ){                      // return # of messages in subj
    if ( subj==NULL || subj->messages==NULL )
      errLog( "nMsgs -- bad subj" );
    return subj->messages->nMsgs;    
}
AudioFile_t *   gMsg( Subject_t * subj, int iMsg ){             // return audioFile [iMsg] from subject subj 
  if ( currPkg==NULL || currPkg->subjects==NULL ) 
      errLog( "gMsg(%d) -- bad subj", iMsg );
  if ( iMsg < 0 || iMsg >= subj->messages->nMsgs ) 
      errLog( "gMsg(%s.%d) bad idx", subj->subjName, iMsg );
  AudioFile_t * aud = subj->messages->msg[ iMsg ]; 
  if ( aud==NULL ) 
      errLog( "gMsg(%s.%d) bad msg file", subj->subjName, iMsg );
  return aud;
}
int             nPkgs( void ){                                  // return # of packages in current deployment
    if ( Deployment==NULL || Deployment->packages==NULL )
        errLog( "nPkgs bad Deployment" );
    return Deployment->packages->nPkgs;
}
Package_t *     gPkg( int iPkg ){                               // return iPkg'th package from current deployment
    int nP = nPkgs();
    if ( iPkg < 0 || iPkg >= nP )
        errLog( "gPkg(%d) bad idx", iPkg );
    return Deployment->packages->pkg[ iPkg ];
}
char *          pkgNm( void ){                                  // return text name of current Package
    return currPkg->packageName; 
}
char *          subjNm( Subject_t *subj ){                      // return text name of Subject
    return subj->subjName;
}
char *          getAudioPath( char * path, AudioFile_t * aud ){ // fill path[] with dir/filename & return it
    AudioPaths_t * paths = Deployment->AudioPaths;
    if ( path==NULL || paths==NULL || aud->pathIdx < 0 || aud->pathIdx >= paths->nPaths )
       errLog( "buildAudioPath bad paths" ); 
    strcpy( path, paths->audPath[ aud->pathIdx ] );   // content directory path (ends in '/')
    strcat( path, aud->filename );                    // filename (with extension)
    return path;
}
char *          gPath( PathList_t *pathList, int idx ){         // return content directory path for pathList[i]
    if ( idx < 0 || pathList==NULL || idx >= pathList->PathLen )
        errLog( "gPath bad args" );
    int dirIdx = pathList->DirIdx[idx];
    if (dirIdx < 0 || dirIdx >= Deployment->AudioPaths->nPaths )
        errLog( "gPath bad dirIdx" );
    return Deployment->AudioPaths->audPath[ dirIdx ];
}
char *          findAudioPath( char * path, PathList_t * srchpaths, char * nm ){ // fill path[] with first dir/nm.wav & return it
    if ( path==NULL || srchpaths==NULL )
       errLog( "findAudioPath bad srchpaths" ); 
    char wavnm[50];
    strcpy( wavnm, nm );
    appendIf( wavnm, ".wav" );       // + .wav if not there
    
    for ( int i=0; i<srchpaths->PathLen; i++ ){
        strcpy( path, gPath( srchpaths, i ) );          // next content directory path in list
        strcat( path, wavnm );  
        if ( fexists( path ) )
            return path;            // found on path => return it
    }
    return NULL;
}
//
// private Deployment routines
AudioPaths_t *  loadAudioPaths( FILE *inF ){                    // parse pkg_dat list of Content directories
    if ( errCount > 0 ) return NULL;
    int dirCnt = ldDepInt( inF, "pkg_dat dirCnt" );
    AudioPaths_t * dirs = (AudioPaths_t *) tbAlloc( sizeof(int) + dirCnt * sizeof(char *), "AudioPaths" );
 
    dirs->nPaths = dirCnt;
    for ( int i=0; i<dirCnt; i++ ){
        dirs->audPath[i] = ldDepStr( inF, "audPath" );
    }
    return dirs;
}
PathList_t *    loadSearchPath( FILE *inF ){                    // parse next pkg_dat line as list of ContentPath indices
    if ( errCount > 0 ) return NULL;
    int d[10];
    ldDepLn( inF, "prompts_paths" );
    int dcnt = sscanf( line, "%d;%d;%d;%d;%d;%d;%d;%d;%d;%d; \n", &d[0], &d[1], &d[2], &d[3], &d[4], &d[5], &d[6], &d[7], &d[8], &d[9] );
    PathList_t * plist = tbAlloc( sizeof( PathList_t ), "pathList" );        // always allocate space for 10  (only 1 per package)
    plist->PathLen = dcnt;
    for (int i=0; i<dcnt; i++ ){
        plist->DirIdx[i] = d[i];
    }
    return plist;
}
AudioFile_t *   loadAudio( FILE *inF, const char *typ ){        // load dirIdx & filename from pkg_dat line
    if ( errCount > 0 ) return NULL;
    AudioFile_t * audfile = tbAlloc( sizeof(AudioFile_t), "audFile" );
    ldDepLn( inF, typ );
    char fname[100];
    if ( sscanf( line, "%d %s ", &audfile->pathIdx, fname )==2  ){
        audfile->filename = allocStr( fname, "audFilename" );
    } else loadErr( typ );        
    return audfile;
}
Subject_t *     loadSubject( FILE *inF ){                       // parse content subject from pkg_dat 
    if ( errCount > 0 ) return NULL;
    Subject_t * subj = tbAlloc( sizeof( Subject_t ), "subject" ); 
    
    subj->subjName = ldDepStr( inF, "subjName" );

    subj->shortPrompt = loadAudio( inF, "subj_pr" );
    subj->invitation = loadAudio( inF, "subj_inv" );

    int nMsgs = ldDepInt( inF, "nMsgs" );

    subj->messages = (MsgList_t *) tbAlloc( sizeof(int) + nMsgs* sizeof(void *), "subjList" );
    subj->stats = tbAlloc( sizeof(MsgStats), "stats" );
    subj->messages->nMsgs = nMsgs;            
    for ( int i=0; i<nMsgs; i++ ){
        subj->messages->msg[i] = loadAudio( inF, "msg" );
    }
    return subj;
}
Package_t *     loadPackage( FILE *inF, int pkgIdx ){           // parse one content package from pkg_dat 
    if ( errCount > 0 ) return NULL;
    Package_t * pkg = ( Package_t * ) tbAlloc( sizeof( Package_t ), "package" ); 

    pkg->pkgIdx = pkgIdx;
    pkg->packageName = ldDepStr( inF, "pkgName" );
    pkg->pkg_prompt = loadAudio( inF, "pkg_pr" );      // audio prompt for package
    pkg->prompt_paths = loadSearchPath( inF );         // search path for prompts

	int nSubjs = ldDepInt( inF, "nSubjs" );            // number of subjects in package
    pkg->subjects = (SubjList_t *) tbAlloc( sizeof(int) + nSubjs* sizeof(void *), "subj_list" );
    pkg->subjects->nSubjs = nSubjs;
    for ( int i=0; i<nSubjs; i++){
        pkg->subjects->subj[i] = loadSubject( inF );
    } 
    logEvtNSNI("LdPkg", "nm", pkg->packageName, "nSubj", pkg->subjects->nSubjs );
    return pkg;
}
//
const int PACKAGE_FORMAT_VERSION = 1;                 // format of Oct 2021
//
// public routine to load a full Deployment from  /content/packages_data.txt
bool            loadPackageData( void ){                        // load structured TBook package contents 
    errType = "Deployment";
    errCount = 0;
    lineNum = 0;
    Deployment = (Deployment_t *) tbAlloc( sizeof(Deployment_t), "Deployment" );
    
	  FILE *inFile = tbOpenRead( TBP[ pPKG_DAT ] );
    if (inFile == NULL) {
        loadErr( "package_data.txt not found" );
    } else {
        int fmtVer = ldDepInt( inFile, "fmt_ver" );
        if (fmtVer != PACKAGE_FORMAT_VERSION) {
            loadErr( "bad format_version" );
        } else {
            ldDepLn( inFile, "version" );
            Deployment->Version = allocStr( line, "deployVer" );
            logEvtNS( "Deploymt", "Ver", Deployment->Version );

            Deployment->AudioPaths = loadAudioPaths( inFile );

            int nPkgs = ldDepInt( inFile, "nPkgs" );
            Deployment->packages        = (PackageList_t *) tbAlloc( sizeof( int ) + nPkgs * sizeof( void * ),
                                                                     "deployment" );
            Deployment->packages->nPkgs = nPkgs;
            for (int i = 0; i < nPkgs; i++) {
                Deployment->packages->pkg[i] = loadPackage( inFile, i );
            }
        }
    }

    if (inFile != NULL) {
        tbCloseFile( inFile );
        if (errCount == 0) {
            errLog("null file but errCount is 0", errCount);
            errCount = 1;
        }
    }
    if (errCount > 0) {
        errLog( "packages_data: %d parse errors", errCount );
        return false;
    }
    return true;
}

//
// private ControlStateMachine loading routines
TBConfig_t *    loadTbConfig( FILE *inF ){                      // load & return CSM configuration variables
    if ( errCount > 0 ) return NULL;
    TBConfig_t * cfg = (TBConfig_t *) tbAlloc( sizeof(TBConfig_t), "TBConfig" );

    cfg->default_volume     = (short) ldDepInt( inF, "def_vol" );
    cfg->powerCheckMS       = ldDepInt( inF, "powerChk" ); 
    cfg->shortIdleMS        = ldDepInt( inF, "shortIdle" ); 
    cfg->longIdleMS         = ldDepInt( inF, "longIdle" ); 
    cfg->minShortPressMS    = ldDepInt( inF, "shortPr" );
    cfg->minLongPressMS     = ldDepInt( inF, "longPr" ); 
    cfg->qcTestState        = ldDepInt( inF, "qcState" );
    cfg->initState          = ldDepInt( inF, "initState" );

    cfg->systemAudio    = ldDepStr( inF, "sysAud" ); 
    cfg->bgPulse        = ldDepStr( inF, "bgPulse" ); 
    cfg->fgPlaying      = ldDepStr( inF, "fgPlaying" ); 
    cfg->fgPlayPaused   = ldDepStr( inF, "fgPlayPaused" ); 
    cfg->fgRecording    = ldDepStr( inF, "fgRecording" ); 
    cfg->fgRecordPaused = ldDepStr( inF, "fgRecordPaused" ); 
    cfg->fgSavingRec    = ldDepStr( inF, "fgSavingRec" ); 
    cfg->fgSaveRec      = ldDepStr( inF, "fgSaveRec" ); 
    cfg->fgCancelRec    = ldDepStr( inF, "fgCancelRec" ); 
    cfg->fgUSB_MSC      = ldDepStr( inF, "fgUSB_MSC" ); 
    cfg->fgTB_Error     = ldDepStr( inF, "fgTB_Error" ); 
    cfg->fgNoUSBcable   = ldDepStr( inF, "fgNoUSBcable" ); 
    cfg->fgUSBconnect   = ldDepStr( inF, "fgUSBconnect" ); 
    cfg->fgPowerDown    = ldDepStr( inF, "fgPowerDown" );  
    return cfg;
}
AudioList_t *   loadSysAudio( FILE *inF ){                      // load & return list of all PlaySys names used in CSM
    if ( errCount > 0 ) return NULL;
    int cnt = ldDepInt( inF, "nSysAudio" );
    AudioList_t * lst = (AudioList_t *) tbAlloc( sizeof(int) + cnt * sizeof(char *), "SysAudio" );
 
    lst->nSysA = cnt;
    for ( int i=0; i<cnt; i++ ){
        lst->sysA[i] = ldDepStr(inF, "sysAud");
    }
    return lst;
}
csmAction_t *   loadAction( FILE *inF ){                        // load & return enum Action & string arg
    if ( errCount > 0 ) return NULL;
    ldDepLn( inF, "action" );
 
    csmAction_t *a = (csmAction_t *) tbAlloc( sizeof(csmAction_t), "csmAct" );
    for ( int i=0; ANms[i]!=NULL; i++ ){
        if ( strcasecmp( line, ANms[i] )==0 ){
            a->act = (Action) i;
            a->arg = (char *) ldDepStr( inF, "act_arg" );
            return a;
        }
    }
    strcat( line, " unrec Action");
    loadErr( line );
    return NULL;
}
CState_t *      loadCState( FILE *inF, int idx ){               // load & return definition of one CSM state
    if ( errCount > 0 ) return NULL;
    CState_t * st = tbAlloc( sizeof( CState_t ), "CState" );
    
    st->idx         = (short) ldDepInt( inF, "st.Idx");
    if ( st->idx != idx )
        errLog("CSM sync st %d!=%d", idx, st->idx );
    st->nm          = ldDepStr( inF, "st.nm");
    st->nNxtStates  = (short) ldDepInt( inF, "st.nNxt");
    st->evtNxtState = (short *) tbAlloc( st->nNxtStates * sizeof(short), "evtNxtSt" );
    for ( int i=0; i<st->nNxtStates; i++ ){
        if ( fscanf( inF, "%hd,", &st->evtNxtState[i] )!= 1 ){ 
            loadErr( "load evtNxtSt" );  return NULL;
        }            
    }
    int nA = (short) ldDepInt( inF, "st.nActs" );
    st->Actions = (ActionList_t *) tbAlloc( sizeof(int) + nA * sizeof(csmAction_t *), "st.Actions" );
    st->Actions->nActs = nA;
    for ( int i=0; i<nA; i++ )
      st->Actions->Act[i] = loadAction( inF );
    return st;
}



//
// public routines to load & access the CSM from /system/control_def.txt
bool            loadControlDef( void ){                         // load structured Control State Machine 
    errType = "CSM";
    errCount = 0;
    lineNum = 0;
	CSM = (CSM_t *) tbAlloc( sizeof( CSM_t ), "CSM" );
    
	FILE *inFile = tbOpenRead( TBP[ pCSM_DEF ] ); 
	if ( inFile == NULL ){ 
        errLog( "csm_data.txt not found" ); 
        return false; 
    }
    ldDepLn( inFile, "version" );
    CSM->Version = allocStr( line, "csmVer" );
    logEvtNS( "TB_CSM", "ver", CSM->Version );		    // log CSM version comment
    
    ldDepLn( inFile, "csmCompVer" );     // allow whitespace in CsmCompileVersion
    // throw CsmCompileVersion away 
    
    TB_Config = CSM->TBConfig = (TBConfig_t *) loadTbConfig( inFile );
    
    CSM->SysAudio = (AudioList_t *) loadSysAudio( inFile );
    
    int nS = ldDepInt( inFile, "nStates" );
    CSM->CsmDef = (CSList_t *) tbAlloc( sizeof(int) + nS * sizeof(void *), "CsmDef" );
    CSM->CsmDef->nCS = nS;
    for (int i=0; i<nS; i++){
        CSM->CsmDef->CS[i] = loadCState( inFile, i );
    }
     
	if ( inFile != NULL ) tbCloseFile( inFile );	
    if ( errCount > 0 ){ errLog( "csm_data: %d parse errors", errCount ); return false; }
    return true;
}
int             nSysAud( void ){                                // return # of SysAudio names used by CSM
    if ( CSM==NULL || CSM->SysAudio==NULL ){
        errLog( "bad SysAudio" );
        return 0;
    }
    return CSM->SysAudio->nSysA;
}
char *          gSysAud( int idx ){                             // return SysAudio[ idx ]
    if ( idx<0 || idx >= nSysAud() )
        errLog( "gSysAud(%d) bad idx", idx );
    return CSM->SysAudio->sysA[ idx ];
}
CState_t *      gCSt( int idx ){                                // return ptr to CState[ idx ]
    if ( idx<0 || idx >= CSM->CsmDef->nCS )
        errLog( "gCst(%d) bad idx", idx );
    return CSM->CsmDef->CS[ idx ];
}
char *          gStNm( CState_t *st ){                          // return name of CSt
    if ( st==NULL ){
        errLog( "bad CState" );
        return "bad";
    }
    return st->nm;
}
int             nActs( CState_t *st ){                          // # of actions for CState
    if ( st==NULL || st->Actions==NULL ){
        errLog( "CSt bad Actions" );
        return 0;
    }
    return st->Actions->nActs;
}
csmAction_t *   gAct( CState_t * st, int idx ){                 // return Action[idx] of CState
    if ( idx<0 || idx >= nActs(st) )
        errLog( "gCst(%d) bad idx", idx );
    return st->Actions->Act[ idx ];
}
// packageData.c 
