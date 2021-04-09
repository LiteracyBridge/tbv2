//******* ak4343.c   converted to use CMSIS-Driver/I2C   JEB 12/2019

/**
  ******************************************************************************
  * @file    ak4343.c
  * @author  MCD Application Team
  * @version V2.0.0
  * @date    11-April-2016
  * @brief   This file provides the AK4343 Audio Codec driver.   
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2016 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "tbook.h"					// defines TBook_V2_Rev1
#include "ak4xxx.h"
#include "Driver_I2C.h"			// CMSIS-Driver I2C interface
#include "Driver_SAI.h"			// CMSIS-Driver SPI interface

//*****************  CMSIS I2C driver interface
extern ARM_DRIVER_I2C     Driver_I2C1;
static ARM_DRIVER_I2C *		I2Cdrv = 				&Driver_I2C1;

#if !defined (VERIFY_WRITTENDATA)  
// Uncomment this line to enable verifying data sent to codec after each write operation (for debug purpose)
#define VERIFY_WRITTENDATA 
#endif /* VERIFY_WRITTENDATA */

static uint8_t 	akFmtVolume;
static bool			akSpeakerOn 	= false;
static bool			akMuted			 	= false;

const int 			I2C_Xmt = 0;
const int 			I2C_Rcv = 1;
const int 			I2C_Evt = 2;		// errors reported from ISR 
const int 			AK_DefReg = 3;
const int 			AK_WrReg = 4;

static bool			akCodecDummyWrite = true;		// flag dummy ak write at startup-- ignore NAK error 
static int 			AK_ErrCnt[ ]= { 0,0,0,0,0 };
static char * 	akGrp[] = { "Xmt", "Rcv", "Evt", "Def", "Wr" };

static AK4637_Registers 				akR;		// akR.reg[1] == akR.R.PM2 (changed values)
static AK4637_Registers 				akC;		// akC.reg[] -- values written to codec

static volatile int32_t 				I2C_Event = 0;
static int											MaxBusyWaitCnt = 0;
static int											eCnt = 0;
const  int 											maxErrLog	= 10;				// max errors to add to log

static int 											reinitCnt = 0;	
static int 											LastVolume 	= 0;			// retain last setting, so audio restarts at last volume
static int 											WatchReg   = -1;			//DEBUG: log register read/write ops for a specific codec register

void 						Codec_WrReg( uint8_t Reg, uint8_t Value);		// FORWARD

#ifdef VERIFY_WRITTENDATA	
	const uint8_t AK_LAST_REG_MKR = 0xff;
	struct {  uint8_t reg; uint8_t def; } RegDefault[] = {
	#if defined( AK4343 )
		{ AK_Power_Management_1, 				AK_PM1_VCM }, // instead of AK_Power_Management_1_RST }, because VCM set before checkRegs
		{ AK_Power_Management_2, 				AK_Power_Management_2_RST },
		{ AK_Signal_Select_1, 					AK_Signal_Select_1_RST },
		{ AK_Signal_Select_2, 					AK_Signal_Select_2_RST },
		{ AK_Mode_Control_1, 						AK_Mode_Control_1_RST },
		{ AK_Mode_Control_2, 						AK_Mode_Control_2_RST },
		{ AK_Timer_Select, 							AK_Timer_Select_RST },
		{ AK_ALC_Mode_Control_1, 				AK_ALC_Mode_Control_1_RST },
		{ AK_ALC_Mode_Control_2, 				AK_ALC_Mode_Control_2_RST },
		{ AK_Lch_Input_Volume_Control, 	AK_Lch_Input_Volume_Control_RST },
		{ AK_Lch_Digital_Volume_Control,AK_Lch_Digital_Volume_Control_RST },
		{ AK_ALC_Mode_Control_3, 				AK_ALC_Mode_Control_3_RST },
		{ AK_Rch_Input_Volume_Control, 	AK_Rch_Input_Volume_Control_RST },
		{ AK_Rch_Digital_Volume_Control,AK_Rch_Digital_Volume_Control_RST },
		{ AK_Mode_Control_3, 						AK_Mode_Control_3_RST },
		{ AK_Mode_Control_4, 						AK_Mode_Control_4_RST },
		{ AK_Power_Management_3, 				AK_Power_Management_3_RST },
		{ AK_Digital_Filter_Select, 		AK_Digital_Filter_Select_RST },
	#endif
	#if defined( AK4637 )
		{ AK_Power_Management_1, 				AK_Power_Management_1_RST }, 
		{ AK_Power_Management_2, 				AK_Power_Management_2_RST },
		{ AK_Signal_Select_1, 					AK_Signal_Select_1_RST },
		{ AK_Signal_Select_2, 					AK_Signal_Select_2_RST },
		{ AK_Signal_Select_3, 					AK_Signal_Select_3_RST },
		{ AK_Mode_Control_1, 						AK_Mode_Control_1_RST },
		{ AK_Mode_Control_2, 						AK_Mode_Control_2_RST },
		{ AK_Mode_Control_3, 						AK_Mode_Control_3_RST },
		{ AK_Digital_MIC, 							AK_Digital_MIC_RST },
		{ AK_Timer_Select, 							AK_Timer_Select_RST },
		{ AK_ALC_Timer_Select,					AK_ALC_Timer_Select_RST },
		{ AK_ALC_Mode_Control_1, 				AK_ALC_Mode_Control_1_RST },
		{ AK_ALC_Mode_Control_2, 				AK_ALC_Mode_Control_2_RST },
		{ AK_Input_Volume_Control, 			AK_Input_Volume_Control_RST },
		{ AK_ALC_Volume, 								AK_ALC_Volume_RST },
		{ AK_BEEP_Control, 							AK_BEEP_Control_RST },
		{ AK_EQ_Common_Gain_Select, 		AK_EQ_Common_Gain_Select_RST },
		{ AK_EQ2_Gain_Setting, 					AK_EQ2_Gain_Setting_RST },
		{ AK_EQ3_Gain_Setting, 					AK_EQ3_Gain_Setting_RST },
		{ AK_EQ4_Gain_Setting, 					AK_EQ4_Gain_Setting_RST },
		{ AK_EQ5_Gain_Setting, 					AK_EQ5_Gain_Setting_RST },
		{ AK_Digital_Filter_Select_1, 	AK_Digital_Filter_Select_1_RST },
		{ AK_Digital_Filter_Select_2, 	AK_Digital_Filter_Select_2_RST },
		{ AK_Digital_Filter_Mode, 			AK_Digital_Filter_Mode_RST },
	#endif  
	{ AK_LAST_REG_MKR,							AK_LAST_REG_MKR }
};
#endif

