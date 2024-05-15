// TBookV2   TB_EVR.h
// Gene Ball   Apr2020  -- TB event recorder events

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef TBEVR_H
#define TBEVR_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
//#include "rl_usb.h"
#include <stdio.h>
//#include "stm32f412vx.h"
#include <string.h>

#include "EventRecorder.h"

#define evrE 		(EventRecordError)
#define evrEA 	(EventRecordError | EventRecordAPI)
#define evrED 	(EventRecordError | EventRecordDetail)
#define evrEAO 	(EventRecordError | EventRecordAPI | EventRecordOp)
#define evrEAOD	(EventRecordAll)
#define evrAOD 	(EventRecordAPI | EventRecordOp | EventRecordDetail)

extern int dbgEvtCnt;

extern void 			dbgEVR( int id, int a1, int a2, int a3, int a4 );
extern void 			dbgEvt( int id, int a1, int a2, int a3, int a4 );
extern void 			dbgEvtD( int id, const void *d, int len );
extern void 			dbgEvtS( int id, const char *d );

// Keil EVR components
/* Fs component number - available range: [0x80-0x9F] */
#define EvtFsCore_No   (0x80 | 0)       /* FsCore component number */
#define EvtFsFAT_No    (0x80 | 1)       /* FsFAT component number */
#define EvtFsEFS_No    (0x80 | 2)       /* FsEFS component number */
#define EvtFsIOC_No    (0x80 | 3)       /* FsIOC component number */
#define EvtFsNFTL_No   (0x80 | 4)       /* FsNFTL component number */
#define EvtFsNAND_No   (0x80 | 5)       /* FsNAND component number */
#define EvtFsMcMCI_No  (0x80 | 6)       /* FsMcMCI component number */
#define EvtFsMcSPI_No  (0x80 | 7)       /* FsMcSPI component number */

#define EvtUsbdCore_No			0xA0
#define EvtUsbdDriver_No		0xA1
#define EvtUsbdMSC_No			  0xA6
#define EvtUsbdEnd_No				0xA6

/// TBook component number
#define TB_no               (0x01)
#define TB_dbgPlay          EventID(EventLevelOp,     TB_no,    0x00)
#define TB_Error            EventID(EventLevelError,  TB_no,    0x01)
#define TB_Fault            EventID(EventLevelError,  TB_no,    0x02)
#define TB_Alloc            EventID(EventLevelOp,     TB_no,    0x03)
#define TB_bootCnt          EventID(EventLevelOp,     TB_no,    0x04)
#define TB_wrMsgFile		EventID(EventLevelAPI,    TB_no,    0x05)
#define TB_wrLogFile		EventID(EventLevelAPI,    TB_no,    0x06)
#define TB_clAud			EventID(EventLevelOp,     TB_no,    0x07)
#define TB_wrLnFile			EventID(EventLevelAPI,    TB_no,    0x08)
#define TB_wrStatFile		EventID(EventLevelAPI,    TB_no,    0x09)
#define TB_LdStatFile		EventID(EventLevelAPI,    TB_no,    0x0A)
#define TB_flshLog			EventID(EventLevelOp,     TB_no,    0x0B)

