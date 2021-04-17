//TBook Control State Machine definition--   tbook_csm.def 
// generated by writeCSM() 
#include "controlmanager.h" 
TBConfig_t  TB_Config = { 
                       5,   // default_volume 
                       5,   // default_speed 
                  300000,   // powerCheckMS 
                    5000,   // shortIdleMS 
                   20000,   // longIdleMS 
     "M0:/system/audio/",   // systemAudio 
                      50,   // minShortPressMS 
                    1000,   // minLongPressMS 
                       0,   // initState 
                  "_49G",   // bgPulse 
                    "G!",   // fgPlaying 
                 "G2_3!",   // fgPlayPaused 
                    "R!",   // fgRecording 
                 "R2_3!",   // fgRecordPaused 
                    "O!",   // fgSavingRec 
                "G3_3G3",   // fgSaveRec 
                "R3_3R3",   // fgCancelRec 
                 "O5o5!",   // fgUSB_MSC 
        "R8_2R8_2R8_20!",   // fgTB_Error 
       "_3R3_3R3_3R3_5!",   // fgNoUSBcable 
                 "G5g5!",   // fgUSBconnect 
    "G_3G_3G_9G_3G_9G_3",   // fgPowerDown 
    "O3_5O3_3O3_2O3_1O3"    // fgEnterDFU 
};  // TB_Config 

int   nPlaySys = 7;  // PlaySys prompts used by CSM 
SysAudio_t	SysAudio[] = { 
  { "welcome",   "M0:/system/audio/welcome.wav" }, 
  { "rh_for_subj",   "M0:/system/audio/rh_for_subj.wav" }, 
  { "cir_record_ideas",   "M0:/system/audio/cir_record_ideas.wav" }, 
  { "louder",   "M0:/system/audio/louder.wav" }, 
  { "softer",   "M0:/system/audio/softer.wav" }, 
  { "faster",   "M0:/system/audio/faster.wav" }, 
  { "slower",   "M0:/system/audio/slower.wav" }  
};  // SysAudio 

int   nCSMstates = 42;  // TBook state machine definition 
csmState stWakeup =  // TBookCMS[0] 
  { 0, "stWakeup", { 
     0,  0,  0,  2,  3,  0,  0,  0,  6,  0,  0,  1,  0,  4,  5,  0,  7,  8,  0,  0,  0,  0, 
     0,  0,  0,  0,  0,  0,  0,  0,  0,  9,  0,  0,  1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  },
    3,
    { 
      {        LED, "_" }, 
      {      bgLED, "G_29" }, 
      {    playSys, "welcome" } 
    }
  }; 
csmState stSleepy =  // TBookCMS[1] 
  { 0, "stSleepy", { 
    27,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  0,  1,  1,  1,  1,  1,  1,  1,  1,  1, 
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1, 28,  1,  1,  1,  1,  },
    3,
    { 
      {      bgLED, "O_9" }, 
      {     saveSt, "1" }, 
      {   setTimer, "10000" } 
    }
  }; 
csmState stPlayLouder =  // TBookCMS[2] 
  { 0, "stPlayLouder", { 
     2,  0,  2,  2,  3,  2,  2,  2,  6,  2,  2,  1,  0,  4,  5,  2,  7,  8,  2,  2,  2,  2, 
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  1,  2,  2,  2,  2,  2,  2,  2,  2,  2,  },
    2,
    { 
      {     volAdj, "1" }, 
      {   goPrevSt, "" } 
    }
  }; 
csmState stPlaySofter =  // TBookCMS[3] 
  { 0, "stPlaySofter", { 
     3,  0,  3,  2,  3,  3,  3,  3,  6,  3,  3,  1,  0,  4,  5,  3,  7,  8,  3,  3,  3,  3, 
     3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  3,  1,  3,  3,  3,  3,  3,  3,  3,  3,  3,  },
    2,
    { 
      {     volAdj, "-1" }, 
      {   goPrevSt, "" } 
    }
  }; 
