//
// Created by bill on 7/26/2023.
//

#include <stddef.h>
#include <stdlib.h>

#include "csm.h"
#include "controlmanager.h"
#include "tbook.h"

#include "control_def_csm.h"
#include "qc_def_csm.h"

#define X(v) #v ,
const char *CSM::eventNames[] = {
        CSM_EVENT_LIST
};
const char *CSM::actionNames[] = {
        CSM_ACTION_LIST
};
#undef X

/**
 * Look up an event by name. Used to allow scripts to refer to events by name in "exitScript(event)"
 * @param eventName to look up.
 * @return the event id.
 */
CSM_EVENT CSM::eventFromName(const char * eventName) {
    for (int ix = 0; ix<static_cast<int>(NUM_CSM_EVENTS); ++ix) {
        if (strcmp(eventName, eventNames[ix]) == 0)
            return static_cast<CSM_EVENT>(ix);
    }
    return eNull;
}

/**
 * Start executing the CSM. Enters the first state, state 0, in the CSM.
 */
void CSM::start(void) {
    dbgLog("C CSM start\n");
    enterState(0);
}

/**
 * Enter the given state, and perform its entry actions.
 * @param newStateIx index of the state to enter.
 */
void CSM::enterState(int newStateIx) {
    if (newStateIx < 0 || newStateIx >= numCStates()) {
        // TODO: Is this the best action? Reset to state 0? tbErr()?
        return;
    }
    CState newState = cState(newStateIx);
    dbgLog("C CSM transition %s => %s\n", newState.name(), formatState(this, newStateIx));
    setCurrentState(newStateIx);
    newState.performActions();
}

/**
 * Enter the named state, if it exists, and perform its entry actions.
 * @param newStateName name of the state to enter.
 */
void CSM::enterState(const char *newStateName) {
    for (int iState=0; iState < numCStates(); ++iState) {
        if (strcmp(newStateName, cState(iState).name()) == 0) {
            enterState(iState);
            return;
        }
    }
}

void CSM::setCurrentState(int stateIx) {
    if (stateIx != this->stateIx) {
        this->prevStateIx = this->stateIx;
        this->stateIx = stateIx;
        this->prevStateName = this->stateName;
        CState state = cState(stateIx);
        this->stateName = state.name();
    }
}

const char *CSM::getStateName() {
    return this->stateName;
}
const char *CSM::getPrevStateName() {
    return this->prevStateName;
}

/**
 * Returns the given CGroup.
 * @param n Index of desired group.
 * @return the CGroup for the group.
 */
const CGroup CSM::cGroup(int n) const {
    // Index, in csmData as array of uint16_t, of offset, within csmData, of the CGroup definition.
    uint16_t offsetIx = 2 + numCStates() + n;
    uint16_t offset = uint16()[offsetIx];
    CGroup group = CGroup(this, csmData + offset);
    return group;
}
const CState CSM::cState(int n) const {
    // Index, in csmData as array of uint16_t, of offset, within csmData, of the CState definition.
    uint16_t offsetIx = 2 + n;
    uint16_t offset = uint16()[offsetIx];
    CState state = CState(this, csmData + offset);
    return state;
}

void CSM::handleEvent(CSM_EVENT event) {
    CState state = cState(stateIx); // cState(this, this->cState(stateIx));
    int nextStateIx = state.nextState(event);
    if (nextStateIx >= 0) {
        CState nextState = cState(nextStateIx);
        dbgLog("C CSM event %s (%d): %s -> %s\n", CSM::eventName(event), event, state.name(), nextState.name());
        logEvtNSNS("csmEvt", "evt", CSM::eventName(event), "nSt", nextState.name());
        enterState(nextStateIx);
    } else {
        dbgLog("C CSM event %s (%d) ignored in state %s\n", CSM::eventName(event), event, state.name());
    }
}

/**
 * Perform the actions defined for the current state. It only makes sense to do this when
 * entering the state, but note that transitioning to the same state based on a TB event
 * is considered to be entering the state.
 */
