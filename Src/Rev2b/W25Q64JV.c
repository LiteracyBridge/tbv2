/*
 * Copyright (c) 2013-2018 Arm Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * -----------------------------------------------------------------------
 *
 * $Date:        19. April 2018
 * $Revision:    V1.3
 *
 * Driver:       Driver_Flash# (default: Driver_Flash0)
 * Project:      Flash Device Driver for Winbond W25Q64JV (SPI)
 * -----------------------------------------------------------------------
 * Use the following configuration settings in the middleware component
 * to connect to this driver.
 *
 *   Configuration Setting                   Value
 *   ---------------------                   -----
 *   Connect to hardware via Driver_Flash# = n (default: 0)
 * -------------------------------------------------------------------- */

/* Note:
    Use the following definitions to customize this driver:

    #define DRIVER_FLASH_NUM   #
    Replace symbol # with any integer to customize the number of exported
    driver (i.e. Driver_Flash#) Default setting is 0.
    
    #define DRIVER_SPI_NUM     #
    Replace symbol # with any integer to customize the number of SPI driver
    (i.e. Driver_SPI#) used to communicate with Flash device.
    Default setting is 0.
    
    #define DRIVER_SPI_BUS_SPEED #
    Replace symbol # with any integer to customize the SPI bus frequency.
    Default setting is 40000000 (40MHz).
 */

#include "Driver_Flash.h"
#include "Driver_SPI.h"
#include "W25Q64JV.h"		//  Winbond W25Q64JV (SPI)
#include "tb_evr.h"

#define ARM_FLASH_DRV_VERSION ARM_DRIVER_VERSION_MAJOR_MINOR(1,3) /* driver version */

#ifndef DRIVER_FLASH_NUM
#define DRIVER_FLASH_NUM        0         /* Default driver number */
#endif

#ifndef DRIVER_SPI_NUM
#define DRIVER_SPI_NUM          4 
#endif

#ifndef DRIVER_SPI_BUS_SPEED
#define DRIVER_SPI_BUS_SPEED    40000000  /* Default SPI bus speed */
#endif

/* SPI Data Flash definitions */
#define PAGE_SIZE       FLASH_PAGE_SIZE   	/* Page size   */
#define SECTOR_SIZE     (16 * PAGE_SIZE)   	/* Sector size */
#define SECTOR_MASK		(0x0FFF)			/* 4KB sector address */

/* SPI Data Flash Commands */
#define CMD_READ_STATUS1        (0x05)
#define CMD_READ_STATUS2        (0x35)
#define CMD_READ_STATUS3        (0x15)
#define CMD_PAGE_READ           (0x03)
#define CMD_WR_ENABLE           (0x06)
#define CMD_PAGE_PROGRAM        (0x02)
#define CMD_SECTOR_ERASE        (0x20)
#define CMD_BLOCK_ERASE         (0x52)
// #define CMD_CHIP_ERASE        	(0xC7)
#define CMD_CHIP_ERASE        	(0x60)

/* Flash Driver Flags */
#define FLASH_INIT              (0x01U)
#define FLASH_POWER             (0x02U)


/* SPI Driver */
#define _SPI_Driver_(n)  Driver_SPI##n
#define  SPI_Driver_(n)  _SPI_Driver_(n)
extern ARM_DRIVER_SPI     SPI_Driver_(DRIVER_SPI_NUM);
#define ptrSPI          (&SPI_Driver_(DRIVER_SPI_NUM))

/* SPI Bus Speed */
#define SPI_BUS_SPEED   ((uint32_t)DRIVER_SPI_BUS_SPEED)

// W25Q64JV has uniform Sectors -- so no FLASH_SECTOR_INFO[]

extern	ARM_DRIVER_FLASH ARM_Driver_Flash_( DRIVER_FLASH_NUM );									// export driver control block for flash