csmState stPlayFaster =  // TBookCMS[4] 
  { 0, "stPlayFaster", { 
     4,  0,  4,  2,  3,  4,  4,  4,  6,  4,  4,  1,  0,  4,  5,  4,  7,  8,  4,  4,  4,  4, 
     4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  1,  4,  4,  4,  4,  4,  4,  4,  4,  4,  },
    2,
    { 
      {     spdAdj, "1" }, 
      {   goPrevSt, "" } 
    }
  }; 
csmState stPlaySlower =  // TBookCMS[5] 
  { 0, "stPlaySlower", { 
     5,  0,  5,  2,  3,  5,  5,  5,  6,  5,  5,  1,  0,  4,  5,  5,  7,  8,  5,  5,  5,  5, 
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  1,  5,  5,  5,  5,  5,  5,  5,  5,  5,  },
    2,
    { 
      {     spdAdj, "-1" }, 
      {   goPrevSt, "" } 
    }
  }; 
csmState stPlayPause =  // TBookCMS[6] 
  { 0, "stPlayPause", { 
     6,  0,  6,  2,  3, 13, 11, 12, 30,  6, 14,  1,  0,  4,  5,  6,  7,  8,  6,  6,  6,  6, 
    15,  6,  6,  6,  6,  6,  6,  6,  6, 10,  6, 10,  1,  6,  6,  6,  6,  6,  6,  6,  6,  6,  },
    2,
    { 
      {     saveSt, "2" }, 
      {  pausePlay, "" } 
    }
  }; 
csmState stPlayJumpBack =  // TBookCMS[7] 
  { 0, "stPlayJumpBack", { 
     7,  0,  7,  2,  3, 13, 11, 12,  6,  7, 14,  1,  0,  4,  5,  7,  7,  8,  7,  7,  7,  7, 
    15,  7,  7,  7,  7,  7,  7,  7,  7, 10,  7, 10,  1,  7,  7,  7,  7,  7,  7,  7,  7,  7,  },
    1,
    { 
      {     posAdj, "-7" } 
    }
  }; 
csmState stPlayJumpFwd =  // TBookCMS[8] 
  { 0, "stPlayJumpFwd", { 
     8,  0,  8,  2,  3, 13, 11, 12,  6,  8, 14,  1,  0,  4,  5,  8,  7,  8,  8,  8,  8,  8, 
    15,  8,  8,  8,  8,  8,  8,  8,  8, 10,  8, 10,  1,  8,  8,  8,  8,  8,  8,  8,  8,  8,  },
    1,
    { 
      {     posAdj, "60" } 
    }
  }; 
csmState stPromptSubj =  // TBookCMS[9] 
  { 0, "stPromptSubj", { 
     9,  0,  9,  2,  3,  9,  9,  9,  6,  9,  9,  1,  0,  4,  5,  9,  7,  8,  9,  9,  9,  9, 
     9,  9,  9,  9,  9,  9,  9,  9,  9, 10,  9,  9,  1,  9,  9,  9,  9,  9,  9,  9,  9,  9,  },
    1,
    { 
      {    playSys, "rh_for_subj" } 
    }
  }; 
csmState stWait =  // TBookCMS[10] 
  { 0, "stWait", { 
    17,  0, 10, 19, 20, 13, 11, 12, 10, 10, 14,  1,  0, 21, 22, 10, 10, 10, 10, 10, 10, 10, 
    15, 18, 24, 10, 10, 26, 25, 10, 23, 10, 10, 10,  1, 16, 10, 10, 10, 10, 10, 10, 10, 10,  },
    0
  };
csmState stOnPrevSubj =  // TBookCMS[11] 
  { 0, "stOnPrevSubj", { 
    11,  0, 11,  2,  3, 13, 11, 12,  6, 11, 14,  1,  0,  4,  5, 11,  7,  8, 11, 11, 11, 11, 
    15, 11, 11, 11, 11, 11, 11, 11, 11, 10, 11, 10,  1, 11, 11, 11, 11, 11, 11, 11, 11, 11,  },
    2,
    { 
      {    subjAdj, "-1" }, 
      {   playSubj, "nm" } 
    }
  }; 
