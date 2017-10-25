/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * o Redistributions of source code must retain the above copyright notice, this list
 *   of conditions and the following disclaimer.
 *
 * o Redistributions in binary form must reproduce the above copyright notice, this
 *   list of conditions and the following disclaimer in the documentation and/or
 *   other materials provided with the distribution.
 *
 * o Neither the name of Freescale Semiconductor, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "board.h"
#include "debug_console_imx.h"
#include "i2c_imx.h"

// Device
#define IAQCORE_ADDRESS				(0x5A)
#define IAQCORE_DATA_SIZE			9
// Status codes
#define IAQCORE_STATUS_OK			(0x00)
#define IAQCORE_STATUS_RUNIN			(0x10)
#define IAQCORE_STATUS_BUSY			(0x01)
#define IAQCORE_STATUS_ERROR			(0x80)

typedef struct _iaq_data
{
    uint8_t 	status;			/*!< Status */
    uint16_t 	CO2prediction;		/*!< CO2prediction */
    uint32_t 	resistance;		/*!< resistance */
    uint16_t 	TVOCprediction;		/*!< TVOCprediction */
} iaq_data_t;


bool IAQ_ReadData(iaq_data_t *val);

bool I2C_MasterSendDataPolling(I2C_Type *base,
                                      const uint8_t *cmdBuff,
                                      uint32_t cmdSize,
                                      const uint8_t *txBuff,
                                      uint32_t txSize);
                                      
bool I2C_MasterReceiveDataPolling(I2C_Type *base,
                                         const uint8_t *cmdBuff,
                                         uint32_t cmdSize,
                                         uint8_t *rxBuff,
                                         uint32_t rxSize);
                                                                              