static ARM_FLASH_INFO 								FlashInfo = {															// Flash Information
  NULL,					// no flash sector info
  FLASH_SECTOR_COUNT,
  FLASH_SECTOR_SIZE,
  FLASH_PAGE_SIZE,
  FLASH_PROGRAM_UNIT,
  FLASH_ERASED_VALUE,
#if (ARM_FLASH_API_VERSION > 0x201U)
  { 0U, 0U, 0U }
#endif
};
static const ARM_DRIVER_VERSION 			DriverVersion = {													// Driver Version
  ARM_FLASH_API_VERSION,
  ARM_FLASH_DRV_VERSION
};
static const ARM_FLASH_CAPABILITIES 	DriverCapabilities = {										// Driver Capabilities
  0U,   	// event_ready */
  0U,   	// data_width = 0:8-bit, 1:16-bit, 2:32-bit */
  1U,   	// erase_chip */
#if (ARM_FLASH_API_VERSION > 0x200U)
  0U    	// reserved */
#endif
};
static ARM_FLASH_STATUS 							FlashStatus;															// Flash Status
static uint8_t 												Flags;
//
static int32_t												getReg( uint8_t cmd, uint8_t * sreg );							// forward for GetStatus
static bool 													isSpiErr( int32_t err, bool reset, int32_t FnStp );	// forward for GetStatus
static ARM_DRIVER_VERSION 						GetVersion (void) {												// return driver version
  return DriverVersion;
}
static ARM_FLASH_CAPABILITIES 				GetCapabilities (void) { 									// return driver capabilities.
  return DriverCapabilities;
}
static ARM_FLASH_INFO * 							GetInfo (void) { 													// return Flash information.
  return &FlashInfo;
}
static ARM_FLASH_STATUS 							GetStatus (void) {												// return flash status
  const int FN = 60;
	const uint8_t STAT1_BUSY = 0x01;
	uint8_t statR1 = 0;
	int result = getReg( CMD_READ_STATUS1, &statR1 );
  if ( isSpiErr( result, false, FN+1 ) )
			FlashStatus.error = 1;

	FlashStatus.busy = (statR1 & STAT1_BUSY)!=0;
	return FlashStatus;
}

// INTERNAL 
static uint8_t 	currCmd;
static uint32_t maxBusyCnt = 0;
const  uint32_t MAX_WAIT = 50000;