void CState::performActions(void) {
    for (uint8_t ix=0; ix<numActions(); ++ix) {
        CSM_ACTION actionId = static_cast<CSM_ACTION>(this->actionId(ix));
        const char * actionArg = this->actionArg(ix);
        if (actionArg == NULL) actionArg = "";
        int intArg = atoi(actionArg); // ignores non-numbers
        ControlManager::ACTION_DISPOSITION disposition = controlManager.doAction(actionId, actionArg, intArg);
        if (disposition == ControlManager::ACTION_DISPOSITION::BREAK_ACTIONS) break;
    }
}

/**
 * Given an event ID, find the next state, if any, to which to transition.
 * @param eventId The event for which to search.
 * @return The next state (index), or -1 if no transition for this->event.
 */
int CState::nextState(CSM_EVENT eventId) const {
//    dbgLog("C Searching %s for handler for %s\n", name(), CSM::eventName(eventId));
//    if (strcmp(name(), "s1q1") == 0) {
//        printf("s1q1\n");
//    }
    // Look for a transition matching eventId.
    for (uint8_t ix=0; ix<numTransitions(); ++ix) {
        CSM_EVENT transitionEventId = transitionEvent(ix);
        if (transitionEventId == eventId) {
            // found it
            int transitionTargetId = transitionTarget(ix);
            return transitionTargetId;
        } else if (transitionEventId == static_cast<CSM_EVENT>(0xff)) {
            // found CGroup; look for transition
            int transitionTargetId = transitionTarget(ix);
            CGroup cGroup = pCsm->cGroup(transitionTargetId);
//            dbgLog("C Looking in group %s\n", cGroup.name());
            int groupState = cGroup.nextState(eventId);
            if (groupState >= 0) {
                // Found it in CGroup
                return groupState;
            }
        }
    }
    // Found nothing.
    return -1;
}

/**
 * Attempts to load a CSM from a file.
 * @param fname The path to the file to be loaded.
 * @return A pointer to the newly loaded CSM, or NULL if the file can't be found or read.
 */
CSM *CSM::loadFromFile(const char *fname) {
    FILE *f = tbOpenReadBinary(fname);
    if (!f) { return NULL; }
    dbgLog("C %s opened as %0x\n", fname, reinterpret_cast<int>(f));
    fseek(f, 0, SEEK_END);
    long l = ftell(f);
    fseek(f, 0, SEEK_SET);
    const void *fbuf = malloc(l);
    if (!fbuf) {
        tbFclose(f);
        return NULL;
    }
    fread(const_cast<void *>(fbuf), 1, l, f);
    tbFclose(f);
    dbgLog("C %s is %ld bytes.\n", fname, l);

    return new("CSM") CSM(fbuf);
}

char CSM::printBuf[400];
int CSM::stateNamesLen = -1;
int CSM::eventNamesLen = -1;
/**
 * Debug formatting of a given CSM state.
 * @param csm The CSM containing the state.
 * @param stateIx The index (ID) of the state.
 * @return a pointer to a global buffer containing the formatted state.
 */