#define TBAud_no            (0x02)
#define TB_ldBuff           EventID(EventLevelDetail, TBAud_no, 0x00)
#define TB_dmaComp          EventID(EventLevelDetail, TBAud_no, 0x01)
#define TB_playWv           EventID(EventLevelAPI,    TBAud_no, 0x02)
#define TB_tstRd            EventID(EventLevelDetail, TBAud_no, 0x03)
#define TB_audSent          EventID(EventLevelDetail, TBAud_no, 0x04)
#define TB_audClose         EventID(EventLevelOp,     TBAud_no, 0x05)
#define TB_audDone          EventID(EventLevelOp,     TBAud_no, 0x06)
#define TB_stPlay           EventID(EventLevelOp,     TBAud_no, 0x07)
#define TB_audPause         EventID(EventLevelAPI,    TBAud_no, 0x08)
#define TB_audResume        EventID(EventLevelAPI,    TBAud_no, 0x09)
#define TB_audSetWPos       EventID(EventLevelAPI,    TBAud_no, 0x0a)
#define TB_audAdjVol        EventID(EventLevelAPI,    TBAud_no, 0x0b)
#define TB_audAdjPos        EventID(EventLevelAPI,    TBAud_no, 0x0c)
#define TB_audAdjSpd        EventID(EventLevelAPI,    TBAud_no, 0x0d)
#define TB_recWv        		EventID(EventLevelAPI,    TBAud_no, 0x0e)
#define TB_stRecord         EventID(EventLevelOp,     TBAud_no, 0x0f)
#define TB_audRecClose      EventID(EventLevelOp,     TBAud_no, 0x10)
#define TB_audRecDn         EventID(EventLevelOp,     TBAud_no, 0x11)
#define TB_wrRecBuff        EventID(EventLevelDetail, TBAud_no, 0x12)
#define TB_FtchSk						EventID(EventLevelError,  TBAud_no, 0x13)
#define TB_FtchRd						EventID(EventLevelError,  TBAud_no, 0x14)
#define TB_StorSk						EventID(EventLevelError,  TBAud_no, 0x15)
#define TB_StorWr						EventID(EventLevelError,  TBAud_no, 0x16)
#define TB_initLogFile			EventID(EventLevelOp,     TBAud_no, 0x17)
#define TB_chkLog						EventID(EventLevelOp,     TBAud_no, 0x18)
#define TB_recPause					EventID(EventLevelOp,     TBAud_no, 0x19)
#define TB_recResume				EventID(EventLevelOp,     TBAud_no, 0x1A)
#define TB_svBuffs        	EventID(EventLevelDetail, TBAud_no, 0x20)


#define TBsai_no            (0x03)
#define TB_saiEvt           EventID(EventLevelDetail, 		TBsai_no, 0x04)
#define TB_saiCtrl          EventID(EventLevelOp,     		TBsai_no, 0x05)
#define TB_saiInit          EventID(EventLevelAPI,    		TBsai_no, 0x06)
#define TB_saiPower         EventID(EventLevelAPI,    		TBsai_no, 0x07)
#define TB_saiSend          EventID(EventLevelDetail, 		TBsai_no, 0x08)
#define TB_cdcInit          EventID(EventLevelOp,     		TBsai_no, 0x09)
#define TB_cdcPwrDn         EventID(EventLevelOp,     		TBsai_no, 0x0a)
#define TB_cdcSetVol        EventID(EventLevelAPI,    		TBsai_no, 0x0b)
#define TB_cdcSetMute       EventID(EventLevelAPI,    		TBsai_no, 0x0c)
#define TB_cdWrReg        	EventID(EventLevelDetail, 		TBsai_no, 0x0d)
#define TB_mediaEvt    	    EventID(EventLevelDetail, 		TBsai_no, 0x0e)
#define TB_saiTXDN          EventID(EventLevelDetail, 		TBsai_no, 0x0f)
#define TB_saiPLYDN         EventID(EventLevelDetail, 		TBsai_no, 0x10)
#define TB_cdcSpkEn         EventID(EventLevelOp,     		TBsai_no, 0x11)
#define TB_saiRcv        		EventID(EventLevelDetail,     TBsai_no, 0x12)
#define TB_saiRXDN          EventID(EventLevelDetail, 		TBsai_no, 0x13)
#define TB_relBuff          EventID(EventLevelDetail, 		TBsai_no, 0x14)
#define TB_i2cErr						EventID(EventLevelError, 			TBsai_no, 0x15)
#define TB_allocBuff        EventID(EventLevelDetail, 		TBsai_no, 0x16)

#define TBCSM_no            (0x04)
#define TB_csmChSt					EventID(EventLevelOp,     		TBCSM_no, 0x00)
#define TB_csmDoAct					EventID(EventLevelOp,     		TBCSM_no, 0x01)
#define TB_csmEvt						EventID(EventLevelDetail, 		TBCSM_no, 0x01)