static bool 		isSpiErr( int32_t err, bool reset, int32_t FnStp ){							// if error report & opt. reset SPI
  if ( err == ARM_DRIVER_OK ) return false;
  
  dbgEvt( TB_norErr, err, FnStp, currCmd, 0);
  FlashStatus.error = 1;
  if ( reset )
		ptrSPI->Control( ARM_SPI_CONTROL_SS, ARM_SPI_SS_INACTIVE );
  return true;
}
static int32_t	getReg( uint8_t cmd, uint8_t * sreg ){																								// read Status 1
  uint32_t result;
  int waitcnt = 0;
  char buf[] = { cmd };  				// Read register Command 
  const int FN = 10;
  
  result = ptrSPI->Control( ARM_SPI_CONTROL_SS, ARM_SPI_SS_ACTIVE );  // Select Slave 
  if ( isSpiErr( result, false, FN+1 ) ) return result;

  result = ptrSPI->Send( buf, 1 );  // Send Command byte
  if ( isSpiErr( result, true, FN+2 ) ) return result;
  while ( ptrSPI->GetDataCount() != 1 ) { waitcnt++;  if (waitcnt > MAX_WAIT) return ARM_DRIVER_ERROR; }

  result = ptrSPI->Receive( buf, 1 );	// Read result byte
  if ( isSpiErr( result, true, FN+3 ) ) return result;
  while ( ptrSPI->GetDataCount() != 1 ) { waitcnt++;  if (waitcnt > MAX_WAIT) return ARM_DRIVER_ERROR; }
	
  result = ptrSPI->Control( ARM_SPI_CONTROL_SS, ARM_SPI_SS_INACTIVE );  	// Deselect Slave
  if ( isSpiErr( result, false, FN+4 ) ) return result;
	
  *sreg = buf[0];
  return ARM_DRIVER_OK;
}
static bool			WaitWhileBusy(){																								// read Status 1 until Status1.BUSY goes false
  const int MAX_LONG_OP = 10000000, MAX_SHORT_OP = 5; 
	uint32_t result, waitcnt = 0, busycnt = 0;
  char buf[] = { CMD_READ_STATUS1 };  	// Read Status Register 1 Command 
  const uint8_t STAT1_BUSY = 0x01;
  const int FN = 20;
  
  result = ptrSPI->Control( ARM_SPI_CONTROL_SS, ARM_SPI_SS_ACTIVE );  // Select Slave 
  if ( isSpiErr( result, false, FN+1 ) ) return false;

  result = ptrSPI->Send( buf, 1 );  // Send Read Status1 Command 
  if ( isSpiErr( result, true, FN+2 ) ) return false;
  while ( ptrSPI->GetDataCount() != 1 ) { waitcnt++;  if (waitcnt > MAX_WAIT) return false; }

  while ( busycnt < MAX_LONG_OP ) {  		// Read Status Register 1 repeatedly, until Busy(bit0)==0 
    result = ptrSPI->Receive( buf, 1 );	
		if ( isSpiErr( result, true, FN+3 ) ) return false;
    while ( ptrSPI->GetDataCount() != 1 ) { waitcnt++;  if (waitcnt > MAX_WAIT) return false; }
	
		uint8_t stat1 = buf[0];
		if ( (stat1 & STAT1_BUSY) == 0 ) break;
		
		busycnt++;
  }
  if ( busycnt > MAX_SHORT_OP ) 
		dbgEvt( TB_longOp, busycnt, currCmd,0,0 );
	if ( busycnt > maxBusyCnt ) 
		maxBusyCnt = busycnt;
	
  result = ptrSPI->Control( ARM_SPI_CONTROL_SS, ARM_SPI_SS_INACTIVE );  	// Deselect Slave
  if ( isSpiErr( result, false, FN+4 ) ) return false;
  return true;
}
static bool			SendWriteCommand( uint8_t cmd ){																// send write enable, then cmd
  uint32_t result, waitcnt = 0;
	const uint8_t STAT1_WEL = 0x02, STAT3_DRV1_0 = 0x60; 
  const int FN = 30;
  char buf[] = { CMD_WR_ENABLE, cmd };
  uint32_t cnt = cmd==0? 1 : 2;
  uint32_t sentCnt;
	currCmd = cmd;
	
  result = ptrSPI->Control( ARM_SPI_CONTROL_SS, ARM_SPI_SS_ACTIVE );  // Select Slave
  if ( isSpiErr( result, false, FN+1 )) return false; 

  result = ptrSPI->Send( buf, cnt ); 		 // Send WriteEnable, then cmd
  if ( isSpiErr( result, true, FN+2 )) 	return false; 
 
  while ( (sentCnt = ptrSPI->GetDataCount()) != cnt ) { 
    waitcnt++;  
    if (waitcnt > MAX_WAIT) 
        return false; 
  }
  if ( cnt==1 ){
		result = ptrSPI->Control( ARM_SPI_CONTROL_SS, ARM_SPI_SS_INACTIVE );  	// Deselect Slave
		if ( isSpiErr( result, false, FN+4 ) ) return false;

		// verify change to Status1.WEL
		uint8_t stat1 = 0, stat3 = 0;
		result = getReg( CMD_READ_STATUS1, &stat1 );
		if ( isSpiErr( result, false, FN+3 ) ) return false;
		
		result = getReg( CMD_READ_STATUS3, &stat3 );
		if ( isSpiErr( result, false, FN+3 ) ) return false;
		
		if ( (stat1 & STAT1_WEL)==0  || (stat3 & STAT3_DRV1_0) != STAT3_DRV1_0 ) 
			printf( "NorLog: stat1=0x%x stat3=0x%x \n", stat1, stat3 );
		return true;
	}
  result = ptrSPI->Control( ARM_SPI_CONTROL_SS, ARM_SPI_SS_INACTIVE );  	// Deselect Slave
  if ( isSpiErr( result, false, FN+4 ) ) return false;

  // if cmd (i.e. erase), wait while busy
  if ( cmd != 0 )
		if ( !WaitWhileBusy() ) 				return false;
  return true;
}
static bool 		SendCommand (uint8_t cmd, uint32_t addr, const uint8_t *data, uint32_t size, bool wait ){		// Send command with optional data and wait until busy
  const int FN = 40;
  uint8_t  buf[4] = { 		// define buffer with cmd & addr
	cmd, 
	(uint8_t)((addr >> 16)& 0xFF),
	(uint8_t)((addr >>  8)& 0xFF),
	(uint8_t)((addr >>  0)& 0xFF) 
  };
  int32_t  result, waitcnt = 0;
  uint32_t sentCnt;
	currCmd = cmd;

  result = ptrSPI->Control( ARM_SPI_CONTROL_SS, ARM_SPI_SS_ACTIVE ); 		 // Select Slave 
  if ( isSpiErr( result, false, FN+1 ) ) return false;

  result = ptrSPI->Send( buf, 4 );  				// Send Command with address 
  if ( isSpiErr( result, true, FN+2 ) ) return false;

  while ( (sentCnt = ptrSPI->GetDataCount()) != 4) { 
    waitcnt++;  
    if (waitcnt > MAX_WAIT) 
        return false; 
  }

  if ( (data != NULL) && (size != 0) ) {  // Send Data?
    result = ptrSPI->Send(data, size);
	if ( isSpiErr( result, true, FN+3 )) return false;
    while ( (sentCnt = ptrSPI->GetDataCount()) != size ) { 
        waitcnt++;  
        if (waitcnt > MAX_WAIT) 
            return false; 
    }
  }
  
  result = ptrSPI->Control( ARM_SPI_CONTROL_SS, ARM_SPI_SS_INACTIVE ); 	 // Deselect Slave 
  if ( isSpiErr( result, false, FN+4 ) ) return false;
 
  if ( wait )
	return WaitWhileBusy();

  return true;
}

