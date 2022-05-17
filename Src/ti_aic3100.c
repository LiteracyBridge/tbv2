//******* ti_aic3100.c   
//   driver for TI TLV320AIC3100 codec
// converted to use CMSIS-Driver/I2C   JEB 12/2019


/* Includes ------------------------------------------------------------------*/
#include "tbook.h"          // defines TBook_V2_Rev3

#include "codecI2C.h"       // I2C access utilities
#include "Driver_I2C.h"     // CMSIS-Driver I2C interface
#include "Driver_SAI.h"     // CMSIS-Driver SPI interface

//*****************  CMSIS I2C driver interface
extern ARM_DRIVER_I2C     Driver_I2C1;
static ARM_DRIVER_I2C *   I2Cdrv =        &Driver_I2C1;

#if !defined (VERIFY_WRITTENDATA)  
// Uncomment this line to enable verifying data sent to codec after each write operation (for debug purpose)
#define VERIFY_WRITTENDATA 
#endif /* VERIFY_WRITTENDATA */

// TODO: Why was this static? It is used only as a temporary, in only one function, in only this file.
//static int8_t   cdcFmtVolume;
static bool     cdcSpeakerOn    = false;
static bool     cdcMuted        = false;

static int      aicCurrPg = 0;      // keeps track of currently selected register page
static int      WatchReg   = 0;     //DEBUG: log register read/write ops for a specific codec register

