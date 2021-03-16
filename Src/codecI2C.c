//******* codecI2C.c   
//  I2C communication to audio codec, using CMSIS-Driver/I2C
//  extracted from ak4xxx.c for use by ti_aic3100

/* Includes ------------------------------------------------------------------*/
#include "Driver_I2C.h"			// CMSIS-Driver I2C interface
#include "codecI2C.h"				// I2C externals

//*****************  CMSIS I2C driver interface
extern ARM_DRIVER_I2C     Driver_I2C1;
static ARM_DRIVER_I2C *		I2Cdrv = 				&Driver_I2C1;

const 					uint8_t		I2C_Xmt = 0;
const 					uint8_t		I2C_Rcv = 1;
const 					uint8_t		I2C_Evt = 2;		// errors reported from ISR 
const 					uint8_t		Cdc_DefReg = 3;
const 					uint8_t		Cdc_WrReg = 4;
static int 			I2C_ErrCnt[ ]= { 0,0,0,0,0 };

static volatile int32_t 				I2C_Event = 0;
static int											MaxBusyWaitCnt = 0;
static int											eCnt = 0;
const  int 											maxErrLog	= 100;				// max errors to add to log

static int 											reinitCnt = 0;	

void 						Codec_WrReg( uint8_t Reg, uint8_t Value);		// FORWARD

void 		 				cntErr( int8_t grp, int8_t exp, int8_t got, int32_t iReg, int16_t caller ){  // count and/or report ak errors
	if ( got == exp ) return;  // || codecDummyWrite ) return;
//	codecDummyWrite = false;
	I2C_ErrCnt[ grp ]++;
	eCnt++;
	dbgEvt( TB_i2cErr, (caller << 8) + grp, exp, got, iReg );
	if ( eCnt < maxErrLog && grp!=I2C_Evt ){	// can't report I2C_Evt (from ISR)
		char pgRg[20];
		uint8_t pg = codec_regs[iReg].pg, reg = codec_regs[iReg].reg;
		sprintf( pgRg, "%d=P%d:R%02d ", iReg, pg, reg );
		switch( caller ){
			case 1: break; // i2c_Sig_Event from ISR -- can't report
			case 2: dbgLog( "2 RdReg xmt: %s got 0x%x not 0x%x \n", pgRg, got, exp ); break;
			case 3: dbgLog( "2 RdReg xmt dn: %s got 0x%x not 0x%x \n", pgRg, got, exp ); break;
			case 4: dbgLog( "2 RdReg rcv: %s got 0x%x not 0x%x \n", pgRg, got, exp ); break;
			case 5: dbgLog( "2 RdReg rcv dn: %s got 0x%x not 0x%x \n", pgRg, got, exp ); break;
			case 6: dbgLog( "2 WrReg xmt: %s got 0x%x not 0x%x \n", pgRg, got, exp ); break;
			case 7: dbgLog( "2 WrReg xmt dn: %s got 0x%x not 0x%x \n", pgRg, got, exp ); break;
			case 8: dbgLog( "2 WrReg cmp: %s got 0x%x not 0x%x \n", pgRg, got, exp ); break;
			case 9: dbgLog( "2 chkRg: %d:%02d  0x%02x not 0x%02x \n", pg, reg, got&0xFF, exp&0xFF ); break;
		}
		//errLog("%s err, exp=0x%x, got=0x%x %s caller=%d", cdcGrp[grp], exp, got, pgRg, caller );
	}
}
void 						i2c_Sig_Event( uint32_t event ){														// called from ISR for I2C errors or complete
	I2C_Event = event;
	cntErr( I2C_Evt, ARM_I2C_EVENT_TRANSFER_DONE, event, I2Cdrv->GetDataCount(), 1 );
}
void		 				i2c_initialize() {																					// Initialize I2C device
//	I2Cdrv->Control( ARM_I2C_BUS_CLEAR, 0 );  // resets GPIO, etc. -- if needed, should be before initialization
  I2Cdrv->Initialize( i2c_Sig_Event );					// sets up SCL & SDA pins, evt handler
  I2Cdrv->PowerControl( ARM_POWER_FULL );		// clocks I2C device & sets up interrupts
  I2Cdrv->Control( ARM_I2C_BUS_SPEED, ARM_I2C_BUS_SPEED_STANDARD );		// set I2C bus speed
  I2Cdrv->Control( ARM_I2C_OWN_ADDRESS, 0 );		// set I2C own address-- REQ to initialize properly
}
void 						i2c_Init() {																								// Initialize I2C connection to reset codec & write a reg
  i2c_initialize();				// init device
}
void						i2c_Reinit(int lev ){																				// lev&1 SWRST, &2 => RCC reset, &4 => device re-init
	if ( lev & 4 ){
		I2Cdrv->Uninitialize( );					// deconfigures SCL & SDA pins, evt handler
		i2c_initialize();
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
		tbDelay_ms( 3 ); //DEBUG 1);
		I2C1->CR1 = cr1;			// reset to previous
	}
}
uint8_t 				i2c_rdReg( uint8_t reg ){																		// return value of codec register Reg 
	int status;
	uint8_t value;
	
  I2C_Event = 0U;  		// Clear event flags before new transfer
  status = I2Cdrv->MasterTransmit( AUDIO_I2C_ADDR, &reg, 1, true );		// send register index
	cntErr( I2C_Xmt, ARM_DRIVER_OK, status, 0, 2 );
	
	int waitCnt = 0;
  while (( I2C_Event & ARM_I2C_EVENT_TRANSFER_DONE ) == 0U)
		waitCnt++;  // busy wait until transfer completed
	
	cntErr( I2C_Xmt, ARM_I2C_EVENT_TRANSFER_DONE, I2C_Event, 0, 3 );  // err if any other bits set

  I2C_Event = 0U;  		// Clear event flags before new transfer
	status = I2Cdrv->MasterReceive( AUDIO_I2C_ADDR, &value, 1, false );  // receive register contents into *value
	cntErr( I2C_Rcv, ARM_DRIVER_OK, status, reg, 4 );

  while (( I2C_Event & ARM_I2C_EVENT_TRANSFER_DONE ) == 0U)
		waitCnt++;  // busy wait until transfer completed
	if ( waitCnt > MaxBusyWaitCnt ) MaxBusyWaitCnt = waitCnt;
	cntErr( I2C_Xmt, ARM_I2C_EVENT_TRANSFER_DONE, I2C_Event, 0, 5 );  // err if any other bits set
	dbgLog( "3     r %02d = %02x\n", reg, value );
	
	return value;
}