void 		 				cntErr( int8_t grp, int8_t exp, int8_t got, int32_t reg, int16_t caller ){  // count and/or report ak errors
	if ( got == exp || akCodecDummyWrite ) return;
	akCodecDummyWrite = false;
	AK_ErrCnt[ grp ]++;
	eCnt++;
	dbgEvt( TB_i2cErr, (caller << 8) + grp, exp, got, reg );
	if ( eCnt < maxErrLog && grp!=I2C_Evt )	// can't report I2C_Evt (from ISR)
		errLog("AK %s err, exp=0x%x, got=0x%x reg=%d caller=%d", akGrp[grp], exp, got, reg, caller );
}
void 						i2c_Sig_Event( uint32_t event ){														// called from ISR for I2C errors or complete
	I2C_Event = event;
	cntErr( I2C_Evt, ARM_I2C_EVENT_TRANSFER_DONE, event, I2Cdrv->GetDataCount(), 1 );
}
void		 				I2C_initialize() {																					// Initialize I2C device
//	I2Cdrv->Control( ARM_I2C_BUS_CLEAR, 0 );  // resets GPIO, etc. -- if needed, should be before initialization
  I2Cdrv->Initialize( i2c_Sig_Event );					// sets up SCL & SDA pins, evt handler
  I2Cdrv->PowerControl( ARM_POWER_FULL );		// clocks I2C device & sets up interrupts
  I2Cdrv->Control( ARM_I2C_BUS_SPEED, ARM_I2C_BUS_SPEED_STANDARD );		// set I2C bus speed
  I2Cdrv->Control( ARM_I2C_OWN_ADDRESS, 0 );		// set I2C own address-- REQ to initialize properly
}
void 						I2C_Init() {																								// Initialize I2C connection to reset AK4637 & write a reg
  I2C_initialize();				// init device
	akCodecDummyWrite = true;
	Codec_WrReg( 0, 0 );		// DUMMY COMMAND: won't be acknowledged-- will generate an error  -- AK4637 datasheet pg 34: SystemReset
}
void						I2C_Reinit(int lev ){																				// lev&1 SWRST, &2 => RCC reset, &4 => device re-init
	if ( lev & 4 ){
		I2Cdrv->Uninitialize( );					// deconfigures SCL & SDA pins, evt handler
		I2C_initialize();
	}
	if ( lev & 2 ){
		RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST;		// set device reset bit for I2C1
		tbDelay_ms( 3 ); //DEBUG  1 );
		RCC->APB1RSTR = 0;		// TURN OFF device reset bit for I2C1
/* reset sequence in PowerControl( ARM_POWER_FULL )
        __HAL_RCC_I2C1_FORCE_RESET();
        __NOP(); __NOP(); __NOP(); __NOP(); 
        __HAL_RCC_I2C1_RELEASE_RESET();
*/		
	}
	if ( lev & 1 ){
		uint32_t cr1 = I2C1->CR1;
		I2C1->CR1 = I2C_CR1_SWRST;			// set Software Reset bit
		tbDelay_ms( 3 ); // I2C reinit DEBUG 1);
		I2C1->CR1 = cr1;			// reset to previous
	}
}
#ifdef VERIFY_WRITTENDATA
uint8_t 				Codec_RdReg( uint8_t Reg ){																	// return value of codec register Reg 
	uint8_t value;
	int status;
	
  I2C_Event = 0U;  		// Clear event flags before new transfer
  status = I2Cdrv->MasterTransmit( AUDIO_I2C_ADDR, &Reg, 1, true );		// send register index
	cntErr( I2C_Xmt, ARM_DRIVER_OK, status, 0, 2 );
	
	int waitCnt = 0;
  while (( I2C_Event & ARM_I2C_EVENT_TRANSFER_DONE ) == 0U)
		waitCnt++;  // busy wait until transfer completed
	
	cntErr( I2C_Xmt, ARM_I2C_EVENT_TRANSFER_DONE, I2C_Event, 0, 3 );  // err if any other bits set

  I2C_Event = 0U;  		// Clear event flags before new transfer
	status = I2Cdrv->MasterReceive( AUDIO_I2C_ADDR, &value, 1, false );  // receive register contents into *value
	cntErr( I2C_Rcv, ARM_DRIVER_OK, status, Reg, 4 );

  while (( I2C_Event & ARM_I2C_EVENT_TRANSFER_DONE ) == 0U)
		waitCnt++;  // busy wait until transfer completed
	if ( waitCnt > MaxBusyWaitCnt ) MaxBusyWaitCnt = waitCnt;
	cntErr( I2C_Xmt, ARM_I2C_EVENT_TRANSFER_DONE, I2C_Event, 0, 5 );  // err if any other bits set
	
	if ( Reg==WatchReg )
		dbgLog( " R%x:%02x \n", Reg, value );
	if ( Reg < AK_NREGS )
		akR.reg[ Reg ] = akC.reg[ Reg ] = value;
	return value;
}
#endif

