// TBookV2    controlmanager.h

#ifndef CONTROL_MGR_H
#define CONTROL_MGR_H

#include "csm.h"
#include "tbook.h"
#include "packageData.h"


extern int iPkg;



typedef struct {            // CSM state variables

    short iSubj;                  // index of current Subj
    short iMsg;                   // index of current Msg within Subj

}              TBook_t;
extern TBook_t TBook;


class ControlManager {
public:
    ControlManager() : csmStack(2), csms(2) {}
    void init(void);
    void USBmode(bool start);
    enum ACTION_DISPOSITION {CONTINUE_ACTIONS, BREAK_ACTIONS};
    ACTION_DISPOSITION doAction(CSM_ACTION action, const char *arg, int iArg);
    void executeCSM(void);

private:

    void assertValidState(int iState);

    void changeSubject(const char *change);
    void changeMessage(const char *change);

    void playSysAudio(const char *arg);
    void playSubjectAudio(const char *arg);
    void playSquareTune(const char *notes);

    void startRecAudio(const char *arg);
    void playRecAudio(void);
    void saveRecAudio(const char *arg);

    void saveWriteMsg(const char *txt);
    void svQCresult(const char *arg );

    void showPackage(void);
    void playNextPackage(void);
    void changePackage(void);

    bool callScript(const char *scriptName);
    void exitScript(CSM_EVENT exitEventId);

    void QCfilesTest(const char *arg );

private:
    CSM_EVENT lastEventId;
    bool audioIdle;
    osTimerId_t timers[3];

    // CSM Script management
    CSM *csm;
    std::vector<CSM*> csmStack;
    std::vector<std::pair<const char *, CSM *>> csms;

    void updateTimersForEvent(CSM_EVENT eventId);
    static void tbTimerFunc(void *eventIx);

    void clearIdle(void);
};

extern void initControlManager(void);           // initialize & run TBook Control State Machine

extern ControlManager controlManager;

#endif