#define TBUSB_no            (0x05)
#define TB_usbInit					EventID(EventLevelDetail,     TBUSB_no, 0x00)
#define TB_usbUninit				EventID(EventLevelDetail,     TBUSB_no, 0x01)
#define TB_usbVbchg					EventID(EventLevelDetail,     TBUSB_no, 0x02)
#define TB_usbReset         EventID(EventLevelDetail,     TBUSB_no, 0x03)
#define TB_usbHSAct         EventID(EventLevelDetail,     TBUSB_no, 0x04)
#define TB_usbSusp          EventID(EventLevelDetail,     TBUSB_no, 0x05)
#define TB_usbResum         EventID(EventLevelDetail,     TBUSB_no, 0x06)
#define TB_usbCfgCh         EventID(EventLevelDetail,     TBUSB_no, 0x07)
#define TB_usbRmtWkup       EventID(EventLevelDetail,     TBUSB_no, 0x08)
#define TB_usbDisRemWk      EventID(EventLevelDetail,     TBUSB_no, 0x09)
#define TB_usbSetup         EventID(EventLevelOp,     		TBUSB_no, 0x0a)
#define TB_usbSuPrLn        EventID(EventLevelDetail,     TBUSB_no, 0x0b)
#define TB_usbSuProc        EventID(EventLevelDetail,     TBUSB_no, 0x0c)
#define TB_usbEpOut         EventID(EventLevelDetail,     TBUSB_no, 0x0d)
#define TB_usbEpIn          EventID(EventLevelDetail,     TBUSB_no, 0x0e)
#define TB_usbLnInit				EventID(EventLevelDetail,     TBUSB_no, 0x10)
#define TB_usbLnUnm         EventID(EventLevelDetail,     TBUSB_no, 0x11)
#define TB_usbDrvId         EventID(EventLevelDetail,     TBUSB_no, 0x12)
#define TB_usbLnIMed        EventID(EventLevelDetail,     TBUSB_no, 0x13)
#define TB_usbLnOk          EventID(EventLevelDetail,     TBUSB_no, 0x14)
#define TB_usbLnUninit			EventID(EventLevelDetail,     TBUSB_no, 0x15)
#define TB_usbEnMSC         EventID(EventLevelDetail,     TBUSB_no, 0x16)
#define TB_usbConn          EventID(EventLevelDetail,     TBUSB_no, 0x17)
#define TB_usbDisMSC        EventID(EventLevelDetail,     TBUSB_no, 0x18)
#define TB_usbCachInfo      EventID(EventLevelDetail,     TBUSB_no, 0x19)
#define TB_usbM0Init        EventID(EventLevelDetail,     TBUSB_no, 0x1a)
#define TB_usbM0Uninit      EventID(EventLevelDetail,     TBUSB_no, 0x1b)
#define TB_usbM0getMaxL			EventID(EventLevelDetail,     TBUSB_no, 0x1c)
#define TB_usbM0getCap			EventID(EventLevelOp,     		TBUSB_no, 0x1d)
#define TB_usbM0read				EventID(EventLevelOp,    			TBUSB_no, 0x1e)
#define TB_usbM0write				EventID(EventLevelOp,    			TBUSB_no, 0x1f)
#define TB_usbM0chkMed			EventID(EventLevelOp,     		TBUSB_no, 0x20)
#define TB_usbDescr					EventID(EventLevelOp,     		TBUSB_no, 0x21)

#define TBUSBDrv_no					(0x06)
#define TBd_usbIRQ					EventID(EventLevelDetail, 		TBUSBDrv_no, 0x00)
#define TBd_usbEpCfg				EventID(EventLevelOp,     		TBUSBDrv_no, 0x01)
#define TBd_usbRdSetup			EventID(EventLevelOp,     		TBUSBDrv_no, 0x02)
#define TBd_usbSetAddr			EventID(EventLevelAPI,     		TBUSBDrv_no, 0x03)
#define TBd_usbPwrCtrl			EventID(EventLevelOp,     		TBUSBDrv_no, 0x04)
#define TBd_usbConn					EventID(EventLevelOp,     		TBUSBDrv_no, 0x05)
#define TBd_usbReset				EventID(EventLevelAPI,     		TBUSBDrv_no, 0x06)
#define TBd_usbStall				EventID(EventLevelAPI,     		TBUSBDrv_no, 0x07)
#define TBd_usbXfrAbort			EventID(EventLevelAPI,     		TBUSBDrv_no, 0x08)
#define TBd_usbRcv					EventID(EventLevelOp,     		TBUSBDrv_no, 0x09)
#define TBd_usbXmt					EventID(EventLevelOp,     		TBUSBDrv_no, 0x0A)
#define TBd_usbEnumFS				EventID(EventLevelOp,     		TBUSBDrv_no, 0x0B)
#define TBd_usbXfrCnt				EventID(EventLevelDetail, 		TBUSBDrv_no, 0x0C)
#define TBd_usbRcvEnab			EventID(EventLevelDetail, 		TBUSBDrv_no, 0x0D)

