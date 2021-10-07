// TBookV2  packageData.c
//   Gene Ball  Sept2021

#include "tbook.h"
#include "packageData.h"
//#include "controlmanager.h"

// --------   read package_data.txt to get structure & audio files for loaded content
Deployment_t *  Deployment = NULL;          // extern cnt & ptrs to info for each loaded content Package
CSM_t *         CSM = NULL;                 // extern ptr to definition of CSM
//
// local shared utilities
char            line[200];                  // internal text line buffer shared by all packageData routines
int             errCount;                   // # errors while parsing this file
char *          errType;                    // name of file type being loaded, for error messages
void *          loadErr( const char * msg ){
    errLog( "%s: %s", errType, msg ); 
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
int             loadInt( FILE *inF, const char *typ ){          // read 'typ' int from inF -- loadErr if fails
    int v;
    if ( fscanf( inF, "%d\n", &v )!= 1 ){ 
        loadErr( typ ); 
        return 0; 
    } else
        return v;
}
char *          loadStr( FILE *inF, const char *typ ){          // read 'typ' string from inF, alloc & return ptr -- loadErr if fails
    if ( fscanf( inF, "%s\n", line )==1  )
        return allocStr( line, typ );
    else {
        loadErr( typ );
        return NULL;
    }
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
    int dirCnt = loadInt( inF, "pkg_dat dirCnt" );
    AudioPaths_t * dirs = (AudioPaths_t *) tbAlloc( sizeof(int) + dirCnt * sizeof(char *), "AudioPaths" );
 
    dirs->nPaths = dirCnt;
    for ( int i=0; i<dirCnt; i++ ){
        dirs->audPath[i] = loadStr( inF, "audPath" );
    }
    return dirs;
}
PathList_t *    loadSearchPath( FILE *inF ){                    // parse next pkg_dat line as list of ContentPath indices
    int d[10];
    int dcnt = fscanf( inF, "%d;%d;%d;%d;%d;%d;%d;%d;%d;%d;", &d[0], &d[1], &d[2], &d[3], &d[4], &d[5], &d[6], &d[7], &d[8], &d[9] );
    PathList_t * plist = tbAlloc( sizeof( PathList_t ), "pathList" );        // always allocate space for 10  (only 1 per package)
    plist->PathLen = dcnt;
    for (int i=0; i<dcnt; i++ ){
        plist->DirIdx[i] = d[i];
    }
    return plist;
}
AudioFile_t *   loadAudio( FILE *inF ){                         // load dirIdx & filename from pkg_dat line
    AudioFile_t * audfile = tbAlloc( sizeof(AudioFile_t), "audFile" );
    if ( fscanf( inF, "%d %s\n", &audfile->pathIdx, line )==2  ){
        audfile->filename = allocStr( line, "audFilename" );
    } else errLog( "pkg_dat audFile fail" );        
   
    return audfile;
}
Subject_t *     loadSubject( FILE *inF ){                       // parse content subject from pkg_dat 
    Subject_t * subj = tbAlloc( sizeof( Subject_t ), "subject" ); 
    if ( fscanf( inF, "%s\n", line )==1  ){
            subj->subjName = allocStr( line, "subjName" );
        } else { errLog( "pkg_dat Subj.name fail" );  return NULL; }   

    subj->shortPrompt = loadAudio( inF );
    subj->invitation = loadAudio( inF );

    int nMsgs;
    if ( fscanf( inF, "%d\n", &nMsgs )!= 1 ) { errLog( "pkg_dat nMsgs fail" ); return NULL; }

    subj->messages = (MsgList_t *) tbAlloc( sizeof(int) + nMsgs* sizeof(void *), "subjList" );
    subj->stats = tbAlloc( sizeof(MsgStats), "stats" );
    subj->messages->nMsgs = nMsgs;            
    for ( int i=0; i<nMsgs; i++ ){
        subj->messages->msg[i] = loadAudio( inF );
    }
    return subj;
}
Package_t *     loadPackage( FILE *inF, int pkgIdx ){           // parse one content package from pkg_dat 
    Package_t * pkg = ( Package_t * ) tbAlloc( sizeof( Package_t ), "package" ); 
    
    if ( fscanf( inF, "%s\n", line )==1  ){
            pkg->packageName = allocStr( line, "pkgName" );
        } else { errLog( "pkg_dat Pkg[%d].name fail", pkgIdx );  return NULL; }   
    
    pkg->pkg_prompt = loadAudio( inF );                // audio prompt for package
    pkg->prompt_paths = loadSearchPath( inF );         // search path for prompts

	int nSubjs;                                         // number of subjects in package
    if ( fscanf( inF, "%d\n", &nSubjs )!= 1 ){ errLog( "pkg_dat nSubjects fail" ); return NULL; }
        
    pkg->subjects = (SubjList_t *) tbAlloc( sizeof(int) + nSubjs* sizeof(void *), "subj_list" );
    pkg->subjects->nSubjs = nSubjs;
    for ( int i=0; i<nSubjs; i++){
        pkg->subjects->subj[i] = loadSubject( inF );
    }       
    return pkg;
}
//
// public routine to load a full Deployment from  /content/packages_data.txt
bool            loadPackageData( void ){                        // load structured TBook package contents 
    errType = "Deployment";
    errCount = 0;
    Deployment = (Deployment_t *) tbAlloc( sizeof(Deployment_t), "Deployment" );
    
	FILE *inFile = tbOpenRead( TBP[ pPKG_DAT ] ); 
	if ( inFile == NULL ){ errLog( "package_data.txt not found\n" );  return false;  }
    // /content/package_data.txt  structure:

	if ( fscanf( inFile, "%[^\n]\n", line )==1  ){    // version string can have white space
        Deployment->Version = allocStr( line, "deployVer" );
        dbgLog("! Package_data: %s",  Deployment->Version );
    } else loadErr( "Version" );        
         
    Deployment->AudioPaths = loadAudioPaths( inFile );
    
    int nPkgs = loadInt( inFile, "nPkgs" );
    Deployment->packages = (PackageList_t *) tbAlloc( sizeof(int) + nPkgs* sizeof(void *), "deployment" ); 
    Deployment->packages->nPkgs = nPkgs;
    for ( int i=0; i<nPkgs; i++ ){
        Deployment->packages->pkg[i] = loadPackage( inFile, i );
    }
    
	tbCloseFile( inFile );
    if ( errCount > 0 ){ errLog( "packages_data: %s parse errors", errCount ); return false; }
    return true;
}

//
// private ControlStateMachine loading routines
TBConfig_t *    loadTbConfig( FILE *inF ){                      // load & return CSM configuration variables
    TBConfig_t * cfg = (TBConfig_t *) tbAlloc( sizeof(TBConfig_t), "TBConfig" );

    cfg->default_volume     = (short) loadInt( inF, "def_vol" );
    cfg->powerCheckMS       = loadInt( inF, "powerChk" ); 
    cfg->shortIdleMS        = loadInt( inF, "shortIdle" ); 
    cfg->longIdleMS         = loadInt( inF, "longIdle" ); 
    cfg->minShortPressMS    = loadInt( inF, "shortPr" );
    cfg->minLongPressMS     = loadInt( inF, "longPr" ); 
    cfg->qcTestState        = loadInt( inF, "qcState" );
    cfg->initState          = loadInt( inF, "initState" );

    cfg->systemAudio    = loadStr( inF, "sysAud" ); 
    cfg->bgPulse        = loadStr( inF, "bgPulse" ); 
    cfg->fgPlaying      = loadStr( inF, "fgPlaying" ); 
    cfg->fgPlayPaused   = loadStr( inF, "fgPlayPaused" ); 
    cfg->fgRecording    = loadStr( inF, "fgRecording" ); 
    cfg->fgRecordPaused = loadStr( inF, "fgRecordPaused" ); 
    cfg->fgSavingRec    = loadStr( inF, "fgSavingRec" ); 
    cfg->fgSaveRec      = loadStr( inF, "fgSaveRec" ); 
    cfg->fgCancelRec    = loadStr( inF, "fgCancelRec" ); 
    cfg->fgUSB_MSC      = loadStr( inF, "fgUSB_MSC" ); 
    cfg->fgTB_Error     = loadStr( inF, "fgTB_Error" ); 
    cfg->fgNoUSBcable   = loadStr( inF, "fgNoUSBcable" ); 
    cfg->fgUSBconnect   = loadStr( inF, "fgUSBconnect" ); 
    cfg->fgPowerDown    = loadStr( inF, "fgPowerDown" );  
    return cfg;
}
AudioList_t *   loadSysAudio( FILE *inF ){                      // load & return list of all PlaySys names used in CSM
    int cnt = loadInt( inF, "nSysAudio" );
    AudioList_t * lst = (AudioList_t *) tbAlloc( sizeof(int) + cnt * sizeof(char *), "SysAudio" );
 
    lst->nSysA = cnt;
    for ( int i=0; i<cnt; i++ ){
        lst->sysA[i] = loadStr(inF, "sysAud");
    }
    return lst;
}
csmAction_t *   loadAction( FILE *inF ){                        // load & return enum Action & string arg
    if ( fscanf( inF, "%s ", line )!=1  ){
        loadErr( "Action" );
    }
    csmAction_t *a = (csmAction_t *) tbAlloc( sizeof(csmAction_t), "csmAct" );
    for ( int i=0; ANms[i]!=NULL; i++ ){
        if ( strcasecmp( line, ANms[i] )==0 ){
            a->act = (Action) i;
            a->arg = (char *) loadStr( inF, "act_arg" );
            return a;
        }
    }
    strcat( line, " unrec Action");
    loadErr( line );
    return NULL;
}
CState_t *      loadCState( FILE *inF, int idx ){               // load & return definition of one CSM state
    CState_t * st = tbAlloc( sizeof( CState_t ), "CState" );
    
    st->idx         = (short) loadInt( inF, "st.Idx");
    if ( st->idx != idx )
        errLog("CSM sync st %d!=%d", idx, st->idx );
    st->nm          = loadStr( inF, "st.nm");
    st->nNxtStates  = (short) loadInt( inF, "st.nNxt");
    st->evtNxtState = (short *) tbAlloc( st->nNxtStates * sizeof(short), "evtNxtSt" );
    for ( int i=0; i<st->nNxtStates; i++ ){
        if ( fscanf( inF, "%hd,", &st->evtNxtState[i] )!= 1 ){ 
            errLog( "load evtNxtSt err" );
        }            
    }
    int nA = (short) loadInt( inF, "st.nActs" );
    st->Actions = (ActionList_t *) tbAlloc( sizeof(int) + nA * sizeof(csmAction_t *), "st.Actions" );
    st->Actions->nActs = nA;
    for ( int i=0; i<nA; i++ )
      st->Actions->Act[i] = loadAction( inF );
    return st;
}



//
// public routine to load the CSM from /system/control_def.txt

bool            loadControlDef( void ){                         // load structured Control State Machine 
    errType = "CSM";
    errCount = 0;
	CSM = (CSM_t *) tbAlloc( sizeof( CSM_t ), "CSM" );
    
	FILE *inFile = tbOpenRead( TBP[ pCSM_DEF ] ); 
	if ( inFile == NULL ){ errLog( "csm_data.txt not found\n" );  return false;  }

	if ( fscanf( inFile, "%[^\n]\n", line )==1  ){     // allow whitespace in Version
        CSM->Version = allocStr( line, "csmVer" );
        dbgLog("! Csm_data: %s",  CSM->Version );
    } else loadErr( "Version" ); 
    
	if ( fscanf( inFile, "%[^\n]\n", line )==1  ){     // allow whitespace in CsmCompileVersion
        // throw CsmCompileVersion away 
    } else loadErr( "Version" );        
    
    CSM->TBConfig = (TBConfig_t *) loadTbConfig( inFile );
    
    CSM->SysAudio = (AudioList_t *) loadSysAudio( inFile );
    
    int nS = loadInt( inFile, "nStates" );
    CSM->CsmDef = (CSList_t *) tbAlloc( sizeof(int) + nS * sizeof(void *), "CsmDef" );
    CSM->CsmDef->nCS = nS;
    for (int i=0; i<nS; i++){
        CSM->CsmDef->CS[i] = loadCState( inFile, i );
    }
     
	tbCloseFile( inFile );	
    if ( errCount > 0 ){ errLog( "control_def: %s parse errors", errCount ); return false; }
    return true;
}
// packageData.c 
