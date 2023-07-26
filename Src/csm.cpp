//
// Created by bill on 3/16/2023.
//

#include "linereader.h"
#include "csm.h"

CSM      *csm = NULL;                 // extern ptr to definition of CSM
//TBConfig_t *TB_Config;                  // TBook configuration variables

#define X(v) #v ,
const char *CSM::eventNames[] = {
        CSM_EVENT_LIST
};
const char *CSM::actionNames[] = {
        CSM_ACTION_LIST
};
#undef X

//
// private ControlStateMachine loading routines
class CSMReader : LineReader {
public:
    CSMReader(const char *filename) : LineReader(filename, "CSM") {}

    void loadTbConfig(void);
    AudioList *loadSysAudio();
    csmAction *loadAction();
    CState *loadCState(int idx);

    bool loadControlDef();
};


void CSMReader::loadTbConfig() {                      // load & return CSM configuration variables
    if (errCount > 0) return;
    //(TBConfig_t *) tbAlloc(sizeof(TBConfig_t), "TBConfig");

    readInt("def_vol");
    readInt("powerChk");
    readInt("shortIdle");
    readInt("longIdle");
    readInt("shortPr");
    readInt("longPr");
    readInt("qcState");
    readInt("initState");

    readString("sysAud");        // obsolete config item.
    readString("bgPulse");
    readString("fgPlaying");
    readString("fgPlayPaused");
    readString("fgRecording");
    readString("fgRecordPaused");
    readString("fgSavingRec");
    readString("fgSaveRec");
    readString("fgCancelRec");
    readString("fgUSB_MSC");
    readString("fgTB_Error");
    readString("fgNoUSBcable");
    readString("fgUSBconnect");
    readString("fgPowerDown");
}

AudioList * CSMReader::loadSysAudio() {                      // load & return list of all PlaySys names used in CSM
    if (errCount > 0) return NULL;
    int         cnt  = readInt("nSysAudio");
    AudioList *lst = (AudioList *) tbAlloc(sizeof(int) + cnt * sizeof(char *), "SysAudio");

    lst->nSysA = cnt;
    for (int i = 0; i < cnt; i++) {
        lst->sysA[i] = readString("sysAud");
    }
    return lst;
}

csmAction * CSMReader::loadAction() {                        // load & return enum Action & string arg
    if (errCount > 0) return NULL;

    readLine("action");
    // Look up the enum value for this action's name.
    for (int    i  = 0; i<CSM_EVENT::NUM_CSM_EVENTS; i++) {
        if (strcasecmp(line, CSM::actionName(i)) == 0) {
            csmAction *a = new("csmAct") csmAction;
            a->act = (CSM_ACTION) i;
            a->arg = readString("act_arg");
            return a;
        }
    }
    strcat(line, " unrecognized Action");
    error(line);
    return NULL;
}

CState *CSMReader::loadCState(int idx) {               // load & return definition of one CSM state
    if (errCount > 0) return NULL;
    CState *cState = new("CState") CState;

    cState->idx = static_cast<short>( readInt("st.Idx"));
    if (cState->idx != idx)
        errLog("CSM sync st %d!=%d", idx, cState->idx);
    cState->nm          = readString("st.nm");
    cState->nNxtStates  = static_cast<short>( readInt("st.nNxt"));
    cState->evtNxtState = new("evtNxtSt") short[cState->nNxtStates];
    for (int i = 0; i < cState->nNxtStates; i++) {
        if (fscanf(inFile, "%hd,", &cState->evtNxtState[i]) != 1) {
            error("load evtNxtSt");
            return NULL;
        }
    }
    cState->nActions = static_cast<short>(readInt("st.nActs"));
    cState->actions  = new("st.Actions") csmAction *[cState->nActions];
    for (int i = 0; i < cState->nActions; i++)
        cState->actions[i] = loadAction();
    return cState;
}


//
// public routines to load & access the CSM from /system/control_def.txt
// TODO: Document the format here.
bool CSMReader::loadControlDef(void) {
    csm      = new("CSM") CSM;

    if (inFile == NULL) {
        errLog("csm_data.txt not found");
        return false;
    }
    csm->Version = readString("CSMver");
    logEvtNS("TB_CSM", "ver", csm->Version);

    readLine("csmCompVer"); // discard CsmCompileVersion

    loadTbConfig();

    loadSysAudio(); // TODO: verify that all of the system audio files can be found.

    csm->nCStates = readInt("nStates");
    csm->cStates = new("CsmDef") CState*[csm->nCStates];
    for (int i = 0; i < csm->nCStates; i++) {
        csm->cStates[i] = loadCState(i);
    }

    if (errCount > 0) {
        errLog("csm_data: %d parse errors", errCount);
        return false;
    }
    return true;
}

bool loadControlDef(void) {
    CSMReader csmr = CSMReader(TBP[pCSM_DATA_TXT]);
    return csmr.loadControlDef();
}

const char * CSM::eventName(int eventIx) {
    return eventNames[eventIx];
}
const char * CSM::actionName(int actionIx) {
    return actionNames[actionIx];
}

