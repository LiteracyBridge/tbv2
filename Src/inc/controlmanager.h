// TBookV2		controlmanager.h

#ifndef CONTROL_MGR_H
#define CONTROL_MGR_H
#include "tbook.h"

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
extern csmState *			TBookCSM[ MAX_CSM_STATES ];		// TBook state machine definition

extern int						nPlaySys;			// # prompts used
extern SysAudio_t 		SysAudio[];		// defined in tbook_csm.c

extern void						initControlManager( void );						// initialize & run TBook Control State Machine
extern void						buildPath( char *dstpath, const char *dir, const char *nm, const char *ext ); // dstpath[] <= "dir/nm.ext"
extern void						findPackages( void );									// scan for M0:/package*/  directories
TBPackage_t * 				readContent( const char * pkgPath, int pkgIdx );		// parse list_of_subjects.txt & messages.txt for each Subj => Content
extern void 					readControlDef( void );								// parse control.def => Config & TBookCSM[]

typedef struct {							// CSM state variables
	short 		iCurrSt;	// index of current csmState
	char * 		currStateName;	// str nm of current state
	Event		lastEvent;		// str nm of last event
	char *		lastEventName;	// str nm of last event
	char *		evtNms[ eUNDEF ];
	char *		nxtEvtSt[ eUNDEF ];
	
	short		iPrevSt;	// idx of previous state
	short		iNextSt;	// nextSt from state machine table ( can be overridden by goBack, etc )
	short		iSavedSt[5];	// possible saved states
  
	short		volume;
	short		speed;
  
	short		iSubj;		// index of current Subj
	short		iMsg;		// index of current Msg within Subj

	csmState *  cSt;		// -> TBookCSM[ iCurrSt ]
	
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