csmState stOnNextSubj =  // TBookCMS[12] 
  { 0, "stOnNextSubj", { 
    12,  0, 12,  2,  3, 13, 11, 12,  6, 12, 14,  1,  0,  4,  5, 12,  7,  8, 12, 12, 12, 12, 
    15, 12, 12, 12, 12, 12, 12, 12, 12, 10, 12, 10,  1, 12, 12, 12, 12, 12, 12, 12, 12, 12,  },
    2,
    { 
      {    subjAdj, "1" }, 
      {   playSubj, "nm" } 
    }
  }; 
csmState stOnNextMsg =  // TBookCMS[13] 
  { 0, "stOnNextMsg", { 
    13,  0, 13,  2,  3, 13, 11, 12,  6, 13, 14,  1,  0,  4,  5, 13,  7,  8, 13, 13, 13, 13, 
    15, 13, 13, 13, 13, 13, 13, 13, 13, 10, 13, 10,  1, 13, 13, 13, 13, 13, 13, 13, 13, 13,  },
    2,
    { 
      {     msgAdj, "-1" }, 
      {   playSubj, "msg" } 
    }
  }; 
csmState stOnPrevMsg =  // TBookCMS[14] 
  { 0, "stOnPrevMsg", { 
    14,  0, 14,  2,  3, 13, 11, 12,  6, 14, 14,  1,  0,  4,  5, 14,  7,  8, 14, 14, 14, 14, 
    15, 14, 14, 14, 14, 14, 14, 14, 14, 10, 14, 10,  1, 14, 14, 14, 14, 14, 14, 14, 14, 14,  },
    2,
    { 
      {     msgAdj, "1" }, 
      {   playSubj, "msg" } 
    }
  }; 
csmState stOnFeedback =  // TBookCMS[15] 
  { 0, "stOnFeedback", { 
    15,  0, 32, 15, 15, 15, 15, 15, 15, 31, 15,  1,  0, 15, 15, 15, 15, 15, 15, 15, 15, 15, 
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,  1, 15, 15, 15, 15, 15, 15, 15, 15, 15,  },
    1,
    { 
      {    playSys, "cir_record_ideas" } 
    }
  }; 
csmState stLowBatt =  // TBookCMS[16] 
  { 0, "stLowBatt", { 
    17,  0, 16, 19, 20, 16, 16, 16, 16, 16, 16,  1,  0, 21, 22, 16, 16, 16, 16, 16, 16, 16, 
    16, 18, 24, 16, 16, 26, 25, 16, 23, 16, 16, 16,  1, 16, 16, 16, 16, 16, 16, 16, 16, 16,  },
    0
	};
csmState stGoFWU =  // TBookCMS[17] 
  { 0, "stGoFWU", { 
    17,  0, 17, 17, 17, 17, 17, 17, 17, 17, 17,  1,  0, 17, 17, 17, 17, 17, 17, 17, 17, 17, 
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,  1, 17, 17, 17, 17, 17, 17, 17, 17, 17,  },
    0
  };
csmState stUSBmode =  // TBookCMS[18] 
  { 0, "stUSBmode", { 
    18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 
    18, 40, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,  },
    2,
    { 
      {      bgLED, "O2_8" }, 
      {   startUSB, "" } 
    }
  }; 
csmState stIdleLouder =  // TBookCMS[19] 
  { 0, "stIdleLouder", { 
    19,  0, 19,  2,  3, 19, 19, 19,  6, 19, 19,  1,  0,  4,  5, 19,  7,  8, 19, 19, 19, 19, 
    19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19,  1, 19, 19, 19, 19, 19, 19, 19, 19, 19,  },
    3,
    { 
      {     volAdj, "1" }, 
      {    playSys, "louder" }, 
      {   goPrevSt, "" } 
    }
  }; 