void 						Codec_WrReg( uint8_t Reg, uint8_t Value){										// write codec register Reg with Value
	dbgEvt( TB_akWrReg, Reg,Value,0,0);
	if ( Reg==WatchReg )
		dbgLog( "Wr R%x = %02x \n", Reg, Value );

	uint32_t status;
	int waitCnt = 0;
  
  uint8_t buf[2];
  buf[0] = Reg;
  buf[1] = Value;
  I2C_Event = 0U;  		// Clear event flags before new transfer
  status = I2Cdrv->MasterTransmit( AUDIO_I2C_ADDR, buf, 2, false );
	cntErr( I2C_Xmt, ARM_DRIVER_OK, status, Reg, 6 );
	
  while (( I2C_Event & ARM_I2C_EVENT_TRANSFER_DONE ) == 0U){
		waitCnt++;  // busy wait until transfer completed
		if ( waitCnt > 1000 ){
			errLog("I2C Tx hang, r=%d v=0x%x", Reg, Value);
			reinitCnt++;
			if ( reinitCnt > 16 ) tbErr( "Can't restart I2C\n" );
			I2C_Reinit( reinitCnt );			// SW, RST, SW+RST, INIT, INIT+SW, ...
			Codec_WrReg( Reg, Value );		// recursive call to retry on reset device
			return;
		}
	}
	if ( waitCnt > MaxBusyWaitCnt ) MaxBusyWaitCnt = waitCnt;
	cntErr( I2C_Xmt, ARM_I2C_EVENT_TRANSFER_DONE, I2C_Event, 0, 7 );  // err if any other bits set
	
//	dbgLog( "W%x:%02x ", Reg, Value );
	#ifdef VERIFY_WRITTENDATA
		/* Verify that the data has been correctly written */  
		int chkVal = Codec_RdReg( Reg );
		cntErr( AK_WrReg, Value, chkVal, Reg, 8 );
	#endif /* VERIFY_WRITTENDATA */
	reinitCnt = 0;			// success-- reset recursive counter
}
void						akUpd(){																										// write values of any modified codec registers
	for ( int i=0; i < AK_NREGS; i++ ){
		uint8_t nv = akR.reg[ i ];
		uint8_t cv = akC.reg[ i ];
		if ( nv != cv ){		// register has changed -- write it to codec
			Codec_WrReg( i, nv );
			akC.reg[i] = nv;
		}
	}
}
void  					ak_ReportErrors(){																					// report I2C error counts
  int tot = 0;
	for ( int i=0; i<= AK_WrReg; i++ )
	  tot += AK_ErrCnt[i];
	if (tot==0) return;
	
	dbgLog( "%d AK Errs-- Xmt:%d Rcv:%d Evt:%d Def:%d WrReg:%d \n", tot,
		AK_ErrCnt[ I2C_Xmt ], AK_ErrCnt[ I2C_Rcv ], AK_ErrCnt[ I2C_Evt ], AK_ErrCnt[ AK_DefReg ], AK_ErrCnt[ AK_WrReg ] );
}
void 						ak_CheckRegs(){																							// Debug -- read main AK4343 regs
	#ifdef VERIFY_WRITTENDATA	
		for ( int i = 0; RegDefault[i].reg != AK_LAST_REG_MKR; i++ ){
			uint8_t reg = RegDefault[i].reg;
			uint8_t defval = RegDefault[i].def;
			uint8_t val = Codec_RdReg( reg );
			cntErr( AK_DefReg, defval, val, reg, 9 );	
		}
		ak_ReportErrors();
	#endif
}
static int RecordTestCnt = 20; //-1;
void						ak_RecordEnable( bool enable ){
#if defined( AK4637 )
	if ( enable ){
		// from AK4637 datasheet:
		//ENABLE
		// done by ak_SetMasterFreq()			//   1)  Clock set up & sampling frequency  FS3_0
		//  05: 51, 06: 0E, 00: 40, 01:08 then 0C
		akR.R.SigSel1.MGAIN3 = 0;					//   2)  setup mic gain  0x02: 
		akR.R.SigSel1.MGAIN2_0 = 0x6;			//   2)  setup mic gain  0x02: 0x6 = +18db
if (RecordTestCnt==0) akR.R.SigSel1.MGAIN2_0 = 0x4;
if (RecordTestCnt==1) akR.R.SigSel1.MGAIN2_0 = 0x2;
if (RecordTestCnt==2) akR.R.SigSel1.MGAIN2_0 = 0x0;
		akR.R.SigSel1.PMMP = 1;						//   2)  MicAmp power
		akUpd();  // 02:0E 
		akR.R.SigSel2.MDIF = 1;						//   3)  input signal	 			Differential from Electret mic	
if (RecordTestCnt==8) akR.R.SigSel2.MDIF = 0;
		akR.R.SigSel2.MICL = 0;						//   3)  mic power					Mic power 2.4V 
if (RecordTestCnt==3) akR.R.SigSel2.MICL = 1;
		akUpd();  // 03: 01
																			//   4)  FRN, FRATT, ADRST1_0  0x09: TimSel
		akR.R.TimSel.FRN = 0;							//   4)  TimSel.FRN   			FastRecovery = Enabled (def)
		akR.R.TimSel.RFATT = 0;						//   4)  TimSel.RFATT  			FastRecovRefAtten = -.00106dB (def)
		akR.R.TimSel.ADRST1_0 = 0x0;			//   4)  TimSel.ADRST1_0  	ADCInitCycle = 66.2ms @ 16kHz (def)
if (RecordTestCnt==4) akR.R.TimSel.ADRST1_0 = 0x1;
if (RecordTestCnt==5) akR.R.TimSel.ADRST1_0 = 0x2;
if (RecordTestCnt==6) akR.R.TimSel.ADRST1_0 = 0x3;
		akUpd();  // --
																			//   5)  ALC mode   0x0A, 0x0B: AlcTimSel, AlcMdCtr1
		akR.R.AlcTimSel.RFST1_0 = 0x3;		//   5)  AlcTimSel.RFST1_0  FastRecoveryGainStep = .0127db
		akR.R.AlcTimSel.WTM1_0 = 0x01;		//   5)  AlcTimSel.WTM1_0   WaitTime = 16ms @ 16kHz
		akR.R.AlcTimSel.EQFC1_0 = 0x1;		//   5)  AlcTimSel.EQFC1_0  ALCEQ Frequency = 12kHz..24kHz
		akR.R.AlcTimSel.IVTM = 1;					//   5)  AlcTimSel.IVTM     InputDigVolSoftTransitionTime = 944/fs (def)
		akR.R.AlcMdCtr1.RGAIN2_0 = 0x0;		//   5)  AlcMdCtr1.RGAIN2_0	RecoveryGainStep = .00424db	(def)			
		akR.R.AlcMdCtr1.LMTH1_0 = 0x2;		//   5)  AlcMdCtr1.LMTH1_0	ALC LimiterDetectionLevel = Detect>-2.5dBFS Reset> -2.5..-4.1dBFS (def)
		akR.R.AlcMdCtr1.LMTH2 = 0;				//   5)  AlcMdCtr1.LMTH2	
		akR.R.AlcMdCtr1.ALCEQN = 0;				//   5)  AlcMdCtr1.ALCEQN		ALC Equalizer = Enabled (def)
		akR.R.AlcMdCtr1.ALC = 1;					//   5)  AlcMdCtr1.ALC			ALC = Enabled 
if (RecordTestCnt==7) akR.R.AlcMdCtr1.ALC = 0;
		akUpd();	// 0A: 47   0B: 22
		akR.R.AlcMdCtr2.REF7_0 = 0xE1;		//   6)  REF value  0x0C: AlcMdCtr2 ALCRefLevel = +30dB (def)
		akUpd();	// nc
		akR.R.InVolCtr.IVOL7_0 = 0xE1;		//   7)  IVOL value  0x0D:  InputDigitalVolume = +30db (def)
if (RecordTestCnt==9) akR.R.InVolCtr.IVOL7_0 = 0x0;
		akUpd();  // nc
																			//   8)  ProgFilter on/off  0x016,17,21: DigFilSel1, DigFilSel2, DigFilSel3
		akR.R.DigFilSel1.HPFAD = 1;				//   8) DigFilSel1.HPFAD		HPF1 Control after ADC = ON (def)
		akR.R.DigFilSel1.HPFC1_0 = 0x1;		//   8) DigFilSel1.HPFC1_0  HPF1 cut-off = 4.9Hz @ 16kHz
		akR.R.DigFilSel2.HPF = 0;					//   8) DigFilSel2.HPF			HPF F1A & F1B Coef Setting = Disabled (def)
		akR.R.DigFilSel2.LPF = 0;					//   8) DigFilSel2.LPF			LPF F2A & F2B Coef Setting = Disabled (def)
		akR.R.DigFilSel3.EQ5_1 = 0x0;			//   8) DigFilSel3.EQ5_1		Equalizer Coefficient 1..5 Setting = Disabled (def)
		akUpd();		// 16: 03
																			//   9)  ProgFilter path  0x18: DigFilMd
		akR.R.DigFilMd.PFDAC1_0 = 0x0;		//   9)  DigFilMd.PFDAC1_0	DAC input selector = SDTI (def)
		akR.R.DigFilMd.PFVOL1_0 = 0x0;		//   9)  DigFilMd.PFVOL1_0	Sidetone digital volume = 0db (def)
		akR.R.DigFilMd.PFSDO = 1;					//   9)  DigFilMd.PFSDO			SDTO output signal select = ALC output (def)
		akR.R.DigFilMd.ADCPF = 1;					//   9)  DigFilMd.ADCPF			ALC input signal select = ADC output (def)
		akUpd();		// nc
																			//	10)	 CoefFilter:  0x19..20, 0x22..3F: HpfC0..3, LpfC0..3 E1C0..5 E2C0..5 E3C0..5 E4C0..5 E5C0..5
		akR.R.HpfC0.F1A7_0 	= 0xB0;				//	10)	 Hpf.F1A13_0 low  (def)  //DISABLED by DigFilSel2.HPF
		akR.R.HpfC1.F1A13_8 = 0x1F;				//	10)	 Hpf.F1A13_0 high (def)  //DISABLED by DigFilSel2.HPF
		akR.R.HpfC2.F1B7_0 	= 0x9F;				//	10)	 Hpf.F1B13_0 low  (def)  //DISABLED by DigFilSel2.HPF
		akR.R.HpfC3.F1B13_8 = 0x02;				//	10)	 Hpf.F1B13_0 high (def)  //DISABLED by DigFilSel2.HPF
		akR.R.LpfC0.F2A7_0 	= 0x0;				//	10)	 Lpf.F2A13_0 low  (def)  //DISABLED by DigFilSel2.LPF
		akR.R.LpfC1.F2A13_8 = 0x0;				//	10)	 Lpf.F2A13_0 high (def)  //DISABLED by DigFilSel2.LPF
		akR.R.LpfC2.F2B7_0 	= 0x0;				//	10)	 Lpf.F2B13_0 low  (def)  //DISABLED by DigFilSel2.LPF
		akR.R.LpfC3.F2B13_8 = 0x0;				//	10)	 Lpf.F2B13_0 high (def)  //DISABLED by DigFilSel2.LPF
		akUpd();		// 19: B0  1A: 1F  1B: 9F 1C: 02
																			//  11)  power up MicAmp, ADC, ProgFilter: 
if (RecordTestCnt==10) { akR.R.DMIC.PMDM = 1;  akR.R.DMIC.DMIC = 1; }
		akR.R.PwrMgmt1.PMADC = 1;					//  11)  ADC								Microphone Amp & ADC = Power-up  (starts init sequence, then output data)
		akR.R.PwrMgmt1.PMPFIL = 1;				//  11)  ProgFilter 				Programmable Filter Block = Power up
		akUpd();		
		
//logEvtNI("RecTst", "cnt", RecordTestCnt );
RecordTestCnt++;

	} else {
		//DISABLE	
		//  12)  power down MicAmp, ADC, ProgFilter:  SigSel1.PMMP, PwrMgmt1.PMADC, .PMPFIL
		akR.R.SigSel1.PMMP = 0;						//  12)  MicAmp 
		akR.R.PwrMgmt1.PMADC = 0;						//  12)  ADC
		akR.R.PwrMgmt1.PMPFIL = 0;					//  12)  ProgFilter 
		akR.R.AlcMdCtr1.ALC = 0;						//  13)  ALC disable
		akUpd();
	}
#endif
}
void 						ak_SpeakerEnable( bool enable ){														// enable/disable speaker -- using mute to minimize pop
	if ( akSpeakerOn==enable )   // no change?
		return;
	
	akSpeakerOn = enable;
	dbgEvt( TB_akSpkEn, enable,0,0,0);

	if ( enable ){ 	
		#if defined( AK4343 )
			// power up speaker amp & codec by setting power bits with mute enabled, then disabling mute
			Codec_SetRegBits( AK_Signal_Select_1, AK_SS1_SPPSN, 0 );						// set power-save (mute) ON (==0)
			Codec_SetRegBits( AK_Power_Management_1, AK_PM1_SPK | AK_PM1_DAC, AK_PM1_SPK | AK_PM1_DAC );	// set spkr & DAC power ON
			Codec_SetRegBits( AK_Signal_Select_1, AK_SS1_DACS, AK_SS1_DACS );				// 2) DACS=1  set DAC->Spkr 
			Codec_SetRegBits( AK_Signal_Select_2, AK_SS2_SPKG1 | AK_SS2_SPKG0, AK_SS2_SPKG1 );   // Speaker Gain (SPKG0-1 bits): Gain=+10.65dB(ALC off)/+12.65(ALC on)
			tbDelay_ms(2); 	// AK wait at least a mSec -- ak4343 datasheet, fig. 59
			Codec_SetRegBits( AK_Signal_Select_1, AK_SS1_SPPSN, AK_SS1_SPPSN );		// set power-save (mute) OFF (==1)
		#endif
		#if defined( AK4637 )
			// startup sequence re: AK4637 Lineout Uutput pg 91
			akR.R.SigSel1.SLPSN = 0;  						// set power-save (mute) ON (==0)
			akUpd();															// and UPDATE
			gSet( gPA_EN, 1 );										// enable power to speaker & headphones
																						// 1) done by ak_SetMasterFreq()
			akR.R.PwrMgmt1.LOSEL = 1; 						// 2) LOSEL=1     MARC 7)
			akR.R.SigSel3.DACL = 1;  								// 3) DACL=1			  MARC 8)
			akR.R.SigSel3.LVCM1_0 = 0;  						// 3) LVCM=01 			MARC 8)
		  akR.R.DigVolCtr.DVOL7_0 = akFmtVolume;	// 4) Output_Digital_Volume 
			akR.R.DigFilMd.PFDAC1_0 = 0;  				// 5) PFDAC=0			 default MARC 9)
			akR.R.DigFilMd.ADCPF = 1;  						// 5) ADCPF=1			 default MARC 9)
			akR.R.DigFilMd.PFSDO = 1;  						// 5) PFSDO=1			 default MARC 9)
			akR.R.PwrMgmt1.PMDAC = 1;							// 6) Power up DAC
		  akR.R.PwrMgmt2.PMSL = 1;							// 7) set spkr power ON
			akUpd();															// UPDATE all settings
			tbDelay_ms( 300 ); // AK  DEBUG 30 );											// 7) wait up to 300ms   // MARC 11)
			akR.R.SigSel1.SLPSN = 1;							// 8) exit power-save (mute) mode (==1)
			akUpd();		
		#endif
	} else {			
		//  power down by enabling mute, then shutting off power
		#if defined( AK4343 )
			Codec_SetRegBits( AK_Signal_Select_1, AK_SS1_SPPSN, 0 );						// set power-save (mute) ON (==0)
			Codec_SetRegBits( AK_Power_Management_1, AK_PM1_SPK | AK_PM1_DAC, AK_PM1_SPK | AK_PM1_DAC );	// set spkr & DAC power ON
			Codec_SetRegBits( AK_Signal_Select_1, AK_SS1_DACS, 0 );	// disconnect DAC => Spkr 
		#endif
		#if defined( AK4637 )
			// shutdown sequence re: AK4637 pg 89
			akR.R.SigSel1.SLPSN = 0;  						// 13) enter SpeakerAmp Power Save Mode 
			akUpd();															// and UPDATE
			akR.R.SigSel1.DACS 	= 0;							// 14) set DACS = 0, disable DAC->Spkr
		  akR.R.PwrMgmt2.PMSL = 0;							// 15) PMSL=0, power down Speaker-Amp
			akR.R.PwrMgmt1.PMDAC = 0;							// 16) power down DAC
			akR.R.PwrMgmt1.PMPFIL = 0;						// 16) power down filter
			akR.R.PwrMgmt2.PMPLL = 0;  						// disable PLL (shuts off Master Clock from codec)
			akUpd();															// and UPDATE
			gSet( gPA_EN, 0 );										// disable power to speaker & headphones
		#endif
	}	
}

