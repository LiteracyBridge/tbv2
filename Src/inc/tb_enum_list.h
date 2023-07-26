//
// Created by bill on 3/17/2023.
//

#ifndef TBV2_TB_ENUM_LIST_H
#define TBV2_TB_ENUM_LIST_H
// @formatter:off

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// IMPORTANT!!
// This file is also pre-processed in a Java source file.
// Everything in this file, when pre-processed, must be valid Java source.
// All #defines are OK, because they are not in the preprocessor output.
// Similarly, C comments are also Java comments, so they're fine.
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////


// This is also expanded for the Java TknEvent
//region CSM_EVENT_LIST
#define CSM_EVENT_LIST \
    X(eNull)          \
    X(Home)           \
    X(Circle)         \
    X(Plus)           \
    X(Minus)          \
    X(Tree)           \
    X(Lhand)          \
    X(Rhand)          \
    X(Pot)            \
    X(Star)           \
    X(Table)          \
    X(Home__)         \
    X(Circle__)       \
    X(Plus__)         \
    X(Minus__)        \
    X(Tree__)         \
    X(Lhand__)        \
    X(Rhand__)        \
    X(Pot__)          \
    X(Star__)         \
    X(Table__)        \
    X(starHome)       \
    X(starCircle)     \
    X(starPlus)       \
    X(starMinus)      \
    X(starTree)       \
    X(starLhand)      \
    X(starRhand)      \
    X(starPot)        \
    X(starStar)       \
    X(starTable)      \
    X(AudioStart)     \
    X(AudioDone)      \
    X(ShortIdle)      \
    X(LongIdle)       \
    X(LowBattery)     \
    X(BattCharging)   \
    X(BattCharged)    \
    X(BattMin)        \
    X(BattMax)        \
    X(FirmwareUpdate) \
    X(Timer)          \
    X(ChargeFault)    \
    X(LithiumHot)     \
    X(MpuHot)         \
    X(FilesSuccess)   \
    X(FilesFail)      \
    X(BattCharging75) \
    X(BattNotCharging)\
    X(anyKey)         \
    X(eUNDEF)         \
//endregion

// This is also expanded for the Java TknAction
//region CSM_ACTION_LIST
#define CSM_ACTION_LIST \
    X(aNull)          \
    X(LED)            \
    X(bgLED)          \
    X(playSys)        \
    X(playSubj)       \
    X(pausePlay)      \
    X(resumePlay)     \
    X(stopPlay)       \
    X(volAdj)         \
    X(spdAdj)         \
    X(posAdj)         \
    X(startRec)       \
    X(pauseRec)       \
    X(resumeRec)      \
    X(finishRec)      \
    X(playRec)        \
    X(saveRec)        \
    X(writeMsg)       \
    X(goPrevSt)       \
    X(saveSt)         \
    X(goSavedSt)      \
    X(subjAdj)        \
    X(msgAdj)         \
    X(setTimer)       \
    X(resetTimer)     \
    X(showCharge)     \
    X(startUSB)       \
    X(endUSB)         \
    X(powerDown)      \
    X(sysBoot)        \
    X(sysTest)        \
    X(playNxtPkg)     \
    X(changePkg)      \
    X(playTune)       \
    X(filesTest)      \
//endregion

#define GDEF_AF_0 0
#define GDEF_AF_4 4
#define GDEF_AF_5 5
#define GDEF_AF_6 6
#define GDEF_AF_9 9
#define GDEF_AF_10 10
#define GDEF_AF_12 12

