//TBook Control State Machine storage  tbook_csm.c 
// 
#include "controlmanager.h" 

// define LOAD_CSM_DYNAMICALLY to load CSM from system/control.def
// #define LOAD_CSM_DYNAMICALLY

#ifdef LOAD_CSM_DYNAMICALLY
  // declare variables to be loaded by controlParser
	char CSM_Version[ MAX_VERSION_LEN ];

	int						nCSMstates = 0;
	TBConfig_t		TB_Config;
	csmState *		TBookCSM[ MAX_CSM_STATES ];		// TBook state machine definition
	
	int						nPlaySys = 0;
	SysAudio_t		SysAudio[ MAX_PLAY_SYS ];
#else
#include "tbook_csm_def.h"
#endif

//end tbook_csm.c
