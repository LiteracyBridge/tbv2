// JEB based on keil/arm/pack/amr/cmsis/pack/example/cmsis_driver/I2S_LBC18xx.h
// maybe names should be all SAI instead of I2S 
// I2C_STM32F4xx.h  
/* -------------------------------------------------------------------------- 
 * Copyright (c) 2013-2016 ARM Limited. All rights reserved.
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
 * $Date:        02. March 2016
 * $Revision:    V1.1
 *
 * Project:      I2S Driver Definitions for NXP STM32F10X
 * -------------------------------------------------------------------------- */

#ifndef __I2S_STM32F4XX_H
#define __I2S_STM32F4XX_H

#include "STM32F4xx.h"
#include "Driver_SAI.h"	
#include "GPIO_STM32F4xx.h"

/* REPLACED: RTE_Device based configuration -- with gpio_def[ GPIO_ID ] from main.h
/#include "RTE_Components.h"
//  #include "RTE_Device.h"		// custom version already included
// use Signal definitions based on GPIO_ID from main.h -- instead of RTE_Device
*/

// I2S2 peripheral configuration defines -- synonyms for SPI2
// device registers: 							I2S2
// device interrupt number: 			I2S2_IRQn
// device clock enable: 					RCC_APB1ENR_I2S2EN
// interrupt service routine: 		I2S2_IRQHandler
#define I2S2                         			SPI2
#define I2S3                         			SPI3
#define I2S2_IRQn													SPI2_IRQn
#define RCC_APB1ENR_I2S2EN								RCC_APB1ENR_SPI2EN
#define I2S2_IRQHandler               		SPI2_IRQHandler

// RCC 							= 0x40023800
// RCC_CR						= 0x40023800
// RCC_PLLI2SCFGR 	= 0x40023884
// RCC_DCKCFGR 			= 0x4002388C
// I2S2 RCC configuration 
// RCC_PLLI2SCFGR_PLLI2SR  R	2..7  		default: 2
// RCC_PLLI2SCFGR_PLLI2SQ  Q	2..15 		default: 4
// RCC_PLLI2SCFGR_PLLI2SRC 0/1   				default: 0 = HSI (16MHz)
// RCC_PLLI2SCFGR_PLLI2SN  N	2..432 		default: 0x0c0 = 192
// RCC_PLLI2SCFGR_PLLI2SM	 M  2..63			default: 0x10 = 16  
//  VCO_clk = 16MHz * N/M 							default: 192MHz
//  USB/SDIO clk = VCO_clk/ Q						default: 48MHz
// 	I2S_clk = VCO_clk/ R								default: 96MHz
// RCC_DCKCFGR_I2S1SRC  0..3  					default: 0 = PLLI2S
//  I2S1SRC  selects I2S for APB1, i.e. SPI2
// RCC_CR_PLLI2SON  1 to enable PLLI2S	default: 0
//

// STM32F412 SPI2 I2SCFGR register values-- only support I2S_MODE_MASTER_xX
// I2S_I2SMOD	-- selects I2S mode instead of SPI
#define I2S_MODE												SPI_I2SCFGR_I2SMOD
// I2S_I2SE -- enables I2S peripheral device (should be off while changing settings)
#define I2S_MODE_ENAB										SPI_I2SCFGR_I2SE

#define I2S_CFG_SLAVE_TX                (0x00000000U)
#define I2S_CFG_SLAVE_RX                (SPI_I2SCFGR_I2SCFG_0)
#define I2S_CFG_MASTER_TX               (SPI_I2SCFGR_I2SCFG_1)
#define I2S_CFG_MASTER_RX              	((SPI_I2SCFGR_I2SCFG_0 | SPI_I2SCFGR_I2SCFG_1))

// I2S_Standard I2S Standard -- only supports I2S_STANDARD_PHILIPS
#define I2S_STANDARD_PHILIPS            (0x00000000U)
#define I2S_STANDARD_MSB                (SPI_I2SCFGR_I2SSTD_0)
#define I2S_STANDARD_LSB                (SPI_I2SCFGR_I2SSTD_1)
#define I2S_STANDARD_PCM_SHORT          ((SPI_I2SCFGR_I2SSTD_0 | SPI_I2SCFGR_I2SSTD_1))
#define I2S_STANDARD_PCM_LONG           ((SPI_I2SCFGR_I2SSTD_0 | SPI_I2SCFGR_I2SSTD_1 | SPI_I2SCFGR_PCMSYNC))

// I2S_Data_Format I2S Data Format
#define I2S_DATAFORMAT_16B               (0x00000000U)
#define I2S_DATAFORMAT_16B_EXTENDED      (SPI_I2SCFGR_CHLEN)
#define I2S_DATAFORMAT_24B               ((SPI_I2SCFGR_CHLEN | SPI_I2SCFGR_DATLEN_0))
#define I2S_DATAFORMAT_32B               ((SPI_I2SCFGR_CHLEN | SPI_I2SCFGR_DATLEN_1))