//region GPIO_DEFINITION_LIST
#define GPIO_DEFINITION_LIST \
    X( HOME,              GPIOA,  0, GDEF_NONE ) \
    X( CIRCLE,            GPIOE,  9, GDEF_NONE ) \
    X( PLUS,              GPIOA,  4, GDEF_NONE ) \
    X( MINUS,             GPIOA,  5, GDEF_NONE ) \
    X( TREE,              GPIOA, 15, GDEF_NONE ) \
    X( LHAND,             GPIOB,  7, GDEF_NONE ) \
    X( RHAND,             GPIOB, 10, GDEF_NONE ) \
    X( POT,               GPIOC,  1, GDEF_NONE ) \
    X( STAR,              GPIOE,  8, GDEF_NONE ) \
    X( TABLE,             GPIOE,  3, GDEF_NONE ) \
    X( RED,               GPIOE,  1, GDEF_NONE ) \
    X( GREEN,             GPIOE,  0, GDEF_NONE ) \
    X( BOARD_REV,         GPIOB,  0, GDEF_NONE ) \
    X( ADC_LI_ION,        GPIOA,  2, GDEF_NONE ) \
    X( ADC_PRIMARY,       GPIOA,  3, GDEF_NONE ) \
    X( ADC_THERM,         GPIOC,  2, GDEF_NONE ) \
    X( PWR_FAIL_N,        GPIOE,  2, GDEF_NONE ) \
    X( ADC_ENABLE,        GPIOE, 15, GDEF_NONE ) \
    X( SC_ENABLE,         GPIOD,  1, GDEF_NONE ) \
    X( EN_IOVDD_N,        GPIOE,  4, GDEF_NONE ) \
    X( EN_AVDD_N,         GPIOE,  5, GDEF_NONE ) \
    X( EN_5V,             GPIOD,  4, GDEF_NONE ) \
    X( EN1V8,             GPIOD,  5, GDEF_NONE ) \
    X( 3V3_SW_EN,         GPIOD,  6, GDEF_NONE ) \
    X( EMMC_RSTN,         GPIOE, 10, GDEF_NONE ) \
    X( BAT_STAT1,         GPIOD,  8, GDEF_NONE ) \
    X( BAT_STAT2,         GPIOD,  9, GDEF_NONE ) \
    X( BAT_PG_N,          GPIOD, 10, GDEF_NONE ) \
    X( BAT_CE,            GPIOD,  0, GDEF_NONE ) \
    X( BAT_TE_N,          GPIOD, 13, GDEF_NONE ) \
    X( BOOT1_PDN,         GPIOB,  2, GDEF_NONE ) \
    X( I2C1_SCL,          GPIOB,  8, GDEF_AF_4 ) \
    X( I2C1_SDA,          GPIOB,  9, GDEF_AF_4 ) \
    X( I2S2_SD,           GPIOB, 15, GDEF_AF_5 ) \
    X( I2S2ext_SD,        GPIOB, 14, GDEF_AF_6 ) \
    X( I2S2_WS,           GPIOB, 12, GDEF_AF_5 ) \
    X( I2S2_CK,           GPIOB, 13, GDEF_AF_5 ) \
    X( I2S2_MCK,          GPIOC,  6, GDEF_AF_5 ) \
    X( I2S3_MCK,          GPIOC,  7, GDEF_AF_6 ) \
    X( SPI4_NSS,          GPIOE, 11, GDEF_AF_5 ) \
    X( SPI4_SCK,          GPIOE, 12, GDEF_AF_5 ) \
    X( SPI4_MISO,         GPIOE, 13, GDEF_AF_5 ) \
    X( SPI4_MOSI,         GPIOE, 14, GDEF_AF_5 ) \
    X( USB_DM,            GPIOA, 11, GDEF_AF_10 ) \
    X( USB_DP,            GPIOA, 12, GDEF_AF_10 ) \
    X( USB_ID,            GPIOA, 10, GDEF_AF_10 ) \
    X( USB_VBUS,          GPIOA,  9, GDEF_AF_10 ) \
    X( QSPI_CLK_A,        GPIOD,  3, GDEF_AF_9 ) \
    X( QSPI_CLK_B,        GPIOB,  1, GDEF_AF_9 ) \
    X( MCO2,              GPIOC,  9, GDEF_AF_0 ) \
    X( QSPI_BK1_IO0,      GPIOD, 11, GDEF_AF_9 ) \
    X( QSPI_BK1_IO1,      GPIOD, 12, GDEF_AF_9 ) \
    X( QSPI_BK1_IO2,      GPIOC,  8, GDEF_AF_9 ) \
    X( QSPI_BK1_IO3,      GPIOA,  1, GDEF_AF_9 ) \
    X( QSPI_BK1_NCS,      GPIOB,  6, GDEF_AF_10 ) \
    X( QSPI_BK2_IO0,      GPIOA,  6, GDEF_AF_10 ) \
    X( QSPI_BK2_IO1,      GPIOA,  7, GDEF_AF_10 ) \
    X( QSPI_BK2_IO2,      GPIOC,  4, GDEF_AF_10 ) \
    X( QSPI_BK2_IO3,      GPIOC,  5, GDEF_AF_10 ) \
    X( QSPI_BK2_NCS,      GPIOC, 11, GDEF_AF_9 ) \
    X( SDIO_DAT0,         GPIOB,  4, GDEF_AF_12 ) \
    X( SDIO_DAT1,         GPIOA,  8, GDEF_AF_12 ) \
    X( SDIO_DAT2,         GPIOC, 10, GDEF_AF_12 ) \
    X( SDIO_DAT3,         GPIOB,  5, GDEF_AF_12 ) \
    X( SDIO_CLK,          GPIOC, 12, GDEF_AF_12 ) \
    X( SDIO_CMD,          GPIOD,  2, GDEF_AF_12 ) \
    X( SWDIO,             GPIOA, 13, GDEF_NONE ) \
    X( SWCLK,             GPIOA, 14, GDEF_NONE ) \
    X( SWO,               GPIOB,  3, GDEF_NONE ) \
    X( OSC32_OUT,         GPIOC, 15, GDEF_NONE ) \
    X( OSC32_IN,          GPIOC, 14, GDEF_NONE ) \
    X( OSC_OUT,           GPIOH,  1, GDEF_NONE ) \
    X( OSC_IN,            GPIOH,  0, GDEF_NONE ) \