void 						i2c_wrReg( uint8_t reg, uint8_t val ){									// write codec register reg with val
	dbgEvt( TB_cdWrReg, 0, 0, reg, val );

	uint32_t status;
	int waitCnt = 0;
  
	uint8_t buf[2];
  buf[0] = reg;
  buf[1] = val; 
	
  I2C_Event = 0U;  		// Clear event flags before new transfer
  status = I2Cdrv->MasterTransmit( AUDIO_I2C_ADDR, buf, 2, false );
	cntErr( I2C_Xmt, ARM_DRIVER_OK, status, reg, 6 );
	
  while (( I2C_Event & ARM_I2C_EVENT_TRANSFER_DONE ) == 0U){
		waitCnt++;  // busy wait until transfer completed
		if ( waitCnt > 1000 ){
			dbgLog("I2C Tx hang, r=%d v=0x%x", reg, val);
			reinitCnt++;
			if ( reinitCnt > 16 ) tbErr( "Can't restart I2C\n" );
			i2c_Reinit( reinitCnt );			// SW, RST, SW+RST, INIT, INIT+SW, ...
			i2c_wrReg( reg, val );		// recursive call to retry on reset device
			return;
		}
	}
	if ( waitCnt > MaxBusyWaitCnt ) MaxBusyWaitCnt = waitCnt;
	cntErr( I2C_Xmt, ARM_I2C_EVENT_TRANSFER_DONE, I2C_Event, 0, 7 );  // err if any other bits set
	
	dbgLog( "3     w %02d = %02x\n", reg, val );
	reinitCnt = 0;			// success-- reset recursive counter
}
void  					i2c_ReportErrors(){																					// report I2C error counts
  int tot = 0;
	for ( int i=0; i<= Cdc_WrReg; i++ )
	  tot += I2C_ErrCnt[i];
	if (tot==0) return;
	
	dbgLog( "! %d I2C Errs-- Xmt:%d Rcv:%d Evt:%d Def:%d WrReg:%d \n", tot,
		I2C_ErrCnt[ I2C_Xmt ], I2C_ErrCnt[ I2C_Rcv ], I2C_ErrCnt[ I2C_Evt ], I2C_ErrCnt[ Cdc_DefReg ], I2C_ErrCnt[ Cdc_WrReg ] );
}

//end file **************** codecI2C.c
