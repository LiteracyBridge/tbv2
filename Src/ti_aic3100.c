//******* ti_aic3100.c   
//   driver for TI TLV320AIC3100 codec
// converted to use CMSIS-Driver/I2C   JEB 12/2019


/* Includes ------------------------------------------------------------------*/
#include "tbook.h"					// defines TBook_V2_Rev3

#include "codecI2C.h"				// I2C access utilities
#include "Driver_I2C.h"			// CMSIS-Driver I2C interface
#include "Driver_SAI.h"			// CMSIS-Driver SPI interface

//*****************  CMSIS I2C driver interface
extern ARM_DRIVER_I2C     Driver_I2C1;
static ARM_DRIVER_I2C *		I2Cdrv = 				&Driver_I2C1;

#if !defined (VERIFY_WRITTENDATA)  
// Uncomment this line to enable verifying data sent to codec after each write operation (for debug purpose)
#define VERIFY_WRITTENDATA 
#endif /* VERIFY_WRITTENDATA */

static int8_t 	cdcFmtVolume;
static bool			cdcSpeakerOn 		= false;
static bool			cdcMuted			 	= false;

static int 			aicCurrPg = 0;			// keeps track of currently selected register page
static int 			WatchReg   = 0;			//DEBUG: log register read/write ops for a specific codec register

