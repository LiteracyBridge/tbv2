// TBookV2		packageData.h

#ifndef PACKAGE_DATA_H
#define PACKAGE_DATA_H
#include "tbook.h"

//  ------------  TBook Content

// package_data.txt  file structure:
// Version string
// NPaths     //  # of content dirs
//   path[0]    //  directory containing audio   0
//   ...
//   path[NPaths-1]
// NPkgs                 // P0
//   text_name           // P0
//   short_name          // P0
//   prompts_path_list   // P0
// NSubjs                // P0
//   text_name           // P0.S0
//   short_prompt        // P0.S0
//   invitation          // P0.S0
// NMessages             // P0.S0
//   dir + audio_file    // P0.S0.M0 
//   ...
//   dir + audio_file    // P0.S0.Mm 
//   text_name           // P0.S1
//   short_prompt        // P0.S1
//   invitation          // P0.S1
// NMessages             // P0.S1
//   dir + audio_file    // P0.S1.M0 
//   ...
//   dir + audio_file    // P0.S0.Mm 

typedef	struct AudioPaths {	// AudioPaths_t--  holds paths for TBook audio contents
	int nPaths;             // number of different content paths
	char * audPath[10];     // audio paths[ 0 .. nPaths-1 ]    
} AudioPaths_t;

typedef struct AudioFile {  // AudioFile_t -- location of a content audio file
    int pathIdx;            // index of audio directory in AudioPaths
    char * filename;        // filename within directory 
} AudioFile_t;

typedef struct PathList {   // PathList_t--  list of ContentPath indices in search order
    short PathLen;          // # of dirs to search
    short DirIdx[10];       // indices in ContentPaths[] 
} PathList_t;

typedef struct MsgList {
	int             nMsgs;          // number of messages
	AudioFile_t*    msg[20];        // path & filename of resolved audio files   (dummy size for debugger)
} MsgList_t;
    
typedef struct Subject { 	// Subject_t-- info for one subject
	char *		    subjName;		// text identifier for log messages
//    PathList_t *    audio_dirs;     // directories to search for audio files
    AudioFile_t*    shortPrompt;    // dir+name of subject's short audio prompt
    AudioFile_t*    invitation;     // dir+name of subject's audio invitation
	MsgStats *		stats;
    MsgList_t *     messages;       // cnt & ptrs to message AudioFile_t
} Subject_t;

typedef struct SubjList {
	int 			nSubjs;         // number of subjects in package
	Subject_t*      subj[20];       // cnt & ptrs to descriptions of subjects   (dummy size for debugger)
} SubjList_t;

typedef struct Package {	// Package_t-- path & subject list for a package
	int				pkgIdx;         // index within TBPackages[]
	char *			packageName;    // text identifier for log messages
    AudioFile_t*    pkg_prompt;     // audio prompt for package
    PathList_t *    prompt_paths;   // search path for prompts
	SubjList_t *	subjects;       // description of each subject
} Package_t;

typedef struct PackageList {
	int 			nPkgs;          // number of packages in deployment
	Package_t*      pkg[20];        // cnt & ptrs to descriptions of packages   (dummy size for debugger)
} PackageList_t;

typedef struct Deployment {
    char *          Version;        // version string for loaded deployment
    AudioPaths_t *  AudioPaths;     // list of all directories containing audio files for this Deployment
    
    PackageList_t * packages;       // cnt & ptrs to descriptions of packages
} Deployment_t;


// ControlStateMachine structures--  read from  system/control_def.txt  
typedef	struct AudioList {	// AudioList_t--  lists all names used in CSM PlaySys() actions
	int nSysA;              // number of different sysAudios
	char * sysA[10];        // sysAudio names  
} AudioList_t;    

typedef struct csmAction {  // csmAction_t -- enum Action & string arg
	Action	act;
	char *	arg;
} csmAction_t;
typedef	struct ActionList {	// ActionList_t--  holds cnt & ptrs to csmActions
	int nActs;              // # actions
	csmAction_t * Act[10];  // Action & arg  
} ActionList_t;    
typedef	struct CState {	// CState_t --  Control State definition
    short idx;              // idx of this state
    char * nm;              // name of this state
	short nNxtStates;       // # possible events
    short * evtNxtState;    // index of next state for each possible event
    ActionList_t * Actions; // cnt & array of csmAction_t
} CState_t;    

typedef	struct CSList {	// CSList_t --  cnt & ptrs to each ControlState
	int nCS;            // number of different ControlStates
	CState_t * CS[100  ];  // ptr to ControlState def  
} CSList_t;

typedef	struct CSM {
    char *          Version;        // CSM version
    TBConfig_t *    TBConfig;       // TBook config values
    AudioList_t *   SysAudio;       // cnt & names of PlaySys() args in CSM
    
    CSList_t *      CsmDef;         // cnt & definitions of ControlStateMachine states
    
} CSM_t;

// Deployment data from  packages_data.txt
extern Deployment_t *   Deployment; // cnt & ptrs to info for each loaded content Package
extern int			    iPkg;       // index of currPkg in Deployment
extern Package_t *      currPkg;	// TBook content package in use

// Deployment loading & access interface
extern bool             loadPackageData( void );                        // load structured TBook package contents 

extern char *           getAudioPath( char * path, AudioFile_t * aud ); // fill path[] with dir/filename & return it
extern char *           findAudioPath( char * path, PathList_t * srchpaths, char * nm ); // fill path[] with first dir/nm.wav & return it
extern int              nSubjs( void );                                 // return # of subjects in current package ( currPkg )
extern Subject_t *      gSubj( int iSubj );                             // return iSubj subject from current package ( currPkg ) 
extern int              nMsgs( Subject_t * subj );                      // return # of messages in subj
extern AudioFile_t *    gMsg( Subject_t * subj, int iMsg );             // return audioFile [iMsg] from subject subj 
extern int              nPkgs( void );                                  // return # of packages in current deployment
extern Package_t *      gPkg( int iPkg );                               // return iPkg'th package from current deployment
extern char *           pkgNm( void );                                  // return text name of current Package
extern char *           subjNm( Subject_t *subj );                      // return text name of Subject

// ControlStateMachine  data from  control_def.txt
extern CSM_t *          CSM;                                            // extern ptr to definition of CSM
// CSM loading & access interface
extern bool             loadControlDef( void );                         // load structured Control State Machine
extern void             preloadCSM( void );	                           	// load pre-defined CSM for empty filesys-- QcTest
extern CState_t *       gCSt( int idx );                                // return ptr to CState[ idx ]
extern char *           gStNm( CState_t *st );                          // return name of CSt
extern int              nActs( CState_t *st );                          // # of actions for CState
extern csmAction_t *    gAct( CState_t * st, int idx );                 // return Action[idx] of CState
extern int              nSysAud( void );                                // return # of SysAudio names used by CSM
extern char *           gSysAud( int idx );                             // return SysAudio[ idx ]
#endif           // PACKAGE_DATA_H

