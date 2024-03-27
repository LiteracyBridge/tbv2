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
    ControlManager();
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
    void playSubject(const char *arg);
    void playMessage(const char *arg);
    void playSquareTune(const char *notes);

    void startRecAudio(const char *arg);
    void playRecAudio(void);
    void saveRecAudio(const char *arg);

    void writeMsg(const char *txt);
    void writeRecId(const char *arg);
    void svQCresult(const char *arg );

    void showPackage(void);
    void playNextPackage(void);
    void changePackage(void);

    bool callScript(const char *scriptName);
    void exitScript(CSM_EVENT exitEventId);

    void qcFilesTest(const char *arg );

    void beginSurvey(const char *arg);
    void endSurvey(const char *arg);

public:
    const char *getSurveyFilename() { return surveyFilename; }
    bool isSurveyActive() { return surveyFilename[0] != '\0'; }
    uint8_t *getSurveyUUID();

private:
    CSM_EVENT       lastEventId;
    int32_t         lastEventArg;
    bool            audioIdle;
    osTimerId_t     timers[3];

    // A locally unique id of a given track of tone playback. Used to correlate PlaySys, PlaySubj, PlayTune, etc
    // with the eventual AudioDone event.
    uint32_t audioPlayCounter;

    // If not blank, file name for Survey. "writeMsg", "writeRecId", and "saveRecording" write to the survey file.
    char surveyFilename[MAX_PATH];
    uint8_t surveyUUID[16];

    // Uniquified filename of current or recent recording.
    char recordingFilename[MAX_PATH];

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