// AIC_REG{ pg, reg, reset_val, W1_allowed, W1_required, nm, curr_val, next_val }
AIC_REG					codec_regs[] = {	// array of info for codec registers in use -- must include Page_Select for all pages in use
//********** AIC3100 PAGE 0
// pg,reg, reset, can1, must1,  "nm",																		curr
   0,   0, 0x00, 0xFF, 0x00, "Page_Select_Register",                    0x00,  // must include Page_Select for all pages in use
   0,   1, 0x00, 0x01, 0x00, "Software_Reset_Register",                 0x00,
	 0,   3, 0x66, 0x02, 0x00, "OT_FLAG",                                 0x66,  // X bits D2, D5,D6 set in practice-- actual: 0x66 not 0x02
   0,   4, 0x00, 0x0F, 0x00, "ClkGen_Mux",                        			0x00,  
   0,   5, 0x11, 0xFF, 0x00, "PLL_P&R",                             		0x11,  
   0,   6, 0x04, 0x3F, 0x00, "PLL_J",                              			0x04,  
   0,   7, 0x00, 0x3F, 0x00, "PLL_D_Hi",                          			0x00,  
   0,   8, 0x00, 0xFF, 0x00, "PLL_D_Lo",                          			0x00,
   0,  11, 0x01, 0xFF, 0x00, "DAC_NDAC",                           		 	0x01,  
   0,  12, 0x01, 0xFF, 0x00, "DAC_MDAC",                           			0x01,  
   0,  13, 0x00, 0x03, 0x00, "DAC_DOSR_Hi",                        			0x00,
   0,  14, 0x80, 0xFF, 0x00, "DAC_DOSR_Lo",                        			0x80,  
   0,  15, 0x80, 0xFF, 0x00, "DAC_IDAC_VAL",                            0x80,  
   0,  16, 0x08, 0x0F, 0x00, "DAC_MAC_Engine_Interpolation",            0x08, 
   0,  18, 0x01, 0xFF, 0x00, "ADC_NADC_VAL",                            0x01, 
   0,  19, 0x01, 0xFF, 0x00, "ADC_MADC_VAL",                            0x01,  
   0,  20, 0x80, 0xFF, 0x00, "ADC_AOSR_VAL",                            0x80,  
   0,  21, 0x80, 0xFF, 0x00, "ADC_IADC_VAL",                            0x80,  
   0,  22, 0x04, 0xFF, 0x00, "ADC_PRB_Engine_Decimation",               0x04,  // use actual: 0x04 not 0x80 (reserved)
   0,  25, 0x00, 0x07, 0x00, "ClkOut_Mux",                              0x00,  
   0,  26, 0x01, 0xFF, 0x00, "ClkOut_M",                            		0x01,  
   0,  27, 0x00, 0xFD, 0x00, "InfcCtrl1",                								0x00,
   0,  29, 0x00, 0x3F, 0x00, "InfcCtrl2",                								0x00,
   0,  30, 0x01, 0xFF, 0x00, "BClk_N",                              		0x01,
   0,  33, 0x00, 0xFF, 0x00, "InfcCtrl3",			 													0x00,	 
   0,  36, 0x80, 0x00, 0x00, "ADC_Flag_Reg",														0x80,		// use actual: 0x80 not 0x00
   0,  37, 0x00, 0x00, 0x00, "DAC_Flag_Reg",                       			0x00,
   0,  53, 0x12, 0x1F, 0x00, "DOutCtrl",                            		0x12,
   0,  60, 0x01, 0x3F, 0x00, "DAC_Instruction_Set",                     0x01,
   0,  61, 0x04, 0x1F, 0x00, "adc_prb",                     						0x04,	
   0,  62, 0x00, 0x77, 0x00, "Prog_Instr_Mode_Cntrl",   								0x00,		// Programmable_Instruction_Mode_Control
   0,  63, 0x14, 0xFF, 0x00, "DAC_Dpath",                               0x14,
   0,  64, 0x0C, 0x0F, 0x00, "DAC_Vol_Ctl",                             0x0C,
   0,  65, 0x00, 0xFF, 0x00, "DAC_LVol_Ctl",                            0x00,
   0,  66, 0x00, 0xFF, 0x00, "DAC_RVol_Ctl",                        		0x00,
   0,  67, 0x00, 0x9F, 0x00, "Headset_Detection",                       0x00,
   0,  68, 0x6F, 0x7F, 0x00, "DRC_Control",                             0x6F,  // use actual reset 0x6f (DRC enabled)
// pg,reg, reset, can1, must1,  "nm",																		curr
   0,  81, 0x00, 0xBB, 0x00, "adc_pwr",																	0x00,
   0,  82, 0x80, 0xF0, 0x00, "adc_sm_vol",															0x80, 
   0,  83, 0x00, 0x7F, 0x00, "adc_vol",																	0x00, 
   0,  86, 0x00, 0xF0, 0x00, "agc_tgt",																	0x00, 
   0,  87, 0x00, 0xFE, 0x00, "AGC_Control2",														0x00, 
   0,  88, 0x7F, 0x7F, 0x00, "agc_maxg",																0x7F, 
   0,  89, 0x00, 0xFF, 0x00, "agc_atak",																0x00, 
   0,  90, 0x00, 0xFF, 0x00, "agc_decy",																0x00, 
   0,  91, 0x00, 0x1F, 0x00, "agc_nois_dbnc",														0x00, 
   0,  92, 0x00, 0x0F, 0x00, "agc_sig_dbnc",														0x00, 
   0,  93, 0x00, 0x00, 0x00, "agc_c_gain",															0x00, 

	 
#ifdef PG0_XTRA_USED
// 0,  28, 0x00, 0xFF, "Data_Slot_Offset_Programmability",
// 0,  31, 0x00, 0xFF, "Codec_Secondary_Interface_Control1",
// 0,  32, 0x00, 0xEF, "Codec_Secondary_Interface_Control2",
// 0,  33, 0x00, 0xFF, "Codec_Secondary_Interface_Control3",
// 0,  34, 0x00, 0xAF, "I2C_Bus_Condition",
// 0,  36, 0x00, 0x00, "ADC_Flag_Register",
// 0,  38, 0x00, 0x00, "DAC_Flag_Register",
// 0,  39, 0x00, 0x00, "Overflow_Flags",
// 0,  44, 0x00, 0x00, "Interrupt_Flags-DAC",
// 0,  45, 0x00, 0x00, "Interrupt_Flags-ADC",
// 0,  46, 0x00, 0x00, "Interrupt_Flags-DAC",
// 0,  47, 0x00, 0x00, "Interrupt_Flags-ADC",
// 0,  48, 0x00, 0xFF, "INT1_Control_Register",
// 0,  49, 0x00, 0xFF, "INT2_Control_Register",
// 0,  50, 0x00, 0xF0, "INT1&INT2_Control_Register",
// 0,  51, 0x00, 0xFD, "GPIO1_Control",
// 0,  54, 0x02, 0x06, "DIN_Control",
// 0,  69, 0x38, 0x7F, "DRC_Control",
// 0,  70, 0x00, 0xFF, "DRC_Control",
// 0,  71, 0x00, 0xFF, "Left_Beep_Generator",
// 0,  72, 0x00, 0xFF, "Right_Beep_Generator",
// 0,  73, 0x00, 0xFF, "Beep_Length_MSB",
// 0,  74, 0x00, 0xFF, "Beep_Length_Middle",
// 0,  75, 0xEE, 0xFF, "Beep_Length_LSB",
// 0,  76, 0x10, 0xFF, "Beep_Sin(x)_MSB",
// 0,  77, 0xD8, 0xFF, "Beep_Sin(x)_LSB",
// 0,  78, 0x7E, 0xFF, "Beep_Cos(x)_MSB",
// 0,  79, 0xE3, 0xFF, "Beep_Cos(x)_LSB",
// 0, 102, 0x00, 0xBF, "ADC_DC_Measurement",
// 0, 103, 0x00, 0x7F, "ADC_DC_Measurement",
// 0, 104, 0x00, 0x00, "ADC_DC_MSB",
// 0, 105, 0x00, 0x00, "ADC_DC_MID",
// 0, 106, 0x00, 0x00, "ADC_DC_LSB",
// 0, 116, 0x00, 0xFF, "DAC_Pin_Volume_Control_SAR_ADC",
// 0, 117, 0x00, 0x00, "VOL_Pin_Gain",
#endif

//********** AIC3100 PAGE 1
// pg,reg, reset, can1, must1,  "nm",																		curr
   1,   0, 0x01, 0x00, 0x00, "Page_Select_Register",                    0x01,  // 1:0 reads as 0x01
   1,  30, 0x00, 0x03, 0x00, "Headphone_Speaker_Amp_Error_Control",     0x00,  // Not Used by init
   1,  31, 0x04, 0xDE, 0x04, "Headphone_Drivers",                       0x04, 
   1,  32, 0x06, 0xFE, 0x06, "Cl-D_Drv",                                0x06, 
   1,  33, 0x3E, 0xFF, 0x00, "HP_Output_Drivers_POP_Removal_Settings",  0x3E, 
   1,  34, 0x00, 0xFF, 0x00, "Output_Driver_PGA_Ramp-Down_Period_Ctrl", 0x00, 
   1,  35, 0x00, 0xFF, 0x00, "LnRDAC_Out",                              0x00,
   1,  36, 0x7F, 0xFF, 0x00, "Left_Analog_Vol_to_HPL",                  0x7F, 
   1,  37, 0x7F, 0xFF, 0x00, "Right_Analog_Vol_to_HPR",                 0x7F,
   1,  38, 0x7F, 0xFF, 0x00, "LAnlg_V_SPL",                             0x7F,
   1,  39, 0x7F, 0xFF, 0x00, "RAnlg_V_SPR",                             0x7F,
   1,  40, 0x02, 0x7E, 0x02, "HPL_Driver",                              0x02,
   1,  41, 0x02, 0x7E, 0x02, "HPR_Driver",                              0x02, 
   1,  42, 0x00, 0x1D, 0x00, "SPK_Driver",                              0x00,
   1,  44, 0x20, 0xFF, 0x00, "HP_Driver_Control",                       0x20, // use actual: 0x20 not 0x00 (debounce 8usec)
   1,  46, 0x00, 0x8B, 0x00, "mic_bias",                                0x00,
   1,  47, 0x80, 0xFF, 0x00, "mic_gain",                                0x80,
   1,  48, 0x00, 0xFC, 0x00, "adc_in_p",                             		0x00,
   1,  49, 0x00, 0xF0, 0x00, "adc_in_m",                             		0x00,
   1,  50, 0x00, 0xE0, 0x00, "adc_in_cm",                               0x00,
   1,  51, 0x00, 0xFD, 0x00, "GPIO1_InOut_Pin_Ctrl",                    0x00,
#ifdef PG1_XTRAS
#endif
#ifdef PG3_7_XTRAS
// 3,   0, 0x00, 0x00, "Page_Select_Register",
// 3,  16, 0x81, 0xFF, "Timer_Clock_MCLK_Divider",
// 4,   0, 0x00, 0x00, "Page_Select_Register",
// 4,   2, 0x01, 0xFF, "C1_MSB",
// 4,   3, 0x17, 0xFF, "C1_LSB",
// 4,   4, 0x01, 0xFF, "C2_MSB",
// 4,   5, 0x17, 0xFF, "C2_LSB",
// 4,   6, 0x7D, 0xFF, "C3_MSB",
// 4,   7, 0xD3, 0xFF, "C3_LSB",
// 4,   8, 0x7F, 0xFF, "C4_MSB",
// 4,   9, 0xFF, 0xFF, "C4_LSB",
// 4,  10, 0x00, 0xFF, "C5_MSB",
// 4,  11, 0x00, 0xFF, "C5_LSB",
// 4,  12, 0x00, 0xFF, "C6_MSB",
// 4,  13, 0x00, 0xFF, "C6_LSB",
// 4,  14, 0x7F, 0xFF, "C7_MSB",
// 4,  15, 0xFF, 0xFF, "C7_LSB",
// 4,  16, 0x00, 0xFF, "C8_MSB",
// 4,  17, 0x00, 0xFF, "C8_LSB",
// 4,  18, 0x00, 0xFF, "C9_MSB",
// 4,  19, 0x00, 0xFF, "C9_LSB",
// 4,  20, 0x00, 0xFF, "C10_MSB",
// 4,  21, 0x00, 0xFF, "C10_LSB",
// 4,  22, 0x00, 0xFF, "C11_MSB",
// 4,  23, 0x00, 0xFF, "C11_LSB",
// 4,  24, 0x7F, 0xFF, "C12_MSB",
// 4,  25, 0xFF, 0xFF, "C12_LSB",
// 4,  26, 0x00, 0xFF, "C13_MSB",
// 4,  27, 0x00, 0xFF, "C13_LSB",
// 4,  28, 0x00, 0xFF, "C14_MSB",
// 4,  29, 0x00, 0xFF, "C14_LSB",
// 4,  30, 0x00, 0xFF, "C15_MSB",
// 4,  31, 0x00, 0xFF, "C15_LSB",
// 4,  32, 0x00, 0xFF, "C16_MSB",
// 4,  33, 0x00, 0xFF, "C16_LSB",
// 4,  34, 0x7F, 0xFF, "C17_MSB",
// 4,  35, 0xFF, 0xFF, "C17_LSB",
// 4,  36, 0x00, 0xFF, "C18_MSB",
// 4,  37, 0x00, 0xFF, "C18_LSB",
// 4,  38, 0x00, 0xFF, "C19_MSB",
// 4,  39, 0x00, 0xFF, "C19_LSB",
// 4,  40, 0x00, 0xFF, "C20_MSB",
// 4,  41, 0x00, 0xFF, "C20_LSB",
// 4,  42, 0x00, 0xFF, "C21_MSB",
// 4,  43, 0x00, 0xFF, "C21_LSB",
// 4,  44, 0x7F, 0xFF, "C22_MSB",
// 4,  45, 0xFF, 0xFF, "C22_LSB",
// 4,  46, 0x00, 0xFF, "C23_MSB",
// 4,  47, 0x00, 0xFF, "C23_LSB",
// 4,  48, 0x00, 0xFF, "C24_MSB",
// 4,  49, 0x00, 0xFF, "C24_LSB",
// 4,  50, 0x00, 0xFF, "C25_MSB",
// 4,  51, 0x00, 0xFF, "C25_LSB",
// 4,  52, 0x00, 0xFF, "C26_MSB",
// 4,  53, 0x00, 0xFF, "C26_LSB",
// 4,  54, 0x7F, 0xFF, "C27_MSB",
// 4,  55, 0xFF, 0xFF, "C27_LSB",
// 4,  56, 0x00, 0xFF, "C28_MSB",
// 4,  57, 0x00, 0xFF, "C28_LSB",
// 4,  58, 0x00, 0xFF, "C29_MSB",
// 4,  59, 0x00, 0xFF, "C29_LSB",
// 4,  60, 0x00, 0xFF, "C30_MSB",
// 4,  61, 0x00, 0xFF, "C30_LSB",
// 4,  62, 0x00, 0xFF, "C31_MSB",
// 4,  63, 0x00, 0xFF, "C31_LSB",
// 4,  64, 0x00, 0xFF, "C32_MSB",
// 4,  65, 0x00, 0xFF, "C32_LSB",
// 4,  66, 0x00, 0xFF, "C33_MSB",
// 4,  67, 0x00, 0xFF, "C33_LSB",
// 4,  68, 0x00, 0xFF, "C34_MSB",
// 4,  69, 0x00, 0xFF, "C34_LSB",
// 4,  70, 0x00, 0xFF, "C35_MSB",
// 4,  71, 0x00, 0xFF, "C35_LSB",
// 4,  72, 0x7F, 0xFF, "C36_MSB",
// 4,  73, 0xFF, 0xFF, "C36_LSB",
// 4,  74, 0x00, 0xFF, "C37_MSB",
// 4,  75, 0x00, 0xFF, "C37_LSB",
// 4,  76, 0x00, 0xFF, "C38_MSB",
// 4,  77, 0x00, 0xFF, "C38_LSB",
// 4,  78, 0x7F, 0xFF, "C39_MSB",
// 4,  79, 0xFF, 0xFF, "C39_LSB",
// 4,  80, 0x00, 0xFF, "C40_MSB",
// 4,  81, 0x00, 0xFF, "C40_LSB",
// 4,  82, 0x00, 0xFF, "C41_MSB",
// 4,  83, 0x00, 0xFF, "C41_LSB",
// 4,  84, 0x00, 0xFF, "C42_MSB",
// 4,  85, 0x00, 0xFF, "C42_LSB",
// 4,  86, 0x00, 0xFF, "C43_MSB",
// 4,  87, 0x00, 0xFF, "C43_LSB",
// 4,  88, 0x7F, 0xFF, "C44_MSB",
// 4,  89, 0xFF, 0xFF, "C44_LSB",
// 4,  90, 0x00, 0xFF, "C45_MSB",
// 4,  91, 0x00, 0xFF, "C45_LSB",
// 4,  92, 0x00, 0xFF, "C46_MSB",
// 4,  93, 0x00, 0xFF, "C46_LSB",
// 4,  94, 0x00, 0xFF, "C47_MSB",
// 4,  95, 0x00, 0xFF, "C47_LSB",
// 4,  96, 0x00, 0xFF, "C48_MSB",
// 4,  97, 0x00, 0xFF, "C48_LSB",
// 4,  98, 0x7F, 0xFF, "C49_MSB",
// 4,  99, 0xFF, 0xFF, "C49_LSB",
// 4, 100, 0x00, 0xFF, "C50_MSB",
// 4, 101, 0x00, 0xFF, "C50_LSB",
// 4, 102, 0x00, 0xFF, "C51_MSB",
// 4, 103, 0x00, 0xFF, "C51_LSB",
// 4, 104, 0x00, 0xFF, "C52_MSB",
// 4, 105, 0x00, 0xFF, "C52_LSB",
// 4, 106, 0x00, 0xFF, "C53_MSB",
// 4, 107, 0x00, 0xFF, "C53_LSB",
// 4, 108, 0x7F, 0xFF, "C54_MSB",
// 4, 109, 0xFF, 0xFF, "C54_LSB",
// 4, 110, 0x00, 0xFF, "C55_MSB",
// 4, 111, 0x00, 0xFF, "C55_LSB",
// 4, 112, 0x00, 0xFF, "C56_MSB",
// 4, 113, 0x00, 0xFF, "C56_LSB",
// 4, 114, 0x00, 0xFF, "C57_MSB",
// 4, 115, 0x00, 0xFF, "C57_LSB",
// 4, 116, 0x00, 0xFF, "C58_MSB",
// 4, 117, 0x00, 0xFF, "C58_LSB",
// 4, 118, 0x7F, 0xFF, "C59_MSB",
// 4, 119, 0xFF, 0xFF, "C59_LSB",
// 4, 120, 0x00, 0xFF, "C60_MSB",
// 4, 121, 0x00, 0xFF, "C60_LSB",
// 4, 122, 0x00, 0xFF, "C61_MSB",
// 4, 123, 0x00, 0xFF, "C61_LSB",
// 4, 124, 0x00, 0xFF, "C62_MSB",
// 4, 125, 0x00, 0xFF, "C62_LSB",
// 4, 126, 0x00, 0xFF, "C63_MSB",
// 4, 127, 0x00, 0xFF, "C63_LSB",
// 5,   0, 0x00, 0x00, "Page_Select_Register",
// 5,   2, 0x00, 0xFF, "C65_MSB",
// 5,   3, 0x00, 0xFF, "C65_LSB",
// 5,   4, 0x00, 0xFF, "C66_MSB",
// 5,   5, 0x00, 0xFF, "C66_LSB",
// 5,   6, 0x00, 0xFF, "C67_MSB",
// 5,   7, 0x00, 0xFF, "C67_LSB",
// 5,   8, 0x00, 0xFF, "C68_MSB",
// 5,   9, 0x00, 0xFF, "C68_LSB",
// 5,  10, 0x00, 0xFF, "C69_MSB",
// 5,  11, 0x00, 0xFF, "C69_LSB",
// 5,  12, 0x00, 0xFF, "C70_MSB",
// 5,  13, 0x00, 0xFF, "C70_LSB",
// 5,  14, 0x00, 0xFF, "C71_MSB",
// 5,  15, 0x00, 0xFF, "C71_LSB",
// 5,  16, 0x00, 0xFF, "C72_MSB",
// 5,  17, 0x00, 0xFF, "C72_LSB",
// 5,  18, 0x00, 0xFF, "C73_MSB",
// 5,  19, 0x00, 0xFF, "C73_LSB",
// 5,  20, 0x00, 0xFF, "C74_MSB",
// 5,  21, 0x00, 0xFF, "C74_LSB",
// 5,  22, 0x00, 0xFF, "C75_MSB",
// 5,  23, 0x00, 0xFF, "C75_LSB",
// 5,  24, 0x00, 0xFF, "C76_MSB",
// 5,  25, 0x00, 0xFF, "C76_LSB",
// 5,  26, 0x00, 0xFF, "C77_MSB",
// 5,  27, 0x00, 0xFF, "C77_LSB",
// 5,  28, 0x00, 0xFF, "C78_MSB",
// 5,  29, 0x00, 0xFF, "C78_LSB",
// 5,  30, 0x00, 0xFF, "C79_MSB",
// 5,  31, 0x00, 0xFF, "C79_LSB",
// 5,  32, 0x00, 0xFF, "C80_MSB",
// 5,  33, 0x00, 0xFF, "C80_LSB",
// 5,  34, 0x00, 0xFF, "C81_MSB",
// 5,  35, 0x00, 0xFF, "C81_LSB",
// 5,  36, 0x00, 0xFF, "C82_MSB",
// 5,  37, 0x00, 0xFF, "C82_LSB",
// 5,  38, 0x00, 0xFF, "C83_MSB",
// 5,  39, 0x00, 0xFF, "C83_LSB",
// 5,  40, 0x00, 0xFF, "C84_MSB",
// 5,  41, 0x00, 0xFF, "C84_LSB",
// 5,  42, 0x00, 0xFF, "C85_MSB",
// 5,  43, 0x00, 0xFF, "C85_LSB",
// 5,  44, 0x00, 0xFF, "C86_MSB",
// 5,  45, 0x00, 0xFF, "C86_LSB",
// 5,  46, 0x00, 0xFF, "C87_MSB",
// 5,  47, 0x00, 0xFF, "C87_LSB",
// 5,  48, 0x00, 0xFF, "C88_MSB",
// 5,  49, 0x00, 0xFF, "C88_LSB",
// 5,  50, 0x00, 0xFF, "C89_MSB",
// 5,  51, 0x00, 0xFF, "C89_LSB",
// 5,  52, 0x00, 0xFF, "C90_MSB",
// 5,  53, 0x00, 0xFF, "C90_LSB",
// 5,  54, 0x00, 0xFF, "C91_MSB",
// 5,  55, 0x00, 0xFF, "C91_LSB",
// 5,  56, 0x00, 0xFF, "C92_MSB",
// 5,  57, 0x00, 0xFF, "C92_LSB",
// 5,  58, 0x00, 0xFF, "C93_MSB",
// 5,  59, 0x00, 0xFF, "C93_LSB",
// 5,  60, 0x00, 0xFF, "C94_MSB",
// 5,  61, 0x00, 0xFF, "C94_LSB",
// 5,  62, 0x00, 0xFF, "C95_MSB",
// 5,  63, 0x00, 0xFF, "C95_LSB",
// 5,  64, 0x00, 0xFF, "C96_MSB",
// 5,  65, 0x00, 0xFF, "C96_LSB",
// 5,  66, 0x00, 0xFF, "C97_MSB",
// 5,  67, 0x00, 0xFF, "C97_LSB",
// 5,  68, 0x00, 0xFF, "C98_MSB",
// 5,  69, 0x00, 0xFF, "C98_LSB",
// 5,  70, 0x00, 0xFF, "C99_MSB",
// 5,  71, 0x00, 0xFF, "C99_LSB",
// 5,  72, 0x00, 0xFF, "C100_MSB",
// 5,  73, 0x00, 0xFF, "C100_LSB",
// 5,  74, 0x00, 0xFF, "C101_MSB",
// 5,  75, 0x00, 0xFF, "C101_LSB",
// 5,  76, 0x00, 0xFF, "C102_MSB",
// 5,  77, 0x00, 0xFF, "C102_LSB",
// 5,  78, 0x00, 0xFF, "C103_MSB",
// 5,  79, 0x00, 0xFF, "C103_LSB",
// 5,  80, 0x00, 0xFF, "C104_MSB",
// 5,  81, 0x00, 0xFF, "C104_LSB",
// 5,  82, 0x00, 0xFF, "C105_MSB",
// 5,  83, 0x00, 0xFF, "C105_LSB",
// 5,  84, 0x00, 0xFF, "C106_MSB",
// 5,  85, 0x00, 0xFF, "C106_LSB",
// 5,  86, 0x00, 0xFF, "C107_MSB",
// 5,  87, 0x00, 0xFF, "C107_LSB",
// 5,  88, 0x00, 0xFF, "C108_MSB",
// 5,  89, 0x00, 0xFF, "C108_LSB",
// 5,  90, 0x00, 0xFF, "C109_MSB",
// 5,  91, 0x00, 0xFF, "C109_LSB",
// 5,  92, 0x00, 0xFF, "C110_MSB",
// 5,  93, 0x00, 0xFF, "C110_LSB",
// 5,  94, 0x00, 0xFF, "C111_MSB",
// 5,  95, 0x00, 0xFF, "C111_LSB",
// 5,  96, 0x00, 0xFF, "C112_MSB",
// 5,  97, 0x00, 0xFF, "C112_LSB",
// 5,  98, 0x00, 0xFF, "C113_MSB",
// 5,  99, 0x00, 0xFF, "C113_LSB",
// 5, 100, 0x00, 0xFF, "C114_MSB",
// 5, 101, 0x00, 0xFF, "C114_LSB",
// 5, 102, 0x00, 0xFF, "C115_MSB",
// 5, 103, 0x00, 0xFF, "C115_LSB",
// 5, 104, 0x00, 0xFF, "C116_MSB",
// 5, 105, 0x00, 0xFF, "C116_LSB",
// 5, 106, 0x00, 0xFF, "C117_MSB",
// 5, 107, 0x00, 0xFF, "C117_LSB",
// 5, 108, 0x00, 0xFF, "C118_MSB",
// 5, 109, 0x00, 0xFF, "C118_LSB",
// 5, 110, 0x00, 0xFF, "C119_MSB",
// 5, 111, 0x00, 0xFF, "C119_LSB",
// 5, 112, 0x00, 0xFF, "C120_MSB",
// 5, 113, 0x00, 0xFF, "C120_LSB",
// 5, 114, 0x00, 0xFF, "C121_MSB",
// 5, 115, 0x00, 0xFF, "C121_LSB",
// 5, 116, 0x00, 0xFF, "C122_MSB",
// 5, 117, 0x00, 0xFF, "C122_LSB",
// 5, 118, 0x00, 0xFF, "C123_MSB",
// 5, 119, 0x00, 0xFF, "C123_LSB",
// 5, 120, 0x00, 0xFF, "C124_MSB",
// 5, 121, 0x00, 0xFF, "C124_LSB",
// 5, 122, 0x00, 0xFF, "C125_MSB",
// 5, 123, 0x00, 0xFF, "C125_LSB",
// 5, 124, 0x00, 0xFF, "C126_MSB",
// 5, 125, 0x00, 0xFF, "C126_LSB",
// 5, 126, 0x00, 0xFF, "C127_MSB",
// 5, 127, 0x00, 0xFF, "C127_LSB",
#endif
//********** AIC3100 PAGE 8
// pg,reg, reset, can1, must1,  "nm",																		curr, 
   8,   0, 0x08, 0x00, 0x00, "Page_Select_Register",                    0x08,  // 8:0 reads as 0x08
   8,   1, 0x00, 0x05, 0x00, "DAC_Coefficient_RAM_Control",             0x00,
#ifdef PG8_15_XTRAS
// 8,   2, 0x7F, 0xFF, "C1_MSB",
// 8,   3, 0xFF, 0xFF, "C1_LSB",
// 8,   4, 0x00, 0xFF, "C2_MSB",
// 8,   5, 0x00, 0xFF, "C2_LSB",
// 8,   6, 0x00, 0xFF, "C3_MSB",
// 8,   7, 0x00, 0xFF, "C3_LSB",
// 8,   8, 0x00, 0xFF, "C4_MSB",
// 8,   9, 0x00, 0xFF, "C4_LSB",
// 8,  10, 0x00, 0xFF, "C5_MSB",
// 8,  11, 0x00, 0xFF, "C5_LSB",
// 8,  12, 0x7F, 0xFF, "C6_MSB",
// 8,  13, 0xFF, 0xFF, "C6_LSB",
// 8,  14, 0x00, 0xFF, "C7_MSB",
// 8,  15, 0x00, 0xFF, "C7_LSB",
// 8,  16, 0x00, 0xFF, "C8_MSB",
// 8,  17, 0x00, 0xFF, "C8_LSB",
// 8,  18, 0x00, 0xFF, "C9_MSB",
// 8,  19, 0x00, 0xFF, "C9_LSB",
// 8,  20, 0x00, 0xFF, "C10_MSB",
// 8,  21, 0x00, 0xFF, "C10_LSB",
// 8,  22, 0x7F, 0xFF, "C11_MSB",
// 8,  23, 0xFF, 0xFF, "C11_LSB",
// 8,  24, 0x00, 0xFF, "C12_MSB",
// 8,  25, 0x00, 0xFF, "C12_LSB",
// 8,  26, 0x00, 0xFF, "C13_MSB",
// 8,  27, 0x00, 0xFF, "C13_LSB",
// 8,  28, 0x00, 0xFF, "C14_MSB",
// 8,  29, 0x00, 0xFF, "C14_LSB",
// 8,  30, 0x00, 0xFF, "C15_MSB",
// 8,  31, 0x00, 0xFF, "C15_LSB",
// 8,  32, 0x7F, 0xFF, "C16_MSB",
// 8,  33, 0xFF, 0xFF, "C16_LSB",
// 8,  34, 0x00, 0xFF, "C17_MSB",
// 8,  35, 0x00, 0xFF, "C17_LSB",
// 8,  36, 0x00, 0xFF, "C18_MSB",
// 8,  37, 0x00, 0xFF, "C18_LSB",
// 8,  38, 0x00, 0xFF, "C19_MSB",
// 8,  39, 0x00, 0xFF, "C19_LSB",
// 8,  40, 0x00, 0xFF, "C20_MSB",
// 8,  41, 0x00, 0xFF, "C20_LSB",
// 8,  42, 0x7F, 0xFF, "C21_MSB",
// 8,  43, 0xFF, 0xFF, "C21_LSB",
// 8,  44, 0x00, 0xFF, "C22_MSB",
// 8,  45, 0x00, 0xFF, "C22_LSB",
// 8,  46, 0x00, 0xFF, "C23_MSB",
// 8,  47, 0x00, 0xFF, "C23_LSB",
// 8,  48, 0x00, 0xFF, "C24_MSB",
// 8,  49, 0x00, 0xFF, "C24_LSB",
// 8,  50, 0x00, 0xFF, "C25_MSB",
// 8,  51, 0x00, 0xFF, "C25_LSB",
// 8,  52, 0x7F, 0xFF, "C26_MSB",
// 8,  53, 0xFF, 0xFF, "C26_LSB",
// 8,  54, 0x00, 0xFF, "C27_MSB",
// 8,  55, 0x00, 0xFF, "C27_LSB",
// 8,  56, 0x00, 0xFF, "C28_MSB",
// 8,  57, 0x00, 0xFF, "C28_LSB",
// 8,  58, 0x00, 0xFF, "C29_MSB",
// 8,  59, 0x00, 0xFF, "C29_LSB",
// 8,  60, 0x00, 0xFF, "C30_MSB",
// 8,  61, 0x00, 0xFF, "C30_LSB",
// 8,  62, 0x00, 0xFF, "C31_MSB",
// 8,  63, 0x00, 0xFF, "C31_LSB",
// 8,  64, 0x00, 0xFF, "C32_MSB",
// 8,  65, 0x00, 0xFF, "C32_LSB",
// 8,  66, 0x7F, 0xFF, "C33_MSB",
// 8,  67, 0xFF, 0xFF, "C33_LSB",
// 8,  68, 0x00, 0xFF, "C34_MSB",
// 8,  69, 0x00, 0xFF, "C34_LSB",
// 8,  70, 0x00, 0xFF, "C35_MSB",
// 8,  71, 0x00, 0xFF, "C35_LSB",
// 8,  72, 0x00, 0xFF, "C36_MSB",
// 8,  73, 0x00, 0xFF, "C36_LSB",
// 8,  74, 0x00, 0xFF, "C37_MSB",
// 8,  75, 0x00, 0xFF, "C37_LSB",
// 8,  76, 0x7F, 0xFF, "C38_MSB",
// 8,  77, 0xFF, 0xFF, "C38_LSB",
// 8,  78, 0x00, 0xFF, "C39_MSB",
// 8,  79, 0x00, 0xFF, "C39_LSB",
// 8,  80, 0x00, 0xFF, "C40_MSB",
// 8,  81, 0x00, 0xFF, "C40_LSB",
// 8,  82, 0x00, 0xFF, "C41_MSB",
// 8,  83, 0x00, 0xFF, "C41_LSB",
// 8,  84, 0x00, 0xFF, "C42_MSB",
// 8,  85, 0x00, 0xFF, "C42_LSB",
// 8,  86, 0x7F, 0xFF, "C43_MSB",
// 8,  87, 0xFF, 0xFF, "C43_LSB",
// 8,  88, 0x00, 0xFF, "C44_MSB",
// 8,  89, 0x00, 0xFF, "C44_LSB",
// 8,  90, 0x00, 0xFF, "C45_MSB",
// 8,  91, 0x00, 0xFF, "C45_LSB",
// 8,  92, 0x00, 0xFF, "C46_MSB",
// 8,  93, 0x00, 0xFF, "C46_LSB",
// 8,  94, 0x00, 0xFF, "C47_MSB",
// 8,  95, 0x00, 0xFF, "C47_LSB",
// 8,  96, 0x7F, 0xFF, "C48_MSB",
// 8,  97, 0xFF, 0xFF, "C48_LSB",
// 8,  98, 0x00, 0xFF, "C49_MSB",
// 8,  99, 0x00, 0xFF, "C49_LSB",
// 8, 100, 0x00, 0xFF, "C50_MSB",
// 8, 101, 0x00, 0xFF, "C50_LSB",
// 8, 102, 0x00, 0xFF, "C51_MSB",
// 8, 103, 0x00, 0xFF, "C51_LSB",
// 8, 104, 0x00, 0xFF, "C52_MSB",
// 8, 105, 0x00, 0xFF, "C52_LSB",
// 8, 106, 0x7F, 0xFF, "C53_MSB",
// 8, 107, 0xFF, 0xFF, "C53_LSB",
// 8, 108, 0x00, 0xFF, "C54_MSB",
// 8, 109, 0x00, 0xFF, "C54_LSB",
// 8, 110, 0x00, 0xFF, "C55_MSB",
// 8, 111, 0x00, 0xFF, "C55_LSB",
// 8, 112, 0x00, 0xFF, "C56_MSB",
// 8, 113, 0x00, 0xFF, "C56_LSB",
// 8, 114, 0x00, 0xFF, "C57_MSB",
// 8, 115, 0x00, 0xFF, "C57_LSB",
// 8, 116, 0x7F, 0xFF, "C58_MSB",
// 8, 117, 0xFF, 0xFF, "C58_LSB",
// 8, 118, 0x00, 0xFF, "C59_MSB",
// 8, 119, 0x00, 0xFF, "C59_LSB",
// 8, 120, 0x00, 0xFF, "C60_MSB",
// 8, 121, 0x00, 0xFF, "C60_LSB",
// 8, 122, 0x00, 0xFF, "C61_MSB",
// 8, 123, 0x00, 0xFF, "C61_LSB",
// 8, 124, 0x00, 0xFF, "C62_MSB",
// 8, 125, 0x00, 0xFF, "C62_LSB",
// 8, 126, 0x00, 0xFF, "C63_MSB",
// 8, 127, 0x00, 0xFF, "C63_LSB",
// 9,   0, 0x00, 0x00, "Page_Select_Register",
// 9,   2, 0x7F, 0xFF, "C65_MSB",
// 9,   3, 0xFF, 0xFF, "C65_LSB",
// 9,   4, 0x00, 0xFF, "C66_MSB",
// 9,   5, 0x00, 0xFF, "C66_LSB",
// 9,   6, 0x00, 0xFF, "C67_MSB",
// 9,   7, 0x00, 0xFF, "C67_LSB",
// 9,   8, 0x7F, 0xFF, "C68_MSB",
// 9,   9, 0xFF, 0xFF, "C68_LSB",
// 9,  10, 0x00, 0xFF, "C69_MSB",
// 9,  11, 0x00, 0xFF, "C69_LSB",
// 9,  12, 0x00, 0xFF, "C70_MSB",
// 9,  13, 0x00, 0xFF, "C70_LSB",
// 9,  14, 0x7F, 0xFF, "C71_MSB",
// 9,  15, 0xF7, 0xFF, "C71_LSB",
// 9,  16, 0x80, 0xFF, "C72_MSB",
// 9,  17, 0x09, 0xFF, "C72_LSB",
// 9,  18, 0x7F, 0xFF, "C73_MSB",
// 9,  19, 0xEF, 0xFF, "C73_LSB",
// 9,  20, 0x00, 0xFF, "C74_MSB",
// 9,  21, 0x11, 0xFF, "C74_LSB",
// 9,  22, 0x00, 0xFF, "C75_MSB",
// 9,  23, 0x11, 0xFF, "C75_LSB",
// 9,  24, 0x7F, 0xFF, "C76_MSB",
// 9,  25, 0xDE, 0xFF, "C76_LSB",
// 9,  26, 0x00, 0xFF, "C77_MSB",
// 9,  27, 0x00, 0xFF, "C77_LSB",
// 9,  28, 0x00, 0xFF, "C78_MSB",
// 9,  29, 0x00, 0xFF, "C78_LSB",
// 9,  30, 0x00, 0xFF, "C79_MSB",
// 9,  31, 0x00, 0xFF, "C79_LSB",
// 9,  32, 0x00, 0xFF, "C80_MSB",
// 9,  33, 0x00, 0xFF, "C80_LSB",
// 9,  34, 0x00, 0xFF, "C81_MSB",
// 9,  35, 0x00, 0xFF, "C81_LSB",
// 9,  36, 0x00, 0xFF, "C82_MSB",
// 9,  37, 0x00, 0xFF, "C82_LSB",
// 9,  38, 0x00, 0xFF, "C83_MSB",
// 9,  39, 0x00, 0xFF, "C83_LSB",
// 9,  40, 0x00, 0xFF, "C84_MSB",
// 9,  41, 0x00, 0xFF, "C84_LSB",
// 9,  42, 0x00, 0xFF, "C85_MSB",
// 9,  43, 0x00, 0xFF, "C85_LSB",
// 9,  44, 0x00, 0xFF, "C86_MSB",
// 9,  45, 0x00, 0xFF, "C86_LSB",
// 9,  46, 0x00, 0xFF, "C87_MSB",
// 9,  47, 0x00, 0xFF, "C87_LSB",
// 9,  48, 0x00, 0xFF, "C88_MSB",
// 9,  49, 0x00, 0xFF, "C88_LSB",
// 9,  50, 0x00, 0xFF, "C89_MSB",
// 9,  51, 0x00, 0xFF, "C89_LSB",
// 9,  52, 0x00, 0xFF, "C90_MSB",
// 9,  53, 0x00, 0xFF, "C90_LSB",
// 9,  54, 0x00, 0xFF, "C91_MSB",
// 9,  55, 0x00, 0xFF, "C91_LSB",
// 9,  56, 0x00, 0xFF, "C92_MSB",
// 9,  57, 0x00, 0xFF, "C92_LSB",
// 9,  58, 0x00, 0xFF, "C93_MSB",
// 9,  59, 0x00, 0xFF, "C93_LSB",
// 9,  60, 0x00, 0xFF, "C94_MSB",
// 9,  61, 0x00, 0xFF, "C94_LSB",
// 9,  62, 0x00, 0xFF, "C95_MSB",
// 9,  63, 0x00, 0xFF, "C95_LSB",
// 9,  64, 0x00, 0xFF, "C96_MSB",
// 9,  65, 0x00, 0xFF, "C96_LSB",
// 9,  66, 0x00, 0xFF, "C97_MSB",
// 9,  67, 0x00, 0xFF, "C97_LSB",
// 9,  68, 0x00, 0xFF, "C98_MSB",
// 9,  69, 0x00, 0xFF, "C98_LSB",
// 9,  70, 0x00, 0xFF, "C99_MSB",
// 9,  71, 0x00, 0xFF, "C99_LSB",
// 9,  72, 0x00, 0xFF, "C100_MSB",
// 9,  73, 0x00, 0xFF, "C100_LSB",
// 9,  74, 0x00, 0xFF, "C101_MSB",
// 9,  75, 0x00, 0xFF, "C101_LSB",
// 9,  76, 0x00, 0xFF, "C102_MSB",
// 9,  77, 0x00, 0xFF, "C102_LSB",
// 9,  78, 0x00, 0xFF, "C103_MSB",
// 9,  79, 0x00, 0xFF, "C103_LSB",
// 9,  80, 0x00, 0xFF, "C104_MSB",
// 9,  81, 0x00, 0xFF, "C104_LSB",
// 9,  82, 0x00, 0xFF, "C105_MSB",
// 9,  83, 0x00, 0xFF, "C105_LSB",
// 9,  84, 0x00, 0xFF, "C106_MSB",
// 9,  85, 0x00, 0xFF, "C106_LSB",
// 9,  86, 0x00, 0xFF, "C107_MSB",
// 9,  87, 0x00, 0xFF, "C107_LSB",
// 9,  88, 0x00, 0xFF, "C108_MSB",
// 9,  89, 0x00, 0xFF, "C108_LSB",
// 9,  90, 0x00, 0xFF, "C109_MSB",
// 9,  91, 0x00, 0xFF, "C109_LSB",
// 9,  92, 0x00, 0xFF, "C110_MSB",
// 9,  93, 0x00, 0xFF, "C110_LSB",
// 9,  94, 0x00, 0xFF, "C111_MSB",
// 9,  95, 0x00, 0xFF, "C111_LSB",
// 9,  96, 0x00, 0xFF, "C112_MSB",
// 9,  97, 0x00, 0xFF, "C112_LSB",
// 9,  98, 0x00, 0xFF, "C113_MSB",
// 9,  99, 0x00, 0xFF, "C113_LSB",
// 9, 100, 0x00, 0xFF, "C114_MSB",
// 9, 101, 0x00, 0xFF, "C114_LSB",
// 9, 102, 0x00, 0xFF, "C115_MSB",
// 9, 103, 0x00, 0xFF, "C115_LSB",
// 9, 104, 0x00, 0xFF, "C116_MSB",
// 9, 105, 0x00, 0xFF, "C116_LSB",
// 9, 106, 0x00, 0xFF, "C117_MSB",
// 9, 107, 0x00, 0xFF, "C117_LSB",
// 9, 108, 0x00, 0xFF, "C118_MSB",
// 9, 109, 0x00, 0xFF, "C118_LSB",
// 9, 110, 0x00, 0xFF, "C119_MSB",
// 9, 111, 0x00, 0xFF, "C119_LSB",
// 9, 112, 0x00, 0xFF, "C120_MSB",
// 9, 113, 0x00, 0xFF, "C120_LSB",
// 9, 114, 0x00, 0xFF, "C121_MSB",
// 9, 115, 0x00, 0xFF, "C121_LSB",
// 9, 116, 0x00, 0xFF, "C122_MSB",
// 9, 117, 0x00, 0xFF, "C122_LSB",
// 9, 118, 0x00, 0xFF, "C123_MSB",
// 9, 119, 0x00, 0xFF, "C123_LSB",
// 9, 120, 0x00, 0xFF, "C124_MSB",
// 9, 121, 0x00, 0xFF, "C124_LSB",
// 9, 122, 0x00, 0xFF, "C125_MSB",
// 9, 123, 0x00, 0xFF, "C125_LSB",
// 9, 124, 0x00, 0xFF, "C126_MSB",
// 9, 125, 0x00, 0xFF, "C126_LSB",
// 9, 126, 0x00, 0xFF, "C127_MSB",
// 9, 127, 0x00, 0xFF, "C127_LSB",
// 10,   0, 0x00, 0x00, "Page_Select_Register",
// 10,   2, 0x7F, 0xFF, "C129_MSB",
// 10,   3, 0xFF, 0xFF, "C129_LSB",
// 10,   4, 0x00, 0xFF, "C130_MSB",
// 10,   5, 0x00, 0xFF, "C130_LSB",
// 10,   6, 0x00, 0xFF, "C131_MSB",
// 10,   7, 0x00, 0xFF, "C131_LSB",
// 10,   8, 0x7F, 0xFF, "C132_MSB",
// 10,   9, 0xFF, 0xFF, "C132_LSB",
// 10,  10, 0x00, 0xFF, "C133_MSB",
// 10,  11, 0x00, 0xFF, "C133_LSB",
// 10,  12, 0x00, 0xFF, "C134_MSB",
// 10,  13, 0x00, 0xFF, "C134_LSB",
// 10,  14, 0x7F, 0xFF, "C135_MSB",
// 10,  15, 0xF7, 0xFF, "C135_LSB",
// 10,  16, 0x80, 0xFF, "C136_MSB",
// 10,  17, 0x10, 0xFF, "C136_LSB",
// 10,  18, 0x7F, 0xFF, "C137_MSB",
// 10,  19, 0xEF, 0xFF, "C137_LSB",
// 10,  20, 0x00, 0xFF, "C138_MSB",
// 10,  21, 0x11, 0xFF, "C138_LSB",
// 10,  22, 0x00, 0xFF, "C139_MSB",
// 10,  23, 0x11, 0xFF, "C139_LSB",
// 10,  24, 0x7F, 0xFF, "C140_MSB",
// 10,  25, 0xDE, 0xFF, "C140_LSB",
// 10,  26, 0x00, 0xFF, "C141_MSB",
// 10,  27, 0x00, 0xFF, "C141_LSB",
// 10,  28, 0x00, 0xFF, "C142_MSB",
// 10,  29, 0x00, 0xFF, "C142_LSB",
// 10,  30, 0x00, 0xFF, "C143_MSB",
// 10,  31, 0x00, 0xFF, "C143_LSB",
// 10,  32, 0x00, 0xFF, "C144_MSB",
// 10,  33, 0x00, 0xFF, "C144_LSB",
// 10,  34, 0x00, 0xFF, "C145_MSB",
// 10,  35, 0x00, 0xFF, "C145_LSB",
// 10,  36, 0x00, 0xFF, "C146_MSB",
// 10,  37, 0x00, 0xFF, "C146_LSB",
// 10,  38, 0x00, 0xFF, "C147_MSB",
// 10,  39, 0x00, 0xFF, "C147_LSB",
// 10,  40, 0x00, 0xFF, "C148_MSB",
// 10,  41, 0x00, 0xFF, "C148_LSB",
// 10,  42, 0x00, 0xFF, "C149_MSB",
// 10,  43, 0x00, 0xFF, "C149_LSB",
// 10,  44, 0x00, 0xFF, "C150_MSB",
// 10,  45, 0x00, 0xFF, "C150_LSB",
// 10,  46, 0x00, 0xFF, "C151_MSB",
// 10,  47, 0x00, 0xFF, "C151_LSB",
// 10,  48, 0x00, 0xFF, "C152_MSB",
// 10,  49, 0x00, 0xFF, "C152_LSB",
// 10,  50, 0x00, 0xFF, "C153_MSB",
// 10,  51, 0x00, 0xFF, "C153_LSB",
// 10,  52, 0x00, 0xFF, "C154_MSB",
// 10,  53, 0x00, 0xFF, "C154_LSB",
// 10,  54, 0x00, 0xFF, "C155_MSB",
// 10,  55, 0x00, 0xFF, "C155_LSB",
// 10,  56, 0x00, 0xFF, "C156_MSB",
// 10,  57, 0x00, 0xFF, "C156_LSB",
// 10,  58, 0x00, 0xFF, "C157_MSB",
// 10,  59, 0x00, 0xFF, "C157_LSB",
// 10,  60, 0x00, 0xFF, "C158_MSB",
// 10,  61, 0x00, 0xFF, "C158_LSB",
// 10,  62, 0x00, 0xFF, "C159_MSB",
// 10,  63, 0x00, 0xFF, "C159_LSB",
// 10,  64, 0x00, 0xFF, "C160_MSB",
// 10,  65, 0x00, 0xFF, "C160_LSB",
// 10,  66, 0x00, 0xFF, "C161_MSB",
// 10,  67, 0x00, 0xFF, "C161_LSB",
// 10,  68, 0x00, 0xFF, "C162_MSB",
// 10,  69, 0x00, 0xFF, "C162_LSB",
// 10,  70, 0x00, 0xFF, "C163_MSB",
// 10,  71, 0x00, 0xFF, "C163_LSB",
// 10,  72, 0x00, 0xFF, "C164_MSB",
// 10,  73, 0x00, 0xFF, "C164_LSB",
// 10,  74, 0x00, 0xFF, "C165_MSB",
// 10,  75, 0x00, 0xFF, "C165_LSB",
// 10,  76, 0x00, 0xFF, "C166_MSB",
// 10,  77, 0x00, 0xFF, "C166_LSB",
// 10,  78, 0x00, 0xFF, "C167_MSB",
// 10,  79, 0x00, 0xFF, "C167_LSB",
// 10,  80, 0x00, 0xFF, "C168_MSB",
// 10,  81, 0x00, 0xFF, "C168_LSB",
// 10,  82, 0x00, 0xFF, "C169_MSB",
// 10,  83, 0x00, 0xFF, "C169_LSB",
// 10,  84, 0x00, 0xFF, "C170_MSB",
// 10,  85, 0x00, 0xFF, "C170_LSB",
// 10,  86, 0x00, 0xFF, "C171_MSB",
// 10,  87, 0x00, 0xFF, "C171_LSB",
// 10,  88, 0x00, 0xFF, "C172_MSB",
// 10,  89, 0x00, 0xFF, "C172_LSB",
// 10,  90, 0x00, 0xFF, "C173_MSB",
// 10,  91, 0x00, 0xFF, "C173_LSB",
// 10,  92, 0x00, 0xFF, "C174_MSB",
// 10,  93, 0x00, 0xFF, "C174_LSB",
// 10,  94, 0x00, 0xFF, "C175_MSB",
// 10,  95, 0x00, 0xFF, "C175_LSB",
// 10,  96, 0x00, 0xFF, "C176_MSB",
// 10,  97, 0x00, 0xFF, "C176_LSB",
// 10,  98, 0x00, 0xFF, "C177_MSB",
// 10,  99, 0x00, 0xFF, "C177_LSB",
// 10, 100, 0x00, 0xFF, "C178_MSB",
// 10, 101, 0x00, 0xFF, "C178_LSB",
// 10, 102, 0x00, 0xFF, "C179_MSB",
// 10, 103, 0x00, 0xFF, "C179_LSB",
// 10, 104, 0x00, 0xFF, "C180_MSB",
// 10, 105, 0x00, 0xFF, "C180_LSB",
// 10, 106, 0x00, 0xFF, "C181_MSB",
// 10, 107, 0x00, 0xFF, "C181_LSB",
// 10, 108, 0x00, 0xFF, "C182_MSB",
// 10, 109, 0x00, 0xFF, "C182_LSB",
// 10, 110, 0x00, 0xFF, "C183_MSB",
// 10, 111, 0x00, 0xFF, "C183_LSB",
// 10, 112, 0x00, 0xFF, "C184_MSB",
// 10, 113, 0x00, 0xFF, "C184_LSB",
// 10, 114, 0x00, 0xFF, "C185_MSB",
// 10, 115, 0x00, 0xFF, "C185_LSB",
// 10, 116, 0x00, 0xFF, "C186_MSB",
// 10, 117, 0x00, 0xFF, "C186_LSB",
// 10, 118, 0x00, 0xFF, "C187_MSB",
// 10, 119, 0x00, 0xFF, "C187_LSB",
// 10, 120, 0x00, 0xFF, "C188_MSB",
// 10, 121, 0x00, 0xFF, "C188_LSB",
// 10, 122, 0x00, 0xFF, "C189_MSB",
// 10, 123, 0x00, 0xFF, "C189_LSB",
// 10, 124, 0x00, 0xFF, "C190_MSB",
// 10, 125, 0x00, 0xFF, "C190_LSB",
// 10, 126, 0x00, 0xFF, "C191_MSB",
// 10, 127, 0x00, 0xFF, "C191_LSB",
// 11,   0, 0x00, 0x00, "Page_Select_Register",
// 11,   2, 0x00, 0xFF, "C193_MSB",
// 11,   3, 0x00, 0xFF, "C193_LSB",
// 11,   4, 0x00, 0xFF, "C194_MSB",
// 11,   5, 0x00, 0xFF, "C194_LSB",
// 11,   6, 0x00, 0xFF, "C195_MSB",
// 11,   7, 0x00, 0xFF, "C195_LSB",
// 11,   8, 0x00, 0xFF, "C196_MSB",
// 11,   9, 0x00, 0xFF, "C196_LSB",
// 11,  10, 0x00, 0xFF, "C197_MSB",
// 11,  11, 0x00, 0xFF, "C197_LSB",
// 11,  12, 0x00, 0xFF, "C198_MSB",
// 11,  13, 0x00, 0xFF, "C198_LSB",
// 11,  14, 0x00, 0xFF, "C199_MSB",
// 11,  15, 0x00, 0xFF, "C199_LSB",
// 11,  16, 0x00, 0xFF, "C200_MSB",
// 11,  17, 0x00, 0xFF, "C200_LSB",
// 11,  18, 0x00, 0xFF, "C201_MSB",
// 11,  19, 0x00, 0xFF, "C201_LSB",
// 11,  20, 0x00, 0xFF, "C202_MSB",
// 11,  21, 0x00, 0xFF, "C202_LSB",
// 11,  22, 0x00, 0xFF, "C203_MSB",
// 11,  23, 0x00, 0xFF, "C203_LSB",
// 11,  24, 0x00, 0xFF, "C204_MSB",
// 11,  25, 0x00, 0xFF, "C204_LSB",
// 11,  26, 0x00, 0xFF, "C205_MSB",
// 11,  27, 0x00, 0xFF, "C205_LSB",
// 11,  28, 0x00, 0xFF, "C206_MSB",
// 11,  29, 0x00, 0xFF, "C206_LSB",
// 11,  30, 0x00, 0xFF, "C207_MSB",
// 11,  31, 0x00, 0xFF, "C207_LSB",
// 11,  32, 0x00, 0xFF, "C208_MSB",
// 11,  33, 0x00, 0xFF, "C208_LSB",
// 11,  34, 0x00, 0xFF, "C209_MSB",
// 11,  35, 0x00, 0xFF, "C209_LSB",
// 11,  36, 0x00, 0xFF, "C210_MSB",
// 11,  37, 0x00, 0xFF, "C210_LSB",
// 11,  38, 0x00, 0xFF, "C211_MSB",
// 11,  39, 0x00, 0xFF, "C211_LSB",
// 11,  40, 0x00, 0xFF, "C212_MSB",
// 11,  41, 0x00, 0xFF, "C212_LSB",
// 11,  42, 0x00, 0xFF, "C213_MSB",
// 11,  43, 0x00, 0xFF, "C213_LSB",
// 11,  44, 0x00, 0xFF, "C214_MSB",
// 11,  45, 0x00, 0xFF, "C214_LSB",
// 11,  46, 0x00, 0xFF, "C215_MSB",
// 11,  47, 0x00, 0xFF, "C215_LSB",
// 11,  48, 0x00, 0xFF, "C216_MSB",
// 11,  49, 0x00, 0xFF, "C216_LSB",
// 11,  50, 0x00, 0xFF, "C217_MSB",
// 11,  51, 0x00, 0xFF, "C217_LSB",
// 11,  52, 0x00, 0xFF, "C218_MSB",
// 11,  53, 0x00, 0xFF, "C218_LSB",
// 11,  54, 0x00, 0xFF, "C219_MSB",
// 11,  55, 0x00, 0xFF, "C219_LSB",
// 11,  56, 0x00, 0xFF, "C220_MSB",
// 11,  57, 0x00, 0xFF, "C220_LSB",
// 11,  58, 0x00, 0xFF, "C221_MSB",
// 11,  59, 0x00, 0xFF, "C221_LSB",
// 11,  60, 0x00, 0xFF, "C222_MSB",
// 11,  61, 0x00, 0xFF, "C222_LSB",
// 11,  62, 0x00, 0xFF, "C223_MSB",
// 11,  63, 0x00, 0xFF, "C223_LSB",
// 11,  64, 0x00, 0xFF, "C224_MSB",
// 11,  65, 0x00, 0xFF, "C224_LSB",
// 11,  66, 0x00, 0xFF, "C225_MSB",
// 11,  67, 0x00, 0xFF, "C225_LSB",
// 11,  68, 0x00, 0xFF, "C226_MSB",
// 11,  69, 0x00, 0xFF, "C226_LSB",
// 11,  70, 0x00, 0xFF, "C227_MSB",
// 11,  71, 0x00, 0xFF, "C227_LSB",
// 11,  72, 0x00, 0xFF, "C228_MSB",
// 11,  73, 0x00, 0xFF, "C228_LSB",
// 11,  74, 0x00, 0xFF, "C229_MSB",
// 11,  75, 0x00, 0xFF, "C229_LSB",
// 11,  76, 0x00, 0xFF, "C230_MSB",
// 11,  77, 0x00, 0xFF, "C230_LSB",
// 11,  78, 0x00, 0xFF, "C231_MSB",
// 11,  79, 0x00, 0xFF, "C231_LSB",
// 11,  80, 0x00, 0xFF, "C232_MSB",
// 11,  81, 0x00, 0xFF, "C232_LSB",
// 11,  82, 0x00, 0xFF, "C233_MSB",
// 11,  83, 0x00, 0xFF, "C233_LSB",
// 11,  84, 0x00, 0xFF, "C234_MSB",
// 11,  85, 0x00, 0xFF, "C234_LSB",
// 11,  86, 0x00, 0xFF, "C235_MSB",
// 11,  87, 0x00, 0xFF, "C235_LSB",
// 11,  88, 0x00, 0xFF, "C236_MSB",
// 11,  89, 0x00, 0xFF, "C236_LSB",
// 11,  90, 0x00, 0xFF, "C237_MSB",
// 11,  91, 0x00, 0xFF, "C237_LSB",
// 11,  92, 0x00, 0xFF, "C238_MSB",
// 11,  93, 0x00, 0xFF, "C238_LSB",
// 11,  94, 0x00, 0xFF, "C239_MSB",
// 11,  95, 0x00, 0xFF, "C239_LSB",
// 11,  96, 0x00, 0xFF, "C240_MSB",
// 11,  97, 0x00, 0xFF, "C240_LSB",
// 11,  98, 0x00, 0xFF, "C241_MSB",
// 11,  99, 0x00, 0xFF, "C241_LSB",
// 11, 100, 0x00, 0xFF, "C242_MSB",
// 11, 101, 0x00, 0xFF, "C242_LSB",
// 11, 102, 0x00, 0xFF, "C243_MSB",
// 11, 103, 0x00, 0xFF, "C243_LSB",
// 11, 104, 0x00, 0xFF, "C244_MSB",
// 11, 105, 0x00, 0xFF, "C244_LSB",
// 11, 106, 0x00, 0xFF, "C245_MSB",
// 11, 107, 0x00, 0xFF, "C245_LSB",
// 11, 108, 0x00, 0xFF, "C246_MSB",
// 11, 109, 0x00, 0xFF, "C246_LSB",
// 11, 110, 0x00, 0xFF, "C247_MSB",
// 11, 111, 0x00, 0xFF, "C247_LSB",
// 11, 112, 0x00, 0xFF, "C248_MSB",
// 11, 113, 0x00, 0xFF, "C248_LSB",
// 11, 114, 0x00, 0xFF, "C249_MSB",
// 11, 115, 0x00, 0xFF, "C249_LSB",
// 11, 116, 0x00, 0xFF, "C250_MSB",
// 11, 117, 0x00, 0xFF, "C250_LSB",
// 11, 118, 0x00, 0xFF, "C251_MSB",
// 11, 119, 0x00, 0xFF, "C251_LSB",
// 11, 120, 0x00, 0xFF, "C252_MSB",
// 11, 121, 0x00, 0xFF, "C252_LSB",
// 11, 122, 0x00, 0xFF, "C253_MSB",
// 11, 123, 0x00, 0xFF, "C253_LSB",
// 11, 124, 0x00, 0xFF, "C254_MSB",
// 11, 125, 0x00, 0xFF, "C254_LSB",
// 11, 126, 0x00, 0xFF, "C255_MSB",
// 11, 127, 0x00, 0xFF, "C255_LSB",
// 12,   0, 0x00, 0x00, "Page_Select_Register",
// 12,   2, 0x7F, 0xFF, "C1_MSB",
// 12,   3, 0xFF, 0xFF, "C1_LSB",
// 12,   4, 0x00, 0xFF, "C2_MSB",
// 12,   5, 0x00, 0xFF, "C2_LSB",
// 12,   6, 0x00, 0xFF, "C3_MSB",
// 12,   7, 0x00, 0xFF, "C3_LSB",
// 12,   8, 0x00, 0xFF, "C4_MSB",
// 12,   9, 0x00, 0xFF, "C4_LSB",
// 12,  10, 0x00, 0xFF, "C5_MSB",
// 12,  11, 0x00, 0xFF, "C5_LSB",
// 12,  12, 0x7F, 0xFF, "C6_MSB",
// 12,  13, 0xFF, 0xFF, "C6_LSB",
// 12,  14, 0x00, 0xFF, "C7_MSB",
// 12,  15, 0x00, 0xFF, "C7_LSB",
// 12,  16, 0x00, 0xFF, "C8_MSB",
// 12,  17, 0x00, 0xFF, "C8_LSB",
// 12,  18, 0x00, 0xFF, "C9_MSB",
// 12,  19, 0x00, 0xFF, "C9_LSB",
// 12,  20, 0x00, 0xFF, "C10_MSB",
// 12,  21, 0x00, 0xFF, "C10_LSB",
// 12,  22, 0x7F, 0xFF, "C11_MSB",
// 12,  23, 0xFF, 0xFF, "C11_LSB",
// 12,  24, 0x00, 0xFF, "C12_MSB",
// 12,  25, 0x00, 0xFF, "C12_LSB",
// 12,  26, 0x00, 0xFF, "C13_MSB",
// 12,  27, 0x00, 0xFF, "C13_LSB",
// 12,  28, 0x00, 0xFF, "C14_MSB",
// 12,  29, 0x00, 0xFF, "C14_LSB",
// 12,  30, 0x00, 0xFF, "C15_MSB",
// 12,  31, 0x00, 0xFF, "C15_LSB",
// 12,  32, 0x7F, 0xFF, "C16_MSB",
// 12,  33, 0xFF, 0xFF, "C16_LSB",
// 12,  34, 0x00, 0xFF, "C17_MSB",
// 12,  35, 0x00, 0xFF, "C17_LSB",
// 12,  36, 0x00, 0xFF, "C18_MSB",
// 12,  37, 0x00, 0xFF, "C18_LSB",
// 12,  38, 0x00, 0xFF, "C19_MSB",
// 12,  39, 0x00, 0xFF, "C19_LSB",
// 12,  40, 0x00, 0xFF, "C20_MSB",
// 12,  41, 0x00, 0xFF, "C20_LSB",
// 12,  42, 0x7F, 0xFF, "C21_MSB",
// 12,  43, 0xFF, 0xFF, "C21_LSB",
// 12,  44, 0x00, 0xFF, "C22_MSB",
// 12,  45, 0x00, 0xFF, "C22_LSB",
// 12,  46, 0x00, 0xFF, "C23_MSB",
// 12,  47, 0x00, 0xFF, "C23_LSB",
// 12,  48, 0x00, 0xFF, "C24_MSB",
// 12,  49, 0x00, 0xFF, "C24_LSB",
// 12,  50, 0x00, 0xFF, "C25_MSB",
// 12,  51, 0x00, 0xFF, "C25_LSB",
// 12,  52, 0x7F, 0xFF, "C26_MSB",
// 12,  53, 0xFF, 0xFF, "C26_LSB",
// 12,  54, 0x00, 0xFF, "C27_MSB",
// 12,  55, 0x00, 0xFF, "C27_LSB",
// 12,  56, 0x00, 0xFF, "C28_MSB",
// 12,  57, 0x00, 0xFF, "C28_LSB",
// 12,  58, 0x00, 0xFF, "C29_MSB",
// 12,  59, 0x00, 0xFF, "C29_LSB",
// 12,  60, 0x00, 0xFF, "C30_MSB",
// 12,  61, 0x00, 0xFF, "C30_LSB",
// 12,  62, 0x00, 0xFF, "C31_MSB",
// 12,  63, 0x00, 0xFF, "C31_LSB",
// 12,  64, 0x00, 0xFF, "C32_MSB",
// 12,  65, 0x00, 0xFF, "C32_LSB",
// 12,  66, 0x7F, 0xFF, "C33_MSB",
// 12,  67, 0xFF, 0xFF, "C33_LSB",
// 12,  68, 0x00, 0xFF, "C34_MSB",
// 12,  69, 0x00, 0xFF, "C34_LSB",
// 12,  70, 0x00, 0xFF, "C35_MSB",
// 12,  71, 0x00, 0xFF, "C35_LSB",
// 12,  72, 0x00, 0xFF, "C36_MSB",
// 12,  73, 0x00, 0xFF, "C36_LSB",
// 12,  74, 0x00, 0xFF, "C37_MSB",
// 12,  75, 0x00, 0xFF, "C37_LSB",
// 12,  76, 0x7F, 0xFF, "C38_MSB",
// 12,  77, 0xFF, 0xFF, "C38_LSB",
// 12,  78, 0x00, 0xFF, "C39_MSB",
// 12,  79, 0x00, 0xFF, "C39_LSB",
// 12,  80, 0x00, 0xFF, "C40_MSB",
// 12,  81, 0x00, 0xFF, "C40_LSB",
// 12,  82, 0x00, 0xFF, "C41_MSB",
// 12,  83, 0x00, 0xFF, "C41_LSB",
// 12,  84, 0x00, 0xFF, "C42_MSB",
// 12,  85, 0x00, 0xFF, "C42_LSB",
// 12,  86, 0x7F, 0xFF, "C43_MSB",
// 12,  87, 0xFF, 0xFF, "C43_LSB",
// 12,  88, 0x00, 0xFF, "C44_MSB",
// 12,  89, 0x00, 0xFF, "C44_LSB",
// 12,  90, 0x00, 0xFF, "C45_MSB",
// 12,  91, 0x00, 0xFF, "C45_LSB",
// 12,  92, 0x00, 0xFF, "C46_MSB",
// 12,  93, 0x00, 0xFF, "C46_LSB",
// 12,  94, 0x00, 0xFF, "C47_MSB",
// 12,  95, 0x00, 0xFF, "C47_LSB",
// 12,  96, 0x7F, 0xFF, "C48_MSB",
// 12,  97, 0xFF, 0xFF, "C48_LSB",
// 12,  98, 0x00, 0xFF, "C49_MSB",
// 12,  99, 0x00, 0xFF, "C49_LSB",
// 12, 100, 0x00, 0xFF, "C50_MSB",
// 12, 101, 0x00, 0xFF, "C50_LSB",
// 12, 102, 0x00, 0xFF, "C51_MSB",
// 12, 103, 0x00, 0xFF, "C51_LSB",
// 12, 104, 0x00, 0xFF, "C52_MSB",
// 12, 105, 0x00, 0xFF, "C52_LSB",
// 12, 106, 0x7F, 0xFF, "C53_MSB",
// 12, 107, 0xFF, 0xFF, "C53_LSB",
// 12, 108, 0x00, 0xFF, "C54_MSB",
// 12, 109, 0x00, 0xFF, "C54_LSB",
// 12, 110, 0x00, 0xFF, "C55_MSB",
// 12, 111, 0x00, 0xFF, "C55_LSB",
// 12, 112, 0x00, 0xFF, "C56_MSB",
// 12, 113, 0x00, 0xFF, "C56_LSB",
// 12, 114, 0x00, 0xFF, "C57_MSB",
// 12, 115, 0x00, 0xFF, "C57_LSB",
// 12, 116, 0x7F, 0xFF, "C58_MSB",
// 12, 117, 0xFF, 0xFF, "C58_LSB",
// 12, 118, 0x00, 0xFF, "C59_MSB",
// 12, 119, 0x00, 0xFF, "C59_LSB",
// 12, 120, 0x00, 0xFF, "C60_MSB",
// 12, 121, 0x00, 0xFF, "C60_LSB",
// 12, 122, 0x00, 0xFF, "C61_MSB",
// 12, 123, 0x00, 0xFF, "C61_LSB",
// 12, 124, 0x00, 0xFF, "C62_MSB",
// 12, 125, 0x00, 0xFF, "C62_LSB",
// 12, 126, 0x00, 0xFF, "C63_MSB",
// 12, 127, 0x00, 0xFF, "C63_LSB",
// 13,   0, 0x00, 0x00, "Page_Select_Register",
// 13,   2, 0x7F, 0xFF, "C65_MSB",
// 13,   3, 0xFF, 0xFF, "C65_LSB",
// 13,   4, 0x00, 0xFF, "C66_MSB",
// 13,   5, 0x00, 0xFF, "C66_LSB",
// 13,   6, 0x00, 0xFF, "C67_MSB",
// 13,   7, 0x00, 0xFF, "C67_LSB",
// 13,   8, 0x7F, 0xFF, "C68_MSB",
// 13,   9, 0xFF, 0xFF, "C68_LSB",
// 13,  10, 0x00, 0xFF, "C69_MSB",
// 13,  11, 0x00, 0xFF, "C69_LSB",
// 13,  12, 0x00, 0xFF, "C70_MSB",
// 13,  13, 0x00, 0xFF, "C70_LSB",
// 13,  14, 0x7F, 0xFF, "C71_MSB",
// 13,  15, 0xF7, 0xFF, "C71_LSB",
// 13,  16, 0x80, 0xFF, "C72_MSB",
// 13,  17, 0x09, 0xFF, "C72_LSB",
// 13,  18, 0x7F, 0xFF, "C73_MSB",
// 13,  19, 0xEF, 0xFF, "C73_LSB",
// 13,  20, 0x00, 0xFF, "C74_MSB",
// 13,  21, 0x11, 0xFF, "C74_LSB",
// 13,  22, 0x00, 0xFF, "C75_MSB",
// 13,  23, 0x11, 0xFF, "C75_LSB",
// 13,  24, 0x7F, 0xFF, "C76_MSB",
// 13,  25, 0xDE, 0xFF, "C76_LSB",
// 13,  26, 0x00, 0xFF, "C77_MSB",
// 13,  27, 0x00, 0xFF, "C77_LSB",
// 13,  28, 0x00, 0xFF, "C78_MSB",
// 13,  29, 0x00, 0xFF, "C78_LSB",
// 13,  30, 0x00, 0xFF, "C79_MSB",
// 13,  31, 0x00, 0xFF, "C79_LSB",
// 13,  32, 0x00, 0xFF, "C80_MSB",
// 13,  33, 0x00, 0xFF, "C80_LSB",
// 13,  34, 0x00, 0xFF, "C81_MSB",
// 13,  35, 0x00, 0xFF, "C81_LSB",
// 13,  36, 0x00, 0xFF, "C82_MSB",
// 13,  37, 0x00, 0xFF, "C82_LSB",
// 13,  38, 0x00, 0xFF, "C83_MSB",
// 13,  39, 0x00, 0xFF, "C83_LSB",
// 13,  40, 0x00, 0xFF, "C84_MSB",
// 13,  41, 0x00, 0xFF, "C84_LSB",
// 13,  42, 0x00, 0xFF, "C85_MSB",
// 13,  43, 0x00, 0xFF, "C85_LSB",
// 13,  44, 0x00, 0xFF, "C86_MSB",
// 13,  45, 0x00, 0xFF, "C86_LSB",
// 13,  46, 0x00, 0xFF, "C87_MSB",
// 13,  47, 0x00, 0xFF, "C87_LSB",
// 13,  48, 0x00, 0xFF, "C88_MSB",
// 13,  49, 0x00, 0xFF, "C88_LSB",
// 13,  50, 0x00, 0xFF, "C89_MSB",
// 13,  51, 0x00, 0xFF, "C89_LSB",
// 13,  52, 0x00, 0xFF, "C90_MSB",
// 13,  53, 0x00, 0xFF, "C90_LSB",
// 13,  54, 0x00, 0xFF, "C91_MSB",
// 13,  55, 0x00, 0xFF, "C91_LSB",
// 13,  56, 0x00, 0xFF, "C92_MSB",
// 13,  57, 0x00, 0xFF, "C92_LSB",
// 13,  58, 0x00, 0xFF, "C93_MSB",
// 13,  59, 0x00, 0xFF, "C93_LSB",
// 13,  60, 0x00, 0xFF, "C94_MSB",
// 13,  61, 0x00, 0xFF, "C94_LSB",
// 13,  62, 0x00, 0xFF, "C95_MSB",
// 13,  63, 0x00, 0xFF, "C95_LSB",
// 13,  64, 0x00, 0xFF, "C96_MSB",
// 13,  65, 0x00, 0xFF, "C96_LSB",
// 13,  66, 0x00, 0xFF, "C97_MSB",
// 13,  67, 0x00, 0xFF, "C97_LSB",
// 13,  68, 0x00, 0xFF, "C98_MSB",
// 13,  69, 0x00, 0xFF, "C98_LSB",
// 13,  70, 0x00, 0xFF, "C99_MSB",
// 13,  71, 0x00, 0xFF, "C99_LSB",
// 13,  72, 0x00, 0xFF, "C100_MSB",
// 13,  73, 0x00, 0xFF, "C100_LSB",
// 13,  74, 0x00, 0xFF, "C101_MSB",
// 13,  75, 0x00, 0xFF, "C101_LSB",
// 13,  76, 0x00, 0xFF, "C102_MSB",
// 13,  77, 0x00, 0xFF, "C102_LSB",
// 13,  78, 0x00, 0xFF, "C103_MSB",
// 13,  79, 0x00, 0xFF, "C103_LSB",
// 13,  80, 0x00, 0xFF, "C104_MSB",
// 13,  81, 0x00, 0xFF, "C104_LSB",
// 13,  82, 0x00, 0xFF, "C105_MSB",
// 13,  83, 0x00, 0xFF, "C105_LSB",
// 13,  84, 0x00, 0xFF, "C106_MSB",
// 13,  85, 0x00, 0xFF, "C106_LSB",
// 13,  86, 0x00, 0xFF, "C107_MSB",
// 13,  87, 0x00, 0xFF, "C107_LSB",
// 13,  88, 0x00, 0xFF, "C108_MSB",
// 13,  89, 0x00, 0xFF, "C108_LSB",
// 13,  90, 0x00, 0xFF, "C109_MSB",
// 13,  91, 0x00, 0xFF, "C109_LSB",
// 13,  92, 0x00, 0xFF, "C110_MSB",
// 13,  93, 0x00, 0xFF, "C110_LSB",
// 13,  94, 0x00, 0xFF, "C111_MSB",
// 13,  95, 0x00, 0xFF, "C111_LSB",
// 13,  96, 0x00, 0xFF, "C112_MSB",
// 13,  97, 0x00, 0xFF, "C112_LSB",
// 13,  98, 0x00, 0xFF, "C113_MSB",
// 13,  99, 0x00, 0xFF, "C113_LSB",
// 13, 100, 0x00, 0xFF, "C114_MSB",
// 13, 101, 0x00, 0xFF, "C114_LSB",
// 13, 102, 0x00, 0xFF, "C115_MSB",
// 13, 103, 0x00, 0xFF, "C115_LSB",
// 13, 104, 0x00, 0xFF, "C116_MSB",
// 13, 105, 0x00, 0xFF, "C116_LSB",
// 13, 106, 0x00, 0xFF, "C117_MSB",
// 13, 107, 0x00, 0xFF, "C117_LSB",
// 13, 108, 0x00, 0xFF, "C118_MSB",
// 13, 109, 0x00, 0xFF, "C118_LSB",
// 13, 110, 0x00, 0xFF, "C119_MSB",
// 13, 111, 0x00, 0xFF, "C119_LSB",
// 13, 112, 0x00, 0xFF, "C120_MSB",
// 13, 113, 0x00, 0xFF, "C120_LSB",
// 13, 114, 0x00, 0xFF, "C121_MSB",
// 13, 115, 0x00, 0xFF, "C121_LSB",
// 13, 116, 0x00, 0xFF, "C122_MSB",
// 13, 117, 0x00, 0xFF, "C122_LSB",
// 13, 118, 0x00, 0xFF, "C123_MSB",
// 13, 119, 0x00, 0xFF, "C123_LSB",
// 13, 120, 0x00, 0xFF, "C124_MSB",
// 13, 121, 0x00, 0xFF, "C124_LSB",
// 13, 122, 0x00, 0xFF, "C125_MSB",
// 13, 123, 0x00, 0xFF, "C125_LSB",
// 13, 124, 0x00, 0xFF, "C126_MSB",
// 13, 125, 0x00, 0xFF, "C126_LSB",
// 13, 126, 0x00, 0xFF, "C127_MSB",
// 13, 127, 0x00, 0xFF, "C127_LSB",
// 14,   0, 0x00, 0x00, "Page_Select_Register",
// 14,   2, 0x7F, 0xFF, "C129_MSB",
// 14,   3, 0xFF, 0xFF, "C129_LSB",
// 14,   4, 0x00, 0xFF, "C130_MSB",
// 14,   5, 0x00, 0xFF, "C130_LSB",
// 14,   6, 0x00, 0xFF, "C131_MSB",
// 14,   7, 0x00, 0xFF, "C131_LSB",
// 14,   8, 0x7F, 0xFF, "C132_MSB",
// 14,   9, 0xFF, 0xFF, "C132_LSB",
// 14,  10, 0x00, 0xFF, "C133_MSB",
// 14,  11, 0x00, 0xFF, "C133_LSB",
// 14,  12, 0x00, 0xFF, "C134_MSB",
// 14,  13, 0x00, 0xFF, "C134_LSB",
// 14,  14, 0x7F, 0xFF, "C135_MSB",
// 14,  15, 0xF7, 0xFF, "C135_LSB",
// 14,  16, 0x80, 0xFF, "C136_MSB",
// 14,  17, 0x10, 0xFF, "C136_LSB",
// 14,  18, 0x7F, 0xFF, "C137_MSB",
// 14,  19, 0xEF, 0xFF, "C137_LSB",
// 14,  20, 0x00, 0xFF, "C138_MSB",
// 14,  21, 0x11, 0xFF, "C138_LSB",
// 14,  22, 0x00, 0xFF, "C139_MSB",
// 14,  23, 0x11, 0xFF, "C139_LSB",
// 14,  24, 0x7F, 0xFF, "C140_MSB",
// 14,  25, 0xDE, 0xFF, "C140_LSB",
// 14,  26, 0x00, 0xFF, "C141_MSB",
// 14,  27, 0x00, 0xFF, "C141_LSB",
// 14,  28, 0x00, 0xFF, "C142_MSB",
// 14,  29, 0x00, 0xFF, "C142_LSB",
// 14,  30, 0x00, 0xFF, "C143_MSB",
// 14,  31, 0x00, 0xFF, "C143_LSB",
// 14,  32, 0x00, 0xFF, "C144_MSB",
// 14,  33, 0x00, 0xFF, "C144_LSB",
// 14,  34, 0x00, 0xFF, "C145_MSB",
// 14,  35, 0x00, 0xFF, "C145_LSB",
// 14,  36, 0x00, 0xFF, "C146_MSB",
// 14,  37, 0x00, 0xFF, "C146_LSB",
// 14,  38, 0x00, 0xFF, "C147_MSB",
// 14,  39, 0x00, 0xFF, "C147_LSB",
// 14,  40, 0x00, 0xFF, "C148_MSB",
// 14,  41, 0x00, 0xFF, "C148_LSB",
// 14,  42, 0x00, 0xFF, "C149_MSB",
// 14,  43, 0x00, 0xFF, "C149_LSB",
// 14,  44, 0x00, 0xFF, "C150_MSB",
// 14,  45, 0x00, 0xFF, "C150_LSB",
// 14,  46, 0x00, 0xFF, "C151_MSB",
// 14,  47, 0x00, 0xFF, "C151_LSB",
// 14,  48, 0x00, 0xFF, "C152_MSB",
// 14,  49, 0x00, 0xFF, "C152_LSB",
// 14,  50, 0x00, 0xFF, "C153_MSB",
// 14,  51, 0x00, 0xFF, "C153_LSB",
// 14,  52, 0x00, 0xFF, "C154_MSB",
// 14,  53, 0x00, 0xFF, "C154_LSB",
// 14,  54, 0x00, 0xFF, "C155_MSB",
// 14,  55, 0x00, 0xFF, "C155_LSB",
// 14,  56, 0x00, 0xFF, "C156_MSB",
// 14,  57, 0x00, 0xFF, "C156_LSB",
// 14,  58, 0x00, 0xFF, "C157_MSB",
// 14,  59, 0x00, 0xFF, "C157_LSB",
// 14,  60, 0x00, 0xFF, "C158_MSB",
// 14,  61, 0x00, 0xFF, "C158_LSB",
// 14,  62, 0x00, 0xFF, "C159_MSB",
// 14,  63, 0x00, 0xFF, "C159_LSB",
// 14,  64, 0x00, 0xFF, "C160_MSB",
// 14,  65, 0x00, 0xFF, "C160_LSB",
// 14,  66, 0x00, 0xFF, "C161_MSB",
// 14,  67, 0x00, 0xFF, "C161_LSB",
// 14,  68, 0x00, 0xFF, "C162_MSB",
// 14,  69, 0x00, 0xFF, "C162_LSB",
// 14,  70, 0x00, 0xFF, "C163_MSB",
// 14,  71, 0x00, 0xFF, "C163_LSB",
// 14,  72, 0x00, 0xFF, "C164_MSB",
// 14,  73, 0x00, 0xFF, "C164_LSB",
// 14,  74, 0x00, 0xFF, "C165_MSB",
// 14,  75, 0x00, 0xFF, "C165_LSB",
// 14,  76, 0x00, 0xFF, "C166_MSB",
// 14,  77, 0x00, 0xFF, "C166_LSB",
// 14,  78, 0x00, 0xFF, "C167_MSB",
// 14,  79, 0x00, 0xFF, "C167_LSB",
// 14,  80, 0x00, 0xFF, "C168_MSB",
// 14,  81, 0x00, 0xFF, "C168_LSB",
// 14,  82, 0x00, 0xFF, "C169_MSB",
// 14,  83, 0x00, 0xFF, "C169_LSB",
// 14,  84, 0x00, 0xFF, "C170_MSB",
// 14,  85, 0x00, 0xFF, "C170_LSB",
// 14,  86, 0x00, 0xFF, "C171_MSB",
// 14,  87, 0x00, 0xFF, "C171_LSB",
// 14,  88, 0x00, 0xFF, "C172_MSB",
// 14,  89, 0x00, 0xFF, "C172_LSB",
// 14,  90, 0x00, 0xFF, "C173_MSB",
// 14,  91, 0x00, 0xFF, "C173_LSB",
// 14,  92, 0x00, 0xFF, "C174_MSB",
// 14,  93, 0x00, 0xFF, "C174_LSB",
// 14,  94, 0x00, 0xFF, "C175_MSB",
// 14,  95, 0x00, 0xFF, "C175_LSB",
// 14,  96, 0x00, 0xFF, "C176_MSB",
// 14,  97, 0x00, 0xFF, "C176_LSB",
// 14,  98, 0x00, 0xFF, "C177_MSB",
// 14,  99, 0x00, 0xFF, "C177_LSB",
// 14, 100, 0x00, 0xFF, "C178_MSB",
// 14, 101, 0x00, 0xFF, "C178_LSB",
// 14, 102, 0x00, 0xFF, "C179_MSB",
// 14, 103, 0x00, 0xFF, "C179_LSB",
// 14, 104, 0x00, 0xFF, "C180_MSB",
// 14, 105, 0x00, 0xFF, "C180_LSB",
// 14, 106, 0x00, 0xFF, "C181_MSB",
// 14, 107, 0x00, 0xFF, "C181_LSB",
// 14, 108, 0x00, 0xFF, "C182_MSB",
// 14, 109, 0x00, 0xFF, "C182_LSB",
// 14, 110, 0x00, 0xFF, "C183_MSB",
// 14, 111, 0x00, 0xFF, "C183_LSB",
// 14, 112, 0x00, 0xFF, "C184_MSB",
// 14, 113, 0x00, 0xFF, "C184_LSB",
// 14, 114, 0x00, 0xFF, "C185_MSB",
// 14, 115, 0x00, 0xFF, "C185_LSB",
// 14, 116, 0x00, 0xFF, "C186_MSB",
// 14, 117, 0x00, 0xFF, "C186_LSB",
// 14, 118, 0x00, 0xFF, "C187_MSB",
// 14, 119, 0x00, 0xFF, "C187_LSB",
// 14, 120, 0x00, 0xFF, "C188_MSB",
// 14, 121, 0x00, 0xFF, "C188_LSB",
// 14, 122, 0x00, 0xFF, "C189_MSB",
// 14, 123, 0x00, 0xFF, "C189_LSB",
// 14, 124, 0x00, 0xFF, "C190_MSB",
// 14, 125, 0x00, 0xFF, "C190_LSB",
// 14, 126, 0x00, 0xFF, "C191_MSB",
// 14, 127, 0x00, 0xFF, "C191_LSB",
// 15,   0, 0x00, 0x00, "Page_Select_Register",
// 15,   2, 0x00, 0xFF, "C193_MSB",
// 15,   3, 0x00, 0xFF, "C193_LSB",
// 15,   4, 0x00, 0xFF, "C194_MSB",
// 15,   5, 0x00, 0xFF, "C194_LSB",
// 15,   6, 0x00, 0xFF, "C195_MSB",
// 15,   7, 0x00, 0xFF, "C195_LSB",
// 15,   8, 0x00, 0xFF, "C196_MSB",
// 15,   9, 0x00, 0xFF, "C196_LSB",
// 15,  10, 0x00, 0xFF, "C197_MSB",
// 15,  11, 0x00, 0xFF, "C197_LSB",
// 15,  12, 0x00, 0xFF, "C198_MSB",
// 15,  13, 0x00, 0xFF, "C198_LSB",
// 15,  14, 0x00, 0xFF, "C199_MSB",
// 15,  15, 0x00, 0xFF, "C199_LSB",
// 15,  16, 0x00, 0xFF, "C200_MSB",
// 15,  17, 0x00, 0xFF, "C200_LSB",
// 15,  18, 0x00, 0xFF, "C201_MSB",
// 15,  19, 0x00, 0xFF, "C201_LSB",
// 15,  20, 0x00, 0xFF, "C202_MSB",
// 15,  21, 0x00, 0xFF, "C202_LSB",
// 15,  22, 0x00, 0xFF, "C203_MSB",
// 15,  23, 0x00, 0xFF, "C203_LSB",
// 15,  24, 0x00, 0xFF, "C204_MSB",
// 15,  25, 0x00, 0xFF, "C204_LSB",
// 15,  26, 0x00, 0xFF, "C205_MSB",
// 15,  27, 0x00, 0xFF, "C205_LSB",
// 15,  28, 0x00, 0xFF, "C206_MSB",
// 15,  29, 0x00, 0xFF, "C206_LSB",
// 15,  30, 0x00, 0xFF, "C207_MSB",
// 15,  31, 0x00, 0xFF, "C207_LSB",
// 15,  32, 0x00, 0xFF, "C208_MSB",
// 15,  33, 0x00, 0xFF, "C208_LSB",
// 15,  34, 0x00, 0xFF, "C209_MSB",
// 15,  35, 0x00, 0xFF, "C209_LSB",
// 15,  36, 0x00, 0xFF, "C210_MSB",
// 15,  37, 0x00, 0xFF, "C210_LSB",
// 15,  38, 0x00, 0xFF, "C211_MSB",
// 15,  39, 0x00, 0xFF, "C211_LSB",
// 15,  40, 0x00, 0xFF, "C212_MSB",
// 15,  41, 0x00, 0xFF, "C212_LSB",
// 15,  42, 0x00, 0xFF, "C213_MSB",
// 15,  43, 0x00, 0xFF, "C213_LSB",
// 15,  44, 0x00, 0xFF, "C214_MSB",
// 15,  45, 0x00, 0xFF, "C214_LSB",
// 15,  46, 0x00, 0xFF, "C215_MSB",
// 15,  47, 0x00, 0xFF, "C215_LSB",
// 15,  48, 0x00, 0xFF, "C216_MSB",
// 15,  49, 0x00, 0xFF, "C216_LSB",
// 15,  50, 0x00, 0xFF, "C217_MSB",
// 15,  51, 0x00, 0xFF, "C217_LSB",
// 15,  52, 0x00, 0xFF, "C218_MSB",
// 15,  53, 0x00, 0xFF, "C218_LSB",
// 15,  54, 0x00, 0xFF, "C219_MSB",
// 15,  55, 0x00, 0xFF, "C219_LSB",
// 15,  56, 0x00, 0xFF, "C220_MSB",
// 15,  57, 0x00, 0xFF, "C220_LSB",
// 15,  58, 0x00, 0xFF, "C221_MSB",
// 15,  59, 0x00, 0xFF, "C221_LSB",
// 15,  60, 0x00, 0xFF, "C222_MSB",
// 15,  61, 0x00, 0xFF, "C222_LSB",
// 15,  62, 0x00, 0xFF, "C223_MSB",
// 15,  63, 0x00, 0xFF, "C223_LSB",
// 15,  64, 0x00, 0xFF, "C224_MSB",
// 15,  65, 0x00, 0xFF, "C224_LSB",
// 15,  66, 0x00, 0xFF, "C225_MSB",
// 15,  67, 0x00, 0xFF, "C225_LSB",
// 15,  68, 0x00, 0xFF, "C226_MSB",
// 15,  69, 0x00, 0xFF, "C226_LSB",
// 15,  70, 0x00, 0xFF, "C227_MSB",
// 15,  71, 0x00, 0xFF, "C227_LSB",
// 15,  72, 0x00, 0xFF, "C228_MSB",
// 15,  73, 0x00, 0xFF, "C228_LSB",
// 15,  74, 0x00, 0xFF, "C229_MSB",
// 15,  75, 0x00, 0xFF, "C229_LSB",
// 15,  76, 0x00, 0xFF, "C230_MSB",
// 15,  77, 0x00, 0xFF, "C230_LSB",
// 15,  78, 0x00, 0xFF, "C231_MSB",
// 15,  79, 0x00, 0xFF, "C231_LSB",
// 15,  80, 0x00, 0xFF, "C232_MSB",
// 15,  81, 0x00, 0xFF, "C232_LSB",
// 15,  82, 0x00, 0xFF, "C233_MSB",
// 15,  83, 0x00, 0xFF, "C233_LSB",
// 15,  84, 0x00, 0xFF, "C234_MSB",
// 15,  85, 0x00, 0xFF, "C234_LSB",
// 15,  86, 0x00, 0xFF, "C235_MSB",
// 15,  87, 0x00, 0xFF, "C235_LSB",
// 15,  88, 0x00, 0xFF, "C236_MSB",
// 15,  89, 0x00, 0xFF, "C236_LSB",
// 15,  90, 0x00, 0xFF, "C237_MSB",
// 15,  91, 0x00, 0xFF, "C237_LSB",
// 15,  92, 0x00, 0xFF, "C238_MSB",
// 15,  93, 0x00, 0xFF, "C238_LSB",
// 15,  94, 0x00, 0xFF, "C239_MSB",
// 15,  95, 0x00, 0xFF, "C239_LSB",
// 15,  96, 0x00, 0xFF, "C240_MSB",
// 15,  97, 0x00, 0xFF, "C240_LSB",
// 15,  98, 0x00, 0xFF, "C241_MSB",
// 15,  99, 0x00, 0xFF, "C241_LSB",
// 15, 100, 0x00, 0xFF, "C242_MSB",
// 15, 101, 0x00, 0xFF, "C242_LSB",
// 15, 102, 0x00, 0xFF, "C243_MSB",
// 15, 103, 0x00, 0xFF, "C243_LSB",
// 15, 104, 0x00, 0xFF, "C244_MSB",
// 15, 105, 0x00, 0xFF, "C244_LSB",
// 15, 106, 0x00, 0xFF, "C245_MSB",
// 15, 107, 0x00, 0xFF, "C245_LSB",
// 15, 108, 0x00, 0xFF, "C246_MSB",
// 15, 109, 0x00, 0xFF, "C246_LSB",
// 15, 110, 0x00, 0xFF, "C247_MSB",
// 15, 111, 0x00, 0xFF, "C247_LSB",
// 15, 112, 0x00, 0xFF, "C248_MSB",
// 15, 113, 0x00, 0xFF, "C248_LSB",
// 15, 114, 0x00, 0xFF, "C249_MSB",
// 15, 115, 0x00, 0xFF, "C249_LSB",
// 15, 116, 0x00, 0xFF, "C250_MSB",
// 15, 117, 0x00, 0xFF, "C250_LSB",
// 15, 118, 0x00, 0xFF, "C251_MSB",
// 15, 119, 0x00, 0xFF, "C251_LSB",
// 15, 120, 0x00, 0xFF, "C252_MSB",
// 15, 121, 0x00, 0xFF, "C252_LSB",
// 15, 122, 0x00, 0xFF, "C253_MSB",
// 15, 123, 0x00, 0xFF, "C253_LSB",
// 15, 124, 0x00, 0xFF, "C254_MSB",
// 15, 125, 0x00, 0xFF, "C254_LSB",
// 15, 126, 0x00, 0xFF, "C255_MSB",
#endif
  16,   0, 0x00, 0x00, 0x00, "LAST_ENTRY_MARKER",                       0x00  // last entry marker
};
int							codecNREGS = (sizeof(codec_regs)/sizeof(AIC_REG)) - 1;  // exclude last entry marker

