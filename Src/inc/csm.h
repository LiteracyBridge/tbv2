//
// Created by bill on 7/26/2023.
//

#ifndef TBV2_CSM_H
#define TBV2_CSM_H

#include <assert.h>
#include <stdint.h>
#include <iostream>

#include "tb_enum.h"

// Layout of the csm data:
// signature (two bytes, to reveal any endian issues)
// num_cstates | num_cgroups << 8
// offset of cstate 0
// ...
// offset of cstate n
// offset of cgroup 0
// ...
// offset of cgroup n
// offset of string pool
// cstate 0:
//   offset of name
//   num_event/new_state pairs | num_actions << 8
//   action 0
//   offset of arg 0
//   ...
//   action n
//   offset of arg n
//   event 0/new_state 0
//   ...
//   event j/new_state j
//   ff / cgroup 0    last cgroup specifiedF
//   ...
//   ff / cgroup n    first cgroup specified
// ...
// cgroup 0:
//   offset of name
//   num_event/new_state pairs
//   event 0/new state 0
//   ...
// stringpool

/**
 * Event-entry actions. The ID of the action followed by the offset of the string argument.
 */
struct CSM_Action_T {
    uint16_t actionId;
    uint16_t argsOffset;
};

/**
 * Event transitions.
 *  May be: Event ID followed by new state ID.
 *  Or may be: 0xff followed by group ID.
 */
struct CSM_Event_t {
    uint8_t eventId;
    uint8_t newStateId;
};

/**
 * State description.
 *  Offset of name in string pool.
 *  NT, Number of transitions (CSM_Event_t).
 *  NA, Number of actions (CSM_Action_T).
 *  List of NA actions.
 *  List of NT transitions.
 */
struct CSM_State_t {
    uint16_t nameOffset;
    uint8_t  numTransitions;
    uint8_t  numActions;
    CSM_Action_T *action(int ix) const {
        auto *actions = (CSM_Action_T*)((uint8_t*)this + sizeof(CSM_State_t));
        return actions+ix;
    };
    CSM_Event_t *event(int ix) const {
        auto *events = (CSM_Event_t*)((uint8_t*)this + sizeof(CSM_State_t) + numActions*sizeof(CSM_Action_T));
        return events+ix;
    }
};

// forwards
class CState;
class CGroup;

/**
 * The state machine definition.
 *
 * Initialized with the binary data of a pre-compiled state machine.
 *
 * Provides access to the Groups, States, and Strings of the state machine.
 */
class CSM {
public:
    CSM(const void *csmData) : csmData(static_cast<const char *const>(csmData)) {
        stateIx = prevStateIx = -1;
        stringpool = static_cast<const char *>(csmData) + stringpoolOffset();
        stateName = prevStateName = "_";
    };

    static CSM *loadFromFile(const char *fname);
    static void print(CSM *csm);
    static char *formatState(CSM *csm, int stateIx);

    void start(void);
    void enterState(int newStateIx);
    void enterState(const char * newStateName);
    void handleEvent(CSM_EVENT event);

    void saveState(int slot) { savedStateIx[slot] = prevStateIx; }
    void resumeSavedState(int slot) { setCurrentState(savedStateIx[slot]); }
    void goPrevState() { setCurrentState(prevStateIx); }

    int getCurrentState() { return this->stateIx; }
    void setCurrentState(int stateIx);

    const char *getStateName();
    const char *getPrevStateName();

    const char *str(uint16_t offset) const {
        return stringpool + offset;
    }

    static const char *eventName(CSM_EVENT eventIx) { return eventNames[eventIx]; };
    static CSM_EVENT eventFromName(const char * eventName);
    static const char *actionName(int actionIx) { return actionNames[actionIx]; };

public:
    uint16_t const *const uint16() const { return reinterpret_cast<const uint16_t *const>(csmData); }

    uint16_t signature(void) const { return uint16()[0]; }

    int numCStates() const { return uint16()[1] & 0xff; }

    int numCGroups() const { return (uint16()[1] >> 8) & 0xff; }

    const CGroup cGroup(int n) const;
    const CState cState(int n) const;


    int stringpoolOffset() const {
        int offsetIx = 1 + 1 + numCGroups() + numCStates();
        int offset = uint16()[offsetIx];
        return offset;
    }

    friend class CState;

public:
    const char *const csmData;
    const char *stringpool;

    int stateIx;
    int prevStateIx;
    int savedStateIx[5];
    const char *stateName;
    const char *prevStateName;

private:
    static char printBuf[400];
    static int stateNamesLen;
    static int eventNamesLen;
    static const char *eventNames[];
    static const char *actionNames[];
};

class CState {
protected:
    const CSM *pCsm;           // The CSM within which this state is defined.
    const CSM_State_t *pState;  // Pointer to the definition of this CState.
    const char *pName;

public:
    CState(const CSM *pCsm, const char *pStateData) : pCsm(pCsm),
                                                        pState(reinterpret_cast<const CSM_State_t *const>(pStateData)) {
        pName = pCsm->str(pState->nameOffset);
    }

    const char *name() const {
        uint16_t offset = pState->nameOffset;
        return pCsm->str(offset);
    }

    int numActions() const { return pState->numActions; }

    int numTransitions() const { return pState->numTransitions; }

    int actionId(int n) const { return pState->action(n)->actionId; }

    const char *actionArg(int n) const {
        uint16_t offset = pState->action(n)->argsOffset;
        return pCsm->str(offset);
    }

    CSM_EVENT transitionEvent(int n) const {
        return static_cast<CSM_EVENT>(pState->event(n)->eventId);
    }

    int transitionTarget(int n) const {
        return pState->event(n)->newStateId;
    }

    int firstGroupIx() const {
        int i;
        for (i=0; i<numTransitions() && transitionEvent(i)!=static_cast<CSM_EVENT>(0xff); ++i) { }
        return i;
    }

    void performActions(void);

    int nextState(CSM_EVENT eventId) const;

};

/**
 * A CGroup is like a CState with no actions.
 */
class CGroup : public CState {
public:
    CGroup(const CSM *pCsm, const char *pGroup) : CState(pCsm, pGroup) {
        assert(numActions() == 0);
    }
};

CSM *loadControlDef(void);

#endif //TBV2_CSM_H
