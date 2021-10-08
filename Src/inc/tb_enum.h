// TBookV2  tb_enum.h
//  Oct2021
#ifndef TB_ENUM_H
#define TB_ENUM_H

#include "stdbool.h"

// MUST MATCH:  char * ENms[] & shENms[] defined in packageData.c
// MUST MATCH:  CsmToken.java -- enum TknEvent 
typedef enum { 	// Event -- TBook event types
			eNull=0, 
			Home, 	  Circle,	  Plus, 	Minus, 	   Tree, 	 Lhand, 	Rhand, 	   Pot,	    Star,	  Table,     //=10
			Home__,   Circle__,   Plus__, 	Minus__,   Tree__, 	 Lhand__, 	Rhand__,   Pot__,   Star__,	  Table__,   //=20
			starHome, starCircle, starPlus, starMinus, starTree, starLhand, starRhand, starPot, starStar, starTable, //=30
			AudioStart,           AudioDone,	       ShortIdle,	        LongIdle,	        LowBattery,          //=35
  	        BattCharging,         BattCharged,	       FirmwareUpdate,      Timer,              ChargeFault,         //=40
            LithiumHot,           MpuHot,              FilesSuccess,        FilesFail,          anyKey,              //=45
            eUNDEF //=46
}	Event;

// MUST MATCH:  char * ANms[] names defined in packageData.c
// MUST MATCH:  CsmToken.java -- enum TknAction 
typedef enum { 	// Action -- TBook actions
				aNull=0, 	
                LED,		bgLED,		playSys, 	
                playSubj,	pausePlay,	resumePlay,		 
                stopPlay,	volAdj,		spdAdj,		
                posAdj,     startRec,	pauseRec,
                resumeRec,	finishRec,  playRec,	
                saveRec,    writeMsg, 	goPrevSt,	
                saveSt,		goSavedSt,  subjAdj, 	
                msgAdj,		setTimer,	resetTimer, 
                showCharge, startUSB,	endUSB,		
                powerDown,	sysBoot,    sysTest, 
                playNxtPkg, changePkg,  playTune, 
                filesTest
}	Action;
extern char *   ANms[];                                             // to translate enum Action values to strings
extern char *   ENms[];                                             // to translate Event values to strings
extern char *   shENms[];                                           // to translate Event values to 2-char strings

extern char *	eventNm( Event e );									// => string for enum Event 
extern char *	shEvntNm( Event e );								// => 2-char for enum Event 
extern char *	actionNm( Action a );								// => string for enum Action 


#endif	/* tb_enum.h */