void						ak_PowerUp( void ){
	gSet( gBOOT1_PDN, 1 );  // OUT: set power_down ACTIVE to so codec doesn't try to PowerUP
	gSet( gEN_5V, 1 );			// OUT: 1 to supply 5V to codec		AP6714 EN		
	gSet( gEN1V8, 1 );		  // OUT: 1 to supply 1.8 to codec  TLV74118 EN		
	tbDelay_ms( 40 ); // AK DEBUG 20 ); 		 	// wait for voltage regulators
	
	gSet( gBOOT1_PDN, 0 );  //  set power_down INACTIVE to Power on the codec 
	tbDelay_ms( 10 ); // AK DEBUG 5); 		 			//  wait for it to start up
}
// external interface functions
void 						ak_Init( ){ 																								// Init codec & I2C (i2s_stm32f4xx.c)
	dbgEvt( TB_akInit, 0,0,0,0);

	memset( &akC, 0, sizeof( AK4637_Registers ));
	akSpeakerOn 	= false;
	akMuted			 	= false;

	ak_PowerUp(); 		// power-up codec
  I2C_Init();  			// powerup & Initialize the Control interface of the Audio Codec

	ak_CheckRegs();		// check all default register values & report

	#if defined( AK4343 )
		Codec_SetRegBits( AK_Signal_Select_1, AK_SS1_SPPSN, 0 );		// set power-save (mute) ON (==0)  (REDUNDANT, since defaults to 0)
	#endif
	#if defined( AK4637 )
		akR.R.SigSel1.SLPSN 		= 0;						// set power-save (mute) ON (==0)  (REDUNDANT, since defaults to 0)
		akUpd();
	#endif
	
	#if defined( AK4343 )
		//  run in Slave mode with external clock 
		Codec_WrReg( AK_Mode_Control_1, AK_I2S_STANDARD_PHILIPS );			// Set DIF0..1 to I2S standard
		Codec_WrReg( AK_Mode_Control_2, 0x00);      					// MCKI input frequency = 256.Fs  (default)
	#endif
	#if defined( AK4637 )
		// will run in PLL Master mode, set up by I2S_Configure
		akR.R.MdCtr3.DIF1_0 		= 3;							// DIF1_0 = 3   ( audio format = Philips )
	#endif

	if ( LastVolume == 0 ) // first time-- set to default_volume
		LastVolume = TB_Config.default_volume;
	ak_SetVolume( LastVolume );						// set to LastVolume used

	// Extra Configuration (of the ALC)  
	#if defined( AK4343 )
		int8_t wtm = AK_TS_WTM1 | AK_TS_WTM0; 			// Recovery Waiting = 3 
		int8_t ztm = AK_TS_ZTM1 | AK_TS_ZTM0; 			// Limiter/Recover Op Zero Crossing Timeout Period = 3 
		Codec_WrReg( AK_Timer_Select, (ztm|wtm)); 
		Codec_WrReg( AK_ALC_Mode_Control_2, 0xE1 ); // ALC_Recovery_Ref (default)
		Codec_WrReg( AK_ALC_Mode_Control_3, 0x00 ); //  (default)
		Codec_WrReg( AK_ALC_Mode_Control_1, AK_AMC1_ALC ); // enable ALC
		// REF -- maximum gain at ALC recovery
		// input volume control for ALC -- should be <= REF
		Codec_WrReg( AK_Lch_Input_Volume_Control, 0xC1 ); // default 0xE1 = +30dB
		Codec_WrReg( AK_Rch_Input_Volume_Control, 0xC1 );
	#endif
	#if defined( AK4637 )
		akR.R.AlcTimSel.IVTM 		= 1;							// IVTM = 1 (default)
		akR.R.AlcTimSel.EQFC1_0 = 2;							// EQFC1_0 = 10 (def)
		akR.R.AlcTimSel.WTM1_0 	= 3;							// WTM1_0 = 3
		akR.R.AlcTimSel.RFST1_0 = 0;							// RFST1_0 = 0 (def)

		akR.R.AlcMdCtr2.REF7_0 	= 0xE1;						// REF -- maximum gain at ALC recovery (default)
		akR.R.InVolCtr.IVOL7_0 	= 0xE1;						// input volume control for ALC -- should be <= REF (default 0xE1 = +30dB)
		akR.R.AlcMdCtr1.ALC 		= 1;							// enable ALC
		akUpd();			// UPDATE all changed registers
	#endif
  // ********** Uncomment these lines and set the correct filters values to use the  
  // ********** codec digital filters (for more details refer to the codec datasheet) 
    /* // Filter 1 programming as High Pass filter (Fc=500Hz, Fs=8KHz, K=20, A=1, B=1)
    Codec_WrReg( AK_FIL1_Coefficient_0, 0x01);
    Codec_WrReg( AK_FIL1_Coefficient_1, 0x80);
    Codec_WrReg( AK_FIL1_Coefficient_2, 0xA0);
    Codec_WrReg( AK_FIL1_Coefficient_3, 0x0B);
    */
    /* // Filter 3 programming as Low Pass filter (Fc=20KHz, Fs=8KHz, K=40, A=1, B=1)
    Codec_WrReg( AK_FIL1_Coefficient_0, 0x01);
    Codec_WrReg( AK_FIL1_Coefficient_1, 0x00);
    Codec_WrReg( AK_FIL1_Coefficient_2, 0x01);
    Codec_WrReg( AK_FIL1_Coefficient_3, 0x01);
    */
  
    /* // Equalizer programming BP filter (Fc1=20Hz, Fc2=2.5KHz, Fs=44.1KHz, K=40, A=?, B=?, C=?)
    Codec_WrReg( AK_EQ_Coefficient_0, 0x00);
    Codec_WrReg( AK_EQ_Coefficient_1, 0x75);
    Codec_WrReg( AK_EQ_Coefficient_2, 0x00);
    Codec_WrReg( AK_EQ_Coefficient_3, 0x01);
    Codec_WrReg( AK_EQ_Coefficient_4, 0x00);
    Codec_WrReg( AK_EQ_Coefficient_5, 0x51);
    */
  

	ak_SetMute( true );		// set soft mute on output
}