//endregion

//region FILENAMES
#define FILENAME_LIST \
        X(BOOTCOUNT_TXT,        "M0:/system/bootcount.txt")             \
        X(CSM_DATA_TXT,         "M0:/system/csm_data.txt")              \
        X(LOG_TXT,              "M0:/log/tbLog.txt")                    \
        X(TB_LOG_NN_TXT,        "M0:/log/tbLog_%d.txt")                 \
        X(BAD_LOG_NN_TXT,       "M0:/log/badLog_%d.txt")                \
        X(TB_TMP_LOG_TXT,       "M0:/log/tbTmpLog.txt")                 \
        X(STATS_PATH,           "M0:/stats/")                           \
        X(RECORDINGS_PATH,      "M0:/recordings/")                      \
        X(QC_PASS_TXT,          "M0:/system/QC_PASS.txt")               \
        X(ERASE_NOR_LOG_TXT,    "M0:/system/EraseNorLog.txt")           \
        X(SET_RTC_TXT,          "M0:/system/SetRTC.txt")                \
        X(DONT_SET_RTC_TXT,     "M0:/system/dontSetRTC.txt")            \
        X(DONT_SET_RTC_FNAME,   "dontSetRTC.txt")                       \
        X(LAST_RTC_TXT,         "M0:/system/lastRTC.txt")               \
        X(PACKAGES_DATA_TXT,    "M0:/content/packages_data.txt")        \
        X(UF_LIST_TXT,          "M0:/content/uf_list.txt")              \
        X(DEVICE_ID_TXT,        "M0:/system/device_ID.txt")             \
        X(FIRMWARE_ID_TXT,      "M0:/system/firmware_ID.txt")           \
        X(AUDIO_WAV,            "M0:/audio.wav")                        \
        X(CONFIG_TXT,           "M0:/system/config.txt")                \
//endregion


// @formatter:on
#endif //TBV2_TB_ENUM_LIST_H