// AIC_REG{ pg, reg, reset_val, W1_allowed, W1_required, nm, prev_val, curr_val }
AIC_REG         codec_regs[] = {  // array of info for codec registers in use -- must include Page_Select for all pages in use
//********** AIC3100 PAGE 0
// pg,reg, reset, can1, must1,  "nm",   prev, curr
   0,   0, 0x00, 0xFF, 0x00, "psr0",      0, 0x00,  // 0=0.0  must include Page_Select for all pages in use
   0,   1, 0x00, 0x01, 0x00, "s_rst",     0, 0x00,  // 1=0.1
   0,   2, 0x01, 0x00, 0xff, "rsv",       0, 0x01,  // actual: 0:02  0x01 not 0x00 // reserved-- reset = XX, can1=0x00, must1=0xff
   0,   3, 0x66, 0x02, 0x00, "ot_flg",    0, 0x66,  // X bits D2, D5,D6 set in practice-- actual: 0x66 not 0x02
   0,   4, 0x00, 0x0F, 0x00, "clk_mux",   0, 0x00,  // 4=0.4
   0,   5, 0x11, 0xFF, 0x00, "pll_p_r",   0, 0x11,  
   0,   6, 0x04, 0x3F, 0x00, "pll_j",     0, 0x04,  
   0,   7, 0x00, 0x3F, 0x00, "pll_dhi",   0, 0x00,  
   0,   8, 0x00, 0xFF, 0x00, "pll_dlo",   0, 0x00,
   0,   9, 0x00, 0x00, 0xff, "rsv",       0, 0x00,  // reserved-- reset = XX, can1=0x00, must1=0xff
   0,  10, 0x00, 0x00, 0xff, "rsv",       0, 0x00,  // reserved-- reset = XX, can1=0x00, must1=0xff
   0,  11, 0x01, 0xFF, 0x00, "dac_n",     0, 0x01,  
   0,  12, 0x01, 0xFF, 0x00, "dac_m",     0, 0x01,  
   0,  13, 0x00, 0x03, 0x00, "dosr_hi",   0, 0x00,
   0,  14, 0x80, 0xFF, 0x00, "dosr_lo",   0, 0x80,  
   0,  15, 0x80, 0xFF, 0x00, "dac_idac",  0, 0x80,  
   0,  16, 0x08, 0x0F, 0x00, "dac_mac_en",0, 0x08, 
   0,  17, 0x00, 0x00, 0xff, "rsv",       0, 0x00,  // reserved-- reset = XX, can1=0x00, must1=0xff
   0,  18, 0x01, 0xFF, 0x00, "adc_n",     0, 0x01, 
   0,  19, 0x01, 0xFF, 0x00, "adc_m",     0, 0x01,  
   0,  20, 0x80, 0xFF, 0x00, "aosr",      0, 0x80,  
   0,  21, 0x80, 0xFF, 0x00, "adc_iadc",  0, 0x80,  
   0,  22, 0x04, 0xFF, 0x00, "adc_prb",   0, 0x04,  // use actual: 0x04 not 0x80 (reserved)
   0,  23, 0x00, 0x00, 0xff, "rsv",       0, 0x00,  // reserved-- reset = XX, can1=0x00, must1=0xff
   0,  24, 0x00, 0x00, 0xff, "rsv",       0, 0x00,  // reserved-- reset = XX, can1=0x00, must1=0xff
   0,  25, 0x00, 0x07, 0x00, "out_mux",   0, 0x00,
   0,  26, 0x01, 0xFF, 0x00, "out_m",     0, 0x01,  
   0,  27, 0x00, 0xFD, 0x00, "i2s",       0, 0x00,
   0,  28, 0x00, 0xff, 0x00, "d_off",     0, 0x00,
   0,  29, 0x00, 0x3F, 0x00, "bclk",      0, 0x00,
   0,  30, 0x01, 0xFF, 0x00, "bclk_n",    0, 0x01,
   0,  31, 0x00, 0xff, 0x00, "cdc2_ctl1", 0, 0x00,
   0,  32, 0x00, 0xff, 0x00, "cdc2_ctl2", 0, 0x00,
   0,  33, 0x00, 0xFF, 0x00, "b&w_clks",  0, 0x00,   
   0,  34, 0x00, 0x20, 0x00, "i2c_bus",   0, 0x00,
   0,  35, 0x00, 0x00, 0xff, "rsv",       0, 0x00,  // reserved-- reset = XX, can1=0x00, must1=0xff
   0,  36, 0x80, 0x00, 0x00, "adc_flg",   0, 0x80,  // use actual: 0x80 not 0x00
   0,  37, 0x00, 0x00, 0x00, "dac_flg",   0, 0x00,  // 37=0.37
   0,  38, 0x00, 0x00, 0x00, "dac_flg2",  0, 0x00,
   0,  39, 0x00, 0x00, 0x00, "ovfl_flg",  0, 0x00,
   0,  40, 0x00, 0x00, 0xff, "rsv",       0, 0x00,  // 40=0.40  reserved-- reset = XX, can1=0x00, must1=0xff
   // dup 41..43
   0,  44, 0x00, 0x00, 0x00, "dac_ints",  0, 0x00,  // 41=0.44
   0,  45, 0x00, 0x00, 0x00, "adc_ints",  0, 0x00,
   0,  46, 0x00, 0x00, 0x00, "dac_ints2", 0, 0x00,
   0,  47, 0x00, 0x00, 0x00, "adc_ints2", 0, 0x00,
   0,  48, 0x00, 0xff, 0x00, "int_ctl1",  0, 0x00,
   0,  49, 0x00, 0xff, 0x00, "int_ctl2",  0, 0x00,
   0,  50, 0x00, 0x00, 0xff, "rsv",       0, 0x00,  // reserved-- reset = XX, can1=0x00, must1=0xff
   0,  51, 0x02, 0xfd, 0x00, "gpio_pin",  0, 0x02,  // actual: 0:51  0x02 not 0x00
   0,  52, 0x32, 0x00, 0xff, "rsv",       0, 0x00,  // actual: 0:52  0x32 not 0x00  //reserved-- reset = XX, can1=0x00, must1=0xff
   0,  53, 0x12, 0x1F, 0x00, "d_out",     0, 0x12,  // 50=0.53
   0,  54, 0x03, 0xfe, 0x00, "d_in",      0, 0x02,  // actual: 0:54  0x03 not 0x02
   0,  55, 0x02, 0x00, 0xff, "rsv",       0, 0x00,  // reserved-- reset = XX, can1=0x00, must1=0xff
   // dup 56..59
   0,  60, 0x01, 0x3F, 0x00, "dac_prb",   0, 0x01,  // 53=0.60
   0,  61, 0x04, 0x1F, 0x00, "adc_prb",   0, 0x04,  
   0,  62, 0x00, 0x77, 0x00, "pr_ctrl",   0, 0x00,    // Programmable_Instruction_Mode_Control
   0,  63, 0x14, 0xFF, 0x00, "dac_path",  0, 0x14,  // 56=0.63
   0,  64, 0x0C, 0x0F, 0x00, "dac_vol",   0, 0x0C,
   0,  65, 0x00, 0xFF, 0x00, "dac_l_vol", 0, 0x00,
   0,  66, 0x00, 0xFF, 0x00, "dac_r_vol", 0, 0x00,
   0,  67, 0x00, 0x9F, 0x00, "hp_det",    0, 0x00,  // 60=0.67
   0,  68, 0x6F, 0x7F, 0x00, "drc_ctl",   0, 0x6F,  // use actual reset 0x6f (DRC enabled)
   0,  69, 0x38, 0x78, 0x00, "drc_ctl2",  0, 0x38,  
   0,  70, 0x00, 0xff, 0x00, "drc_ctl3",  0, 0x00, 
   0,  71, 0x00, 0xbf, 0x00, "l_beep",    0, 0x00, 
   0,  72, 0x00, 0xff, 0x00, "r_beep",    0, 0x00, 
   0,  73, 0x00, 0xff, 0x00, "bp_ln_msb", 0, 0x00, 
   0,  74, 0x00, 0xff, 0x00, "bp_ln_mid", 0, 0x00, 
   0,  75, 0xee, 0xff, 0x00, "bp_ln_lsb", 0, 0x00,
   0,  76, 0x10, 0xff, 0x00, "bp_sin_msb",0, 0x10, 
   0,  77, 0xd8, 0xff, 0x00, "bp_sin_lsb",0, 0xd8,  // 70=0.77
   0,  78, 0x7e, 0xff, 0x00, "bp_cos_msb",0, 0x7e, 
   0,  79, 0xe3, 0xff, 0x00, "bp_cos_lsb",0, 0xe3, 
   0,  80, 0x00, 0x00, 0xff, "rsv",       0, 0x00,  // reserved-- reset = XX, can1=0x00, must1=0xff
// pg,reg, reset, can1, must1,  "nm",   prev, curr
   0,  81, 0x00, 0xBB, 0x00, "adc_pwr",   0, 0x00,  // 74=0.81
   0,  82, 0x80, 0xF0, 0x00, "adc_sm_vol",0, 0x80, 
   0,  83, 0x00, 0x7F, 0x00, "adc_vol",   0, 0x00, 
   0,  84, 0x00, 0x00, 0xff, "rsv",       0, 0x00,  // reserved-- reset = XX, can1=0x00, must1=0xff
   0,  85, 0x00, 0x00, 0xff, "rsv",       0, 0x00,  // reserved-- reset = XX, can1=0x00, must1=0xff
   0,  86, 0x00, 0xF0, 0x00, "agc_tgt",   0, 0x00,  // 79=0.86
   0,  87, 0x00, 0xFE, 0x00, "agc_ctl2",  0, 0x00,  // 80=0.87
   0,  88, 0x7F, 0x7F, 0x00, "agc_maxg",  0, 0x7F, 
   0,  89, 0x00, 0xFF, 0x00, "agc_atak",  0, 0x00, 
   0,  90, 0x00, 0xFF, 0x00, "agc_decy",  0, 0x00, 
   0,  91, 0x00, 0x1F, 0x00, "agc_nois",  0, 0x00, 
   0,  92, 0x00, 0x0F, 0x00, "agc_sig",   0, 0x00,  // 85=0.92
   0,  93, 0x00, 0x00, 0x00, "agc_gain",  0, 0x00, 
   0,  94, 0x00, 0x00, 0xff, "rsv",       0, 0x00,  // reserved-- reset = XX, can1=0x00, must1=0xff
   // dup 95..101
   0, 102, 0x00, 0xBF, 0x00, "adc_meas",  0, 0x00,  // 88=0.102  // discovered by Marc
   0, 103, 0x00, 0x7F, 0x00, "adc_meas2", 0, 0x00,
   0, 104, 0x00, 0x00, 0x00, "dc_meas1",  0, 0x00,  // 90=0.104
   0, 105, 0x00, 0x00, 0x00, "dc_meas2",  0, 0x00,
   0, 106, 0x00, 0x00, 0x00, "dc_meas3",  0, 0x00,
   0, 107, 0x00, 0x00, 0xff, "rsv",       0, 0x00,  // 93=0.107 // reserved-- reset = XX, can1=0x00, must1=0xff
   // dup 108..115
   0, 116, 0x00, 0xf7, 0x00, "mdet_vol",  0, 0x00,  // 94=0.116
   0, 117, 0xFF, 0x80, 0x00, "mdet_gn",   0, 0x00,  // reset: 0xxx xxxx
   0, 118, 0x00, 0x00, 0xff, "rsv",       0, 0x00,  // 96=0.118 // reserved-- reset = XX, can1=0x00, must1=0xff
   // dup 119..127

// ********** AIC3100 PAGE 1
// pg,reg, reset, can1, must1,  "nm",   prev, curr
   1,   0, 0x01, 0x00, 0x00, "psr1",      0, 0x01,  // 97=1.0  // 1:0 reads as 0x01
   1,   1, 0x00, 0x00, 0xff, "rsv",       0, 0x00,  // reserved-- reset = XX, can1=0x00, must1=0xff
   // dup 2..29
   1,  30, 0x00, 0x03, 0x00, "err_ctl",   0, 0x00,  // 99=1.30 // Not Used by init
   1,  31, 0x04, 0xDE, 0x04, "dp_dr",     0, 0x04,  // 100=1.31
   1,  32, 0x06, 0xFE, 0x06, "cls_d",     0, 0x06, 
   1,  33, 0x3E, 0xFF, 0x00, "pop_rem",   0, 0x3E, 
   1,  34, 0x00, 0xFF, 0x00, "pga_ramp",  0, 0x00, 
   1,  35, 0x00, 0xFF, 0x00, "dac_rout",  0, 0x00,
   1,  36, 0x7F, 0xFF, 0x00, "l_vol_hp",  0, 0x7F,  // 105=1.36
   1,  37, 0x7F, 0xFF, 0x00, "r_vol_hp",  0, 0x7F,
   1,  38, 0x7F, 0xFF, 0x00, "l_vol_spk", 0, 0x7F,
   1,  39, 0x7F, 0xFF, 0x00, "r_vol_spk", 0, 0x7F,  // 108=1.39
   1,  40, 0x02, 0x7E, 0x02, "hpl_dr",    0, 0x02,
   1,  41, 0x02, 0x7E, 0x02, "hpl_dr",    0, 0x02,  // 110=1.41
   1,  42, 0x00, 0x1D, 0x00, "spk_dr",    0, 0x00,
   1,  43, 0x00, 0x00, 0xff, "rsv",       0, 0x00,    // reserved-- reset = XX, can1=0x00, must1=0xff
   1,  44, 0x20, 0xFF, 0x00, "hp_dr",     0, 0x20,    // use actual: 0x20 not 0x00 (debounce 8usec)
   1,  45, 0x86, 0x00, 0xff, "rsv",       0, 0x00,  // actual: 1:45  0x86 not 0x00   // reserved-- reset = XX, can1=0x00, must1=0xff
   1,  46, 0x00, 0x8B, 0x00, "mic_bias",  0, 0x00,  // 115=1.46
   1,  47, 0x80, 0xFF, 0x00, "mic_gain",  0, 0x80,
   1,  48, 0x00, 0xFC, 0x00, "adc_in_p",  0, 0x00,  // 117=1.48
   1,  49, 0x00, 0xF0, 0x00, "adc_in_m",  0, 0x00,  // 118=1.49
   1,  50, 0x00, 0xE0, 0x00, "adc_in_cm", 0, 0x00,  
   1,  51, 0x00, 0x00, 0xff, "rsv",       0, 0x00,  // 120=1.51  // reserved-- reset = XX, can1=0x00, must1=0xff
   // dup 52..127

 
// ********** AIC3100 PAGE 3
// pg,reg, reset, can1, must1,  "nm",   prev, curr
   3,   0, 0x03, 0x00, 0x00, "psr3",      0, 0x03,  // 121=3.0 3:0 reads as 0x03
   3,   1, 0x00, 0x00, 0xff, "rsv",       0, 0x00,  // reserved-- reset = XX, can1=0x00, must1=0xff
   // dup 2..15
   3,  16, 0x81, 0xff, 0x00, "mclk_div",  0, 0x81,  // 123=3.16
   3,  17, 0x81, 0x00, 0xff, "rsv",       0, 0x00,  // actual: 3:17  0x81 not 0x00  // reserved-- reset = XX, can1=0x00, must1=0xff
   // dup 18..127
// ********** AIC3100 PAGE 4
// pg,reg, reset, can1, must1,  "nm",   prev, curr
   4,   0, 0x04, 0x00, 0x00, "psr4",      0, 0x04,  // 125=4.0  4:0 reads as 0x04
   4,   1, 0x00, 0x00, 0xff, "rsv",       0, 0x00,  // reserved-- reset = XX, can1=0x00, must1=0xff
   4,   2, 0x01, 0xff, 0x00, "n0_lpf_h",    0, 0x01,
   4,   3, 0x17, 0xff, 0x00, "n0_lpf_l",    0, 0x17,
   4,   4, 0x01, 0xff, 0x00, "n1_lpf_h",    0, 0x01,
   4,   5, 0x17, 0xff, 0x00, "n1_lpf_l",    0, 0x17, // 130=4.5
   4,   6, 0x7d, 0xff, 0x00, "d1_lpf_h",    0, 0x7d,
   4,   7, 0xd3, 0xff, 0x00, "d1_lpf_l",    0, 0xd3,
   4,   8, 0x7f, 0xff, 0x00, "n0_iir_h",    0, 0x7f,
   4,   9, 0xff, 0xff, 0x00, "n0_iir_l",    0, 0xff,
   4,  10, 0x00, 0xff, 0x00, "iir",         0, 0x00,
   // dup 11..13
   4,  14, 0x7f, 0xff, 0x00, "n0_bqa_h",    0, 0x7f,
   4,  15, 0xff, 0xff, 0x00, "n0_bqa_l",    0, 0xff,
   4,  16, 0x00, 0xff, 0x00, "bqa",         0, 0x00,
   // dup 17..23
   4,  24, 0x7f, 0xff, 0x00, "n0_bqb_h",    0, 0x7f,
   4,  25, 0xff, 0xff, 0x00, "n0_bqb_l",    0, 0xff, // 140=4.25
   4,  26, 0x00, 0xff, 0x00, "bqb",         0, 0x00,
   // dup 27..33
   4,  34, 0x7f, 0xff, 0x00, "n0_bqc_h",    0, 0x7f,
   4,  35, 0xff, 0xff, 0x00, "n0_bqc_l",    0, 0xff,
   4,  36, 0x00, 0xff, 0x00, "bqc",         0, 0x00,
   // dup 37..43
   4,  44, 0x7f, 0xff, 0x00, "n0_bqd_h",    0, 0x7f,
   4,  45, 0xff, 0xff, 0x00, "n0_bqd_l",    0, 0xff,
   4,  46, 0x00, 0xff, 0x00, "bqd",         0, 0x00,
   // dup 47..53
   4,  54, 0x7f, 0xff, 0x00, "n0_bqe_h",    0, 0x7f,
   4,  55, 0xff, 0xff, 0x00, "n0_bqe_l",    0, 0xff,
   4,  56, 0x00, 0xff, 0x00, "bqe",         0, 0x00, // 150=4.56
   // dup 57..127


// ********** AIC3100 PAGE 8
// pg,reg, reset, can1, must1,  "nm",   prev, curr
   8,   0, 0x08, 0x00, 0x00, "psr8",      0, 0x08,   // 151=8.0  // 8:0 reads as 0x08
   8,   1, 0x00, 0x05, 0x00, "dac_mode",  0, 0x00,

   8,   2, 0x7f, 0xff, 0x00, "dac_bqa_h", 0, 0x7f,
   8,   3, 0xff, 0xff, 0x00, "dac_bqa_l", 0, 0xff,
   8,   4, 0x00, 0xff, 0x00, "dac_bqa",   0, 0x00,
   // dup 5..11
   8,  12, 0x7f, 0xff, 0x00, "dac_bqb_h", 0, 0x7f,
   8,  13, 0xff, 0xff, 0x00, "dac_bqb_l", 0, 0xff,
   8,  14, 0x00, 0xff, 0x00, "dac_bqb",   0, 0x00,
   // dup 15..21
   8,  22, 0x7f, 0xff, 0x00, "dac_bqc_h", 0, 0x7f,
   8,  23, 0xff, 0xff, 0x00, "dac_bqc_l", 0, 0xff,   // 160=8.23
   8,  24, 0x00, 0xff, 0x00, "dac_bqc",   0, 0x00,
   // dup 25..31
   8,  32, 0x7f, 0xff, 0x00, "dac_bqd_h", 0, 0x7f,
   8,  33, 0xff, 0xff, 0x00, "dac_bqd_l", 0, 0xff,
   8,  34, 0x00, 0xff, 0x00, "dac_bqd",   0, 0x00,
   // dup 35..41
   8,  42, 0x7f, 0xff, 0x00, "dac_bqe_h", 0, 0x7f,
   8,  43, 0xff, 0xff, 0x00, "dac_bqe_l", 0, 0xff,
   8,  44, 0x00, 0xff, 0x00, "dac_bqe",   0, 0x00,
   // dup 45..51
   8,  52, 0x7f, 0xff, 0x00, "dac_bqf_h", 0, 0x7f,
   8,  53, 0xff, 0xff, 0x00, "dac_bqf_l", 0, 0xff,
   8,  54, 0x00, 0xff, 0x00, "dac_bqf",   0, 0x00,  // 170=8.54
   // dup 55..61
   8,  62, 0x00, 0x00, 0xff, "rsv",       0, 0x00,  // reserved-- reset = XX, can1=0x00, must1=0xff
   8,  63, 0x00, 0x00, 0xff, "rsv",       0, 0x00,  // reserved-- reset = XX, can1=0x00, must1=0xff
   8,  64, 0x00, 0xff, 0x00, "pga_gn_h",  0, 0x00,
   8,  65, 0x00, 0xff, 0x00, "pga_gn_l",  0, 0x00,
   8,  66, 0x7f, 0xff, 0x00, "dac_rbqa_h",0, 0x7f,
   8,  67, 0xff, 0xff, 0x00, "dac_rbqa_l",0, 0xff,
   8,  68, 0x00, 0xff, 0x00, "dac_rbqa",  0, 0x00,
   // dup 69..75
   8,  76, 0x7f, 0xff, 0x00, "dac_rbqb_h",0, 0x7f,
   8,  77, 0xff, 0xff, 0x00, "dac_rbqb_l",0, 0xff,
   8,  78, 0x00, 0xff, 0x00, "dac_rbqb",  0, 0x00,  // 180=8.78
   // dup 79..85
   8,  86, 0x7f, 0xff, 0x00, "dac_rbqc_h",0, 0x7f,
   8,  87, 0xff, 0xff, 0x00, "dac_rbqc_l",0, 0xff,
   8,  88, 0x00, 0xff, 0x00, "dac_rbqc",  0, 0x00,
   // dup 89..95
   8,  96, 0x7f, 0xff, 0x00, "dac_rbqd_h",0, 0x7f,
   8,  97, 0xff, 0xff, 0x00, "dac_rbqd_l",0, 0xff,
   8,  98, 0x00, 0xff, 0x00, "dac_rbqd",  0, 0x00,
   // dup 99..105
   8, 106, 0x7f, 0xff, 0x00, "dac_rbqe_h",0, 0x7f,
   8, 107, 0xff, 0xff, 0x00, "dac_rbqe_l",0, 0xff,
   8, 108, 0x00, 0xff, 0x00, "dac_rbqe",  0, 0x00,
   // dup 109..115
   8, 116, 0x7f, 0xff, 0x00, "dac_rbqf_h",0, 0x7f,  // 190=8.116
   8, 117, 0xff, 0xff, 0x00, "dac_rbqf_l",0, 0xff,
   8, 118, 0x00, 0xff, 0x00, "dac_rbqf",  0, 0x00,
   // dup 119..125
   8, 126, 0x00, 0x00, 0xff, "rsv",       0, 0x00,  // reserved-- reset = XX, can1=0x00, must1=0xff
   8, 127, 0x00, 0x00, 0xff, "rsv",       0, 0x00,  // reserved-- reset = XX, can1=0x00, must1=0xff
// ********** AIC3100 PAGE 9
// pg,reg, reset, can1, must1,  "nm",   prev, curr
   9,   0, 0x09, 0x00, 0x00, "psr9",      0, 0x09,  // 195=9.0   9:0 reads as 0x09
   9,   1, 0x00, 0x00, 0xff, "rsv",       0, 0x00,  // reserved-- reset = XX, can1=0x00, must1=0xff
   9,   2, 0x7f, 0xff, 0x00, "ldac_iir_h",0, 0x7f,
   9,   3, 0xff, 0xff, 0x00, "ldac_iir_l",0, 0xff,
   9,   4, 0x00, 0xff, 0x00, "ldac_iir",  0, 0x00,
   // dup 5..7
   9,   8, 0x7f, 0xff, 0x00, "rdac_iir_h",0, 0x7f,  // 200=9.8
   9,   9, 0xff, 0xff, 0x00, "rdac_iir_l",0, 0xff,
   9,  10, 0x00, 0xff, 0x00, "rdac_iir",  0, 0x00,
   // dup 11..13
   9,  14, 0x7f, 0xff, 0x00, "drc_hn0_h", 0, 0x7f,
   9,  15, 0xf7, 0xff, 0x00, "drc_hn0_l", 0, 0xf7,
   9,  16, 0x80, 0xff, 0x00, "drc_hn1_h", 0, 0x80,
   9,  17, 0x09, 0xff, 0x00, "drc_hn1_l", 0, 0x09,
   9,  18, 0x7f, 0xff, 0x00, "drc_hd1_h", 0, 0x7f,
   9,  19, 0xef, 0xff, 0x00, "drc_hd1_l", 0, 0xef,
   9,  20, 0x00, 0xff, 0x00, "drc_ln0_h", 0, 0x00,
   9,  21, 0x11, 0xff, 0x00, "drc_ln0_l", 0, 0x11,  // 210=9.21
   9,  22, 0x00, 0xff, 0x00, "drc_ln1_h", 0, 0x00,
   9,  23, 0x11, 0xff, 0x00, "drc_ln1_l", 0, 0x11,
   9,  24, 0x7f, 0xff, 0x00, "drc_ld1_h", 0, 0x7f,
   9,  25, 0xde, 0xff, 0x00, "drc_ld1_l", 0, 0xde,
   9,  26, 0x00, 0x00, 0xff, "rsv",       0, 0x00,  //215 // reserved-- reset = XX, can1=0x00, must1=0xff
   // dup 27..127
// ********** AIC3100 PAGE 12
// pg,reg, reset, can1, must1,  "nm",   prev, curr
  12,  0, 0x0c, 0x00, 0x00, "psr12",      0, 0x0c,  // 216=12.0   12:0 reads as 0x0c
  12,  1, 0x00, 0x00, 0xff, "rsv",        0, 0x00,  // reserved-- reset = XX, can1=0x00, must1=0xff
  12,  2, 0x7f, 0xff, 0x00, "ldac_bqa_h", 0, 0x7f,
  12,  3, 0xff, 0xff, 0x00, "ldac_bqa_l", 0, 0xff,
  12,  4, 0x00, 0xff, 0x00, "ldac_bqa",   0, 0x00,  // 220
   // dup 5..11
  12, 12, 0x7f, 0xff, 0x00, "ldac_bqb_h", 0, 0x7f,
  12, 13, 0xff, 0xff, 0x00, "ldac_bqb_l", 0, 0xff,
  12, 14, 0x00, 0xff, 0x00, "ldac_bqb",   0, 0x00,
   // dup 15..21
  12, 22, 0x7f, 0xff, 0x00, "ldac_bqc_h", 0, 0x7f,
  12, 23, 0xff, 0xff, 0x00, "ldac_bqc_l", 0, 0xff,
  12, 24, 0x00, 0xff, 0x00, "ldac_bqc",   0, 0x00,
   // dup 25..31
  12, 32, 0x7f, 0xff, 0x00, "ldac_bqd_h", 0, 0x7f,
  12, 33, 0xff, 0xff, 0x00, "ldac_bqd_l", 0, 0xff,
  12, 34, 0x00, 0xff, 0x00, "ldac_bqd",   0, 0x00,
   // dup 35..41
  12, 42, 0x7f, 0xff, 0x00, "ldac_bqe_h", 0, 0x7f,  // 230
  12, 43, 0xff, 0xff, 0x00, "ldac_bqe_l", 0, 0xff,
  12, 44, 0x00, 0xff, 0x00, "ldac_bqe",   0, 0x00,
   // dup 45..51
  12, 52, 0x7f, 0xff, 0x00, "ldac_bqf_h", 0, 0x7f,
  12, 53, 0xff, 0xff, 0x00, "ldac_bqf_l", 0, 0xff,
  12, 54, 0x00, 0xff, 0x00, "ldac_bqf",   0, 0x00,
   // dup 55..61
  12, 62, 0x00, 0x00, 0xff, "rsv",        0, 0x00,  // reserved-- reset = XX, can1=0x00, must1=0xff
  12, 63, 0x00, 0x00, 0xff, "rsv",        0, 0x00,  // reserved-- reset = XX, can1=0x00, must1=0xff
  12, 64, 0x00, 0xff, 0x00, "pga_gn_h",   0, 0x00,
  12, 65, 0x00, 0xff, 0x00, "pga_gn_l",   0, 0x00,
  12, 66, 0x7f, 0xff, 0x00, "dac_rbqa_h", 0, 0x7f,  // 240
  12, 67, 0xff, 0xff, 0x00, "dac_rbqa_l", 0, 0xff,
  12, 68, 0x00, 0xff, 0x00, "dac_rbqa",   0, 0x00,
   // dup 69..75
  12, 76, 0x7f, 0xff, 0x00, "dac_rbqb_h", 0, 0x7f,
  12, 77, 0xff, 0xff, 0x00, "dac_rbqb_l", 0, 0xff,
  12, 78, 0x00, 0xff, 0x00, "dac_rbqb",   0, 0x00,
   // dup 79..85
  12, 86, 0x7f, 0xff, 0x00, "dac_rbqc_h", 0, 0x7f,
  12, 87, 0xff, 0xff, 0x00, "dac_rbqc_l", 0, 0xff,
  12, 88, 0x00, 0xff, 0x00, "dac_rbqc",   0, 0x00,
   // dup 89..95
  12, 96, 0x7f, 0xff, 0x00, "dac_rbqd_h", 0, 0x7f,
  12, 97, 0xff, 0xff, 0x00, "dac_rbqd_l", 0, 0xff,  // 250
  12, 98, 0x00, 0xff, 0x00, "dac_rbqd",   0, 0x00,
   // dup 99..105
  12, 106, 0x7f, 0xff, 0x00, "dac_rbqe_h",0, 0x7f,
  12, 107, 0xff, 0xff, 0x00, "dac_rbqe_l",0, 0xff,
  12, 108, 0x00, 0xff, 0x00, "dac_rbqe",  0, 0x00,
   // dup 109..115
  12, 116, 0x7f, 0xff, 0x00, "dac_rbqf_h",0, 0x7f,
  12, 117, 0xff, 0xff, 0x00, "dac_rbqf_l",0, 0xff,
  12, 118, 0x00, 0xff, 0x00, "dac_rbqf",  0, 0x00,
   // dup 119..125
  12, 126, 0x00, 0x00, 0xff, "rsv",       0, 0x00,  // reserved-- reset = XX, can1=0x00, must1=0xff
  12, 127, 0x00, 0x00, 0xff, "rsv",       0, 0x00,  // reserved-- reset = XX, can1=0x00, must1=0xff
// ********** AIC3100 PAGE 13
// pg,reg, reset, can1, must1,  "nm",   prev, curr
  13,  0, 0x0d, 0x00, 0x00, "psr13",      0, 0x0d,  // 260=13.0    13:0 reads as 0x0d
  13,  1, 0x00, 0x00, 0xff, "rsv",        0, 0x00,  // reserved-- reset = XX, can1=0x00, must1=0xff
  13,  2, 0x7f, 0xff, 0x00, "ldac_iir_h", 0, 0x7f,
  13,  3, 0xff, 0xff, 0x00, "ldac_iir_l", 0, 0xff,
  13,  4, 0x00, 0xff, 0x00, "ldac_iir",   0, 0x00,
   // dup 5..7
  13,  8, 0x7f, 0xff, 0x00, "rdac_iir_h", 0, 0x7f,
  13,  9, 0xff, 0xff, 0x00, "rdac_iir_l", 0, 0xff,
  13, 10, 0x00, 0xff, 0x00, "rdac_iir",   0, 0x00,
   // dup 11..13
  13, 14, 0x7f, 0xff, 0x00, "h_drc_h",    0, 0x7f,
  13, 15, 0xf7, 0xff, 0x00, "h_drc_l",    0, 0xf7,
  13, 16, 0x80, 0xff, 0x00, "h_drc",      0, 0x80,  // 270
  13, 17, 0x09, 0xff, 0x00, "h_drc",      0, 0x09,
  13, 18, 0x7f, 0xff, 0x00, "h_drc_h",    0, 0x7f,
  13, 19, 0xef, 0xff, 0x00, "h_drc_l",    0, 0xef,
  13, 20, 0x00, 0xff, 0x00, "h_drc",      0, 0x00,
  13, 21, 0x11, 0xff, 0x00, "h_drc",      0, 0x11,
  13, 22, 0x00, 0xff, 0x00, "h_drc",      0, 0x00,
  13, 23, 0x11, 0xff, 0x00, "h_drc",      0, 0x11,
  13, 24, 0x7f, 0xff, 0x00, "h_drc_h",    0, 0x7f,
  13, 25, 0xde, 0xff, 0x00, "h_drc_l",    0, 0xde,
  13, 26, 0x00, 0x00, 0xff, "rsv",        0, 0x00,  // 280 // reserved-- reset = XX, can1=0x00, must1=0xff

// dup 27..127
  16,   0, 0x00, 0x00, 0x00, "LAST_ENTRY",0, 0x00  // 281=16.0    last entry marker
};
int             codecNREGS = (sizeof(codec_regs)/sizeof(AIC_REG)) - 1;  // exclude last entry marker