csmState stIdleSofter =  // TBookCMS[20] 
  { 0, "stIdleSofter", { 
    20,  0, 20,  2,  3, 20, 20, 20,  6, 20, 20,  1,  0,  4,  5, 20,  7,  8, 20, 20, 20, 20, 
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,  1, 20, 20, 20, 20, 20, 20, 20, 20, 20,  },
    3,
    { 
      {     volAdj, "-1" }, 
      {    playSys, "softer" }, 
      {   goPrevSt, "" } 
    }
  }; 
csmState stIdleFaster =  // TBookCMS[21] 
  { 0, "stIdleFaster", { 
    21,  0, 21,  2,  3, 21, 21, 21,  6, 21, 21,  1,  0,  4,  5, 21,  7,  8, 21, 21, 21, 21, 
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  1, 21, 21, 21, 21, 21, 21, 21, 21, 21,  },
    3,
    { 
      {     spdAdj, "1" }, 
      {    playSys, "faster" }, 
      {   goPrevSt, "" } 
    }
  }; 
csmState stIdleSlower =  // TBookCMS[22] 
  { 0, "stIdleSlower", { 
    22,  0, 22,  2,  3, 22, 22, 22,  6, 22, 22,  1,  0,  4,  5, 22,  7,  8, 22, 22, 22, 22, 
    22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22,  1, 22, 22, 22, 22, 22, 22, 22, 22, 22,  },
    3,
    { 
      {     spdAdj, "-1" }, 
      {    playSys, "slower" }, 
      {   goPrevSt, "" } 
    }
  }; 
csmState stTest =  // TBookCMS[23] 
  { 0, "stTest", { 
    23,  0, 23, 23, 23, 23, 23, 23, 23, 23, 23,  1,  0, 23, 23, 23, 23, 23, 23, 23, 23, 23, 
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23,  1, 23, 23, 23, 23, 23, 23, 23, 23, 23,  },
    1,
    { 
      {    sysTest, "" } 
    }
  }; 
csmState stOnUpdate =  // TBookCMS[24] 
  { 0, "stOnUpdate", { 
    24,  0, 24, 24, 24, 24, 24, 24, 24, 24, 24,  1,  0, 24, 24, 24, 24, 24, 24, 24, 24, 24, 
    24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,  1, 24, 24, 24, 24, 24, 24, 24, 24, 24,  },
    1,
    { 
      {    sysBoot, "" } 
    }
  }; 
csmState stNxtPkg =  // TBookCMS[25] 
  { 0, "stNxtPkg", { 
    17,  0, 25, 19, 20, 25, 25, 25, 25, 25, 25,  1,  0, 21, 22, 25, 25, 25, 25, 25, 25, 25, 
    25, 18, 24, 25, 25, 26, 25, 25, 23, 25, 25, 25,  1, 16, 25, 25, 25, 25, 25, 25, 25, 25,  },
    1,
    { 
      { playNxtPkg, "" } 
    }
  }; 
csmState stChgPkg =  // TBookCMS[26] 
  { 0, "stChgPkg", { 
    17,  0, 26, 19, 20, 13, 11, 12, 26, 26, 14,  1,  0, 21, 22, 26, 26, 26, 26, 26, 26, 26, 
    15, 18, 24, 26, 26, 26, 25, 26, 23, 10, 26, 10,  1, 16, 26, 26, 26, 26, 26, 26, 26, 26,  },
    1,
    { 
      {  changePkg, "" } 
    }
  }; 
csmState stUnSleepy =  // TBookCMS[27] 
  { 0, "stUnSleepy", { 
    27,  0, 27, 27, 27, 27, 27, 27, 27, 27, 27,  1,  0, 27, 27, 27, 27, 27, 27, 27, 27, 27, 
    27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,  1, 27, 27, 27, 27, 27, 27, 27, 27, 27,  },
    2,
    { 
      {      bgLED, "G_29" }, 
      {  goSavedSt, "1" } 
    }
  }; 