// define constants to match slots of codec_regs -- verified by 
 const int  P0_R0_Page_Select_Register                   =  0;
 const int  P0_R1_Software_Reset_Register                =  1;
 const int  P0_R3_OT_FLAG                                =  2;
const int   P0_R4_ClockGen_Muxing                        =  3;
 const int  P0_R5_PLL_P_R_VAL                            =  4;
 const int  P0_R6_PLL_J_VAL                              =  5;
 const int  P0_R7_PLL_D_VAL_16B                          =  6;
 const int  P0_R8_PLL_D_VAL_LSB                          =  7;
 const int  P0_R11_DAC_NDAC_VAL                          =  8;
 const int  P0_R12_DAC_MDAC_VAL                          =  9;
 const int  P0_R13_DAC_DOSR_VAL_16B                      = 10;
const int   P0_R14_DAC_DOSR_VAL_LSB                      = 11;
 const int  P0_R15_DAC_IDAC_VAL                          = 12;
 const int  P0_R16_DAC_MAC_Engine_Interpolation          = 13;
 const int  P0_R18_ADC_NADC_VAL                          = 14;
 const int  P0_R19_ADC_MADC_VAL                          = 15;
 const int  P0_R20_ADC_AOSR_VAL                          = 16;
 const int  P0_R21_ADC_IADC_VAL                          = 17;