// define constants to match slots of codec_regs -- verified by 
 const int  P0_R0_Page_Select_Register                   =  0;
 const int  P0_R1_Software_Reset_Register                =  1;
 const int  P0_R3_OT_FLAG                                =  3;
const int   P0_R4_ClockGen_Muxing                        =  4;
 const int  P0_R5_PLL_P_R_VAL                            =  5;
 const int  P0_R6_PLL_J_VAL                              =  6;
 const int  P0_R7_PLL_D_VAL_16B                          =  7;
 const int  P0_R8_PLL_D_VAL_LSB                          =  8;
 const int  P0_R11_DAC_NDAC_VAL                          = 11;
 const int  P0_R12_DAC_MDAC_VAL                          = 12;
 const int  P0_R13_DAC_DOSR_VAL_16B                      = 13;
const int   P0_R14_DAC_DOSR_VAL_LSB                      = 14;
 const int  P0_R15_DAC_IDAC_VAL                          = 15;
 const int  P0_R16_DAC_MAC_Engine_Interpolation          = 16;
 const int  P0_R18_ADC_NADC_VAL                          = 18;
 const int  P0_R19_ADC_MADC_VAL                          = 19;
 const int  P0_R20_ADC_AOSR_VAL                          = 20;
 const int  P0_R21_ADC_IADC_VAL                          = 21;
const int   P0_R22_ADC_MAC_Engine_Decimation             = 22;
 const int  P0_R25_CLKOUT_MUX                            = 25;
 const int  P0_R26_CLKOUT_M_VAL                          = 26;
 const int  P0_R27_Codec_Interface_Control1              = 27;
const int   P0_R29_Codec_Interface_Control2              = 29;
 const int  P0_R30_BCLK_N_VAL                            = 30;
 const int  P0_R33_Codec_Interface_Control3              = 33;
 const int  P0_R36_ADC_Flag_Register                     = 36;  //  verify ADC power from P0_R36_  D6
 const int  P0_R37_DAC_Flag_Register                     = 37;
 const int  P0_R51_gpio_pin                              = 48;
 const int  P0_R53_DOUT_Pin_Control                      = 50;
 const int  P0_R60_DAC_Instruction_Set                   = 53;
 const int  P0_R61_ADC_Instr_Set                         = 54;  // P0_R61_ADC_Instruction_Set
 const int  P0_R62_prb_ctl                               = 55;  // P0_R62_Programmable_Instruction_Mode_Control
 const int  P0_R63_DAC_Datapath_SETUP                    = 56;
 const int  P0_R64_DAC_VOLUME_CONTROL                    = 57;
 const int  P0_R65_DAC_Left_Volume_Control               = 58;
 const int  P0_R66_DAC_Right_Volume_Control              = 59;
 const int  P0_R67_Headset_Detection                     = 60;
 const int  P0_R68_DRC_Control                           = 61;
 const int  P0_R81_ADC_Digital_Mic                       = 74; // ADC power up  P0_R81_  D7     
 const int  P0_R82_ADC_Volume_Control                    = 75;
 const int  P0_R83_ADC_Volume_Control                    = 76;
 const int  P0_R86_AGC_Control1                          = 79;
 const int  P0_R87_AGC_Control2                          = 80;
 const int  P0_R88_AGC_MAX_Gain                          = 81;
 const int  P0_R89_AGC_Attack_Time                       = 82;
 const int  P0_R90_AGC_Decay_Time                        = 83;
 const int  P0_R91_AGC_Noise_Debounce                    = 84;
 const int  P0_R92_AGC_Signal_Debounce                   = 85;
 const int  P0_R93_AGC_Gain                              = 86;
 const int  P0_R102_ADC_DC_Measurement1                  = 88;
const int   P1_R0_Page_Select_Register                   = 97;
 const int  P1_R30_HP_Spkr_Amp_Err_Ctl                   = 99;
 const int  P1_R31_Headphone_Drivers                     = 100;
 const int  P1_R32_ClassD_Drivers                        = 101;
 const int  P1_R33_HP_Output_Drivers_POP_Rem_Settings    = 102;
 const int  P1_R34_Out_Driver_PGA_RampDown_Period_Ctrl   = 103;
 const int  P1_R35_LDAC_and_RDAC_Output_Routing          = 104;
 const int  P1_R36_Left_Analog_Vol_to_HPL                = 105;
 const int  P1_R37_Right_Analog_Vol_to_HPR               = 106;
 const int  P1_R38_Left_Analog_Vol_to_SPL                = 107;
const int   P1_R39_Right_Analog_Vol_to_SPR               = 108;
 const int  P1_R40_HPL_Driver                            = 109;
 const int  P1_R41_HPR_Driver                            = 110;
 const int  P1_R42_SPK_Driver                            = 111;
 const int  P1_R44_HP_Driver_Control                     = 113;
 const int  P1_R46_MICBIAS                               = 115;
 const int  P1_R47_MIC_PGA                               = 116;
 const int  P1_R48_ADC_Input_P                           = 117;
 const int  P1_R49_ADC_Input_M                           = 118;
 const int  P1_R50_Input_CM                              = 119;
const int   P3_R0_psr                                    = 121; // 121=3.0
const int   P4_R0_psr                                    = 125; // 125=4.0 
const int   P8_R0_psr                                    = 151; // 151=8.0
const int   P9_R0_psr                                    = 195; // 195=9.0
const int   P12_R0_psr                                   = 216; // 216=12.0
const int   P13_R0_psr                                   = 260; // 260=13.0 


#if !defined (VERIFY_WRITTENDATA)  
// Uncomment this line to enable verifying data sent to codec after each write operation (for debug purpose)
#define VERIFY_WRITTENDATA 
#endif /* VERIFY_WRITTENDATA */

bool codecIsReady = false;
bool codecHasPower = false;

int  codecClockFreq = 0;