void 						ak_PowerDown( void ){																				// power down entire codec (i2s_stm..)
dbgEvt( TB_akPwrDn, 0,0,0,0);
	I2Cdrv->PowerControl( ARM_POWER_OFF );	// power down I2C
	I2Cdrv->Uninitialize( );								// deconfigures SCL & SDA pins, evt handler
	
	gSet( gPA_EN, 0 );				// amplifier off
	gSet( gBOOT1_PDN, 1 );    // OUT: set power_down ACTIVE to Power Down the codec 
	gSet( gEN_5V, 0 );				// OUT: 1 to supply 5V to codec		AP6714 EN		
	gSet( gEN1V8, 0 );			  // OUT: 1 to supply 1.8 to codec  TLV74118 EN		
}
//
//
static uint8_t testVol = 0x19;			// DEBUG
void		 				ak_SetVolume( uint8_t Volume ){														// sets volume 0..10  ( mediaplayer )
	const uint8_t akMUTEVOL = 0xCC, akMAXVOL = 0x18, akVOLRNG = akMUTEVOL-akMAXVOL;		// ak4637 digital volume range to use
	uint8_t v = Volume>10? 10 : Volume; 

	LastVolume = v;
	if ( audGetState()!=Playing ) return;			// just remember for next ak_Init()
	
	// Conversion of volume from user scale [0:10] to audio codec AK4343 scale  [akMUTEVOL..akMAXVOL] == 0xCC..0x18 
  //   values >= 0xCC force mute on AK4637
  //   limit max volume to 0x18 == 0dB (to avoid increasing digital level-- causing resets?)
  akFmtVolume = akMUTEVOL - ( v * akVOLRNG )/10; 
	

	if (Volume==99)	//DEBUG
		{ akFmtVolume = testVol; 	testVol--; }	//DEBUG: test if vol>akMAXVOL causes problems
	
//	logEvtNI( "setVol", "vol", v );
	dbgEvt( TB_akSetVol, Volume, akFmtVolume,0,0);
	dbgLog( "akSetVol v=%d akV=0x%x \n", v, akFmtVolume );
	#if defined( AK4343 )
		Codec_WrReg( AK_Lch_Digital_Volume_Control, akFmtVolume );  // Left Channel Digital Volume control
		Codec_WrReg( AK_Rch_Digital_Volume_Control, akFmtVolume );  // Right Channel Digital Volume control
	#endif
	#if defined( AK4637 )
		akR.R.DigVolCtr.DVOL7_0 = akFmtVolume;			
		akUpd();
	#endif
}