const int   P0_R22_ADC_MAC_Engine_Decimation             = 18;
 const int  P0_R25_CLKOUT_MUX                            = 19;
 const int  P0_R26_CLKOUT_M_VAL                          = 20;
 const int  P0_R27_Codec_Interface_Control1              = 21;
const int   P0_R29_Codec_Interface_Control2              = 22;
 const int  P0_R30_BCLK_N_VAL                            = 23;
 const int  P0_R33_Codec_Interface_Control3              = 24;
 const int  P0_R36_ADC_Flag_Register                     = 25;		//  verify ADC power from P0_R36_  D6
 const int  P0_R37_DAC_Flag_Register                     = 26;
 const int  P0_R53_DOUT_Pin_Control                      = 27;
 const int  P0_R60_DAC_Instruction_Set                   = 28;
 const int  P0_R61_ADC_Instr_Set                  			 = 29;		// P0_R61_ADC_Instruction_Set
 const int  P0_R62_Prog_Instr_Md_Cntrl 									 = 30;		// P0_R62_Programmable_Instruction_Mode_Control
 const int  P0_R63_DAC_Datapath_SETUP                    = 31;
 const int  P0_R64_DAC_VOLUME_CONTROL                    = 32;
 const int  P0_R65_DAC_Left_Volume_Control               = 33;
 const int  P0_R66_DAC_Right_Volume_Control              = 34;
 const int  P0_R67_Headset_Detection                     = 35;
 const int  P0_R68_DRC_Control                           = 36;

 const int  P0_R81_ADC_Digital_Mic                    	 = 37;     		// ADC power up  P0_R81_  D7     
 const int  P0_R82_ADC_Volume_Control                    = 38;          
 const int  P0_R83_ADC_Volume_Control                    = 39;          
 const int  P0_R86_AGC_Control1                          = 40;    
 const int  P0_R87_AGC_Control2                          = 41;    
 const int  P0_R88_AGC_MAX_Gain                          = 42;    
 const int  P0_R89_AGC_Attack_Time                       = 43;       
 const int  P0_R90_AGC_Decay_Time                        = 44;      
 const int  P0_R91_AGC_Noise_Debounce                    = 45;          
 const int  P0_R92_AGC_Signal_Debounce                   = 46;           
 const int  P0_R93_AGC_Gain                              = 47;