void            aicSetCurrPage( uint8_t page ){
  if ( !codecIsReady ) return;
  if ( page == aicCurrPg )    // already on correct page?
    return;

  i2c_wrReg( 0, page );     // write current page's PageSelectRegister with new page
  dbgLog( "1 PgSw: %d \n", page );
  aicCurrPg = page;
}
uint8_t         aicGetReg( int idx ){
  if ( !codecIsReady )
    return 0;

  uint8_t pg = codec_regs[ idx ].pg;
  uint8_t reg = codec_regs[ idx ].reg;

  aicSetCurrPage( pg );
  uint8_t val = i2c_rdReg( reg );
  if ( WatchReg<0 || idx==WatchReg )
    dbgLog( "Rd %d:%02d == 0x%02x \n", pg, reg, val );
  return val;
}
void            aicSetReg( int idx, uint8_t val ){
  if ( !codecIsReady ) return;

  uint8_t pg = codec_regs[ idx ].pg;
  uint8_t reg = codec_regs[ idx ].reg;

  val = ( val & codec_regs[idx].W1_allowed ) | codec_regs[idx].W1_required;   // mask off bits that must be 0, and set bits that must be 1

  aicSetCurrPage( pg );
  if ( WatchReg<0 || idx==WatchReg )
    dbgLog( "Wr %d:%02d <- 0x%02x \n", pg, reg, val );

  i2c_wrReg( reg, val );  // write register of AIC3100
  codec_regs[idx].curr_val = val;

  #ifdef VERIFY_WRITTENDATA
    int chkVal = i2c_rdReg( reg );
    if ( chkVal != val ){
      uint8_t statRegs[] = { // registers we write, that also have read-only bits
        P0_R1_Software_Reset_Register,  P0_R67_Headset_Detection,
        P1_R31_Headphone_Drivers,       P1_R32_ClassD_Drivers,
        P1_R40_HPL_Driver,              P1_R41_HPR_Driver,
        P1_R42_SPK_Driver,              P1_R50_Input_CM
      };
      for (int i=0; i<sizeof(statRegs); i++)
        if ( idx==statRegs[i] ) return;  // don't report, non-writeable bits
      dbgLog( "Read after write mismatch %d:%02d wr 0x%02x rd 0x%02x \n", pg, reg, val, chkVal );
    }
  #endif /* VERIFY_WRITTENDATA */
}
void            aicSet16Bits( int idx, uint16_t val ){  // write (val>>8) to idx, (val & 0xFF) to idx+1
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

void            i2c_CheckRegs(){                                            // Debug -- read codec regs
  const           uint8_t   Cdc_DefReg = 3;
  int nErrs = 0;
  #ifdef VERIFY_WRITTENDATA
    int svWReg = WatchReg;
    WatchReg = codecNREGS+1;
    for ( int i = 0; i < codecNREGS; i++ ){
      //uint8_t reg = codec_regs[i].reg;
      uint8_t defval = codec_regs[i].reset_val;
      uint8_t val = aicGetReg( i );
      codec_regs[i].curr_val = val;
      codec_regs[i].prev_val = val;

      if (defval!=0xFF){
                cntErr( Cdc_DefReg, defval, val, i, 9 );  
                if (defval != val) nErrs++;
            }
//      tbDelay_ms(10);  // dbgLog
    }
    WatchReg = svWReg;
    i2c_ReportErrors();
  #endif
}
void            i2c_Upd(){
}
void            verifyCodecReg( int idx, int pg, int reg ){       // verify that codec_regs[idx] is entry for pg : reg
  int cr_pg = codec_regs[ idx ].pg;
  int cr_reg = codec_regs[ idx ].reg;
  if ( cr_pg != pg || cr_reg != reg )
    dbgLog( "! v_codec_reg!: idx=%d  %d:%d != %d:%d \n", idx, pg, reg, cr_pg, cr_reg );
}

static int                      LastVolume  = 0;      // retain last setting, so audio restarts at last volume
int       RecDBG;   // used by dbgLoop

int RegsCallCnt = 0;      // to detect 1st call
void            showCdcRegs( bool always, bool nonReset ){  // display changes in codec regs
  if ( !always && !dbgEnab( '2' )) return;
  if ( !codecIsReady ){ dbgLog( "! showRegs: not powered up! \n" ); return; }

  typedef struct {
    uint8_t val;  // register idx in codec_vals[] if valNm==NULL
    char *valNm;
  } ValDef;
  ValDef reg_vals[] = {  // define strings for register values
    { P0_R4_ClockGen_Muxing, NULL },      // clk_mux    0.4:  0x03="M>PLL>Cdc"  0x00="M>Cdc"
      { 0x03, "M>PLL>Cdc" },
      { 0x00, "M>Cdc" },
    { P0_R5_PLL_P_R_VAL, NULL },          // pll_p_r    0.5:   0x91="PllOn,R=1,P=1"
      { 0x91, "PllOn,R=1,P=1" },
      { 0x00, "off" },
    { P0_R6_PLL_J_VAL, NULL },            // pll_j      0.6:  0x07=7, 0x08=8
      { 0x07, "J=7" },
      { 0x08, "J=8" },
      { 0x00, "??" },
    { P0_R7_PLL_D_VAL_16B, NULL },        // pll_d      0.7,8:  0x0230=560, 0x1a90=6800, 0x0780=1920
      { 0x07, "1920" },
      { 0x1a, "6800" },
      { 0x02, "560" },
      { 0x01, "368" },
      { 0x00, "??" },
    { P0_R8_PLL_D_VAL_LSB, NULL },        // pll_d
      { 0x80, "0780" },
      { 0x90, "1a90" },
      { 0x30, "0230" },
      { 0x70, "0170" },
      { 0x00, "??" },
    { P0_R11_DAC_NDAC_VAL, NULL },        // pll_dac_n  0.11: 0x04=4, 0x18=24, 0x03=3, 0x06=6, 0x0c=12
      { 0x84, "N=4" },
      { 0x98, "N=24" },
      { 0x83, "N=3" },
      { 0x86, "N=6" },
      { 0x8c, "N=12" },
      { 0x00, "off" },
    { P0_R12_DAC_MDAC_VAL, NULL },        // pll_dac_m  0.12:  0x2d=45, 0x05=5, 0x28=40, 0x10=16, 0x0a=10
      { 0x85, "M=5" },
      { 0x8a, "M=10" },
      { 0x90, "M=16" },
      { 0xa8, "M=40" },
      { 0xad, "M=45" },
      { 0x00, "off" },
    { P0_R13_DAC_DOSR_VAL_16B, NULL },    // dosr       0.13,14:  0x0040=64
      { 0x00, "64" },
      { 0x00, "??" },
    { P0_R14_DAC_DOSR_VAL_LSB, NULL },    // dosr lsb
      { 0x40, "0040" },
    { P0_R20_ADC_AOSR_VAL, NULL },        // aosr       0.20: 0x40=64, 0x80=128
      { 0x40, "64" },
      { 0x80, "128" },
      { 0x00, "??" },
    { P0_R25_CLKOUT_MUX, NULL },          // outMux     0.25:  0x00="out=M" 0x03="out=PLL"  0x04="out=Dac"  0x05="out=DacMod"  0x06="out=Adc"  0x07="out=AdcMod"
      { 0x00, "out=M" },
      { 0x03, "out=PLL" },
      { 0x04, "out=Dac" },
      { 0x05, "out=DacMod" },
      { 0x06, "out=Adc" },
      { 0x07, "out=AdcMod" },
    { P0_R26_CLKOUT_M_VAL, NULL },        // outM       0.26:  0x80="M=0"
      { 0x01, "off=1" },
      { 0x00, "off" },
    { P0_R27_Codec_Interface_Control1, NULL }, // i2s   0.27:  0x0c="I2S 16 B&Wout"
      { 0x0c, "I2S_16_B&Wout" },
      { 0x00, "??" },
    { P0_R29_Codec_Interface_Control2, NULL }, // bclk  0.29:  0x01="BDIV=DAC_MOD_CLK"
      { 0x01, "DacMod>B" },
      { 0x00, "??" },
    { P0_R30_BCLK_N_VAL, NULL },          // BclkN      0.30:  0x82=2
      { 0x82, "N=2" },
      { 0x00, "off" },
    { P0_R33_Codec_Interface_Control3, NULL }, // b&w_clks    0.33:  0x00="B,W=Dacfs,D"
      { 0x00, "B,W=Dacfs,D" },
      { 0x00, "??" },
    { P0_R37_DAC_Flag_Register, NULL },   // DacFlg     0.37: 0x80=LDac_On,  0x10=ClsD_on, 0x08="RDac_on", 0x90="LDac_ClsD_on"
      { 0x90, "LDac&ClsD_on" },
      { 0x98, "L&RDac&ClsD_on" },
      { 0x88, "L&RDac_on" },
      { 0x00, "??" },
    { P0_R53_DOUT_Pin_Control, NULL },    // D_pin      0.53: 0x11="~BK,Dout", 0x01="BK,DOut"  0x12="~BK,D=Gpio"
      { 0x01, "Dout,BK" },
      { 0x11, "Dout,~BK" },
      { 0x00, "noDOut" },
    { P0_R61_ADC_Instr_Set, NULL },       // prb        0.61: 0x04=PRB_R4, 0x0B=PRB_R11
      { 0x04, "PRB_R4" },
      { 0x0B, "PRB_R11" },
      { 0x00, "??" },
    { P0_R63_DAC_Datapath_SETUP, NULL },  // DacPath    0.63:  0x90="LDacOnL",
      { 0x90, "LDacOnL" },
    { P0_R64_DAC_VOLUME_CONTROL, NULL },  // DacVol     0.64:  0x0c="L&Rmute" 0x04="Lon,Rmute"
      { 0x0c, "L&Rmute" },
      { 0x04, "Lon,Rmute" },
      { 0x00, "??" },
    { P0_R65_DAC_Left_Volume_Control, NULL }, // DacLVol    0.65:  0x30="24dB", 0x0a="5dB", 0x00="0dB", 0xc0="-32dB", 0x81="-63.5dB"
      { 0x30, "24dB" },
      { 0x0a, "5dB" },
      { 0x00, "0dB" },
      { 0xc0, "-32dB" },
      { 0x81, "-63.5dB" },
      { 0x00, "??" },
    { P0_R81_ADC_Digital_Mic, NULL },     // dig_mic    0.81:
      { 0x00, "adc_off" },
      { 0x80, "adc_on" },
    { P0_R82_ADC_Volume_Control, NULL }, // fn_vol      0.82: 0x68=-12dB, 0x78=-6dB, 0x00=0dB, 0x10=8dB, 0x20=16dB
      { 0x00, "unmute 0dB" },
      { 0x80, "mute 0dB" },
    { P0_R83_ADC_Volume_Control, NULL }, // vol       0.83: 0x68=-12dB, 0x78=-6dB, 0x00=0dB, 0x10=8dB, 0x20=16dB
      { 0x68, "-12dB" },
      { 0x78, "-6dB" },
      { 0x00, "0dB" },
      { 0x10, "8dB" },
      { 0x20, "16dB" },
    { P0_R86_AGC_Control1, NULL },      // targ       0.86: 0x00=AGCoff, 0x80=targ -5.5dB, 0xA0=-10dB, 0xC0=-14dB, 0xD0=-17dB, 0xE0=-20dB, 0xF0=-24dB
      { 0x00, "AGCoff" },
      { 0x80, "-5.5dB" },
      { 0xA0, "-10dB" },
      { 0xC0, "-14dB" },
      { 0xD0, "-17dB"},
      { 0xE0, "-20dB"},
      { 0xF0, "-24dB"},
    { P0_R88_AGC_MAX_Gain, NULL },      // maxg       0.88: 0x00=0dB, 0x08=4dB, 0x10=8dB, 0x20=16dB, 0x30=24dB, 0x40=32dB, 0x50=40dB, 0x7f=reset
      { 0x00, "0dB" },
      { 0x08, "4dB" },
      { 0x10, "8dB" },
      { 0x20, "16dB" },
      { 0x30, "24dB" },
      { 0x40, "32dB" },
      { 0x50, "40dB" },
      { 0x7f, "reset" },
    { P0_R102_ADC_DC_Measurement1, NULL },  // dc_meas    0.102:  0x00=off
      { 0x00, "off" },
  // PAGE 1
    { P1_R32_ClassD_Drivers, NULL },        // ClsD       1.32: 0x86="PwrOn"
      { 0x86, "PwrOn" }, { 0x06, "off" },
    { P1_R35_LDAC_and_RDAC_Output_Routing, NULL },  // DacRout    1.35: 0x40="DacL_Spk", 0x20="MicLP_Spk"  0x60="DacL&MicL_Spk"
      { 0x40, "DacL>Spk" },
      { 0x20, "MicLP>Spk" },
      { 0x60, "DacL+MicL>Spk" },
      { 0x00, "off" },
    { P1_R38_Left_Analog_Vol_to_SPL, NULL },  // l_to_spkr  1.38:
      { 0x00, "noLtoSpkr", },
      { 0x80, "LtoSpkr_0dB" },
      { 0x7F, "noLtoSpkr", },
      { 0xFF, "LtoSpkr_-78dB" },
    { P1_R42_SPK_Driver, NULL },              // SpkDrv     1.42:   0x00="SpkMute", 0x04="Spk=6dB"
      { 0x04, "Spk=6dB" },
      { 0x00, "SpkMute" },
    { P1_R46_MICBIAS, NULL },                 // bias       1.46: 0x0A=2.5V, 0x09=1.5V
      { 0x0A, "2.5V" },
      { 0x09, "1.5V" },
      { 0x00, "??" },
    { P1_R47_MIC_PGA, NULL },                 // pga        1.47: (0x08=4dB), 0x00=0dB, 0x10=8dB, 0x20=16dB, 0x40=32dB  0x80="0dB"
      { 0x08, "4dB" },
      { 0x00, "0dB" },
      { 0x10, "8dB" },
      { 0x20, "16dB" },
      { 0x30, "24dB" },
      { 0x40, "32dB" },
      { 0x80, "0dB" },
    { P1_R48_ADC_Input_P, NULL },             // in_p       1.48: 0x40=MIC1LP_10k, 0x80=MIC1LP_20k, 0xC0=MIC1LP_40k
      { 0x40, "1LP_10k" },
      { 0x80, "1LP_20k" },
      { 0xC0, "1LP_40k" },
      { 0x10, "1RP_10k" },
      { 0x00, "no_P" },
    { P1_R49_ADC_Input_M, NULL },             // in_m       1.49: 0x10=MIC1LM_10k, 0x20=MIC1LM_20k, 0x30=MIC1LM_40k
      { 0x10, "1LM_10k" },
      { 0x20, "1LM_20k" },
      { 0x30, "1LM_40k" },
      { 0x40, "L_CM10k" },
      { 0x00, "no_M" },
    { P1_R50_Input_CM, NULL },                // in_cm      1.50: 0x00=no CM, 0x80=MIC1LP to CM, 0x20=MIC1LM to CM, 0xA0 = MIC1LP & MIC1LM to CM
      { 0x00, "no_CM" },
      { 0x80, "1LP_CM" },
      { 0x20, "1LM_CM" },
      { 0xA0, "P&M_CM" },
      { 0x01, "noCM+" },
      { 0x81, "1LP_CM+" },
      { 0x21, "1LM_CM+" },
      { 0xA1, "P&M_CM+" },
    { 0,  NULL } // end of list
  };

  int             codecNVALS = (sizeof(reg_vals)/sizeof(ValDef)) - 1;  // exclude last entry marker

  for ( int ridx=0; ridx < codecNREGS; ridx++){
    AIC_REG regDef = codec_regs[ ridx ];
    if ( regDef.reg != 0 ){
      if ((regDef.pg > 1) && !dbgEnab( '8' )) break;   // higher pages only if debug '8'

      if ( RegsCallCnt==0 ) // 1st time, set prev_val to reset_val
        codec_regs[ ridx ].prev_val = regDef.reset_val;
      uint8_t val = aicGetReg( ridx );
      bool changed = nonReset? (val != regDef.reset_val) : (val != regDef.prev_val);

      int valIdx = -1;
      char *valnm = NULL;
      for ( int j=0; j < codecNVALS; j++ )
        if ( reg_vals[j].valNm==NULL )
          valIdx = reg_vals[j].val;
        else if ( valIdx==ridx && val==reg_vals[j].val ){  // found value in list
          valnm = reg_vals[j].valNm;
          break;
        }

      char *rst = (val == regDef.reset_val)? "*":"";
      if ( changed ){
        if (valnm==NULL)
          dbgLog( "R%d.%3d: %12s = 0x%02x %s (0x%02x)\n", regDef.pg, regDef.reg, regDef.nm, val, rst, regDef.prev_val );
        else
          dbgLog( "R%d.%3d: %12s = 0x%02x = %s %s (0x%02x)\n", regDef.pg, regDef.reg, regDef.nm, val, valnm, rst, regDef.prev_val );
      }
      codec_regs[ ridx ].prev_val = val;
      tbDelay_ms(10); // dbgLog, only if DbgMask 2
    }
  }
  RegsCallCnt++;
  i2c_ReportErrors();
}
void            cdc_RecordEnable( bool enable ){
#if defined( AIC3100 )
  if ( enable ){
    typedef struct  {
      const uint8_t in_p;   // in_p 1.48: 0x40=MIC1LP_10k, 0x80=MIC1LP_20k, 0xC0=MIC1LP_40k
      const uint8_t in_m;   // in_m 1.49: 0x10=MIC1LM_10k, 0x20=MIC1LM_20k, 0x30=MIC1LM_40k
      const uint8_t in_cm;  //in_cm 1.50: 0x00=no CM, 0x80=MIC1LP to CM, 0x20=MIC1LM to CM, 0xA0 = MIC1LP & MIC1LM to CM

      const uint8_t bias;   // bias 1.46: 0x0A=2.5V, 0x09=1.5V                        //AK4637: used 2.4V
      const uint8_t pga;    // pga  1.47: (0x08=4dB), 0x00=0dB, 0x10=8dB, 0x20=16dB   //AK4637: used +18dB
      const uint8_t aosr;   // aosr 0.20: 0x40=64, 0x80=128
      const uint8_t prb;    // prb  0.61: 0x04=PRB_R4, 0x0B=PRB_R11
      const uint8_t targ;   // targ 0.86: 0x00=AGCoff, 0x80=targ -5.5dB, 0xA0=-10dB, 0xC0=-14dB, 0xD0=-17dB, 0xE0=-20dB, 0xF0=-24dB

      const uint8_t maxg;   // maxg 0.88: 0x00=0dB, 0x08=4dB, 0x10=8dB, 0x20=16dB, 0x30=24dB, 0x40=32dB, 0x50=40dB
      const uint8_t vol;    // vol  0.83: 0x68=-12dB, 0x78=-6dB, 0x00=0dB, 0x10=8dB, 0x20=16dB
    } recP;
 
    recP opts[] =
    {
  /* Record settings suggested by Marc
      Page 1 Reg 46:  0x0a    P1_R46_MICBIAS      2.5V
      Page 1 Reg 47:  0x40    P1_R47_MIC_PGA      32dB
      Page 1 Reg 48:  0x40    P1_R48_ADC_Input_P  1LP_10k
      Page 1 Reg 49:  0x10    P1_R49_ADC_Input_M  1LM_10k
      Page 1 Reg 50:  0x00    P1_R50_Input_CM     no_CM

      Page 0 Reg 81:  0x80    P0_R81_ADC_Digital_Mic              ADC_pwr_on
      Page 0 Reg 82:  0x00    P0_R82_ADC_Volume_Control (fine)    unmute 0dB
      Page 0 Reg 83:  0x00    P0_R83_ADC_Volume_Control (coarse)  0dB  [reset]
      Page 0 Reg 86:  0x00    P0_R86_AGC_Control1                 agc_disabled    (for no AGC; can discuss if we want AGC later)
      Page 0 Reg 102:  0x00   P0_R102_ADC_DC_Measurement1         meas_disabled [reset]
      P0_R53 should be configured to 0x02.
    */

     // 1.48, 1.49, 1.50,  1.46,  1.47, 0.20, 0.61, 0.83, 0.88, 0.83
     // in_p, in_m,  in_cm,bias,  pga,  aosr, prb,  targ, maxg, vol     DbgRec
  // pga 1.47:  (0x08=4dB), 0x00=0dB, 0x10=8dB, 0x20=16dB 0x40=32dB 0x50=40dB 0x60=48dB 0x70=56dB 0x77=59.5dB
  //                              pga                     maxg  vol
      { 0x40, 0x10,  0x00, 0x0A,  0x70, 0x40, 0x04, 0x80, 0x58, 0x00 }, // 18 pga=56dB, maxG=44dB, vul=0dB
  //  { 0x40, 0x10,  0x00, 0x0A,  0x77, 0x40, 0x04, 0x80, 0x58, 0x00 }, // 0  pga=59.5dB, AGCtarg=-5.5dB maxGain=44dB vol=0dB
  /*
      { 0x40, 0x10,  0x00, 0x0A,  0x70, 0x40, 0x04, 0x80, 0x50, 0x10 }, // 1  pga=56dB, AGCtarg=-5.5dB maxGain=40dB vol=8dB
      { 0x40, 0x10,  0x00, 0x0A,  0x68, 0x40, 0x04, 0x80, 0x50, 0x10 }, // 2  pga=52dB, AGCtarg=-5.5dB maxGain=40dB vol=8dB
      { 0x40, 0x10,  0x00, 0x0A,  0x60, 0x40, 0x04, 0x80, 0x50, 0x10 }, // 3  pga=48dB, AGCtarg=-5.5dB maxGain=40dB vol=8dB
  // maxg 0.88: 0x00=0dB, 0x08=4dB, 0x10=8dB, 0x20=16dB, 0x30=24dB, 0x40=32dB, 0x50=40dB 0x60=48dB 0x70=56dB 0x77=59.5dB
  //                                                      maxg
      { 0x40, 0x10,  0x00, 0x0A,  0x77, 0x40, 0x04, 0x80, 0x50, 0x00 }, // 4  pga=59.5dB, AGCtarg=-5.5dB maxGain=40dB
      { 0x40, 0x10,  0x00, 0x0A,  0x77, 0x40, 0x04, 0x80, 0x54, 0x00 }, // 5  pga=59.5dB, AGCtarg=-5.5dB maxGain=42dB
      { 0x40, 0x10,  0x00, 0x0A,  0x77, 0x40, 0x04, 0x80, 0x58, 0x00 }, // 6  pga=59.5dB, AGCtarg=-5.5dB maxGain=44dB
      { 0x40, 0x10,  0x00, 0x0A,  0x77, 0x40, 0x04, 0x80, 0x5C, 0x00 }, // 7  pga=59.5dB, AGCtarg=-5.5dB maxGain=46dB
      { 0x40, 0x10,  0x00, 0x0A,  0x77, 0x40, 0x04, 0x80, 0x5C, 0x00 }, // 8  pga=59.5dB, AGCtarg=-5.5dB maxGain=48dB
  // vol  0.83: 0x68=-12dB, 0x78=-6dB, 0x00=0dB, 0x10=8dB, 0x20=16dB  0x28=20dB
  //                                                            vol
      { 0x40, 0x10,  0x00, 0x0A,  0x77, 0x40, 0x04, 0x80, 0x58, 0x10 }, // 9  pga=59.5dB, AGCtarg=-5.5dB maxGain=44dB vol=8dB
      { 0x40, 0x10,  0x00, 0x0A,  0x77, 0x40, 0x04, 0x80, 0x58, 0x20 }, // 10  pga=59.5dB, AGCtarg=-5.5dB maxGain=44dB vol=16dB
      { 0x40, 0x10,  0x00, 0x0A,  0x77, 0x40, 0x04, 0x80, 0x58, 0x28 }, // 11  pga=59.5dB, AGCtarg=-5.5dB maxGain=44dB vol=20dB
  //                              pga                     maxg
      { 0x40, 0x10,  0x00, 0x0A,  0x68, 0x40, 0x04, 0x80, 0x50, 0x00 }, // 12 pga=52dB, maxG=40dB
      { 0x40, 0x10,  0x00, 0x0A,  0x68, 0x40, 0x04, 0x80, 0x58, 0x00 }, // 13 pga=52dB, maxG=44dB
      { 0x40, 0x10,  0x00, 0x0A,  0x68, 0x40, 0x04, 0x80, 0x60, 0x00 }, // 14 pga=52dB, maxG=48dB
      { 0x40, 0x10,  0x00, 0x0A,  0x68, 0x40, 0x04, 0x80, 0x68, 0x00 }, // 15 pga=52dB, maxG=52dB
  //                              pga                           vol
      { 0x40, 0x10,  0x00, 0x0A,  0x60, 0x40, 0x04, 0x80, 0x58, 0x00 }, // 16 pga=48dB, maxG=44dB, vul=0dB
      { 0x40, 0x10,  0x00, 0x0A,  0x68, 0x40, 0x04, 0x80, 0x58, 0x00 }, // 17 pga=52dB, maxG=44dB, vul=0dB
      { 0x40, 0x10,  0x00, 0x0A,  0x70, 0x40, 0x04, 0x80, 0x58, 0x00 }, // 18 pga=56dB, maxG=44dB, vul=0dB
      { 0x40, 0x10,  0x00, 0x0A,  0x60, 0x40, 0x04, 0x80, 0x58, 0x10 }, // 19 pga=48dB, maxG=44dB, vul=8dB
      { 0x40, 0x10,  0x00, 0x0A,  0x68, 0x40, 0x04, 0x80, 0x58, 0x10 }, // 20 pga=52dB, maxG=44dB, vul=8dB
      { 0x40, 0x10,  0x00, 0x0A,  0x70, 0x40, 0x04, 0x80, 0x58, 0x10 }, // 21 pga=56dB, maxG=44dB, vul=8dB

  // *
  // targ 0.86: 0x00=AGCoff, 0x80=targ -5.5dB, 0xA0=-10dB, 0xC0=-14dB, 0xD0=-17dB, 0xE0=-20dB, 0xF0=-24dB

  // ************** RecDBG > 2 --  enable Speaker & route MIC1LP + ADC to it
  //    in_p, in_m,  in_cm,bias,  pga,  aosr, prb,  targ, maxg, vol     DbgRec
  //  { 0x40, 0x10,  0x00, 0x0a,  0x40, 0x40, 0x04, 0x00, 0x00, 0x00 }, // 3  Marc: AGC off, mic_pga=32dB
  //  { 0x40, 0x10,  0x00, 0x0A,  0x40, 0x40, 0x04, 0xC0, 0x30, 0x00 }, // 4  pga=32dB, targ=-14dB, maxg=24dB
  //  { 0x40, 0x10,  0x00, 0x0A,  0x40, 0x40, 0x04, 0xE0, 0x30, 0x00 }, // 5  pga=32dB, targ=-20dB, maxg=24dB
  // Test no amplification:  pga=0dB, AGC=off, coarse_vol=-12dB
  //  { 0xC0, 0x30,  0x00, 0x0A,  0x00, 0x40, 0x04, 0x00, 0x00, 0x68 }, //0

  //    in_p, in_m, in_cm, bias,  pga,  aosr, prb,  targ, maxg, vol     DbgRec
  //  { 0x40, 0x10,  0x00, 0x0A,  0x08, 0x40, 0x04, 0xC0, 0x30, 0x00 }, // 0   defaults

  //  in_p 1.48: 0x40=MIC1LP_10k, 0x80=MIC1LP_20k, 0xC0=MIC1LP_40k, 0x10=MIC1RP_10k
  //  in_m 1.49: 0x10=MIC1LM_10k, 0x20=MIC1LM_20k, 0x30=MIC1LM_40k
  //    in_p  in_m 
      { 0x80, 0x20,  0x00, 0x0A,  0x08, 0x40, 0x04, 0xC0, 0x30, 0x00 }, // 1   MIC1LP & MIC1LM @ 20kOhm
      { 0xC0, 0x30,  0x00, 0x0A,  0x08, 0x40, 0x04, 0xC0, 0x30, 0x00 }, // 2   MIC1LP & MIC1LM @ 40kOhm

  //  in_cm 1.50: 0x00=no CM, 0x80=MIC1LP to CM, 0x20=MIC1LM to CM, 0xA0 = MIC1LP & MIC1LM to CM
  //                in_cm
      { 0x40, 0x10,  0x80, 0x0A,  0x08, 0x40, 0x04, 0xC0, 0x30, 0x00 }, // 3   MIC1LP to CM
      { 0x40, 0x10,  0x20, 0x0A,  0x08, 0x40, 0x04, 0xC0, 0x30, 0x00 }, // 4   MIC1LM to CM
      { 0x40, 0x10,  0xA0, 0x0A,  0x08, 0x40, 0x04, 0xC0, 0x30, 0x00 }, // 5   MIC1LP & LM to CM

  // pga 1.47:  (0x08=4dB), 0x00=0dB, 0x10=8dB, 0x20=16dB 0x40=32dB
  //                              pga
      { 0xC0, 0x30,  0x00, 0x0A,  0x08, 0x40, 0x04, 0xC0, 0x30, 0x00 }, // 6   MIC1LP & MIC1LM @ 40kOhm, pga 4dB
      { 0xC0, 0x30,  0x00, 0x0A,  0x00, 0x40, 0x04, 0xC0, 0x30, 0x00 }, // 7   MIC1LP & MIC1LM @ 40kOhm, pga 0dB


  // targ 0.86: 0x00=AGCoff, 0x80=targ -5.5dB, 0xA0=-10dB, 0xC0=-14dB, 0xD0=-17dB, 0xE0=-20dB, 0xF0=-24dB
  //                                                targ
      { 0x40, 0x10,  0x00, 0x0A,  0x40, 0x40, 0x04, 0x00, 0x30, 0x00 }, // 8   pga 32dB, AGC off
      { 0x40, 0x10,  0x00, 0x0A,  0x40, 0x40, 0x04, 0xE0, 0x30, 0x00 }, // 9   pga 32dB, targ -20dB
      { 0x40, 0x10,  0x00, 0x0A,  0x40, 0x40, 0x04, 0xF0, 0x30, 0x00 }, //10   pga 32dB, targ -24dB

  // maxg 0.88: 0x00=0dB, 0x08=4dB, 0x10=8dB, 0x20=16dB, 0x30=24dB, 0x40=32dB, 0x50=40dB
  //                                                      maxg
      { 0xC0, 0x30,  0x00, 0x0A,  0x00, 0x40, 0x04, 0xE0, 0x08, 0x00 }, //11   MIC1LP & MIC1LM @ 40kOhm, pga 0dB, targ -20dB, maxg 4dB
      { 0xC0, 0x30,  0x00, 0x0A,  0x00, 0x40, 0x04, 0xE0, 0x10, 0x00 }, //12   MIC1LP & MIC1LM @ 40kOhm, pga 0dB, targ -20dB, maxg 8dB
      { 0xC0, 0x30,  0x00, 0x0A,  0x00, 0x40, 0x04, 0xE0, 0x20, 0x00 }, //13   MIC1LP & MIC1LM @ 40kOhm, pga 0dB, targ -20dB, maxg 16dB

  // vol  0.83: 0x68=-12dB, 0x78=-6dB, 0x00=0dB, 0x10=8dB, 0x20=16dB
  //                                                            vol
      { 0xC0, 0x30,  0x00, 0x0A,  0x00, 0x40, 0x04, 0xE0, 0x10, 0x68 }, //14   MIC1LP & MIC1LM @ 40kOhm, pga 0dB, targ -20dB, maxg 8dB, vol -12dB
      { 0xC0, 0x30,  0x00, 0x0A,  0x00, 0x40, 0x04, 0xE0, 0x10, 0x78 }, //15   MIC1LP & MIC1LM @ 40kOhm, pga 0dB, targ -20dB, maxg 8dB, vol -6dB
      { 0xC0, 0x30,  0x00, 0x0A,  0x00, 0x40, 0x04, 0xE0, 0x10, 0x10 }, //16   MIC1LP & MIC1LM @ 40kOhm, pga 0dB, targ -20dB, maxg 8dB, vol 8dB
  */
    };
    const int MAX_OPTS = sizeof(opts) / sizeof(recP);
    if ( RecDBG < 0 ) RecDBG = MAX_OPTS-1;
    if ( RecDBG >= MAX_OPTS ) RecDBG = 0;
    recP RP = opts[ RecDBG ];

    aicSetReg( P0_R53_DOUT_Pin_Control, 0x02 );     // because Marc specified it-- enable bus-keeper?
    aicSetReg( P0_R102_ADC_DC_Measurement1, 0x00 ); // because Marc specified it-- reset value

    // power-on MIC & ADC, setup MIC PGA -- left channel only
    // mic settings used for AK4637 --  Differential input, mic gain = +18dB, mic_pwr = 2.4V
    aicSetReg( P1_R46_MICBIAS,            RP.bias ); //0x0A ); // P1_R46: MICBIAS output at 2.5V (re 7.3.9.1) even without headset detected, Software Power Down disabled
    aicSetReg( P1_R47_MIC_PGA,            RP.pga ); //0x08 ); // P1_R47: D7=0, D6_0=00 1000: MIC PGA is at 8 = 4dB gain
    aicSetReg( P1_R48_ADC_Input_P,        RP.in_p );  // P1_R48: D7_6 = 01: MIC1LP is selected for the MIC PGA with feed-forward resistance RIN = 10 k?.
    aicSetReg( P1_R49_ADC_Input_M,        RP.in_m );  // P1_R49: D5_4 = 01: MIC1LM is selected for the MIC PGA with feed-forward resistance RIN = 10 k?.
    aicSetReg( P1_R50_Input_CM,           RP.in_cm ); // P1_R50: no connections to internal Common Mode voltage

    aicSetReg( P0_R20_ADC_AOSR_VAL,       RP.aosr ); //0x40 );  // P0_R20: D7_0=64
    aicSetReg( P0_R61_ADC_Instr_Set,      RP.prb ); //0x04 ); // P0_R61: D4_0= 0 0100: ADC signal-processing block PRB_R4
    aicSetReg( P0_R62_prb_ctl,            0x00 ); // P0_R62: defaults

    // enable & set up Auto Gain Control -- defaults for now
    // datasheet  7.3.9.2 Automatic Gain Control  -- pg 30
    //####### AGC ENABLE EXAMPLE CODE  --
    //# Set AGC enable and Target Level = -10dB
    //# Target level can be set lower if clipping occurs during speech
    //# Target level is adjusted considering Max Gain also
    aicSetReg( P0_R86_AGC_Control1,       RP.targ ); //0xA0 );  // P0_R86: D7=1: AGC enabled, D6_4=010 AGC target level = �10 dB

    //# AGC hysteresis=DISABLE, noise threshold = -90dB
    //# Noise threshold should be set at higher level if noisy background is present in application
    aicSetReg( P0_R87_AGC_Control2,       0xFE ); // P0_R87: D7_6=11: AGC hysteresis disabled, D5_1=11 111 = AGC noise threshold = �90 dB
 
    //# AGC maximum gain= 40 dB
    //# Higher Max gain is a trade off between gaining up a low sensitivity MIC, and the background acoustic noise
    //# Microphone bias voltage (MICBIAS) level can be used to change the Microphone Sensitivity
    aicSetReg( P0_R88_AGC_MAX_Gain,       RP.maxg ); //0x50 );  // P0_R88: D6_0=101 0000: ACG maximum gain = 40dB

    // Guidance on the AGC attack and decay time would be very helpful. The original values caused recording to ramp
    // up over a couple of seconds, so the start of the recordings were generally cut off. Zero worked better in that
    // regard, and seemed to sound OK, but a really short AGC easily leads to rapid AGC changes. These values were
    // arrived by trial and error; they seem OK, but they're really just plucked out of thin air.
    //# Attack time=864/Fs   NOTE: the original recommendation was 0x68
    aicSetReg( P0_R89_AGC_Attack_Time,    0x04 ); // P0_R89: D7_3=0110 1: AGC attack time = 13 � (32 / fS) ( 13*32=416 not 864? )
    //                                                       D2_0=000: Multiply factor for the programmed AGC attack time = 1
    //# Decay time=22016/Fs  NOTE: the original recommendation was 0x80, 21 x ...
    aicSetReg( P0_R90_AGC_Decay_Time,     0x04 ); // P0_R90: D7_3=0000 0: AGC decay time = N+1 x (512 / fS)
    //                                                       D2_0=000: Multiply factor for the programmed AGC decay time = 1

    //# Noise debounce 0 ms
    //# Noise debounce time can be increased if needed
    aicSetReg( P0_R91_AGC_Noise_Debounce, 0x00 ); // P0_R91: D4_0=0 0000: AGC noise debounce = 0 / fS

    //# Signal debounce 0 ms
    //# Signal debounce time can be increased if needed
    aicSetReg( P0_R92_AGC_Signal_Debounce,0x00 ); // P0_R92: D3_0=0000: AGC signal debounce = 0 / fS
    //######### END of AGC SET UP  */

    // set default gain for ADC volume, then unmute
    aicSetReg( P0_R83_ADC_Volume_Control, RP.vol ); //0x00 ); // P0_R83: D6_0=000 0000: Delta-Sigma Mono ADC Channel Volume Control Coarse Gain = 0dB
    aicSetReg( P0_R82_ADC_Volume_Control, 0x00 ); // P0_R82: D7=0: ADC channel not muted, D6_4=000: Delta-Sigma Mono ADC Channel Volume Control Fine Gain = 0dB

    // power up ADC (after AGC configured, re: pg. 30)
    aicSetReg( P0_R81_ADC_Digital_Mic,    0x80 ); // P0_R81: D7= 1: ADC channel is powered up, D1_0 = 00: ADC digital soft-stepping enabled 1 step/sample
    int8_t agc_gain = aicGetReg( P0_R93_AGC_Gain ); // P0_R93: applied reading of current AGC gain-- -24..63 = -12dB..59.5dB
    dbgLog( "8 RecEn[%d] \n", RecDBG );
    showCdcRegs( false, false );

  } else {
      // Disable
    const int MAX_ADC_PWR_WAIT = 10000;
    // power down the ADC and the MIC
    aicSetReg( P0_R81_ADC_Digital_Mic,    0x00 ); // P0_R81: Rst  D7= 0: ADC channel is powered down.
    aicSetReg( P1_R46_MICBIAS,            0x00 ); // P1_R46: Rst  MICBIAS output off, Software Power Down disabled

    //AIC3100: follow pg 65 rules -- wait till 0.36 ADC_Flag says ADC is off, then pwr down clock dividers
    uint8_t pwr = 0x40;
    int cnt = 0;
    while ( pwr != 0 && cnt < MAX_ADC_PWR_WAIT ){ // wait till ADC powers down
      pwr = aicGetReg( P0_R36_ADC_Flag_Register ) & 0x40;  // P0_R36: D6=ADC_powered
      cnt++;
    }
    if ( cnt == MAX_ADC_PWR_WAIT ) dbgLog( "! ADC failed to turn off" );
  //  aicSetReg( P0_R20_ADC_AOSR_VAL, 0 );
    aicSetReg( P0_R19_ADC_MADC_VAL, 0x01 );  // Rst -- M=1 off
    aicSetReg( P0_R18_ADC_NADC_VAL, 0x01 );  // Rst -- N=1 off

    dbgLog( "8 AIC ADC & MIC off \n");
  }
  showCdcRegs( false, false );
#endif

#if defined( AK4637 )
  if ( enable ){
    // from AK4637 datasheet:
    //ENABLE
    // done by cdc_SetMasterFreq()      //   1)  Clock set up & sampling frequency  FS3_0
    //  05: 51, 06: 0E, 00: 40, 01:08 then 0C
    akR.R.SigSel1.MGAIN3 = 0;         //   2)  setup mic gain  0x02:
    akR.R.SigSel1.MGAIN2_0 = 0x6;     //   2)  setup mic gain  0x02: 0x6 = +18db
if (RecordTestCnt==0) akR.R.SigSel1.MGAIN2_0 = 0x4;
if (RecordTestCnt==1) akR.R.SigSel1.MGAIN2_0 = 0x2;
if (RecordTestCnt==2) akR.R.SigSel1.MGAIN2_0 = 0x0;
    akR.R.SigSel1.PMMP = 1;           //   2)  MicAmp power
    akUpd();  // 02:0E
    akR.R.SigSel2.MDIF = 1;           //   3)  input signal       Differential from Electret mic
if (RecordTestCnt==8) akR.R.SigSel2.MDIF = 0;
    akR.R.SigSel2.MICL = 0;           //   3)  mic power          Mic power 2.4V
if (RecordTestCnt==3) akR.R.SigSel2.MICL = 1;
    akUpd();  // 03: 01
                                      //   4)  FRN, FRATT, ADRST1_0  0x09: TimSel
    akR.R.TimSel.FRN = 0;             //   4)  TimSel.FRN         FastRecovery = Enabled (def)
    akR.R.TimSel.RFATT = 0;           //   4)  TimSel.RFATT       FastRecovRefAtten = -.00106dB (def)
    akR.R.TimSel.ADRST1_0 = 0x0;      //   4)  TimSel.ADRST1_0    ADCInitCycle = 66.2ms @ 16kHz (def)
if (RecordTestCnt==4) akR.R.TimSel.ADRST1_0 = 0x1;
if (RecordTestCnt==5) akR.R.TimSel.ADRST1_0 = 0x2;
if (RecordTestCnt==6) akR.R.TimSel.ADRST1_0 = 0x3;
    akUpd();  // --
                                      //   5)  ALC mode   0x0A, 0x0B: AlcTimSel, AlcMdCtr1
    ak.R.AlcTimSel.RFST1_0 = 0x3;   //   5)  AlcTimSel.RFST1_0  FastRecoveryGainStep = .0127db
    akR.R.AlcTimSel.WTM1_0 = 0x01;    //   5)  AlcTimSel.WTM1_0   WaitTime = 16ms @ 16kHz
    akR.R.AlcTimSel.EQFC1_0 = 0x1;    //   5)  AlcTimSel.EQFC1_0  ALCEQ Frequency = 12kHz..24kHz
    akR.R.AlcTimSel.IVTM = 1;         //   5)  AlcTimSel.IVTM     InputDigVolSoftTransitionTime = 944/fs (def)
    akR.R.AlcMdCtr1.RGAIN2_0 = 0x0;   //   5)  AlcMdCtr1.RGAIN2_0 RecoveryGainStep = .00424db (def)
    akR.R.AlcMdCtr1.LMTH1_0 = 0x2;    //   5)  AlcMdCtr1.LMTH1_0  ALC LimiterDetectionLevel = Detect>-2.5dBFS Reset> -2.5..-4.1dBFS (def)
    akR.R.AlcMdCtr1.LMTH2 = 0;        //   5)  AlcMdCtr1.LMTH2
    akR.R.AlcMdCtr1.ALCEQN = 0;       //   5)  AlcMdCtr1.ALCEQN   ALC Equalizer = Enabled (def)
    akR.R.AlcMdCtr1.ALC = 1;          //   5)  AlcMdCtr1.ALC      ALC = Enabled
if (RecordTestCnt==7) akR.R.AlcMdCtr1.ALC = 0;
    akUpd();  // 0A: 47   0B: 22
    akR.R.AlcMdCtr2.REF7_0 = 0xE1;    //   6)  REF value  0x0C: AlcMdCtr2 ALCRefLevel = +30dB (def)
    akUpd();  // nc
    akR.R.InVolCtr.IVOL7_0 = 0xE1;    //   7)  IVOL value  0x0D:  InputDigitalVolume = +30db (def)
if (RecordTestCnt==9) akR.R.InVolCtr.IVOL7_0 = 0x0;
    akUpd();  // nc
                                      //   8)  ProgFilter on/off  0x016,17,21: DigFilSel1, DigFilSel2, DigFilSel3
    akR.R.DigFilSel1.HPFAD = 1;       //   8) DigFilSel1.HPFAD    HPF1 Control after ADC = ON (def)
    akR.R.DigFilSel1.HPFC1_0 = 0x1;   //   8) DigFilSel1.HPFC1_0  HPF1 cut-off = 4.9Hz @ 16kHz
    akR.R.DigFilSel2.HPF = 0;         //   8) DigFilSel2.HPF      HPF F1A & F1B Coef Setting = Disabled (def)
    akR.R.DigFilSel2.LPF = 0;         //   8) DigFilSel2.LPF      LPF F2A & F2B Coef Setting = Disabled (def)
    akR.R.DigFilSel3.EQ5_1 = 0x0;     //   8) DigFilSel3.EQ5_1    Equalizer Coefficient 1..5 Setting = Disabled (def)
    akUpd();    // 16: 03
                                      //   9)  ProgFilter path  0x18: DigFilMd
    akR.R.DigFilMd.PFDAC1_0 = 0x0;    //   9)  DigFilMd.PFDAC1_0  DAC input selector = SDTI (def)
    akR.R.DigFilMd.PFVOL1_0 = 0x0;    //   9)  DigFilMd.PFVOL1_0  Sidetone digital volume = 0db (def)
    akR.R.DigFilMd.PFSDO = 1;         //   9)  DigFilMd.PFSDO     SDTO output signal select = ALC output (def)
    akR.R.DigFilMd.ADCPF = 1;         //   9)  DigFilMd.ADCPF     ALC input signal select = ADC output (def)
    akUpd();    // nc
                                      //  10)  CoefFilter:  0x19..20, 0x22..3F: HpfC0..3, LpfC0..3 E1C0..5 E2C0..5 E3C0..5 E4C0..5 E5C0..5
    akR.R.HpfC0.F1A7_0  = 0xB0;       //  10)  Hpf.F1A13_0 low  (def)  //DISABLED by DigFilSel2.HPF
    akR.R.HpfC1.F1A13_8 = 0x1F;       //  10)  Hpf.F1A13_0 high (def)  //DISABLED by DigFilSel2.HPF
    akR.R.HpfC2.F1B7_0  = 0x9F;       //  10)  Hpf.F1B13_0 low  (def)  //DISABLED by DigFilSel2.HPF
    akR.R.HpfC3.F1B13_8 = 0x02;       //  10)  Hpf.F1B13_0 high (def)  //DISABLED by DigFilSel2.HPF
    akR.R.LpfC0.F2A7_0  = 0x0;        //  10)  Lpf.F2A13_0 low  (def)  //DISABLED by DigFilSel2.LPF
    akR.R.LpfC1.F2A13_8 = 0x0;        //  10)  Lpf.F2A13_0 high (def)  //DISABLED by DigFilSel2.LPF
    akR.R.LpfC2.F2B7_0  = 0x0;        //  10)  Lpf.F2B13_0 low  (def)  //DISABLED by DigFilSel2.LPF
    akR.R.LpfC3.F2B13_8 = 0x0;        //  10)  Lpf.F2B13_0 high (def)  //DISABLED by DigFilSel2.LPF
    akUpd();    // 19: B0  1A: 1F  1B: 9F 1C: 02
                                      //  11)  power up MicAmp, ADC, ProgFilter:
if (RecordTestCnt==10) { akR.R.DMIC.PMDM = 1;  akR.R.DMIC.DMIC = 1; }
    akR.R.PwrMgmt1.PMADC = 1;         //  11)  ADC                Microphone Amp & ADC = Power-up  (starts init sequence, then output data)
    akR.R.PwrMgmt1.PMPFIL = 1;        //  11)  ProgFilter         Programmable Filter Block = Power up
    akUpd();

//logEvtNI("RecTst", "cnt", RecordTestCnt );
RecordTestCnt++;

  } else {
    //DISABLE
    //  12)  power down MicAmp, ADC, ProgFilter:  SigSel1.PMMP, PwrMgmt1.PMADC, .PMPFIL
    akR.R.SigSel1.PMMP = 0;           //  12)  MicAmp
    akR.R.PwrMgmt1.PMADC = 0;           //  12)  ADC
    akR.R.PwrMgmt1.PMPFIL = 0;          //  12)  ProgFilter
    akR.R.AlcMdCtr1.ALC = 0;            //  13)  ALC disable
    akUpd();
  }
#endif
}

void            cdc_SpeakerEnable( bool enable ){                           // enable/disable speaker -- using mute to minimize pop
  if ( cdcSpeakerOn==enable )   // no change?
    return;

  cdcSpeakerOn = enable;
  dbgEvt( TB_cdcSpkEn, enable,0,0,0);

#if defined( AIC3100 )
  if ( enable ){
      cdc_SetMute( true );    // no noise during transition
      // power-on DAC -- left channel only
      aicSetReg( P1_R35_LDAC_and_RDAC_Output_Routing, 0x40 ); // P1_R35: DacLtoMix: 01 Mic&Rnowhere: 00 0000
      aicSetReg( P1_R32_ClassD_Drivers,               0x80 ); // P1_R32: SpkrAmpPwrOn: 1
      aicSetReg( P1_R38_Left_Analog_Vol_to_SPL,       0x80 ); // R1_38: LchanOutToSpkr: 1  SPKgain: 000 0000 (0dB)
      aicSetReg( P0_R63_DAC_Datapath_SETUP,           0x90 ); // P0_R63: PwrLDAC: 1  PwrRDAC: 0  LDACleft: 01  RDACoff: 00  DACvol1step: 00
      dbgLog( "2 AIC DAC -> Spkr, On %d \n", tbTimeStamp());
  } else {
      cdc_SetMute( true );    // no noise during transition
      aicSetReg( P1_R32_ClassD_Drivers,               0x00 ); // P1_R32: Rst SpkrAmpPwrOn: 0
      dbgLog( "2 AIC Spkr off %d \n", tbTimeStamp());
  }
#endif
#if defined( AK4343 )
  if ( enable ){
      // power up speaker amp & codec by setting power bits with mute enabled, then disabling mute
      Codec_SetRegBits( AK_Signal_Select_1, AK_SS1_SPPSN, 0 );            // set power-save (mute) ON (==0)
      Codec_SetRegBits( AK_Power_Management_1, AK_PM1_SPK | AK_PM1_DAC, AK_PM1_SPK | AK_PM1_DAC );  // set spkr & DAC power ON
      Codec_SetRegBits( AK_Signal_Select_1, AK_SS1_DACS, AK_SS1_DACS );       // 2) DACS=1  set DAC->Spkr
      Codec_SetRegBits( AK_Signal_Select_2, AK_SS2_SPKG1 | AK_SS2_SPKG0, AK_SS2_SPKG1 );   // Speaker Gain (SPKG0-1 bits): Gain=+10.65dB(ALC off)/+12.65(ALC on)
      tbDelay_ms(2);  // AK wait at least a mSec -- ak4343 datasheet, fig. 59
      Codec_SetRegBits( AK_Signal_Select_1, AK_SS1_SPPSN, AK_SS1_SPPSN );   // set power-save (mute) OFF (==1)
  } else {
      Codec_SetRegBits( AK_Signal_Select_1, AK_SS1_SPPSN, 0 );            // set power-save (mute) ON (==0)
      Codec_SetRegBits( AK_Power_Management_1, AK_PM1_SPK | AK_PM1_DAC, AK_PM1_SPK | AK_PM1_DAC );  // set spkr & DAC power ON
      Codec_SetRegBits( AK_Signal_Select_1, AK_SS1_DACS, 0 ); // disconnect DAC => Spkr
  }
#endif
#if defined( AK4637 )
  if ( enable ){
      // startup sequence re: AK4637 Lineout Uutput pg 91
      akR.R.SigSel1.SLPSN = 0;              // set power-save (mute) ON (==0)
      akUpd();                              // and UPDATE
      gSet( gPA_EN, 1 );                    // enable power to speaker & headphones
                                            // 1) done by cdc_SetMasterFreq()
      akR.R.PwrMgmt1.LOSEL = 1;             // 2) LOSEL=1     MARC 7)
      akR.R.SigSel3.DACL = 1;                 // 3) DACL=1        MARC 8)
      akR.R.SigSel3.LVCM1_0 = 0;              // 3) LVCM=01       MARC 8)
      akR.R.DigVolCtr.DVOL7_0 = akFmtVolume;  // 4) Output_Digital_Volume
      akR.R.DigFilMd.PFDAC1_0 = 0;          // 5) PFDAC=0      default MARC 9)
      akR.R.DigFilMd.ADCPF = 1;             // 5) ADCPF=1      default MARC 9)
      akR.R.DigFilMd.PFSDO = 1;             // 5) PFSDO=1      default MARC 9)
      akR.R.PwrMgmt1.PMDAC = 1;             // 6) Power up DAC
      akR.R.PwrMgmt2.PMSL = 1;              // 7) set spkr power ON
      akUpd();                              // UPDATE all settings
      tbDelay_ms( 300 ); // AK DEBUG 30 );                      // 7) wait up to 300ms   // MARC 11)
      akR.R.SigSel1.SLPSN = 1;              // 8) exit power-save (mute) mode (==1)
      akUpd();
  } else {
    //  power down by enabling mute, then shutting off power
      // shutdown sequence re: AK4637 pg 89
      akR.R.SigSel1.SLPSN = 0;              // 13) enter SpeakerAmp Power Save Mode
      akUpd();                              // and UPDATE
      akR.R.SigSel1.DACS  = 0;              // 14) set DACS = 0, disable DAC->Spkr
      akR.R.PwrMgmt2.PMSL = 0;              // 15) PMSL=0, power down Speaker-Amp
      akR.R.PwrMgmt1.PMDAC = 0;             // 16) power down DAC
      akR.R.PwrMgmt1.PMPFIL = 0;            // 16) power down filter
      akR.R.PwrMgmt2.PMPLL = 0;             // disable PLL (shuts off Master Clock from codec)
      akUpd();                              // and UPDATE
      gSet( gPA_EN, 0 );                    // disable power to speaker & headphones

  }
#endif
}

void            cdc_PowerUp( void ){
  if ( codecHasPower ) return;
  // AIC3100 power up sequence based on sections 7.3.1-4 of Datasheet: https://www.ti.com/lit/ds/symlink/tlv320aic3100.pdf
  //  delays as recommended by Marc on 12/31/20
  gSet( gBOOT1_PDN, 0 );      // put codec in reset state PB2

  gSet( gEN_5V, 1 );          // power up EN_V5 for codec SPKVDD PD4
  tbDelay_ms( 200 );          // AIC wait for voltage regulators  // Marc: 200ms

  gSet( gEN_IOVDD_N, 0 );     // power up codec IOVDD PE4
  tbDelay_ms( 100 );          // AIC wait for voltage regulators  // Marc: 100ms
  gSet( gEN1V8, 1 );          // power up EN1V8 for codec DVDD PD5 ("shortly" after IOVDD)
  tbDelay_ms( 10 );           // AIC wait for voltage regulators

  gSet( gEN_AVDD_N, 0 );      // power up codec AVDD & HPVDD PE5 (at least 10ns after DVDD)
  tbDelay_ms( 100 );          // AIC wait for it to start up  // Marc: 100ms

  gSet( gBOOT1_PDN, 1 );      // set codec RESET_N inactive to Power on the codec PB2
  tbDelay_ms( 10 );           // AIC wait for it to start up
  codecHasPower = true;
  dbgLog( "2 AIC_pwrup %d \n", tbTimeStamp() );
}
// external interface functions


void            cdc_Init( ){                                                // Init codec & I2C (i2s_stm32f4xx.c)
  dbgEvt( TB_cdcInit, 0,0,0,0);

  // make sure aic index constats match codec_regs[] entries
  verifyCodecReg( codecNREGS, 16, 0 );  // marker at end
  verifyCodecReg( P0_R4_ClockGen_Muxing, 0, 4 );
  verifyCodecReg( P0_R14_DAC_DOSR_VAL_LSB, 0, 14 );
  verifyCodecReg( P0_R22_ADC_MAC_Engine_Decimation, 0, 22 );
  verifyCodecReg( P0_R29_Codec_Interface_Control2, 0, 29 );
  verifyCodecReg( P1_R0_Page_Select_Register, 1, 0 );
  verifyCodecReg( P1_R39_Right_Analog_Vol_to_SPR, 1, 39 );
  verifyCodecReg( P3_R0_psr, 3, 0 );
  verifyCodecReg( P4_R0_psr, 4, 0 );
  verifyCodecReg( P8_R0_psr, 8, 0 );
  verifyCodecReg( P9_R0_psr, 9, 0 );
  verifyCodecReg( P12_R0_psr, 12, 0 );
  verifyCodecReg( P13_R0_psr, 13, 0 );

//  memset( &akC, 0, sizeof( AK4637_Registers ));
  cdcSpeakerOn    = false;
  cdcMuted        = false;

  if ( !codecIsReady ){
    cdc_PowerUp();    // power-up codec
    dbgLog( "2 I2C_init %d \n", tbTimeStamp() );
    i2c_Init();       // powerup & Initialize the Control interface of the Audio Codec
    codecIsReady = true;
  }

  uint8_t rst = 1;  // software reset bit -- hardware returns to 0
  uint32_t st = tbTimeStamp();

  aicSetReg( P0_R1_Software_Reset_Register, rst );    // initiate software reset
  while ( rst != 0 ){
    rst = aicGetReg( P0_R1_Software_Reset_Register );
  }
  dbgLog( "2 AIC SftReset %d..%d \n", st, tbTimeStamp() );

  i2c_CheckRegs();    // check all default register values & report
  dbgLog( "2 AIC Regs  %d \n", tbTimeStamp() );

  #if defined( AK4343 )
    Codec_SetRegBits( AK_Signal_Select_1, AK_SS1_SPPSN, 0 );    // set power-save (mute) ON (==0)  (REDUNDANT, since defaults to 0)
  #endif
  #if defined( AK4637 )
    akR.R.SigSel1.SLPSN     = 0;            // set power-save (mute) ON (==0)  (REDUNDANT, since defaults to 0)
    akUpd();
  #endif

  #if defined( AK4343 )
    //  run in Slave mode with external clock
    Codec_WrReg( AK_Mode_Control_1, AK_I2S_STANDARD_PHILIPS );      // Set DIF0..1 to I2S standard
    Codec_WrReg( AK_Mode_Control_2, 0x00);                // MCKI input frequency = 256.Fs  (default)
  #endif
  #if defined( AK4637 )
    // will run in PLL Master mode, set up by I2S_Configure
    akR.R.MdCtr3.DIF1_0     = 3;              // DIF1_0 = 3   ( audio format = Philips )
  #endif

  if ( LastVolume == 0 ){ // first time-- set to default_volume
    LastVolume = TB_Config->default_volume;
    if (LastVolume==0) LastVolume = DEFAULT_VOLUME_SETTING;
  }
  cdc_SetVolume( LastVolume );            // set to LastVolume used

  // Extra Configuration (of the ALC)
#if defined( AIC3100 )
#endif
  #if defined( AK4343 )
    int8_t wtm = AK_TS_WTM1 | AK_TS_WTM0;       // Recovery Waiting = 3
    int8_t ztm = AK_TS_ZTM1 | AK_TS_ZTM0;       // Limiter/Recover Op Zero Crossing Timeout Period = 3
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
    akR.R.AlcTimSel.IVTM    = 1;              // IVTM = 1 (default)
    akR.R.AlcTimSel.EQFC1_0 = 2;              // EQFC1_0 = 10 (def)
    akR.R.AlcTimSel.WTM1_0  = 3;              // WTM1_0 = 3
    akR.R.AlcTimSel.RFST1_0 = 0;              // RFST1_0 = 0 (def)

    akR.R.AlcMdCtr2.REF7_0  = 0xE1;           // REF -- maximum gain at ALC recovery (default)
    akR.R.InVolCtr.IVOL7_0  = 0xE1;           // input volume control for ALC -- should be <= REF (default 0xE1 = +30dB)
    akR.R.AlcMdCtr1.ALC     = 1;              // enable ALC
    akUpd();      // UPDATE all changed registers
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

  cdc_SetMute( true );    // set soft mute on output
}
void            cdc_ClocksOff( void ){                                      // properly shut down ADC, DAC, clock tree, & PLL 
  #if defined( AIC3100 )
    cdc_RecordEnable( false );  // power down ADC & it's dividers

    // shutdown DAC & all clocks, then PLL
    const int MAX_DAC_PWR_WAIT = 10000;
    //AIC3100: follow pg 65 rules
    aicSetReg( P0_R63_DAC_Datapath_SETUP,   0x14 ); // P0_R63: DAC L & R Pwr Off (reset val) -- pg 65: begins internal sequence

    uint8_t pwrLR = 0x88;
    int cnt = 0;
    while ( pwrLR != 0 && cnt < MAX_DAC_PWR_WAIT ){ // wait till DACs power down
      pwrLR = aicGetReg( P0_R37_DAC_Flag_Register ) & 0x88;  // P0_R37: D7=LDACPwrd, D3=RDACPwrd
      cnt++;
    }
    if ( cnt == MAX_DAC_PWR_WAIT ) dbgLog( "! DACs failed to turn off" );

    // now shut down all clocks from bottom up:  CLKOUT, BCLK, M_DAC & N_DAC dividers
    aicSetReg( P0_R26_CLKOUT_M_VAL,   0x01 );    // R0.26: Rst CLKOUT_M Pwr: D7=0, powered down
    aicSetReg( P0_R30_BCLK_N_VAL,     0x01 );    // R0.30: Rst BCLK_Pwr: D7=0 powered down
    aicSetReg( P0_R12_DAC_MDAC_VAL,   0x01 );    // P0.12: Rst NDAC_PWR: 0 powers down MDAC
    aicSetReg( P0_R11_DAC_NDAC_VAL,   0x01 );    // P0.11: Rst NDAC_PWR: 0 powers down NDAC
    //  and finally, with nothing depending on it, power down PLL
    aicSetReg( P0_R5_PLL_P_R_VAL,     0x11 );    // P0.04: PLL_PWR: 0  PLL_P: 001 PLL_R: 0001 = 0x11   powers down PLL (MUST BE LAST pg 68)
    codecClockFreq = 0;
    dbgLog( "2 ClocksOff \n");
    showCdcRegs( false, false );
dbgLog( "2 AIC_clksoff %d \n", tbTimeStamp() );
  #endif
}
void            cdc_PowerDown( void ){                                        // power down entire codec (i2s_stm..)
  dbgEvt( TB_cdcPwrDn, 0,0,0,0);
  cdc_ClocksOff();  //to power down DAC channel, wait till P0_R37_Power_Status_d7_d3 == 0, then power down MDAC, then NDAC

  I2Cdrv->PowerControl( ARM_POWER_OFF );  // power down I2C
  I2Cdrv->Uninitialize( );                // deconfigures SCL & SDA pins, evt handler

  // reset, then power down codec chip
  gSet( gPA_EN, 0 );        // PD3 amplifier off
  gSet( gBOOT1_PDN, 0 );    // PB2 OUT: set power_down ACTIVE to Power Down the codec
  gSet( gEN_5V, 0 );        // PD4 OUT: 1 to supply 5V to codec   AP6714 EN
  gSet( gEN1V8, 0 );        // PD5 OUT: 1 to supply 1.8 to codec  TLV74118 EN
  gSet( gEN_IOVDD_N, 1 );   // PE4 power down codec IOVDD PE4
  gSet( gEN_AVDD_N, 1 );    // PE5 power down codec AVDD & HPVDD PE5 (at least 10ns after DVDD)

  dbgLog( "2 AIC3100 pwr down %d \n", tbTimeStamp() );
  codecHasPower = false;
  codecIsReady = false;
}
//
//
/// \brief Sets the codec volume per config file or in response to volume up/down
/// \param[in] Volume    A value between MIN_VOLUME_SETTING and MAX_VOLUME_SETTING.
///                      The volume will be set to a value that is the linear interpolation
///                      between an empirically determined minimum and maximum usable volume.
/// \return             nothing
void cdc_SetVolume( uint8_t Volume  ) {
  uint8_t v = min(MAX_VOLUME_SETTING, max(MIN_VOLUME_SETTING, Volume));

  LastVolume = v;
  if ( !codecIsReady ) return;      // just remember for next cdc_Init()

#if defined( AIC3100 )
    // The original code was this:
    // const int8_t cdcMUTEVOL = -127, cdcMAXVOL = 48;
    // const uint8_t cdcVOLRNG = cdcMAXVOL - cdcMUTEVOL;    // 175 = aic3100 digital volume range to use
    // cdcFmtVolume = cdcMUTEVOL + (v * cdcVOLRNG)/10;   // v=0 => -127 -- v=5 => -40 -- v=10 => 48
    // In words:
    // (with maximum volume = 10)
    // desired fraction of volume range = (desired volume) / (maximum volume)
    // new volume = (codec minimum volume) + (codec volume range) * (desired fraction of range)
    // This yielded these values: -127, -110, -92, -75, -57, -40, -22, -5, 13, 31, 48
    // The default initial value was 6 (via config file.
    // Empirically, we determined that 6 (-22) was a good initial value, that 10 (48) was far too loud, 9 (31) was too
    // loud, and somewhere between 8 and 9 (~22) would be a good max volume. And that 4 (-57) is the quietest useful
    // volume. The desire is four steps from the initial volume (-22) to the maximum (22), and three steps to the
    // minimum (-57). The steps are slightly different for the lower range (11-2/3) vs upper (11), but close
    // enough. If that proves otherwise, use a lookup table:
    //static int8_t AIC3100_VOLUME[] = {-57, -45, -34, -22, -11, 0, 11, 22};
    //int8_t cdcFmtVolume = AIC3100_VOLUME[v];
    const int8_t minUseful = -57;
    const int8_t maxUseful = 22;
    const uint8_t usefulRange = maxUseful - minUseful;
    int8_t cdcFmtVolume = minUseful + (usefulRange * (v-MIN_VOLUME_SETTING)) / MAX_VOLUME_SETTING;

    // Uncomment and adjust for volume level testing.
    //static uint8_t testVol = 0x19;      // DEBUG
    //if (Volume==99) //DEBUG
    //    { cdcFmtVolume = testVol;   testVol--; }  //DEBUG: test if vol>akMAXVOL causes problems

    dbgEvt( TB_cdcSetVol, Volume, cdcFmtVolume,0,0);
    dbgLog( "2 cdcSetVol v=%d cdcV=0x%x \n", v, cdcFmtVolume );

    // AIC3100 -- left channel volume:  -127..48 => -63.5dB .. 24dB
    aicSetReg( P0_R65_DAC_Left_Volume_Control, cdcFmtVolume  );      // P0_R65: -127..48 (0x81..0x30)
    dbgLog( "2 AIC Lvol %d \n", cdcFmtVolume );
#endif

#if defined( AK4343 )
    #error "This AK4343 code is unmaintained and is certainly incorrect. Retained for history/documentation."
    Codec_WrReg( AK_Lch_Digital_Volume_Control, akFmtVolume );  // Left Channel Digital Volume control
    Codec_WrReg( AK_Rch_Digital_Volume_Control, akFmtVolume );  // Right Channel Digital Volume control
#endif

#if defined( AK4637 )
    #error "This AK46573 code is unmaintained and is certainly incorrect. Retained for history/documentation."
   const uint8_t cdcMUTEVOL = 0xCC, cdcMAXVOL = 0x18, cdcVOLRNG = cdcMUTEVOL-cdcMAXVOL;   // ak4637 digital volume range to use
    // Conversion of volume from user scale [0:10] to audio codec scale [cdcMUTEVOL..cdcMAXVOL]  (AK cc..18) (TI 81..c0)
    //   values >= 0xCC force mute on AK4637
    //   limit max volume to 0x18 == 0dB (to avoid increasing digital level-- causing resets?)
    cdcFmtVolume = cdcMUTEVOL - ( v * cdcVOLRNG )/10;
    akR.R.DigVolCtr.DVOL7_0 = akFmtVolume;
    akUpd();
#endif
}

void            cdc_SetMute( bool muted ){                                  // true => enable mute on codec  (audio)
  if ( cdcMuted==muted ) return;
  dbgEvt( TB_cdcSetMute, muted,0,0,0);
  cdcMuted = muted;

  #if defined( AIC3100 )
    if ( muted ){   // enable mute -- both channels
      aicSetReg( P1_R42_SPK_Driver, 0x00   );       // P1_R42: Rst SpkrAmpGain: 00  SpkrMuteOff: 0
      aicSetReg( P0_R64_DAC_VOLUME_CONTROL, 0x0C ); // P0_R64: Rst LMuteOn: 1  RMuteOn: 1 LRsep: 00
      dbgLog( "2 AIC mute: SpkrAmp, L&R mutes on \n" );

    } else {  // disable left channel mute
      aicSetReg( P0_R64_DAC_VOLUME_CONTROL, 0x04 ); // P0_R64: LMuteOn: 0  RMuteOn: 1 LRsep: 00
      aicSetReg( P1_R42_SPK_Driver, 0x04   );       // P1_R42: SpkrAmpGain: 00  SpkrMuteOff: 1
      tbDelay_ms( 100 );  // AIC wait to let amp stabilize? before starting I2S
      dbgLog( "2 AIC unmute: SpkrAmp & L mutes off, SpkrAmp=6dB \n" );
dbgLog( "2 AIC_unmute %d \n", tbTimeStamp() );
    }
  #endif
//  akR.R.MdCtr3.SMUTE = (muted? 1 : 0);
  i2c_Upd();
}

void            cdc_SetMasterFreq( int freq ){                              // set AK4637 to MasterMode, 12MHz ref input to PLL, audio @ 'freq', start PLL  (i2s_stm32f4xx)
  #if defined( AIC3100 )
    if ( freq != codecClockFreq ){
      cdc_ClocksOff();   // changing frequency-- shut down all clocks & PLL
      if (freq==0) return;
    }
    // start PLL and all DAC clocks at new freq
    // choose parameters for codec PLL
    int PLL = 92; // 84=84.672MHz  92=92.160MHz, or 98 = 98.304MHz
    int MDAC = 90, NDAC = 2;  // 0..128 divisors of PLL_CLK
    switch ( freq ){
//        case  8000: PLL=92; MDAC=90; NDAC= 2; break;
        case  8000: PLL=92; MDAC=45; NDAC= 4; break;    // ensure DAC_CLK (and ADC_CLK < 24.576MHz) for recording at 8Khz
        case 11025: PLL=84; MDAC= 5; NDAC=24; break;
        case 12000: PLL=92; MDAC=40; NDAC= 3; break;
        case 16000: PLL=98; MDAC=16; NDAC= 6; break;
        case 22050: PLL=84; MDAC= 5; NDAC=12; break;
        case 24000: PLL=92; MDAC=10; NDAC= 6; break;
        case 32000: PLL=98; MDAC=16; NDAC= 3; break;
        case 44100: PLL=84; MDAC= 5; NDAC= 6; break;
        case 48000: PLL=92; MDAC= 5; NDAC= 6; break;
    }
    // configure PLL for 12MHz MCLK to generate audio at 'freq'
    // PLL_CLK = MCLK * R * J.D / P   (R=1 & P=1)
    int PLL_J, PLL_D;
    if      (PLL==84) {  PLL_J = 7;  PLL_D =  560; }  // J.D = 7.0560 * 12MHz => 84.672MHz
    else if (PLL==92) {  PLL_J = 7;  PLL_D = 6800; }  // J.D = 7.6800 * 12MHz => 92.160MHz
    else if (PLL==98) {  PLL_J = 8;  PLL_D = 1920; }  // J.D = 8.1920 * 12MHz => 98.304MHz
    dbgLog( "2 PLL= 12MHz * %d.%04d \n", PLL_J, PLL_D );
    // AIC3100 configuration:
    //   MClk is fed 12MHz from STM32F412 I2S3_MCLK-- set up by I2S3_ClockEnable() in I2S_stm32F4xx.c
    //   codec mode I2S 16-bit, output WClk & BClk
    //   external MClk is input to PLL_ClkIn & PLL_Clk as Codec_ClkIn

    aicSetReg(    P0_R27_Codec_Interface_Control1, 0x0C );// P0_R27:  I2S: 00  Wd16: 00 BClkOut: 1 WClkOut: 1  Rx: 0 DOut: 0  = 00001100 = 0x0C;
    aicSetReg(    P0_R4_ClockGen_Muxing,  0x03 );         // P0_R04: PLL=MClk: 00, Codec=PLL: 11 = 0x03
    aicSetReg(    P0_R6_PLL_J_VAL,        PLL_J     );    // P0_R06: PLL_J      ( integer part of PLL multiplier J.D )
    aicSet16Bits( P0_R7_PLL_D_VAL_16B,    PLL_D );        // P0_R07/08: PLL_D    fraction D (0..9999)
    aicSetReg(    P0_R5_PLL_P_R_VAL,      0x91 );         // P0_R04: PLL_PWR: 1  PLL_P: 001 PLL_R: 0001 = 0x91   powers up PLL with these parameters
    //set both DAC & ADC clock dividers
    aicSetReg(    P0_R11_DAC_NDAC_VAL,    0x80 + NDAC );  // P0_R11: NDAC_VAL  ( NDAC_PWR + NDAC divider value )
    aicSetReg(    P0_R12_DAC_MDAC_VAL,    0x80 + MDAC );  // P0_R12: MDAC_VAL  ( MDAC_PWR + MDAC divider value )
    // for recording!  ADC_N & ADC_M must be active--  ADC doesn't use DAC_MOD_CLK as implied by datasheet pg. 64,65
    aicSetReg(    P0_R18_ADC_NADC_VAL,    0x80 + NDAC );  // P0_R18: NADC_VAL  ( NADC_PWR + NADC divider value )
    aicSetReg(    P0_R19_ADC_MADC_VAL,    0x80 + MDAC );  // P0_R12: MDAC_VAL  ( MADC_PWR + MADC divider value )
    aicSet16Bits( P0_R13_DAC_DOSR_VAL_16B, 64 );          // P0_R13/14: DOSR_VAL = 0x0040   ( for DOSR = 64 )
    aicSetReg(    P0_R29_Codec_Interface_Control2, 0x01 );// P0_R29: BDIV_CLKIN = DAC_MOD_CLK, BCLK&WCLK not always active
    aicSetReg(    P0_R33_Codec_Interface_Control3, 0x00 ); // P0_R33: pri BCLK: 0 (internal), pri WCLK: 00 (Dac_fs) pri DOUT: 0 codec
    aicSetReg(    P0_R30_BCLK_N_VAL,      0x80 + 2 );     // P0_R30: BCLK N_VAL = 2  ( with DOSR = 64, BCLK = DAC_FS * 32 )
    // start DAC & external clocks
    aicSetReg(    P0_R63_DAC_Datapath_SETUP,  0x90 );     // P0_R63: PwrLDAC: 1  PwrRDAC: 0  LDACleft: 01  RDACoff: 00  DACvol1step: 00
    dbgLog( "2 AIC: BCLK = CLK / %d / %d / %d = 32* %d \n", NDAC, MDAC, 2, freq );
//    showCdcRegs( false, false );
    codecClockFreq = freq;    // remember current freq
    dbgLog( "2 AIC_freq %d \n", tbTimeStamp() );
  #endif
  #if defined( AK4637 )
  // set up AK4637 to run in MASTER mode, using PLL to generate audio clock at 'freq'
  // ref AK4637 Datasheet: pg 27-29
    akR.R.PwrMgmt2.PMPLL    = 0;  // disable PLL -- while changing settings
    akUpd();
    akR.R.MdCtr1.PLL3_0     = 6;  // PLL3_0 = 6 ( 12MHz ref clock )
    akR.R.MdCtr1.CKOFF      = 0;  // CKOFF = 0  ( output clocks ON )
    akR.R.MdCtr1.BCKO1_0    = 1;  // BICK = 1   ( BICK at 32fs )
    int FS3_0 = 0;
    switch ( freq ){
      case 8000:  FS3_0 = 1; break;
      case 11025: FS3_0 = 2; break;
      case 12000: FS3_0 = 3; break;
      case 16000: FS3_0 = 5; break;
      case 22050: FS3_0 = 6; break;
      case 24000: FS3_0 = 7; break;
      case 32000: FS3_0 = 9; break;
      case 44100: FS3_0 = 10; break;
      case 48000: FS3_0 = 11; break;
    }
    akR.R.MdCtr2.FS3_0      = FS3_0;      // FS3_0 specifies audio frequency
    akUpd();                              // UPDATE frequency settings
    akR.R.PwrMgmt2.M_S      = 1;          // M/S = 1   WS CLOCK WILL START HERE!
    akR.R.PwrMgmt1.PMVCM    = 1;          // set VCOM bit first, then individual blocks (at SpeakerEnable)
    akUpd();                              // update power & M/S
    tbDelay_ms( 4 ); // AK DEBUG 2 );                     // 2ms for power regulator to stabilize
    akR.R.PwrMgmt2.PMPLL    = 1;          // enable PLL
    akUpd();                              // start PLL
    tbDelay_ms( 10 ); // AK DEBUG 5 );                      // 5ms for clocks to stabilize
  #endif
}


//end file **************** ti_aic3100.c