void		 				ak_SetMute( bool muted ){																	// true => enable mute on codec  (audio)
	if ( akMuted==muted ) return;
	dbgEvt( TB_akSetMute, muted,0,0,0);
	akMuted = muted;
	
	akR.R.MdCtr3.SMUTE = (muted? 1 : 0);
	akUpd();
}
void						ak_SetMasterFreq( int freq ){															// set AK4637 to MasterMode, 12MHz ref input to PLL, audio @ 'freq', start PLL  (i2s_stm32f4xx)
	#if defined( AK4637 )
	// set up AK4637 to run in MASTER mode, using PLL to generate audio clock at 'freq'
	// ref AK4637 Datasheet: pg 27-29
		akR.R.PwrMgmt2.PMPLL 		= 0;	// disable PLL -- while changing settings
		akUpd();
		akR.R.MdCtr1.PLL3_0 		= 6;	// PLL3_0 = 6 ( 12MHz ref clock )
		akR.R.MdCtr1.CKOFF	 		= 0;	// CKOFF = 0  ( output clocks ON )
		akR.R.MdCtr1.BCKO1_0 		= 1;	// BICK = 1   ( BICK at 32fs )
		int FS3_0 = 0;
		switch ( freq ){
			case 8000:	FS3_0 = 1; break;
			case 11025:	FS3_0 = 2; break;
			case 12000:	FS3_0 = 3; break;
			case 16000:	FS3_0 = 5; break;
			case 22050:	FS3_0 = 6; break;
			case 24000:	FS3_0 = 7; break;
			case 32000:	FS3_0 = 9; break;
			case 44100:	FS3_0 = 10; break;
			case 48000:	FS3_0 = 11; break;
		}
		akR.R.MdCtr2.FS3_0 			= FS3_0;			// FS3_0 specifies audio frequency
		akUpd();															// UPDATE frequency settings
		akR.R.PwrMgmt2.M_S 			= 1;					// M/S = 1   WS CLOCK WILL START HERE!
		akR.R.PwrMgmt1.PMVCM 		= 1;					// set VCOM bit first, then individual blocks (at SpeakerEnable)
		akUpd();															// update power & M/S
		tbDelay_ms( 4 ); // AK DEBUG 2 );		// 2ms for power regulator to stabilize
		akR.R.PwrMgmt2.PMPLL 		= 1;					// enable PLL 
		akUpd();															// start PLL 
		tbDelay_ms( 10 ); // AK DEBUG 5 );											// 5ms for clocks to stabilize
	#endif
}


//end file **************** ak4343.c
