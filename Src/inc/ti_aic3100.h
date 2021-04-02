/**    ti_aic3100.h
  header for use of TexasInstruments TLV320AIC3100 codec
	https://www.ti.com/product/TLV320AIC3100?dcmp=dsproject&hqs=SLAS667td&#doctype2
******************************************************************************
*/

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __TI_AIC3100_H
#define __TI_AIC3100_H

#include "tbook.h"

#include "audio.h"

//**************************  Codec User defines ******************************/
/* Codec output DEVICE */
#define OUTPUT_DEVICE_OFF							0x00
#define OUTPUT_DEVICE_SPEAKER         0x01
#define OUTPUT_DEVICE_HEADPHONE       0x02
#define OUTPUT_DEVICE_BOTH            (OUTPUT_DEVICE_SPEAKER | OUTPUT_DEVICE_HEADPHONE)

/* Volume Levels values */
#define DEFAULT_VOLMIN                0x00
#define DEFAULT_VOLMAX                0xFF
#define DEFAULT_VOLSTEP               0x04

#define AUDIO_PAUSE                   0
#define AUDIO_RESUME                  1

/* Codec POWER DOWN modes */
#define CODEC_PDWN_HW                 1
#define CODEC_PDWN_SW                 2

/* MUTE commands */
#define AUDIO_MUTE_ON                 1
#define AUDIO_MUTE_OFF                0

#define MAX_VOLUME   10
#define MIN_VOLUME   0
//#define DEFAULT_VOLUME   80

/***************** 	// AIC_init_sequence -- from eval board
struct {		// AIC_init_sequence -- from eval board
	uint8_t pg;
	uint8_t reg; 
	uint8_t val;
} AIC_init_sequence[] = {  // DAC3100 init sequence -- from eval board
//  ------------- page 0 
  0x00,    1, 0x01,	// 	s/w reset
  0x00,    4, 0x07,	// 	PLL_clkin = BCLK,codec_clkin = PLL_CLK	
  0x00,    5, 0x91, //  PLL power up, P=1, R=1
  0x00,    6, 0x20, //  PLL J=32
  0x00,    7, 0x00, //  PLL D= 0/0
  0x00,    8, 0x00, //  PLL D= 0/0
  0x00,   27, 0x00,	// 	mode is i2s,wordlength is 16, BCLK in, WCLK in
  0x00,   11, 0x84,	// 	NDAC is powered up and set to 4	
  0x00,   12, 0x84,	// 	MDAC is powered up and set to 4	
  0x00,   13, 0x00,	// 	DOSR = 128, DOSR(9:8) = 0
  0x00,   14, 0x80,	// 	DOSR(7:0) = 128
  0x00,   64, 0x00,	// 	DAC => volume control thru pin disable 
  0x00,   68, 0x00,	// 	DAC => drc disable, th and hy
  0x00,   65, 0x00,	// 	DAC => 0 db gain left
  0x00,   66, 0x00,	// 	DAC => 0 db gain right

// ------------- page 1
  0x01,   33, 0x4e,	// 	De-pop, Power on = 800 ms, Step time = 4 ms  /// ?values
  0x01,   31, 0xc2,	// 	HPL and HPR powered up
  0x01,   35, 0x44,	// 	LDAC routed to HPL, RDAC routed to HPR
  0x01,   40, 0x0e,	// 	HPL unmute and gain 1db
  0x01,   41, 0x0e,	// 	HPR unmute and gain 1db
  0x01,   36, 0x00,	// 	No attenuation on HPL
  0x01,   37, 0x00,	// 	No attenuation on HPR
  0x01,   46, 0x0b,	// 	MIC BIAS = AVDD
  0x01,   48, 0x40,	// 	MICPGA P-term = MIC 10k  (MIC1LP RIN=10k)
  0x01,   49, 0x40,	// 	MICPGA M-term = CM 10k
	
// ---------- page 0 
  0x00,   60, 0x0b,	// 	select DAC DSP mode 11 & enable adaptive filter  (signal-proc block PRB_P11)
// ---------- page 8 
  0x08,    1, 0x04,	//  Adaptive filtering enabled in DAC processing blocks
// ---------- page 0 
  0x00,   63, 0xd6,	// 	POWERUP DAC left and right channels (soft step disable)
  0x00,   64, 0x00,	// 	UNMUTE DAC left and right channels
	
// ----------- page 1 
  0x01,   38, 0x24,	//	Left Analog NOT routed to speaker
  0x01,   42, 0x04,	// 	Unmute Class-D Left	***** FIXED
  0x01,   32, 0xc6 	// 	Power-up Class-D drivers	) (should be 0x86 for DAC3100)	(SHOULD BE 0x83 for AIC3100
};
*************************/

typedef struct { 		// AIC_reg_def -- definitions for TLV320AIC3100 registers to that are referenced
	const uint8_t pg;						// register page index, 0..15
	const uint8_t reg;					// register number 0..127
	const uint8_t reset_val;
	const uint8_t W1_allowed;		// mask of bits that can be written as 1
	const uint8_t W1_required;  //  bits that MUST be written as 1
	const char * nm;
	uint8_t prev_val;			// for reporting changes
	uint8_t curr_val;			// last value written to (or read from) this register of chip
	} AIC_REG;

typedef __packed struct { // pg=0  reg=4 
		unsigned int CODEC_IN		: 2;		// d0..1
		unsigned int PLL_IN			: 2;		// d2..3
		unsigned int z4 				: 4;		// d4..7 == 0
} TI_ClkGenMux_val;

typedef __packed struct { // pg=0  reg=5 
		unsigned int PLL_R			: 4;		// d0..3
		unsigned int PLL_P			: 3;		// d4..6
		unsigned int PLL_PWR		: 1;		// d7  1 => PLL on
} TI_PllPR_val;

typedef __packed struct { // pg=0  reg=6 
		unsigned int PLL_J			: 6;		// d0..5
		unsigned int z2					: 2;		// d6..7
} TI_PLL_J_val;
	


// audio codec functions --  aic3100
void 						cdc_Init( void );											// init I2C & codec
void						I2C_Reinit(int lev );									// lev&1 SWRST, &2 => RCC reset, &4 => device re-init
void						cdc_PowerUp( void );										// supply power to codec, & disable PDN, before re-init
void 						cdc_PowerDown( void );									// power down entire codec, requires re-init
void		 				cdc_SetVolume( uint8_t Volume );				// 0..10
void		 				cdc_SetMute( bool muted );							// for temporarily muting, while speaker powered
void 						cdc_SetOutputMode( uint8_t Output );
void 						cdc_SpeakerEnable( bool enable );			// power up/down lineout, DAC, speaker amp
void						cdc_SetMasterFreq( int freq );					// set aic3100 to MasterMode, 12MHz ref input to PLL, audio @ 'freq'
void						cdc_MasterClock( bool enable );				// enable/disable PLL (Master Clock from codec) -- fill DataReg before enabling 
void						cdc_RecordEnable( bool enable );				// power up/down microphone amp, ADC, ALC, filters
#endif /* __TI_AIC3100_H */
// end file ti_aic3100.h