const int   P1_R0_Page_Select_Register                   = 48;
 const int  P1_R30_Headphone_Speaker_Amp_Error_Control   = 49;
 const int  P1_R31_Headphone_Drivers                     = 50;
 const int  P1_R32_ClassD_Drivers                        = 51;
 const int  P1_R33_HP_Output_Drivers_POP_Rem_Settings    = 52;
 const int  P1_R34_Out_Driver_PGA_RampDown_Period_Ctrl   = 53;
 const int  P1_R35_LDAC_and_RDAC_Output_Routing          = 54;
 const int  P1_R36_Left_Analog_Vol_to_HPL                = 55;
 const int  P1_R37_Right_Analog_Vol_to_HPR               = 56;
 const int  P1_R38_Left_Analog_Vol_to_SPL                = 57;
const int   P1_R39_Right_Analog_Vol_to_SPR               = 58;
 const int  P1_R40_HPL_Driver                            = 59;
 const int  P1_R41_HPR_Driver                            = 60;
 const int  P1_R42_SPK_Driver                            = 61;
 const int  P1_R44_HP_Driver_Control                     = 62;
 const int  P1_R46_MICBIAS                               = 63;
 const int  P1_R47_MIC_PGA                               = 64;
 const int  P1_R48_ADC_Input_P                           = 65;
 const int  P1_R49_ADC_Input_M                           = 66;
 const int  P1_R50_Input_CM                              = 67;
 const int  P1_R51_GPIO1_Pin_Ctrl                        = 68;
 const int  P8_R0_Page_Select_Register                   = 69;