csmState stGoSleep =  // TBookCMS[28] 
  { 0, "stGoSleep", { 
    28,  0,  0, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 29, 28, 28, 28, 28,  },
    2,
    { 
      {      bgLED, "R_4" }, 
      {   setTimer, "2000" } 
    }
  }; 
csmState stFallAsleep =  // TBookCMS[29] 
  { 0, "stFallAsleep", { 
    29,  0,  0, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 
    29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,  },
    1,
    { 
      {  powerDown, "" } 
    }
  }; 
csmState stPlayResume =  // TBookCMS[30] 
  { 0, "stPlayResume", { 
    30,  0, 30,  2,  3, 13, 11, 12,  6, 30, 14,  1,  0,  4,  5, 30,  7,  8, 30, 30, 30, 30, 
    15, 30, 30, 30, 30, 30, 30, 30, 30, 10, 30, 10,  1, 30, 30, 30, 30, 30, 30, 30, 30, 30,  },
    2,
    { 
      { resumePlay, "" }, 
      {  goSavedSt, "2" } 
    }
  }; 
csmState stFbOptions =  // TBookCMS[31] 
  { 0, "stFbOptions", { 
    31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 
    31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31,  },
    0
  };
csmState stStartRec =  // TBookCMS[32] 
  { 0, "stStartRec", { 
    32,  0, 35, 32, 32, 36, 32, 32, 34, 38, 37,  1,  0, 32, 32, 32, 32, 32, 32, 32, 32, 32, 
    32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,  1, 32, 32, 32, 32, 32, 32, 32, 32, 32,  },
    1,
    { 
      {   startRec, "" } 
    }
  }; 
csmState stOnOptions =  // TBookCMS[33] 
  { 0, "stOnOptions", { 
    33,  0, 33, 33, 33, 33, 33, 33, 33, 33, 33,  1,  0, 33, 33, 33, 33, 33, 33, 33, 33, 33, 
    33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33,  1, 33, 33, 33, 33, 33, 33, 33, 33, 33,  },
    0
  };
csmState stPauseRec =  // TBookCMS[34] 
  { 0, "stPauseRec", { 
    34,  0, 35, 34, 34, 36, 34, 34, 34, 38, 37,  1,  0, 34, 34, 34, 34, 34, 34, 34, 34, 34, 
    34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34,  1, 34, 34, 34, 34, 34, 34, 34, 34, 34,  },
    2,
    { 
      {     saveSt, "2" }, 
      {   pauseRec, "" } 
    }
  }; 
csmState stFinishRec =  // TBookCMS[35] 
  { 0, "stFinishRec", { 
    35,  0, 35, 35, 35, 36, 35, 35, 34, 38, 37,  1,  0, 35, 35, 35, 35, 35, 35, 35, 35, 35, 
    35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35, 35,  1, 35, 35, 35, 35, 35, 35, 35, 35, 35,  },
    1,
    { 
      {  finishRec, "" } 
    }
  }; 
csmState stPlayRec =  // TBookCMS[36] 
  { 0, "stPlayRec", { 
    36,  0, 35, 36, 36, 36, 36, 36, 34, 38, 37,  1,  0, 36, 36, 36, 36, 36, 36, 36, 36, 36, 
    36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36, 36,  1, 36, 36, 36, 36, 36, 36, 36, 36, 36,  },
    1,
    { 
      {    playRec, "" } 
    }
  }; 
csmState stCancelRec =  // TBookCMS[37] 
  { 0, "stCancelRec", { 
    37,  0, 37, 37, 37, 13, 11, 12, 37, 37, 14,  1,  0, 37, 37, 37, 37, 37, 37, 37, 37, 37, 
    15, 37, 37, 37, 37, 37, 37, 37, 37, 10, 37, 10,  1, 37, 37, 37, 37, 37, 37, 37, 37, 37,  },
    1,
    { 
      {    saveRec, "del" } 
    }
  }; 
