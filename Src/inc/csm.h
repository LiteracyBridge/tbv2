//
// Created by bill on 3/16/2023.
//

#ifndef TBV2_CSM_H
#define TBV2_CSM_H

#include "tb_enum.h"
#include "tbook.h"

// ControlStateMachine structures--  read from  system/control_def.txt
class AudioList {  // AudioList_t--  lists all names used in CSM PlaySys() actions
public:
    int nSysA;          // number of different sysAudios
    const char *sysA[10];       // sysAudio names
};

class csmAction {  // csmAction_t -- enum Action & string arg
public:
    CSM_ACTION act;
    const char *arg;
};

class ActionList { // ActionList_t--  holds cnt & ptrs to csmActions
public:
    int         nActs;              // # actions
    csmAction *Act[10];  // Action & arg
};

class CState { // CState_t --  Control State definition
public:
    int numActions() { return nActions;};
    csmAction *getAction(int ix) {return actions[ix];};
    const char *getName() { return nm; };

public:
    short        idx;                // idx of this state
    const char   *nm;                 // name of this state
    short        nNxtStates;         // # possible events
    const short  *evtNxtState;        // index of next state for each possible event
    short nActions;
    csmAction **actions;            // cnt & array of csmAction_t
};

class CSM {
public:
    static const char *eventName(int eventIx);
    static const char *actionName(int actionIx);

    int numStates() { return nCStates; }
    CState *getCState(int ix) { return cStates[ix];}

public:
    const char  *Version;            // CSM version
    int nCStates;
    CState **cStates;

private:
    static const char *eventNames[];
    static const char *actionNames[];
};

// ControlStateMachine  data from  control_def.txt
extern CSM *csm;                                            // extern ptr to definition of CSM
// CSM loading & access interface
extern bool loadControlDef(void);                         // load structured Control State Machine
extern void preloadCSM(void);                             // load pre-defined CSM for empty filesys-- QcTest

#endif //TBV2_CSM_H