const int   P8_R1_DAC_Coefficient_RAM_Control            = 70; 

#if !defined (VERIFY_WRITTENDATA)  
// Uncomment this line to enable verifying data sent to codec after each write operation (for debug purpose)
#define VERIFY_WRITTENDATA 
#endif /* VERIFY_WRITTENDATA */

void 						aicSetCurrPage( uint8_t page ){
	if ( page == aicCurrPg ) 		// already on correct page?
		return;

	i2c_wrReg( 0, page );			// write current page's PageSelectRegister with new page
	dbgLog( "1 PgSw: %d \n", page );
	aicCurrPg = page;
}
uint8_t					aicGetReg( int idx ){
	uint8_t	pg = codec_regs[ idx ].pg;
	uint8_t reg = codec_regs[ idx ].reg;

	aicSetCurrPage( pg );
	uint8_t val = i2c_rdReg( reg );
	if ( WatchReg<0 || idx==WatchReg )
		dbgLog( "Rd %d:%02d == 0x%02x \n", pg, reg, val );
	return val;
}
void						aicSetReg( int idx, uint8_t val ){
	uint8_t pg = codec_regs[ idx ].pg;
	uint8_t reg = codec_regs[ idx ].reg;
	
	val = ( val & codec_regs[idx].W1_allowed ) | codec_regs[idx].W1_required;		// mask off bits that must be 0, and set bits that must be 1
	
	aicSetCurrPage( pg );
	if ( WatchReg<0 || idx==WatchReg )
		dbgLog( "Wr %d:%02d <- 0x%02x \n", pg, reg, val );

	i2c_wrReg( reg, val );	// write register of AIC3100
	codec_regs[idx].curr_val = val;
	
	#ifdef VERIFY_WRITTENDATA
		int chkVal = i2c_rdReg( reg );
		if ( chkVal != val ){
			uint8_t statRegs[] = { // registers we write, that also have read-only bits
				P0_R1_Software_Reset_Register, 	P0_R67_Headset_Detection, 
				P1_R31_Headphone_Drivers, 			P1_R32_ClassD_Drivers, 
				P1_R40_HPL_Driver, 							P1_R41_HPR_Driver, 
				P1_R42_SPK_Driver, 							P1_R50_Input_CM
			};
			for (int i=0; i<sizeof(statRegs); i++) 
				if ( idx==statRegs[i] ) return;  // don't report, non-writeable bits
			dbgLog( "Read after write mismatch %d:%02d wr 0x%02x rd 0x%02x \n", pg, reg, val, chkVal );
		}
	#endif /* VERIFY_WRITTENDATA */
}
void						aicSet16Bits( int idx, uint16_t val ){	// write (val>>8) to idx, (val & 0xFF) to idx+1
	uint8_t pg = codec_regs[ idx ].pg;
	uint8_t reg = codec_regs[ idx ].reg;
	if ( codec_regs[ idx+1 ].pg != pg || codec_regs[ idx+1 ].reg != reg+1 )
		dbgLog("! set16: i%d/%d not paired\n", idx, idx+1);
	
	aicSetCurrPage( pg );
	
	if ( WatchReg<0 || idx==WatchReg )
		dbgLog( "Wr %d:%02d/%02d <- 0x%04x \n", pg, reg,reg+1, val );
	
	i2c_wrReg( reg, val >> 8 );
	codec_regs[idx].curr_val = val;
	
	i2c_wrReg( reg+1, val & 0xFF );
	codec_regs[idx].curr_val = val;
	
	#ifdef VERIFY_WRITTENDATA
		uint16_t chkVal = ( i2c_rdReg( reg )<<8 ) | i2c_rdReg( reg+1 );
		if ( chkVal != val )
			dbgLog( "! Read after write mismatch  %d:%02d_%02d wr 0x%04x rd 0x%04x \n", pg, reg,reg+1, val, chkVal );
	#endif /* VERIFY_WRITTENDATA */
	
}

void 						i2c_CheckRegs(){																						// Debug -- read codec regs
	const 					uint8_t		Cdc_DefReg = 3;
	int nErrs = 0;
	#ifdef VERIFY_WRITTENDATA	
		int svWReg = WatchReg;
	  WatchReg = codecNREGS+1;
		for ( int i = 0; i < codecNREGS; i++ ){
			//uint8_t reg = codec_regs[i].reg;
			uint8_t defval = codec_regs[i].reset_val;
			uint8_t val = aicGetReg( i ); 
			codec_regs[i].curr_val = val;
			
			cntErr( Cdc_DefReg, defval, val, i, 9 );	
			if (defval != val) nErrs++;
		}
		WatchReg = svWReg;
		i2c_ReportErrors();
	#endif
}
void						i2c_Upd(){
}
void						verifyCodecReg( int idx, int pg, int reg ){				// verify that codec_regs[idx] is entry for pg : reg
	int cr_pg = codec_regs[ idx ].pg;
	int cr_reg = codec_regs[ idx ].reg;
	if ( cr_pg != pg || cr_reg != reg )
		dbgLog( "! v_codec_reg!: idx=%d  %d:%d != %d:%d \n", idx, pg, reg, cr_pg, cr_reg );
}

static int 											LastVolume 	= 0;			// retain last setting, so audio restarts at last volume