// I2S_MCLK_Output I2S MCLK Output -- only supports I2S_MCLKOUTPUT_ENABLE
#define I2S_MCLKOUTPUT_ENABLE            (SPI_I2SPR_MCKOE)
#define I2S_MCLKOUTPUT_DISABLE           (0x00000000U)

// I2S_Audio_Frequency I2S Audio Frequency
#define I2S_AUDIOFREQ_192K               (192000U)
#define I2S_AUDIOFREQ_96K                (96000U)
#define I2S_AUDIOFREQ_48K                (48000U)
#define I2S_AUDIOFREQ_44K                (44100U)
#define I2S_AUDIOFREQ_32K                (32000U)
#define I2S_AUDIOFREQ_22K                (22050U)
#define I2S_AUDIOFREQ_16K                (16000U)
#define I2S_AUDIOFREQ_11K                (11025U)
#define I2S_AUDIOFREQ_8K                 (8000U)
#define I2S_AUDIOFREQ_DEFAULT            (2U)

// I2S_Clock_Polarity I2S Clock Polarity
#define I2S_CPOL_LOW                     (0x00000000U)
#define I2S_CPOL_HIGH                    (SPI_I2SCFGR_CKPOL)

// I2S flags
#define I2S_FLAG_INITIALIZED            (     1U)
#define I2S_FLAG_POWERED                (1U << 1)
#define I2S_FLAG_CONFIGURED             (1U << 2)

// SPI2 CR1 register values
#define I2S_CR1_DFF											SPI_CR1_DFF    		/*!< Data Frame Format */

// SPI2 CR2 register values-- only support I2S_MODE_MASTER_xX
#define I2S_CR2_RXDMAEN									SPI_CR2_RXDMAEN   /*!< Rx Buffer DMA Enable */
#define I2S_CR2_TXDMAEN									SPI_CR2_TXDMAEN   /*!< Tx Buffer DMA Enable */
#define I2S_CR2_SSOE                   	SPI_CR2_SSOE     	/*!< SS Output Enable */
#define I2S_CR2_ERRIE                  	SPI_CR2_ERRIE    	/*!< Error Interrupt Enable */
#define I2S_CR2_RXNEIE                 	SPI_CR2_RXNEIE   	/*!< RX buffer Not Empty Interrupt Enable */
#define I2S_CR2_TXEIE                  	SPI_CR2_TXEIE    	/*!< Tx buffer Empty Interrupt Enable */

#define USE_I2S0

// I2S Information (Run-Time)
typedef struct _I2S_INFO {
  ARM_SAI_SignalEvent_t   cb_event;      // Event callback
  ARM_SAI_STATUS          status;        // Status flags
//	bool 										monoMode;	 		 // mono data stream-- use sample duplicating intq handler
//	uint16_t							* dataPtr;			 // monoMode-- curr 16bit sample being transmitted
//	bool										firstChannel;	 // monoMode-- ready to send 1st copy of *dataPtr
	
	uint32_t                tx_req;				 // bytes requested to be sent
	uint32_t                tx_cnt;				 // tx bytes sent

	uint32_t                rx_req;				 // bytes requested to be received
	uint32_t                rx_cnt;				 // rx bytes received
  uint8_t                 data_bytes;    // Number of bytes/sample
  uint8_t                 flags;         // I2S driver flags
} I2S_INFO;

// I2S Resource definitions
typedef struct _I2S_RESOURCES {
  ARM_SAI_CAPABILITIES    capabilities;  // Capabilities
  SPI_TypeDef		          *xmt_inst;     // Pointer to I2S peripheral for playback
	SPI_TypeDef							*rcv_inst;		 // I2S peripheral for recording
	SPI_TypeDef							*clock;				 // I2S peripheral used to generate clock
	DMA_TypeDef							*dma;					 // DMA controller for tx & rx streams
	DMA_Stream_TypeDef			*tx_dma;			 // DMA stream for TX
	uint16_t 								tx_dma_chan;	 // tx_dma_chan  channel corresponding to tx stream
	DMA_Stream_TypeDef			*rx_dma;			 // DMA stream for RX
	uint16_t 								rx_dma_chan;	 // rx_dma_chan  channel corresponding to rx stream
	
	//REPLACED:  I2S_PINS                rxtx_pins;     // I2S receive pins configuration
	// pins used are  gI2S2_SD,	gI2S2_WS,	gI2S2_CK,	gI2S2_MCK

  IRQn_Type               irq_num;       // I2S IRQ Number
  I2S_INFO                *info;         // Run-Time information
} const I2S_RESOURCES;

#endif // __I2S_STM32F4XX_H