#define TBd_usbRcv0					EventID(EventLevelOp,     		TBUSBDrv_no, 0x10)
#define TBd_usbXmt0					EventID(EventLevelOp,     		TBUSBDrv_no, 0x20)
#define TBd_usbRcv1					EventID(EventLevelOp,     		TBUSBDrv_no, 0x11)
#define TBd_usbXmt1					EventID(EventLevelOp,     		TBUSBDrv_no, 0x21)
#define TBd_usbRcv2					EventID(EventLevelOp,    			TBUSBDrv_no, 0x12)
#define TBd_usbXmt2					EventID(EventLevelOp,    	 		TBUSBDrv_no, 0x22)
#define TBd_usbRcv3					EventID(EventLevelOp,     		TBUSBDrv_no, 0x13)
#define TBd_usbXmt3					EventID(EventLevelOp,     		TBUSBDrv_no, 0x23)
#define TBd_usbRcv4					EventID(EventLevelOp,     		TBUSBDrv_no, 0x14)
#define TBd_usbXmt4					EventID(EventLevelOp,     		TBUSBDrv_no, 0x24)
#define TBd_usbRcv5					EventID(EventLevelOp,     		TBUSBDrv_no, 0x15)
#define TBd_usbXmt5					EventID(EventLevelOp,     		TBUSBDrv_no, 0x25)
#define TBd_usbRcv6					EventID(EventLevelOp,     		TBUSBDrv_no, 0x16)
#define TBd_usbXmt6					EventID(EventLevelOp,     		TBUSBDrv_no, 0x26)
#define TBd_usbRcv7					EventID(EventLevelOp,     		TBUSBDrv_no, 0x17)
#define TBd_usbXmt7					EventID(EventLevelOp,     		TBUSBDrv_no, 0x27)

#define TBkey_no						(0x07)
#define TB_keyEvt						EventID(EventLevelAPI, 				TBkey_no, 0x00)
#define TB_keyBnc						EventID(EventLevelDetail, 		TBkey_no, 0x01)
#define TB_keyStar					EventID(EventLevelOp, 				TBkey_no, 0x02)
#define TB_keyLong					EventID(EventLevelOp, 				TBkey_no, 0x03)
#define TB_keyShort					EventID(EventLevelOp, 				TBkey_no, 0x04)
#define TB_keyTest					EventID(EventLevelAPI, 				TBkey_no, 0x05)
#define TB_ktReset					EventID(EventLevelOp, 				TBkey_no, 0x06)
#define TB_ktFirst					EventID(EventLevelOp, 				TBkey_no, 0x07)
#define TB_ktDone						EventID(EventLevelOp, 				TBkey_no, 0x08)
#define TB_ktRepeat					EventID(EventLevelOp, 				TBkey_no, 0x09)
#define TB_keyDFU						EventID(EventLevelAPI, 				TBkey_no, 0x0a)
#define TB_keyIRQ						EventID(EventLevelDetail, 		TBkey_no, 0x0b)
#define TB_keyDn						EventID(EventLevelDetail, 		TBkey_no, 0x0c)
#define TB_keyStUp					EventID(EventLevelDetail, 		TBkey_no, 0x0d)
#define TB_keyUp						EventID(EventLevelDetail, 		TBkey_no, 0x0e)
#define TB_keyWk						EventID(EventLevelDetail, 		TBkey_no, 0x0f)
#define TB_keyTmr						EventID(EventLevelDetail, 		TBkey_no, 0x10)
#define TBkeyMismatch				EventID(EventLevelError, 		  TBkey_no, 0x11)
//#define TB_keyX						EventID(EventLevelAPI, 				TBkey_no, 0x0a)

#define TBnor_no						(0x08)
#define TB_norErr						EventID(EventLevelError, 				TBnor_no, 0x01)
#define TB_longOp						EventID(EventLevelError, 				TBnor_no, 0x02)

//#define TB_norX						EventID(EventLevelAPI, 				TBnor_no, 0x01)

#endif /* __TBEVR_H__ */
