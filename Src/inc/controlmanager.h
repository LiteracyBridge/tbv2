// TBookV2		controlmanager.h

#ifndef CONTROL_MGR_H
#define CONTROL_MGR_H
#include "tbook.h"

// define LOAD_CSM_DYNAMICALLY to load CSM from system/control.def
//  #define LOAD_CSM_DYNAMICALLY

//  ifdef LOAD_CSM_DYNAMICALLY controls:
//    in tbook_csm.c  --  allocation of space for: CSM_Version, nCSMstates,	TB_Config, TBookCSM[],	nPlaySys, &	SysAudio[]
//    in tknTable.c -- code & storage tables for parsing:  

typedef struct {
	Action	act;
	char *	arg;
} csmAction;


#define		MAX_SUBJ_MSGS			 20		// allocates: (array of ptrs) per subject
#define		MAX_SUBJS					 20		// allocates: (array of ptrs)
#define		MAX_ACTIONS					6		// max # actions per state -- allocs (array of csmAction)
#define		MAX_CSM_STATES		120		// max # csm states -- (allocs array of ptrs)
#define   MAX_PKGS						4		// max # of content packages on a device
#define		MAX_PLAY_SYS			 30		// max # of distince PlaySys() prompts in use

#define		MAX_VERSION_LEN		100		// max length of contol.def 1st line -- version string
extern char CSM_Version[ ];

// ---------TBook Control State Machine
typedef struct {	// csmState
	short 							nmTknID;										// TknID for name of this state
	char *							nm;													// str name of this state
	short 							evtNxtState[ eUNDEF ];			// nxtState for each incoming event (as idx in TBookCSM[])
	short								nActions;
	csmAction 					Actions[ MAX_ACTIONS ];
} csmState;

typedef	struct {		// paths for all playSys() prompts used in CSM
		char * sysNm;
		char * sysPath;
}	SysAudio_t;

//  ------------  TBook Content
typedef struct { 	// tbSubject										// info for one subject
	char *							path;												// path to Subj directory
	char *							name;												// identifier
	char *  						audioName;									// file path
	char *  						audioPrompt;								// file path
	short								NMsgs;											// number of messages
	char *  						msgAudio[ MAX_SUBJ_MSGS ];	// file paths for each message
	MsgStats *					stats;
} tbSubject;
//
typedef struct TBPackage {	// TBPackage_t				// path & subject list for a package
	int									idx;
	char * 							path;
	char *							packageName;
	int 								nSubjs;
	tbSubject *					TBookSubj[ MAX_SUBJS ];
} TBPackage_t;

extern int 						nPackages;									// num packages found on device
extern TBPackage_t *	TBPackage[ MAX_PKGS ];			// package info
extern int						iPkg;
extern TBPackage_t 	* TBPkg;											// TBook content in use

// TBook ControlStateMachine
extern int						nCSMstates;										// #states defined				
extern csmState *			TBookCSM[ ];		    // TBook state machine definition

extern int						nPlaySys;			// # prompts used
extern SysAudio_t 		SysAudio[];		// defined in tbook_csm.c

extern void						initControlManager( void );						// initialize & run TBook Control State Machine
extern void						buildPath( char *dstpath, const char *dir, const char *nm, const char *ext ); // dstpath[] <= "dir/nm.ext"
extern void						findPackages( void );									// scan for M0:/package*/  directories
TBPackage_t * 				readContent( const char * pkgPath, int pkgIdx );		// parse list_of_subjects.txt & messages.txt for each Subj => Content
extern void 					readControlDef( void );								// parse control.def => Config & TBookCSM[]
extern void 					writeCSM( void );			                        // controlmanagertest.c -- write current CSM to tbook_csm.c

typedef struct { 		                //DBG:  Evt -> NxtSt  (both numbers & names)
	Event    evt;
	char *   evtNm;
	short    nxtSt;
	char *   nstStNm;
} StTrans;

typedef struct {						// CSM state variables
	short 		iCurrSt;				// index of current csmState
	short		iPrevSt;				// idx of previous state
	short		iSavedSt[5];			// possible saved states

	Event		lastEvent;				//DBG: id of last event
	char *		lastEventName;			//DBG: str nm of last event
	char * 		currStateName;			//DBG: str nm of current state
	StTrans  	evtNxtSt[ eUNDEF ];	    //DBG: expanded transitions for current state
	char *      prevStateName;			//DBG: name of prev
    char *		saveStName[5];			//DBG: names of remembered states
	
	short		volume;
	short		speed;
  
	short		iSubj;					// index of current Subj
	short		iMsg;					// index of current Msg within Subj

	csmState *  cSt;					// -> TBookCSM[ iCurrSt ]	
}	TBook_t;

extern TBook_t TBook;
extern osTimerId_t  	timers[3];

extern void						assertValidState( int iState );
extern void 					USBmode( bool start);
extern void   				playRecAudio( void );
extern void						saveRecAudio( char *arg );
extern void 					playSysAudio( char *arg );
extern void 					playSubjAudio( char *arg );
extern void						startRecAudio( char *arg );
extern void						saveWriteMsg( char *txt );
extern void 					showPkg( void );
extern void 					playNxtPackage( void );
extern void 				  changePackage( void );
extern void						executeCSM( void );

extern void 					controlTest( void );

#endif
