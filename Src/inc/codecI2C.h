/**    codecI2C.h
  header for I2C interface functions
******************************************************************************
*/

#ifndef CODEC_I2C_H
#define CODEC_I2C_H

#include "tbook.h"

// Define this to build debugging code into the codec handler.
#define DEBUG_CODEC

#include "ti_aic3100.h"


// I2C utilities for communicating with audio codecs --  ak4343 or ak4637 or aic3100
void            i2c_Upd( void );                      // write values of any modified codec registers

void            i2c_wrReg( uint8_t Reg, uint8_t Value); // external from codecI2C
uint8_t         i2c_rdReg( uint8_t Reg );               // external from codecI2C

void            i2c_ReportErrors( void );             // report I2C error counts

void            i2c_Init( void );                     // init I2C & codec
void            i2c_Reinit(int lev );                 // lev&1 SWRST, &2 => RCC reset, &4 => device re-init
void            i2c_PowerUp( void );                  // supply power to codec, & disable PDN, before re-init
void            i2c_PowerDown( void );                // power down entire codec, requires re-init
void            i2c_SetVolume( uint8_t Volume );      // 0..10
void            i2c_SetMute( bool muted );            // for temporarily muting, while speaker powered
void            i2c_SetOutputMode( uint8_t Output );
void            i2c_SpeakerEnable( bool enable );     // power up/down lineout, DAC, speaker amp
void            i2c_SetMasterFreq( int freq );        // set AK4637 to MasterMode, 12MHz ref input to PLL, audio @ 'freq'
void            i2c_MasterClock( bool enable );       // enable/disable PLL (Master Clock from codec) -- fill DataReg before enabling 
void            i2c_RecordEnable( bool enable );      // power up/down microphone amp, ADC, ALC, filters

#if defined(DEBUG_CODEC)

void            i2c_CheckRegs( void );                // Debug -- read codec regs
void            cntErr( int8_t grp, int8_t exp, int8_t got, int32_t iReg, int16_t caller );  // count and/or report ak errors
#define CDC_DBG_LOG(...) dbgLog(__VA_ARGS__)
#define CDC_DBG_LOG_IF(condition, args...) while(condition){dbgLog(args);break;}

#else // DEBUG_CODEC

// Empty code that looks like a statement. Compiler will optimize.
#define i2c_CheckRegs(...) while(false){}
#define cntErr(...) while(false){}
#define CDC_DBG_LOG(...) while(false){}
#define CDC_DBG_LOG_IF(...) while(false){}

#endif // DEBUG_CODEC

#endif // CODEC_I2C_H