void						debugRecordingRegs(){
	if ( !dbgEnab( '8' )) return;
	const int MAX_REC_OPTS = 8;
	struct 
	{
		uint32_t idx;
		char * optNm;
		struct { 
			uint8_t val; 
			char *valNm; 
		} vals[ MAX_REC_OPTS ]; 
	} recOpts[] = 	// recording options -- reg values & description  -- (must have a value for 0x00)
	{		
// bias 1.46: 0x0A=2.5V, 0x09=1.5V
		{ P1_R46_MICBIAS, 			"mic_bias", { { 0x0A, "2.5V" }, 	{ 0x09, "1.5V" },   { 0x00, "??" }													}},
// pga  1.47: (0x08=4dB), 0x00=0dB, 0x10=8dB, 0x20=16dB		
		{ P1_R47_MIC_PGA, 			"mic_pga",  { { 0x08, "4dB" }, 		{ 0x00, "0dB" }, 		{ 0x10, "8dB" }, 	{ 0x20, "16dB" } 			}},
// aosr 0.20: 0x40=64, 0x80=128		
		{ P0_R20_ADC_AOSR_VAL, 	"aosr", 		{ { 0x40, "64" }, 		{ 0x80, "128" },    { 0x00, "??" }													}},
// prb  0.61: 0x04=PRB_R4, 0x0B=PRB_R11		
		{ P0_R61_ADC_Instr_Set, "prb", 			{ { 0x04, "PRB_R4" }, { 0x0B, "PRB_R11" },{ 0x00, "??" }													}}, 
// targ 0.86: 0x00=AGCoff, 0x80=targ -5.5dB, 0xA0=-10dB, 0xC0=-14dB, 0xD0=-17dB, 0xE0=-20dB, 0xF0=-24dB		
		{ P0_R86_AGC_Control1, 	"agc_targ",	{ { 0x00, "AGCoff" }, { 0x80, "-5.5dB" }, { 0xA0, "-10dB" },{ 0xC0, "-14dB" },
																					{ 0xD0, "-17dB"},   { 0xE0, "-20dB"},   { 0xF0, "-24dB"} 											  }},
// maxg 0.88: 0x00=0dB, 0x08=4dB, 0x10=8dB, 0x20=16dB, 0x30=24dB, 0x40=32dB, 0x50=40dB
		{ P0_R88_AGC_MAX_Gain, 	"agc_maxg", { { 0x00, "0dB" }, 		{ 0x08, "4dB" }, 		{ 0x10, "8dB" }, 	{ 0x20, "16dB" }, 
																					{ 0x30, "24dB" }, 	{ 0x40, "32dB" }, 	{ 0x50, "40dB" } 												}},
// vol  0.83: 0x68=-12dB, 0x78=-6dB, 0x00=0dB, 0x10=8dB, 0x20=16dB		
		{ P0_R83_ADC_Volume_Control, "vol", { { 0x68, "-12dB" }, 	{ 0x78, "-6dB" }, 	{ 0x00, "0dB" }, 	{ 0x10, "8dB" },
																					{ 0x20, "16dB" }																																}},
// in_p 1.48: 0x40=MIC1LP_10k, 0x80=MIC1LP_20k, 0xC0=MIC1LP_40k	
		{ P1_R48_ADC_Input_P, 	"in_p", 		{ { 0x40, "1LP_10k" },{ 0x80, "1LP_20k" },{ 0xC0, "1LP_40k" }, { 0x10, "1RP_10k" },
																				  { 0x00, "no_P" }																																}},
// in_m 1.49: 0x10=MIC1LM_10k, 0x20=MIC1LM_20k, 0x30=MIC1LM_40k	
		{ P1_R49_ADC_Input_M, 	"in_m", 		{ { 0x10, "1LM_10k" },{ 0x20, "1LM_20k" },{ 0x30, "1LM_40k" }, { 0x40, "L_CM10k" },
																					{ 0x00, "no_M" }																																}},
//in_cm 1.50: 0x00=no CM, 0x80=MIC1LP to CM, 0x20=MIC1LM to CM, 0xA0 = MIC1LP & MIC1LM to CM	
		{ P1_R50_Input_CM, 			"in_cm",		{ { 0x00, "no_CM" }, 	{ 0x80, "1LP_CM" }, { 0x20, "1LM_CM" }, { 0xA0, "P&M_CM" }	}},
//out_route 1.35: 0x00=none, 0x20=MicL, 0x40=DacL, 0x60=MicL&DacL 
		{ P1_R35_LDAC_and_RDAC_Output_Routing, "out_Route", 
																				{ { 0x00, "none" }, 	{ 0x20, "MicL" }, 	{ 0x40, "DacL" }, 	{ 0x60, "DacL&MicL" } }},
//classD  1.32:  0x80=SpkOn, 0x00=SpkOff
		{ P1_R32_ClassD_Drivers, "classD",  { { 0x80, "SpkAmpOn" }, { 0x00, "SpkAmpOff" } }},
//l_to_spkr 1.38: 
		{ P1_R38_Left_Analog_Vol_to_SPL, "l_to_spkr", { { 0x00, "noLtoSpkr", }, {	0x80, "LtoSpkr_0dB" } }},	
//L_ToSpkr 0.63:	0x00=Off  0x90=LDacOn		
		{ P0_R63_DAC_Datapath_SETUP, "dac_pwr", { { 0x00, "off" }, { 0x90, "LDacOn" }	}},	
		
		{ 0, "", {{ 0,"" }}} // end of list
	};
	
	for ( int i=0; i < 100; i++ ){
		int ridx = recOpts[i].idx;
		if ( ridx == 0 ) break;
		
		uint8_t val = aicGetReg( ridx );
		int valIdx = -1;
		for ( int j=0; j< MAX_REC_OPTS; j++ )
		  if ( recOpts[i].vals[j].val == val ){ // found value in list
				valIdx = j;
				break;
			}
		char *valnm = "??";
		if (valIdx >= 0) 
			valnm = recOpts[i].vals[ valIdx ].valNm;
			
		dbgLog( "R%d.%d: %12s = 0x%02x = %s \n", codec_regs[ ridx ].pg, codec_regs[ ridx ].reg, recOpts[i].optNm, val, valnm );
		tbDelay_ms(50);
	}
}
void						cdc_RecordEnable( bool enable ){
#if defined( AIC3100 )
	if ( enable ){ 
		typedef struct  { 
			uint8_t in_p;		// in_p 1.48: 0x40=MIC1LP_10k, 0x80=MIC1LP_20k, 0xC0=MIC1LP_40k
			uint8_t in_m;		// in_m 1.49: 0x10=MIC1LM_10k, 0x20=MIC1LM_20k, 0x30=MIC1LM_40k
			uint8_t in_cm;	//in_cm 1.50: 0x00=no CM, 0x80=MIC1LP to CM, 0x20=MIC1LM to CM, 0xA0 = MIC1LP & MIC1LM to CM
			
			uint8_t bias;		// bias 1.46: 0x0A=2.5V, 0x09=1.5V												//AK4637: used 2.4V
			uint8_t pga; 		// pga  1.47: (0x08=4dB), 0x00=0dB, 0x10=8dB, 0x20=16dB		//AK4637: used +18dB
			uint8_t aosr; 	// aosr 0.20: 0x40=64, 0x80=128
			uint8_t prb; 		// prb  0.61: 0x04=PRB_R4, 0x0B=PRB_R11
			uint8_t targ; 	// targ 0.86: 0x00=AGCoff, 0x80=targ -5.5dB, 0xA0=-10dB, 0xC0=-14dB, 0xD0=-17dB, 0xE0=-20dB, 0xF0=-24dB

			uint8_t maxg;		// maxg 0.88: 0x00=0dB, 0x08=4dB, 0x10=8dB, 0x20=16dB, 0x30=24dB, 0x40=32dB, 0x50=40dB
			uint8_t vol;		// vol  0.83: 0x68=-12dB, 0x78=-6dB, 0x00=0dB, 0x10=8dB, 0x20=16dB
		} recP;
 
		recP opts[] = 
		{
		// Record MIC1RP (per James Kim's suggestion)
		 // in_p, in_m, in_cm, bias,  pga,  aosr, prb,  targ, maxg, vol			DbgRec    
		//	{ 0x10, 0x00,  0x00, 0x0A,	0x08,	0x40,	0x04,	0xC0,	0x30, 0x00 }, // 0  P=MIC1RP 10k, M=nothing
		//	{ 0x10, 0x40,  0x00, 0x0A,	0x08,	0x40,	0x04,	0xC0,	0x30, 0x00 }, // 1  P=MIC1RP 10k, M=CM 10k

			// Test no amplification:  pga=0dB, AGC=off, coarse_vol=-12dB
		//	{ 0xC0, 0x30,  0x00, 0x0A,	0x00,	0x40,	0x04,	0x00,	0x00, 0x68 }, //0
			
		 // in_p, in_m, in_cm, bias,  pga,  aosr, prb,  targ, maxg, vol			DbgRec    
			{ 0x40, 0x10,  0x00, 0x0A,	0x08,	0x40,	0x04,	0xC0,	0x30, 0x00 }, // 0   defaults
			
	//	in_p 1.48: 0x40=MIC1LP_10k, 0x80=MIC1LP_20k, 0xC0=MIC1LP_40k
	//	in_m 1.49: 0x10=MIC1LM_10k, 0x20=MIC1LM_20k, 0x30=MIC1LM_40k
  //    in_p  in_m 
			{ 0x80, 0x20,  0x00, 0x0A,	0x08,	0x40,	0x04,	0xC0,	0x30, 0x00 }, // 1   MIC1LP & MIC1LM @ 20kOhm
			{ 0xC0, 0x30,  0x00, 0x0A,	0x08,	0x40,	0x04,	0xC0,	0x30, 0x00 }, // 2   MIC1LP & MIC1LM @ 40kOhm
			
	//  in_cm 1.50: 0x00=no CM, 0x80=MIC1LP to CM, 0x20=MIC1LM to CM, 0xA0 = MIC1LP & MIC1LM to CM
	//                in_cm
			{ 0x40, 0x10,  0x80, 0x0A,	0x08,	0x40,	0x04,	0xC0,	0x30, 0x00 }, // 3   MIC1LP to CM
			{ 0x40, 0x10,  0x20, 0x0A,	0x08,	0x40,	0x04,	0xC0,	0x30, 0x00 }, // 4   MIC1LM to CM
			{ 0x40, 0x10,  0xA0, 0x0A,	0x08,	0x40,	0x04,	0xC0,	0x30, 0x00 }, // 5   MIC1LP & LM to CM

	// pga 1.47:  (0x08=4dB), 0x00=0dB, 0x10=8dB, 0x20=16dB
	//                              pga
			{ 0xC0, 0x30,  0x00, 0x0A,	0x08,	0x40,	0x04,	0xC0,	0x30, 0x00 }, // 6   MIC1LP & MIC1LM @ 40kOhm, pga 4dB
			{ 0xC0, 0x30,  0x00, 0x0A,	0x00,	0x40,	0x04,	0xC0,	0x30, 0x00 }, // 7   MIC1LP & MIC1LM @ 40kOhm, pga 0dB

	// targ 0.86: 0x00=AGCoff, 0x80=targ -5.5dB, 0xA0=-10dB, 0xC0=-14dB, 0xD0=-17dB, 0xE0=-20dB, 0xF0=-24dB
	//                                                targ
			{ 0xC0, 0x30,  0x00, 0x0A,	0x00,	0x40,	0x04,	0x00,	0x30, 0x00 }, // 8   MIC1LP & MIC1LM @ 40kOhm, pga 0dB, AGC off
			{ 0xC0, 0x30,  0x00, 0x0A,	0x00,	0x40,	0x04,	0xE0,	0x30, 0x00 }, // 9   MIC1LP & MIC1LM @ 40kOhm, pga 0dB, targ -20dB
			{ 0xC0, 0x30,  0x00, 0x0A,	0x00,	0x40,	0x04,	0xF0,	0x30, 0x00 }, //10   MIC1LP & MIC1LM @ 40kOhm, pga 0dB, targ -24dB

	// maxg 0.88: 0x00=0dB, 0x08=4dB, 0x10=8dB, 0x20=16dB, 0x30=24dB, 0x40=32dB, 0x50=40dB			
	//                                                      maxg
			{ 0xC0, 0x30,  0x00, 0x0A,	0x00,	0x40,	0x04,	0xE0,	0x08, 0x00 }, //11   MIC1LP & MIC1LM @ 40kOhm, pga 0dB, targ -20dB, maxg 4dB
			{ 0xC0, 0x30,  0x00, 0x0A,	0x00,	0x40,	0x04,	0xE0,	0x10, 0x00 }, //12   MIC1LP & MIC1LM @ 40kOhm, pga 0dB, targ -20dB, maxg 8dB
			{ 0xC0, 0x30,  0x00, 0x0A,	0x00,	0x40,	0x04,	0xE0,	0x20, 0x00 }, //13   MIC1LP & MIC1LM @ 40kOhm, pga 0dB, targ -20dB, maxg 16dB
			
	// vol  0.83: 0x68=-12dB, 0x78=-6dB, 0x00=0dB, 0x10=8dB, 0x20=16dB		
	//                                                            vol
			{ 0xC0, 0x30,  0x00, 0x0A,	0x00,	0x40,	0x04,	0xE0,	0x10, 0x68 }, //14   MIC1LP & MIC1LM @ 40kOhm, pga 0dB, targ -20dB, maxg 8dB, vol -12dB
			{ 0xC0, 0x30,  0x00, 0x0A,	0x00,	0x40,	0x04,	0xE0,	0x10, 0x78 }, //15   MIC1LP & MIC1LM @ 40kOhm, pga 0dB, targ -20dB, maxg 8dB, vol -6dB
			{ 0xC0, 0x30,  0x00, 0x0A,	0x00,	0x40,	0x04,	0xE0,	0x10, 0x10 }, //16   MIC1LP & MIC1LM @ 40kOhm, pga 0dB, targ -20dB, maxg 8dB, vol 8dB
		};
		const int MAX_OPTS = sizeof(opts) / sizeof(recP);

		if ( RecDBG < 0 ) RecDBG = 0;
		if ( RecDBG >= MAX_OPTS ) RecDBG = MAX_OPTS-1;
		recP RP = opts[ RecDBG ];
		
		
		//DEBUG-- route MIC to speaker
		cdc_SpeakerEnable( true );		// turn on DAC to SPKR
		aicSetReg( P1_R35_LDAC_and_RDAC_Output_Routing, 0x60 );	// P1_R35: D7_6=01 DacL->Mix, D5=1: MIC1LP->mix, R->nowhere: 0 0000
		//DEBUG
		
		// power-on MIC & ADC, setup MIC PGA -- left channel only
		// mic settings used for AK4637 --  Differential input, mic gain = +18dB, mic_pwr = 2.4V
		aicSetReg( P1_R46_MICBIAS,						RP.bias ); //0x0A ); // P1_R46: MICBIAS output at 2.5V (re 7.3.9.1) even without headset detected, Software Power Down disabled
 		aicSetReg( P1_R47_MIC_PGA,  					RP.pga ); //0x08 );	// P1_R47: D7=0, D6_0=00 1000: MIC PGA is at 8 = 4dB gain
		aicSetReg( P1_R48_ADC_Input_P,  			RP.in_p );	// P1_R48: D7_6 = 01: MIC1LP is selected for the MIC PGA with feed-forward resistance RIN = 10 k?.
		aicSetReg( P1_R49_ADC_Input_M,  			RP.in_m );	// P1_R49: D5_4 = 01: MIC1LM is selected for the MIC PGA with feed-forward resistance RIN = 10 k?.
		aicSetReg( P1_R50_Input_CM,  					RP.in_cm );	// P1_R50: no connections to internal Common Mode voltage
		
		aicSetReg( P0_R20_ADC_AOSR_VAL, 			RP.aosr ); //0x40 );	// P0_R20: D7_0=64
		aicSetReg( P0_R61_ADC_Instr_Set, 			RP.prb ); //0x04 ); // P0_R61: D4_0= 0 0100: ADC signal-processing block PRB_R4 
		aicSetReg( P0_R62_Prog_Instr_Md_Cntrl,0x00 ); // P0_R62: defaults 
																											  
		// enable & set up Auto Gain Control -- defaults for now
		// datasheet  7.3.9.2 Automatic Gain Control  -- pg 30 
		//####### AGC ENABLE EXAMPLE CODE  -- 
		//# Set AGC enable and Target Level = -10dB
		//# Target level can be set lower if clipping occurs during speech
		//# Target level is adjusted considering Max Gain also
		aicSetReg( P0_R86_AGC_Control1, 			RP.targ ); //0xA0 );	// P0_R86: D7=1: AGC enabled, D6_4=010 AGC target level = –10 dB

		//# AGC hysteresis=DISABLE, noise threshold = -90dB
		//# Noise threshold should be set at higher level if noisy background is present in application
		aicSetReg( P0_R87_AGC_Control2, 			0xFE );	// P0_R87: D7_6=11: AGC hysteresis disabled, D5_1=11 111 = AGC noise threshold = –90 dB
 
		//# AGC maximum gain= 40 dB
    //# Higher Max gain is a trade off between gaining up a low sensitivity MIC, and the background acoustic noise
		//# Microphone bias voltage (MICBIAS) level can be used to change the Microphone Sensitivity
		aicSetReg( P0_R88_AGC_MAX_Gain, 			RP.maxg ); //0x50 );	// P0_R88: D6_0=101 0000: ACG maximum gain = 40dB

		//# Attack time=864/Fs
		aicSetReg( P0_R89_AGC_Attack_Time, 		0x68 );	// P0_R89: D7_3=0110 1: AGC attack time = 13 × (32 / fS) ( 13*32=416 not 864? )
		//																					             D2_0=000: Multiply factor for the programmed AGC attack time = 1
		//# Decay time=22016/Fs
		aicSetReg( P0_R90_AGC_Decay_Time,     0xA8 );	// P0_R90: D7_3=1010 1: AGC decay time = 21 × (512 / fS), ( 21*512=10752 not 22016? )
		//																							         D2_0=000: Multiply factor for the programmed AGC decay time = 1

		//# Noise debounce 0 ms
		//# Noise debounce time can be increased if needed
		aicSetReg( P0_R91_AGC_Noise_Debounce, 0x00 );	// P0_R91: D4_0=0 0000: AGC noise debounce = 0 / fS

		//# Signal debounce 0 ms
		//# Signal debounce time can be increased if needed
		aicSetReg( P0_R92_AGC_Signal_Debounce,0x00 ); // P0_R92: D3_0=0000: AGC signal debounce = 0 / fS
		//######### END of AGC SET UP  */		
		
		// set default gain for ADC volume, then unmute
		aicSetReg( P0_R83_ADC_Volume_Control, RP.vol ); //0x00 );	// P0_R83: D6_0=000 0000: Delta-Sigma Mono ADC Channel Volume Control Coarse Gain = 0dB
		aicSetReg( P0_R82_ADC_Volume_Control, 0x00 );	// P0_R82: D7=0: ADC channel not muted, D6_4=000: Delta-Sigma Mono ADC Channel Volume Control Fine Gain = 0dB

		// power up ADC (after AGC configured, re: pg. 30)
		aicSetReg( P0_R81_ADC_Digital_Mic,    0x80 );	// P0_R81: D7= 1: ADC channel is powered up, D1_0 = 00: ADC digital soft-stepping enabled 1 step/sample
		int8_t agc_gain = aicGetReg( P0_R93_AGC_Gain );	// P0_R93: applied reading of current AGC gain-- -24..63 = -12dB..59.5dB
	}	else {
		// power down the ADC and the MIC
		aicSetReg( P0_R81_ADC_Digital_Mic,    0x00 );	// P0_R81: D7= 0: ADC channel is powered down.
		aicSetReg( P1_R46_MICBIAS,						0x00 ); // P1_R46: MICBIAS output off, Software Power Down disabled
		dbgLog( "8 AIC ADC & MIC off \n");
	}
	debugRecordingRegs();
#endif
	
#if defined( AK4637 )
	if ( enable ){
		// from AK4637 datasheet:
		//ENABLE
		// done by cdc_SetMasterFreq()			//   1)  Clock set up & sampling frequency  FS3_0
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
		ak.R.AlcTimSel.RFST1_0 = 0x3;		//   5)  AlcTimSel.RFST1_0  FastRecoveryGainStep = .0127db
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
void 						cdc_SpeakerEnable( bool enable ){														// enable/disable speaker -- using mute to minimize pop
	if ( cdcSpeakerOn==enable )   // no change?
		return;
	
	cdcSpeakerOn = enable;
	dbgEvt( TB_cdcSpkEn, enable,0,0,0);

#if defined( AIC3100 )
	if ( enable ){ 	
		  cdc_SetMute( true );		// no noise during transition
			// power-on DAC -- left channel only
			aicSetReg( P1_R35_LDAC_and_RDAC_Output_Routing, 0x40 );	// P1_R35: DacLtoMix: 01 Mic&Rnowhere: 00 0000
			aicSetReg( P1_R32_ClassD_Drivers, 							0x80 );	// P1_R32: SpkrAmpPwrOn: 1
			aicSetReg( P1_R38_Left_Analog_Vol_to_SPL, 			0x80 );	// R1_38: LchanOutToSpkr: 1  SPKgain: 000 0000 (0dB)
			aicSetReg( P0_R63_DAC_Datapath_SETUP, 					0x90 );	// P0_R63: PwrLDAC: 1  PwrRDAC: 0  LDACleft: 01  RDACoff: 00  DACvol1step: 00
			dbgLog( "2 AIC DAC -> Spkr, Spkr on \n");
	} else {			
		  cdc_SetMute( true );		// no noise during transition
			aicSetReg( P1_R32_ClassD_Drivers, 							0x00 );	// P1_R32: SpkrAmpPwrOn: 0
			dbgLog( "2 AIC Spkr off \n");
	}
#endif
#if defined( AK4343 )
	if ( enable ){ 	
			// power up speaker amp & codec by setting power bits with mute enabled, then disabling mute
			Codec_SetRegBits( AK_Signal_Select_1, AK_SS1_SPPSN, 0 );						// set power-save (mute) ON (==0)
			Codec_SetRegBits( AK_Power_Management_1, AK_PM1_SPK | AK_PM1_DAC, AK_PM1_SPK | AK_PM1_DAC );	// set spkr & DAC power ON
			Codec_SetRegBits( AK_Signal_Select_1, AK_SS1_DACS, AK_SS1_DACS );				// 2) DACS=1  set DAC->Spkr 
			Codec_SetRegBits( AK_Signal_Select_2, AK_SS2_SPKG1 | AK_SS2_SPKG0, AK_SS2_SPKG1 );   // Speaker Gain (SPKG0-1 bits): Gain=+10.65dB(ALC off)/+12.65(ALC on)
			tbDelay_ms(2); 	// wait at least a mSec -- ak4343 datasheet, fig. 59
			Codec_SetRegBits( AK_Signal_Select_1, AK_SS1_SPPSN, AK_SS1_SPPSN );		// set power-save (mute) OFF (==1)
	} else {			
			Codec_SetRegBits( AK_Signal_Select_1, AK_SS1_SPPSN, 0 );						// set power-save (mute) ON (==0)
			Codec_SetRegBits( AK_Power_Management_1, AK_PM1_SPK | AK_PM1_DAC, AK_PM1_SPK | AK_PM1_DAC );	// set spkr & DAC power ON
			Codec_SetRegBits( AK_Signal_Select_1, AK_SS1_DACS, 0 );	// disconnect DAC => Spkr 
	}
#endif
#if defined( AK4637 )
	if ( enable ){ 	
			// startup sequence re: AK4637 Lineout Uutput pg 91
			akR.R.SigSel1.SLPSN = 0;  						// set power-save (mute) ON (==0)
			akUpd();															// and UPDATE
			gSet( gPA_EN, 1 );										// enable power to speaker & headphones
																						// 1) done by cdc_SetMasterFreq()
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
			tbDelay_ms( 300 ); //DEBUG 30 );											// 7) wait up to 300ms   // MARC 11)
			akR.R.SigSel1.SLPSN = 1;							// 8) exit power-save (mute) mode (==1)
			akUpd();		
	} else {	
		//  power down by enabling mute, then shutting off power
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
		
	}	
#endif
}

void						cdc_PowerUp( void ){
  // AIC3100 power up sequence based on sections 7.3.1-4 of Datasheet: https://www.ti.com/lit/ds/symlink/tlv320aic3100.pdf
	//  delays as recommended by Marc on 12/31/20
	gSet( gBOOT1_PDN, 0 );			// put codec in reset state PB2
	
	gSet( gEN_5V, 1 );					// power up EN_V5 for codec SPKVDD PD4
	tbDelay_ms( 200 );  		 		// wait for voltage regulators  // Marc: 200ms

	gSet( gEN_IOVDD_N, 0 );			// power up codec IOVDD PE4
	tbDelay_ms( 100 );  		 		// wait for voltage regulators  // Marc: 100ms
	gSet( gEN1V8, 1 );					// power up EN1V8 for codec DVDD PD5 ("shortly" after IOVDD)
	tbDelay_ms( 10 );  		 			// wait for voltage regulators
	
	gSet( gEN_AVDD_N, 0 );			// power up codec AVDD & HPVDD PE5 (at least 10ns after DVDD)
	tbDelay_ms( 100 );  		 		//  wait for it to start up  // Marc: 100ms
	
	gSet( gBOOT1_PDN, 1 );  		// set codec RESET_N inactive to Power on the codec PB2
	tbDelay_ms( 10 );  		 			//  wait for it to start up
	dbgLog( "2 AIC3100 powered up\n");
}
// external interface functions
void 						cdc_Init( ){ 																								// Init codec & I2C (i2s_stm32f4xx.c)
	dbgEvt( TB_cdcInit, 0,0,0,0);

	// make sure aic index constats match codec_regs[] entries
	verifyCodecReg( codecNREGS, 16, 0 );	// marker at end
	verifyCodecReg( P0_R4_ClockGen_Muxing, 0, 4 );
	verifyCodecReg( P0_R14_DAC_DOSR_VAL_LSB, 0, 14 );
	verifyCodecReg( P0_R22_ADC_MAC_Engine_Decimation, 0, 22 );
	verifyCodecReg( P0_R29_Codec_Interface_Control2, 0, 29 );
	verifyCodecReg( P1_R0_Page_Select_Register, 1, 0 );
	verifyCodecReg( P1_R39_Right_Analog_Vol_to_SPR, 1, 39 );
	verifyCodecReg( P8_R1_DAC_Coefficient_RAM_Control, 8, 1 );
	
//	memset( &akC, 0, sizeof( AK4637_Registers ));
	cdcSpeakerOn 		= false;
	cdcMuted			 	= false;

	cdc_PowerUp(); 		// power-up codec
  i2c_Init();  			// powerup & Initialize the Control interface of the Audio Codec

	uint8_t rst = 1;  // software reset bit -- hardware returns to 0
	uint32_t st = tbTimeStamp();
	
	aicSetReg( P0_R1_Software_Reset_Register, rst );		// initiate software reset
	while ( rst != 0 ){
		rst = aicGetReg( P0_R1_Software_Reset_Register );
	}
	dbgLog( "2 AIC Sft Reset took %d ms \n", tbTimeStamp()-st );
	
	i2c_CheckRegs();		// check all default register values & report

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
	cdc_SetVolume( LastVolume );						// set to LastVolume used

	// Extra Configuration (of the ALC)  
#if defined( AIC3100 )
#endif
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

#ifdef USE_CODEC_FILTERS
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
 #endif

	cdc_SetMute( true );		// set soft mute on output
}


void 						cdc_PowerDown( void ){																				// power down entire codec (i2s_stm..)
	dbgEvt( TB_cdcPwrDn, 0,0,0,0);
	#if defined( AIC3100 )	//to power down DAC channel, wait till P0_R37_Power_Status_d7_d3 == 0, then power down MDAC, then NDAC 
	  const int MAX_DAC_PWR_WAIT = 10000;
	//AIC3100: follow pg 65 rules
		aicSetReg( P0_R63_DAC_Datapath_SETUP, 	0x14 ); // P0_R63: DAC L & R Pwr Off -- pg 65: begins internal sequence
	
		uint8_t pwrLR = 0x88;
		int cnt = 0;
		while ( pwrLR != 0 && cnt < MAX_DAC_PWR_WAIT ){	// wait till DACs power down
			pwrLR = aicGetReg( P0_R37_DAC_Flag_Register ) & 0x88;  // P0_R37: D7=LDACPwrd, D3=RDACPwrd
			cnt++;
		}
		if ( cnt == MAX_DAC_PWR_WAIT ) dbgLog( "! DACs failed to turn off" );

		// now shut down M_DAC & N_DAC dividers and finally, PLL 
		aicSetReg( P0_R11_DAC_NDAC_VAL, 	0x01 ); 	 // P0_R11: NDAC_PWR: 0 reset val-- powers down NDAC
		aicSetReg( P0_R12_DAC_MDAC_VAL, 	0x01 ); 	 // P0_R12: NDAC_PWR: 0 reset val-- powers down MDAC
		aicSetReg( P0_R5_PLL_P_R_VAL, 		0x11 ); 	 // P0_R04: PLL_PWR: 0  PLL_P: 001 PLL_R: 0001 = 0x11   powers down PLL (MUST BE LAST pg 68)
	#endif
	
	I2Cdrv->PowerControl( ARM_POWER_OFF );	// power down I2C
	I2Cdrv->Uninitialize( );								// deconfigures SCL & SDA pins, evt handler
	
	// reset, then power down codec chip
	gSet( gPA_EN, 0 );				// amplifier off
	gSet( gBOOT1_PDN, 0 );    // OUT: set power_down ACTIVE to Power Down the codec 
	gSet( gEN_5V, 0 );				// OUT: 1 to supply 5V to codec		AP6714 EN		
	gSet( gEN1V8, 0 );			  // OUT: 1 to supply 1.8 to codec  TLV74118 EN		
	gSet( gEN_IOVDD_N, 1 );		// power down codec IOVDD PE4
	gSet( gEN_AVDD_N, 1 );		// power down codec AVDD & HPVDD PE5 (at least 10ns after DVDD)
	dbgLog( "2 AIC3100 powered down\n");
}
//
//
static uint8_t testVol = 0x19;			// DEBUG
void		 				cdc_SetVolume( uint8_t Volume ){														// sets volume 0..10  ( mediaplayer )
	uint8_t v = Volume>10? 10 : Volume; 

	LastVolume = v;
	if ( audGetState()!=Playing ) return;			// just remember for next cdc_Init()

	#if defined( AIC3100 )
		const int8_t cdcMUTEVOL = -127, cdcMAXVOL = 48;
		const uint8_t cdcVOLRNG = cdcMAXVOL - cdcMUTEVOL;		// 175 = aic3100 digital volume range to use
		cdcFmtVolume = cdcMUTEVOL + (v * cdcVOLRNG)/10;   // v=0 => -127 -- v=5 => -40 -- v=10 => 48
	#endif
	#if defined( AK4637 )
		const uint8_t cdcMUTEVOL = 0xCC, cdcMAXVOL = 0x18, cdcVOLRNG = cdcMUTEVOL-cdcMAXVOL;		// ak4637 digital volume range to use
		// Conversion of volume from user scale [0:10] to audio codec scale [cdcMUTEVOL..cdcMAXVOL]  (AK cc..18) (TI 81..c0)
		//   values >= 0xCC force mute on AK4637
		//   limit max volume to 0x18 == 0dB (to avoid increasing digital level-- causing resets?)
		cdcFmtVolume = cdcMUTEVOL - ( v * cdcVOLRNG )/10; 
	#endif
	
	if (Volume==99)	//DEBUG
		{ cdcFmtVolume = testVol; 	testVol--; }	//DEBUG: test if vol>akMAXVOL causes problems
	
//	logEvtNI( "setVol", "vol", v );
	dbgEvt( TB_cdcSetVol, Volume, cdcFmtVolume,0,0);
	dbgLog( "2 cdcSetVol v=%d cdcV=0x%x \n", v, cdcFmtVolume );
	#if defined( AIC3100 )
		// AIC3100 -- left channel volume:  -127..48 => -63.5dB .. 24dB
		aicSetReg( P0_R65_DAC_Left_Volume_Control, cdcFmtVolume	 ); 		 // P0_R65: -127..48 (0x81..0x30)
		dbgLog( "2 AIC Lvol %d \n", cdcFmtVolume );
	#endif
	#if defined( AK4343 )
		Codec_WrReg( AK_Lch_Digital_Volume_Control, akFmtVolume );  // Left Channel Digital Volume control
		Codec_WrReg( AK_Rch_Digital_Volume_Control, akFmtVolume );  // Right Channel Digital Volume control
	#endif
	#if defined( AK4637 )
		akR.R.DigVolCtr.DVOL7_0 = akFmtVolume;			
		akUpd();
	#endif
}

void		 				cdc_SetMute( bool muted ){																	// true => enable mute on codec  (audio)
	if ( cdcMuted==muted ) return;
	dbgEvt( TB_cdcSetMute, muted,0,0,0);
	cdcMuted = muted;
	
	#if defined( AIC3100 )
		if ( muted ){		// enable mute -- both channels
			aicSetReg( P1_R42_SPK_Driver, 0x00	 ); 			// P1_R42: SpkrAmpGain: 00  SpkrMuteOff: 0
			aicSetReg( P0_R64_DAC_VOLUME_CONTROL, 0x0C );	// P0_R64: LMuteOn: 1  RMuteOn: 1 LRsep: 00
			dbgLog( "2 AIC mute: SpkrAmp, L&R mutes on \n" );
		} else {	// disable left channel mute
			aicSetReg( P0_R64_DAC_VOLUME_CONTROL, 0x04 );	// P0_R64: LMuteOn: 0  RMuteOn: 1 LRsep: 00
			aicSetReg( P1_R42_SPK_Driver, 0x04	 ); 			// P1_R42: SpkrAmpGain: 00  SpkrMuteOff: 1
			tbDelay_ms( 500 );  // wait to let stabilize? before starting I2S
			dbgLog( "2 AIC unmute: SpkrAmp & L mutes off, SpkrAmp=6dB \n" );
		}
	#endif
//	akR.R.MdCtr3.SMUTE = (muted? 1 : 0);
	i2c_Upd();
}
void 						debugTimingRegs(){																					// configure ClkOut & report Reg values
	if ( !dbgEnab( '2' )) 
		return;
	
	uint8_t regList[] = { 
		P0_R4_ClockGen_Muxing, 	
		P0_R5_PLL_P_R_VAL, 		
		P0_R6_PLL_J_VAL, 					
		P0_R7_PLL_D_VAL_16B, 			
		P0_R8_PLL_D_VAL_LSB,
		P0_R11_DAC_NDAC_VAL, 		
		P0_R12_DAC_MDAC_VAL, 	
		P0_R13_DAC_DOSR_VAL_16B, 	
		P0_R14_DAC_DOSR_VAL_LSB, 	
		P0_R25_CLKOUT_MUX,
		P0_R26_CLKOUT_M_VAL,		
		P0_R27_Codec_Interface_Control1,			
		P0_R29_Codec_Interface_Control2,		
		P0_R30_BCLK_N_VAL,
		P0_R33_Codec_Interface_Control3,
		P0_R37_DAC_Flag_Register, 
		P0_R53_DOUT_Pin_Control, 											
		P0_R63_DAC_Datapath_SETUP,  
		P0_R64_DAC_VOLUME_CONTROL,
		P0_R65_DAC_Left_Volume_Control,
		P1_R32_ClassD_Drivers, 
		P1_R35_LDAC_and_RDAC_Output_Routing,
		P1_R38_Left_Analog_Vol_to_SPL,
		P1_R42_SPK_Driver, 
		P1_R46_MICBIAS
	};
	
	//DEBUG ONLY - configure CLKOUT to DOUT
	// select ClkOut source: 0=MCLK 3=PLL_CLK  4=DAC_CLK  5=DAC_MOD_CLK
	tbDelay_ms(20);			// let debug printf catch up
	if (gGet( gHOME )){  					// HOM => PLL_CLK to ClkOut
		aicSetReg( P0_R25_CLKOUT_MUX, 			0x03 );		
		dbgLog( "2 HOM: PLL_Clk => ClkOut \n");
	} else if (gGet( gCIRCLE)){  	// CIR => DAC_CLK
		aicSetReg( P0_R25_CLKOUT_MUX, 			0x04 );		
		dbgLog( "2 CIR: DAC_Clk => ClkOut \n");
	} else if (gGet( gRHAND)){  	// RH => DAC_MOD_CLK
		aicSetReg( P0_R25_CLKOUT_MUX, 			0x05 );		
		dbgLog( "2 RH: DAC_MOD_Clk => ClkOut \n");
	} else {											// default => MClk
		aicSetReg( P0_R25_CLKOUT_MUX, 			0x00 );				// select ClkOut source: 000=MCLK 011=PLL_CLK  100=DAC_CLK  101=DAC_MOD_CLK
		dbgLog( "2 def: MClk => ClkOut \n");
	}
	const int ClkOutM = 10;
	aicSetReg( P0_R26_CLKOUT_M_VAL, 		0x80 + ClkOutM );  // PWR & set divider for ClkOut /ClkOutM


	if (gGet( gMINUS )){
		aicSetReg( P0_R53_DOUT_Pin_Control,	0x04 );			// MINUS: DOUT gets GPout = 0
		dbgLog( "2 MINUS: DOUT = 0 \n");
	} else if (gGet( gPLUS )){
		aicSetReg( P0_R53_DOUT_Pin_Control,	0x05 );			// PLUS: DOUT gets GPout = 1
		dbgLog( "2 PLUS: DOUT = 1 \n");
	} else {
		aicSetReg( 		P0_R53_DOUT_Pin_Control,0x16 );		// default: DOUT gets CLKOUT (from CLKOUT_MUX)
		dbgLog( "2 def: DOUT = MClk/%d \n", ClkOutM );
	}
		
	for ( int i=0; i < sizeof(regList); i++ ){
		uint8_t ridx = regList[i];
		uint8_t val = aicGetReg( ridx );
		dbgLog( "2 P%dR%02d %12s = %3d 0x%02x \n", codec_regs[ridx].pg, codec_regs[ridx].reg, codec_regs[ridx].nm, val, val );
	}
}

void						cdc_SetMasterFreq( int freq ){															// set AK4637 to MasterMode, 12MHz ref input to PLL, audio @ 'freq', start PLL  (i2s_stm32f4xx)
	#if defined( AIC3100 )
		// choose parameters for codec PLL
		int PLL = 92; // 84=84.672MHz  92=92.160MHz, or 98 = 98.304MHz
		int MDAC = 90, NDAC = 2;  // 0..128 divisors of PLL_CLK 
		switch ( freq ){
				case  8000:	PLL=92; MDAC=90; NDAC= 2; break;
				case 11025:	PLL=84; MDAC= 5; NDAC=24; break;
				case 12000:	PLL=92; MDAC=40; NDAC= 3; break;
				case 16000:	PLL=98; MDAC=16; NDAC= 6; break;
				case 22050:	PLL=84; MDAC= 5; NDAC=12; break;
				case 24000:	PLL=92; MDAC=10; NDAC= 6; break;
				case 32000:	PLL=98; MDAC=16; NDAC= 3; break;
				case 44100:	PLL=84; MDAC= 5; NDAC= 6; break;
				case 48000:	PLL=92; MDAC= 5; NDAC= 6; break;
		}
		// configure PLL for 12MHz MCLK to generate audio at 'freq'
		// PLL_CLK = MCLK * R * J.D / P   (R=1 & P=1)
		int PLL_J, PLL_D;
		if (PLL == 84)		{  PLL_J = 7;  PLL_D = 0560; }  // J.D = 7.0560 * 12MHz => 84.672MHz
		else if (PLL==92)	{  PLL_J = 7;  PLL_D = 6800; }  // J.D = 7.6800 * 12MHz => 92.160MHz
		else if (PLL==98)	{  PLL_J = 8;  PLL_D = 1920; }  // J.D = 8.1920 * 12MHz => 98.304MHz
		dbgLog( "2 AIC3100 PLL= 12MHz * %d.%04d \n", PLL_J, PLL_D );
		// AIC3100 configuration:
		//   MClk is fed 12MHz from STM32F412 I2S3_MCLK-- set up by I2S3_ClockEnable() in I2S_stm32F4xx.c
		//   codec mode I2S 16-bit, output WClk & BClk
		//   external MClk is input to PLL_ClkIn & PLL_Clk as Codec_ClkIn
		
		aicSetReg( 		P0_R27_Codec_Interface_Control1, 0x0C );// P0_R27:  I2S: 00  Wd16: 00 BClkOut: 1 WClkOut: 1  Rx: 0 DOut: 0  = 00001100 = 0x0C;
		aicSetReg( 		P0_R4_ClockGen_Muxing, 	0x03 ); 				// P0_R04: 0000 PLL=MClk: 00 Codec=PLL: 11 = 0x03
		aicSetReg( 		P0_R6_PLL_J_VAL, 				PLL_J 		); 		// P0_R06: PLL_J      ( integer part of PLL multiplier J.D )
		aicSet16Bits( P0_R7_PLL_D_VAL_16B, 		PLL_D );  			// P0_R07/08: PLL_D    fraction D (0..9999)
		aicSetReg( 		P0_R5_PLL_P_R_VAL, 			0x91 ); 		 		// P0_R04: PLL_PWR: 1  PLL_P: 001 PLL_R: 0001 = 0x91   powers up PLL with these parameters
		aicSetReg( 		P0_R11_DAC_NDAC_VAL, 		0x80 + NDAC );	// P0_R11: NDAC_VAL  ( NDAC_PWR + NDAC divider value ) 
		aicSetReg( 		P0_R12_DAC_MDAC_VAL, 		0x80 + MDAC );	// P0_R12: MDAC_VAL  ( MDAC_PWR + MDAC divider value ) 
		aicSet16Bits( P0_R13_DAC_DOSR_VAL_16B, 64 );  				// P0_R13/14: DOSR_VAL = 0x0040   ( for DOSR = 64 )
		aicSetReg( 		P0_R29_Codec_Interface_Control2, 0x01 );// P0_R29: BDIV_CLKIN = DAC_MOD_CLK, BCLK&WCLK not always active
		aicSetReg( 		P0_R33_Codec_Interface_Control3, 0x00 ); // P0_R33: pri BCLK: 0 (internal), pri WCLK: 00 (Dac_fs) pri DOUT: 0 codec
		aicSetReg( 		P0_R30_BCLK_N_VAL, 			0x80 + 2 );  		// P0_R30: BCLK N_VAL = 2  ( with DOSR = 64, BCLK = DAC_FS * 32 )
		dbgLog( "2 AIC: BCLK = CLK / %d / %d / %d = 32* %d \n", NDAC, MDAC, 2, freq );

		debugTimingRegs();
	#endif
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
		tbDelay_ms( 4 ); //DEBUG 2 );											// 2ms for power regulator to stabilize
		akR.R.PwrMgmt2.PMPLL 		= 1;					// enable PLL 
		akUpd();															// start PLL 
		tbDelay_ms( 10 ); //DEBUG 5 );											// 5ms for clocks to stabilize
	#endif
}


//end file **************** ti_aic3100.c
