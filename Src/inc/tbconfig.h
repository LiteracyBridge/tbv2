//
// Created by bill on 7/20/2023.
//

#ifndef TBV2_TBCONFIG_H
#define TBV2_TBCONFIG_H

class TBConfig {
public:
    static void initConfig(void);

    short default_volume;
    int powerCheckMS;         // used by powermanager.c
    int shortIdleMS;
    int longIdleMS;
    int minShortPressMS;      // used by inputmanager.c
    int minLongPressMS;       // used by inputmanager.c
    int qcTestState;          // first state if running QC acceptance test
    int initState;

    const char *bgPulse;              // LED sequences used in Firmware
    const char *fgPlaying;
    const char *fgPlayPaused;
    const char *fgRecording;
    const char *fgRecordPaused;
    const char *fgSavingRec;
    const char *fgSaveRec;
    const char *fgCancelRec;
    const char *fgUSB_MSC;
    const char *fgTB_Error;
    const char *fgNoUSBcable;
    const char *fgUSBconnect;
    const char *fgPowerDown;

    TBConfig(void);
    void setValue(const char *line);
    void setValue(const char *key, const char *value);

    const char *ledStr(const char *name);
};

extern TBConfig tbConfig;

#endif //TBV2_TBCONFIG_H
