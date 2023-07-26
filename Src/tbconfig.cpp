//
// Created by bill on 7/20/2023.
//
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "tbook.h"

#include "linereader.h"
#include "tbconfig.h"

TBConfig tbConfig = TBConfig();
TBConfig *TB_Config = &tbConfig;

/**
 * TBConfig with default values.
 * @return a default config
 */
TBConfig::TBConfig() {
    default_volume = 4;
    powerCheckMS    = 10000;
    shortIdleMS     = 5000;
    longIdleMS      = 180000;
    minShortPressMS = 50;
    minLongPressMS  = 1000;
    qcTestState     = 0;
    initState       = 1;
    bgPulse         = "_49G";
    fgPlaying       = "G!";
    fgPlayPaused    = "G2_3!";
    fgRecording     = "R!";
    fgRecordPaused  = "R2_3!";
    fgSavingRec     = "O!";
    fgSaveRec       = "G3_3G3";
    fgCancelRec     = "R3_3R3";
    fgUSB_MSC       = "O5o5!";
    fgTB_Error      = "R8_2R8_2R8_20!";
    fgNoUSBcable    = "_3R3_3R3_3R3_5!";
    fgUSBconnect    = "G5g5!";
    fgPowerDown     = "G_3G_3G_9G_3G_9G_3";
}

/**
 * Sets key from value.
 * @param key - the config item to set.
 * @param value - the value to set it to.
 */
void TBConfig::setValue(const char *key, const char *value) {
    if (strcmp(key, "default_volume") == 0) default_volume = atoi(value);
    else if (strcmp(key, "powerCheckMS") == 0) powerCheckMS = atoi(value);
    else if (strcmp(key, "shortIdleMS") == 0) shortIdleMS = atoi(value);
    else if (strcmp(key, "longIdleMS") == 0) longIdleMS = atoi(value);
    else if (strcmp(key, "minShortPressMS") == 0) minShortPressMS = atoi(value);
    else if (strcmp(key, "minLongPressMS") == 0) minLongPressMS = atoi(value);
    else if (strcmp(key, "qcTestState") == 0) qcTestState = atoi(value);
    else if (strcmp(key, "initState") == 0) initState = atoi(value);
    else if (strcmp(key, "bgPulse") == 0) bgPulse = allocStr(value, "config");
    else if (strcmp(key, "fgPlaying") == 0) fgPlaying = allocStr(value, "config");
    else if (strcmp(key, "fgPlayPaused") == 0) fgPlayPaused = allocStr(value, "config");
    else if (strcmp(key, "fgRecording") == 0) fgRecording = allocStr(value, "config");
    else if (strcmp(key, "fgRecordPaused") == 0) fgRecordPaused = allocStr(value, "config");
    else if (strcmp(key, "fgSavingRec") == 0) fgSavingRec = allocStr(value, "config");
    else if (strcmp(key, "fgSaveRec") == 0) fgSaveRec = allocStr(value, "config");
    else if (strcmp(key, "fgCancelRec") == 0) fgCancelRec = allocStr(value, "config");
    else if (strcmp(key, "fgUSB_MSC") == 0) fgUSB_MSC = allocStr(value, "config");
    else if (strcmp(key, "fgTB_Error") == 0) fgTB_Error = allocStr(value, "config");
    else if (strcmp(key, "fgNoUSBcable") == 0) fgNoUSBcable = allocStr(value, "config");
    else if (strcmp(key, "fgUSBconnect") == 0) fgUSBconnect = allocStr(value, "config");
    else if (strcmp(key, "fgPowerDown") == 0) fgPowerDown = allocStr(value, "config");
}

void TBConfig::setValue(const char *line) {
    char key[25];
    char value[175];
    char *pCh = key;
    // key is alphanum, -, _
    while (isalnum(*line) || *line=='_' || *line=='-')
        *pCh++ = *line++;
    *pCh = '\0';
    // skip white space, = or :
    while (isspace(*line) || *line=='=' || *line==':')
        line++;
    // value
    strncpy(value, line, sizeof(value)-1);
    value[sizeof(value)-1] = '\0';
    setValue(key, value);
}

void TBConfig::initConfig(void) {
    LineReader lr = LineReader(TBP[pCONFIG_TXT], "config");
    if (lr.isOpen() ) {
        while (lr.readLine("config")) {
            tbConfig.setValue(lr.getLine());
        }
    }
}