char *CSM::formatState(CSM *csm, int stateIx) {
    char *pOut = printBuf;
    if (stateIx < 0 || stateIx > 255) {
        printf("formatState: Unexpected stateIx: %d\n", stateIx);
    }
    CState state = csm->cState(stateIx);
    pOut += sprintf(pOut, "     %-*s:  { ", stateNamesLen, state.name());
    if (state.numActions() > 0) {
        pOut += sprintf(pOut, " Actions: [ ");
        for (int iAction = 0; iAction < state.numActions(); ++iAction) {
            if (iAction > 0) {
                pOut += sprintf(pOut, " , ");
            }
            pOut += sprintf(pOut, " %s(%s)", CSM::actionName(state.actionId(iAction)), state.actionArg(iAction));
        }
        pOut += sprintf(pOut, " ], ");
    }
    int firstGroupIx = state.firstGroupIx();
    if (firstGroupIx < state.numTransitions()) {
        pOut += sprintf(pOut, "  CGroups: [");
        for (int groupIx=state.numTransitions()-1; groupIx>=firstGroupIx; --groupIx) {
            if (groupIx != state.numTransitions() - 1) {
                pOut += sprintf(pOut, " , ");
            }
            int target = state.transitionTarget(groupIx);
            CGroup group = csm->cGroup(target);
            pOut += sprintf(pOut, " %s", group.name());
        }
        pOut += sprintf(pOut, " ], ");
    }
    for (int eventIx=0; eventIx<firstGroupIx; ++eventIx) {
        CSM_EVENT event = state.transitionEvent(eventIx);
        int target = state.transitionTarget(eventIx);
        CState nextState = csm->cState(target);
        pOut += sprintf(pOut, " %s: %s, ", CSM::eventName(event), nextState.name());
    }
    pOut += sprintf(pOut, " },\n");
    return printBuf;
}

/**
 * Print a CSM definition. For debugging.
 * @param csm The CSM to be printed.
 */
void CSM::print(CSM *csm) {
    if (!dbgEnabled('C')) { return; }
    printf("%d groups, %d states.\n", csm->numCGroups(), csm->numCStates());
    printf("{ CGroups: {\n");
    size_t groupNamesLen = 0;
    for (int iGroup=0; iGroup < csm->numCGroups(); ++iGroup) {
        CGroup group = csm->cGroup(iGroup);
        groupNamesLen = max(groupNamesLen, strlen(group.name()));
    }
    if (stateNamesLen < 0) {
        for (int iState = 0; iState < csm->numCStates(); ++iState) {
            CState state  = csm->cState(iState);
            stateNamesLen = max(stateNamesLen, strlen(state.name()));
        }
    }
    if (eventNamesLen < 0) {
        for (int iEvent = 0; iEvent < NUM_CSM_EVENTS; ++iEvent) {
            eventNamesLen = max(eventNamesLen, strlen(CSM::eventName(static_cast<CSM_EVENT>(iEvent))));
        }
    }
    tbDelay_ms(10);
    for (int iGroup=0; iGroup < csm->numCGroups(); ++iGroup) {
        CGroup group = csm->cGroup(iGroup);
        printf("%-*s: { ", groupNamesLen, group.name());
        for (int eventIx=0; eventIx < group.numTransitions(); ++eventIx) {
            if (eventIx > 0) printf(",\n%*s", groupNamesLen + 4, "");
            CSM_EVENT event = group.transitionEvent(eventIx);
            int target = group.transitionTarget(eventIx);
            CState nextState = csm->cState(target);
            printf("%-*s: %s", eventNamesLen, CSM::eventName(event), nextState.name());
        }
        printf(" },\n");
        tbDelay_ms(1);
    }
    tbDelay_ms(10);
    printf("},\n  CStates: {\n");
    for (int i = 0; i < csm->numCStates(); ++i) {
        printf("%s", formatState(csm, i));
        tbDelay_ms(2);
    }
    printf("},\n");
}

/**
 * Loads the control_def.csm to be used in this run. If the "QC Test" option is set, or if there is no
 * "QC_PASS.txt" file in the /system directory, load the built-in QC definition. Otherwise, if there is
 * a "control_def.csm" file in the /system directory, load it. Otherwise use the built-in control_def.csm.
 * @return a pointer to the CSM definition.
 */
CSM *loadControlDef(void) {
    CSM *csm;
    if (runQcTest) {
        csm = new("CSMQC") CSM(qc_def_csm);
    } else if (!(csm=CSM::loadFromFile(TBP[pCONTROL_DEF]))) {
        csm = new("CSMDEF") CSM(control_def_csm);
    }
    logEvtFmt( "TB_CSM", "Ver: %04x", csm->signature() );        // log CSM version comment
    return csm;
}