// EXPORTED in ARM_Driver_Flash_
static int32_t 	Initialize (ARM_Flash_SignalEvent_t cb_event) {									// init flash interface (ignores callback)
  int32_t status;
  (void)cb_event;

  FlashStatus.busy  = 0U;
  FlashStatus.error = 0U;

  status = ptrSPI->Initialize(NULL);
  if (status != ARM_DRIVER_OK) { return ARM_DRIVER_ERROR; }

  Flags = FLASH_INIT;

  return ARM_DRIVER_OK;
}
static int32_t 	Uninitialize (void) { 																					// De-initialize the Flash Interface.

  Flags = 0U;
  return ptrSPI->Uninitialize();
} 
static int32_t 	PowerControl( ARM_POWER_STATE state ) {													// set flash power to 'state'
  int32_t status;

  switch ((int32_t)state) {
    case ARM_POWER_OFF:
      Flags &= ~FLASH_POWER;
      FlashStatus.busy  = 0U;
      FlashStatus.error = 0U;

      return ptrSPI->PowerControl(ARM_POWER_FULL);

    case ARM_POWER_FULL:
      if ((Flags & FLASH_INIT) == 0U)       return ARM_DRIVER_ERROR;
      if ((Flags & FLASH_POWER) == 0U) {
        status = ptrSPI->PowerControl(ARM_POWER_FULL);
        if ( status != ARM_DRIVER_OK ) 		return ARM_DRIVER_ERROR;

        status = ptrSPI->Control(ARM_SPI_MODE_MASTER | ARM_SPI_CPOL0_CPHA0  |
                                                       ARM_SPI_DATA_BITS(8) |
                                                       ARM_SPI_MSB_LSB      |
                                                       ARM_SPI_SS_MASTER_SW,
                                                       SPI_BUS_SPEED);
        if ( status != ARM_DRIVER_OK ) 		return ARM_DRIVER_ERROR;
        FlashStatus.busy  = 0U;
        FlashStatus.error = 0U;
        Flags |= FLASH_POWER;
      }
      return ARM_DRIVER_OK;

    case ARM_POWER_LOW:
      return ARM_DRIVER_ERROR_UNSUPPORTED;

    default:
      return ARM_DRIVER_ERROR;
  }
}
static int32_t 	ReadData (uint32_t addr, void *data, uint32_t cnt) {						// read 'cnt' bytes into 'data' from 'addr' in flash --> status
  const int FN = 50;
  uint8_t  buf[] = { CMD_PAGE_READ, ((addr >> 16) & 0xFF), ((addr >> 8) & 0xFF), (addr & 0xFF) };
  int32_t  result, waitcnt = 0;

  if (data == NULL) 			  return ARM_DRIVER_ERROR_PARAMETER;

  result = ptrSPI->Control(ARM_SPI_CONTROL_SS, ARM_SPI_SS_ACTIVE);  // Select Slave
  if ( isSpiErr( result, false, FN+1 )) return ARM_DRIVER_ERROR; 

  result = ptrSPI->Send( buf, 4 ); 		 // Send Command with Address
  if ( isSpiErr( result, true, FN+2 )) return ARM_DRIVER_ERROR; 
  while ( ptrSPI->GetDataCount() != 4 ) { waitcnt++;  if (waitcnt > MAX_WAIT) return 0; }

  result = ptrSPI->Receive( data, cnt ); 		 // Receive Data 
  if ( isSpiErr( result, true, FN+3 )) return ARM_DRIVER_ERROR; 
  while ( ptrSPI->GetDataCount() != cnt ) { waitcnt++;  if (waitcnt > MAX_WAIT) return 0; }

  result = ptrSPI->Control(ARM_SPI_CONTROL_SS, ARM_SPI_SS_INACTIVE);  // Deselect Slave 
  if ( isSpiErr( result, false, FN+4 )) return ARM_DRIVER_ERROR; 

  return (int32_t) cnt;
}
static int32_t 	ProgramData (uint32_t addr, const void *data, uint32_t cnt) {		// write 'cnt' bytes of 'data' at 'addr' in flash
  if ( !SendWriteCommand(0) ) 										return ARM_DRIVER_ERROR;
  if ( !SendCommand( CMD_PAGE_PROGRAM, addr, data, cnt, true ))  	return ARM_DRIVER_ERROR; 

  return ARM_DRIVER_OK;
}
static int32_t 	EraseSector( uint32_t addr ) {																	// erase Flash 4KB Sector at addr --> status
  if ( (addr & SECTOR_MASK) != 0 ) 									return ARM_DRIVER_ERROR; 

  if ( !SendWriteCommand( 0 ) ) 										return ARM_DRIVER_ERROR;
  if ( !SendCommand( CMD_SECTOR_ERASE, addr, NULL, 0, true ))	    return ARM_DRIVER_ERROR;
 
  return ARM_DRIVER_OK;
}
static int32_t 	EraseChip (void) {																							// erase complete Flash.

  if ( !SendWriteCommand( CMD_CHIP_ERASE ) ) 						return ARM_DRIVER_ERROR;

  return ARM_DRIVER_OK;
}

ARM_DRIVER_FLASH 											ARM_Driver_Flash_(DRIVER_FLASH_NUM) = {		// define Flash Driver Control Block 
  GetVersion,
  GetCapabilities,
  Initialize,
  Uninitialize,
  PowerControl,
  ReadData,
  ProgramData,
  EraseSector,
  EraseChip,
  GetStatus,
  GetInfo
};
// end W25Q64JV.c