csmState stSaveRec =  // TBookCMS[38] 
  { 0, "stSaveRec", { 
    38,  0, 38, 38, 38, 13, 11, 12, 38, 38, 14,  1,  0, 38, 38, 38, 38, 38, 38, 38, 38, 38, 
    15, 38, 38, 38, 38, 38, 38, 38, 38, 10, 38, 10,  1, 38, 38, 38, 38, 38, 38, 38, 38, 38,  },
    1,
    { 
      {    saveRec, "sv" } 
    }
  }; 
csmState stOnLowBatt =  // TBookCMS[39] 
  { 0, "stOnLowBatt", { 
    39,  0, 39, 39, 39, 39, 39, 39, 39, 39, 39,  1,  0, 39, 39, 39, 39, 39, 39, 39, 39, 39, 
    39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39,  1, 39, 39, 39, 39, 39, 39, 39, 39, 39,  },
    1,
    { 
      {      bgLED, "R_9" } 
    }
  }; 
csmState stCloseUSB =  // TBookCMS[40] 
  { 0, "stCloseUSB", { 
    40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 
    40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40,  0, 40, 40, 40, 40,  },
    3,
    { 
      {     endUSB, "" }, 
      {      bgLED, "G_29" }, 
      {   setTimer, "1000" } 
    }
  }; 
csmState stOnCharging =  // TBookCMS[41] 
  { 0, "stOnCharging", { 
    17,  0, 41, 19, 20, 41, 41, 41, 41, 41, 41,  1,  0, 21, 22, 41, 41, 41, 41, 41, 41, 41, 
    41, 18, 24, 41, 41, 26, 25, 41, 23, 41, 41, 41,  1, 16, 41, 41, 41, 41, 41, 41, 41, 41,  },
    2,
    { 
      { showCharge, "" }, 
      {   goPrevSt, "" } 
    }
  }; 

csmState * TBookCSM[] = { 
&stWakeup,  // TBookCSM[0]
&stSleepy,  // TBookCSM[1]
&stPlayLouder,  // TBookCSM[2]
&stPlaySofter,  // TBookCSM[3]
&stPlayFaster,  // TBookCSM[4]
&stPlaySlower,  // TBookCSM[5]
&stPlayPause,  // TBookCSM[6]
&stPlayJumpBack,  // TBookCSM[7]
&stPlayJumpFwd,  // TBookCSM[8]
&stPromptSubj,  // TBookCSM[9]
&stWait,  // TBookCSM[10]
&stOnPrevSubj,  // TBookCSM[11]
&stOnNextSubj,  // TBookCSM[12]
&stOnNextMsg,  // TBookCSM[13]
&stOnPrevMsg,  // TBookCSM[14]
&stOnFeedback,  // TBookCSM[15]
&stLowBatt,  // TBookCSM[16]
&stGoFWU,  // TBookCSM[17]
&stUSBmode,  // TBookCSM[18]
&stIdleLouder,  // TBookCSM[19]
&stIdleSofter,  // TBookCSM[20]
&stIdleFaster,  // TBookCSM[21]
&stIdleSlower,  // TBookCSM[22]
&stTest,  // TBookCSM[23]
&stOnUpdate,  // TBookCSM[24]
&stNxtPkg,  // TBookCSM[25]
&stChgPkg,  // TBookCSM[26]
&stUnSleepy,  // TBookCSM[27]
&stGoSleep,  // TBookCSM[28]
&stFallAsleep,  // TBookCSM[29]
&stPlayResume,  // TBookCSM[30]
&stFbOptions,  // TBookCSM[31]
&stStartRec,  // TBookCSM[32]
&stOnOptions,  // TBookCSM[33]
&stPauseRec,  // TBookCSM[34]
&stFinishRec,  // TBookCSM[35]
&stPlayRec,  // TBookCSM[36]
&stCancelRec,  // TBookCSM[37]
&stSaveRec,  // TBookCSM[38]
&stOnLowBatt,  // TBookCSM[39]
&stCloseUSB,  // TBookCSM[40]
&stOnCharging  // TBookCSM[41]
};  // end of TBookCSM.c 
