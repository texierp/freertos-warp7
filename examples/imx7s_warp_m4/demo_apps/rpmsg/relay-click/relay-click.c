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

#include "rpmsg/rpmsg_rtos.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "string.h"
#include "board.h"
#include "mu_imx.h"
#include "debug_console_imx.h"
#include "rdc_semaphore.h"
#include "gpio_imx.h"
#include "gpio_pins.h"
#include "queue.h"
#include "eeprom.h"

////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////
#define APP_TASK_STACK_SIZE 256

/*
 * APP decided interrupt priority
 */
#define APP_MU_IRQ_PRIORITY 3

/* Globals */
static char buffer[512]; /* Each RPMSG buffer can carry less than 512 payload */
static bool gpioValue = false;

static void GPIO_Ctrl_InitRL1Pin(void)
{
#ifdef BOARD_GPIO_RL1_CONFIG
    	/* GPIO module initialize, configure "RL1" as output and drive the output level high */
    	gpio_init_config_t RL1InitConfig = 
   	 {
		.pin = BOARD_GPIO_RL1_CONFIG->pin,
        	.direction = gpioDigitalOutput,
        	.interruptMode = gpioNoIntmode
    	};
    	RDC_SEMAPHORE_Lock(BOARD_GPIO_RL1_RDC_PDAP);
    	GPIO_Init(BOARD_GPIO_RL1_CONFIG->base, &RL1InitConfig);
    	RDC_SEMAPHORE_Unlock(BOARD_GPIO_RL1_RDC_PDAP);
#endif
}

static void GPIO_Ctrl_InitRL2Pin(void)
{
#ifdef BOARD_GPIO_RL2_CONFIG
    	/* GPIO module initialize, configure "RL2" as output and drive the output level high */
    	gpio_init_config_t RL2InitConfig = 
    	{
		.pin = BOARD_GPIO_RL2_CONFIG->pin,
		.direction = gpioDigitalOutput,
		.interruptMode = gpioNoIntmode
    	};
    	RDC_SEMAPHORE_Lock(BOARD_GPIO_RL2_RDC_PDAP);
    	GPIO_Init(BOARD_GPIO_RL2_CONFIG->base, &RL2InitConfig);
    	RDC_SEMAPHORE_Unlock(BOARD_GPIO_RL2_RDC_PDAP);
#endif
}

static void GPIO_Ctrl_InitINTPin(void)
{
#ifdef BOARD_GPIO_INT_CONFIG
    	/* GPIO module initialize, configure "INT" as input */
    	gpio_init_config_t INTInitConfig = 
    	{
        	.pin = BOARD_GPIO_INT_CONFIG->pin,
        	.direction = gpioDigitalInput,
        	.interruptMode = gpioNoIntmode
    	};
    	RDC_SEMAPHORE_Lock(BOARD_GPIO_INT_RDC_PDAP);
    	GPIO_Init(BOARD_GPIO_INT_CONFIG->base, &INTInitConfig);
    	RDC_SEMAPHORE_Unlock(BOARD_GPIO_INT_RDC_PDAP);
#endif
}

/*!
 * @brief Toggle RL1
 */
static void GPIO_RL1_Toggle(bool value)
{
#ifdef BOARD_GPIO_RL1_CONFIG
    	RDC_SEMAPHORE_Lock(BOARD_GPIO_RL1_RDC_PDAP);
    	GPIO_WritePinOutput(BOARD_GPIO_RL1_CONFIG->base,
    				BOARD_GPIO_RL1_CONFIG->pin, 
    				value);
    	RDC_SEMAPHORE_Unlock(BOARD_GPIO_RL1_RDC_PDAP);
#endif
}

/*!
 * @brief Toggle RL2
 */
static void GPIO_RL2_Toggle(bool value)
{
#ifdef BOARD_GPIO_RL2_CONFIG
    	RDC_SEMAPHORE_Lock(BOARD_GPIO_RL2_RDC_PDAP);	
    	GPIO_WritePinOutput(BOARD_GPIO_RL2_CONFIG->base,
    				BOARD_GPIO_RL2_CONFIG->pin, 
    				value);
    	RDC_SEMAPHORE_Unlock(BOARD_GPIO_RL2_RDC_PDAP);
#endif
}

static bool readINT(void)
{
#ifdef BOARD_GPIO_INT_CONFIG
	RDC_SEMAPHORE_Lock(BOARD_GPIO_INT_RDC_PDAP);
	gpioValue = GPIO_ReadPinInput(BOARD_GPIO_INT_CONFIG->base, 
    					 BOARD_GPIO_INT_CONFIG->pin);
	RDC_SEMAPHORE_Unlock(BOARD_GPIO_INT_RDC_PDAP);
#endif
    	return gpioValue; 
}

static bool writeToEEPROM(uint8_t wantedSlaveID)
{
	uint8_t txBuffer[4];
    	uint8_t cmdBuffer[4];
    	
    	// CMD Buffer
	cmdBuffer[0] = EEPROM_ADDRESS << 1;
    	cmdBuffer[1] = 0x00;
    	cmdBuffer[2] = 0x00;
    
    	// TX Buffer
    	txBuffer[0] = 0 ;
	txBuffer[1] = wantedSlaveID ;
	txBuffer[2] = 0 ;
	txBuffer[3] = 0 ;
	
	if (I2C_MasterSendDataPolling(BOARD_I2C_BASEADDR, cmdBuffer, 3, txBuffer, 4)) {
		PRINTF("Send to EEPROM OK [%d]\n", ((txBuffer[1]) & 0x00ff));
		return true;
	}
	else 
		return false;
}

static uint32_t readFromEEPROM()
{
	uint8_t rxBuffer[4];
    	uint8_t cmdBuffer[4];
    	uint32_t slaveID;
    	
	cmdBuffer[0] = EEPROM_ADDRESS << 1;
	cmdBuffer[1] = 0x00;
	cmdBuffer[2] = 0x50;
	cmdBuffer[3] = (EEPROM_ADDRESS << 1) + 1;

	I2C_MasterReceiveDataPolling(BOARD_I2C_BASEADDR, cmdBuffer, 4, rxBuffer, 4);
	
	slaveID = ((rxBuffer[2]) & 0x00ff);
    	
    	PRINTF("Stored in EEPROM: [%d]\n\r", slaveID);
    	
    	return slaveID;
}

QueueHandle_t xQueue = 0;
static void producerTask(void *pvParameters)
{
	uint8_t queueValue = 0;
	for (;;)
	{	
		queueValue = readINT();
		if (!xQueueSend(xQueue, &queueValue, 500)) {
        		PRINTF("Failed to send item to queue ...\n");
        	}
        	vTaskDelay(300);
     	}
}

static void heartBeatTask(void *pvParameters)
{
	uint8_t queueValue = 0;
	for (;;)
     	{
     		if (!xQueueReceive(xQueue, &queueValue, 1000)) {
        		PRINTF("Failed to receive item ...\n");
        	}
		PRINTF("Flame Click Interrrupt: %d\n", queueValue);		
     	}
}

/*!
 * @brief A basic RPMSG task
 */
static void commandTask(void *pvParameters)
{
    	int result;
    	struct remote_device *rdev = NULL;
    	struct rpmsg_channel *app_chnl = NULL;
    	void *rx_buf;
    	int len;
    	unsigned long src;
    	void *tx_buf;
    	unsigned long size;
    	char command[20];
    	int wantedValue;
    	int isValid;   	

    	/* RPMSG Init as REMOTE */
    	result = rpmsg_rtos_init(0, &rdev, RPMSG_MASTER, &app_chnl);
    	assert(result == 0);

    	PRINTF("Name service handshake is done, M4 has setup a rpmsg channel [%d ---> %d]\r\n", 
    											app_chnl->src, 
    											app_chnl->dst);

	for (;;)
    	{
        	/* Get RPMsg rx buffer with message */
        	result = rpmsg_rtos_recv_nocopy(app_chnl->rp_ept, &rx_buf, &len, &src, 0xFFFFFFFF);
        	assert(result == 0);

        	/* Copy string from RPMsg rx buffer */
        	assert(len < sizeof(buffer));
        	memcpy(buffer, rx_buf, len);
        	/* End string by '\0' */
        	buffer[len] = 0; 

        	if ((len == 2) && (buffer[0] == 0xd) && (buffer[1] == 0xa))
            		PRINTF("Received but not handled\r\n");
        	else
        	{
        		PRINTF("Get Message From Master Side : \"%s\" [len : %d]\r\n", buffer, len);
        		
        		switch (buffer[0]) {
        		
        		case '!':        	
				/* Force isValid to false */
				isValid=false;
			
				/* Get Arg from remote */
				sscanf(buffer, "!%[^:\n]:%d", command, &wantedValue);

				/* Check if command is valid */
				if (0 == strcmp(command, "out_RL1")) {
					GPIO_RL1_Toggle(wantedValue);
					isValid=true;
				} else if (0 == strcmp(command, "out_RL2")) {
					GPIO_RL2_Toggle(wantedValue);
					isValid=true;
				} else if (0 == strcmp(command, "out_eeprom")) {
					writeToEEPROM(wantedValue);
					isValid=true;
				} 
				else isValid=false;
		
				/* Update Buffer */
				if (isValid) {			
					len = snprintf(buffer, sizeof(buffer), "%s:ok\n", command);
				} else {
					len = snprintf(buffer, sizeof(buffer), "%s:error\n", command);
				}
				break;
				
			case '?':
				sscanf(buffer, "?%s", command);
			
				if (0 == strcmp(command, "in_eeprom")) 
					len = snprintf(buffer, sizeof(buffer), "%s:%d\n", command, (int)readFromEEPROM());
				else if (0 == strcmp(command, "in_INT_FLAME")) 
					len = snprintf(buffer, sizeof(buffer), "%s:ok\n", command);			
				break;
		    	default:
		        	len = snprintf(buffer, sizeof(buffer), "%s:wrong command\n", command);
		        	break;
		        }
        	}
        
		/* Allocates the tx buffer for message payload */
		tx_buf = rpmsg_rtos_alloc_tx_buffer(app_chnl->rp_ept, &size);
		assert(tx_buf);		
		/* Copy string to RPMsg tx buffer */
		memcpy(tx_buf, buffer, len);
		/* Send buffer to Cortex A7 */
		result = rpmsg_rtos_send_nocopy(app_chnl->rp_ept, tx_buf, len, src);
		assert(result == 0);	
		/* Release held RPMsg rx buffer */
		result = rpmsg_rtos_recv_nocopy_free(app_chnl->rp_ept, rx_buf);
		assert(result == 0);
    	}
}

/*
 * MU Interrrupt ISR
 */
void BOARD_MU_HANDLER(void)
{
	/*
	* calls into rpmsg_handler provided by middleware
	*/
	rpmsg_handler();
}

int main(void)
{
    	hardware_init();
    	
    	// I2c Configuration
    	i2c_init_config_t i2cInitConfig = {
		.baudRate     = 400000u,
		.slaveAddress = 0x00
    	};
    	
    	i2cInitConfig.clockRate = get_i2c_clock_freq(BOARD_I2C_BASEADDR);
    	I2C_Init(BOARD_I2C_BASEADDR, &i2cInitConfig);
    	I2C_Enable(BOARD_I2C_BASEADDR);
    	
   	GPIO_Ctrl_InitRL1Pin();	// Init RL1
    	GPIO_Ctrl_InitRL2Pin();	// Init RL2
    	GPIO_Ctrl_InitINTPin();	// Init INT
    	
    	// Queue Creation
    	xQueue = xQueueCreate(1, sizeof(uint8_t));
    	
    	if(xQueue == NULL) goto err;
    	/*
     	* Prepare for the MU Interrupt
     	*  MU must be initialized before rpmsg init is called
     	*/
    	MU_Init(BOARD_MU_BASE_ADDR);
    	NVIC_SetPriority(BOARD_MU_IRQ_NUM, APP_MU_IRQ_PRIORITY);
    	NVIC_EnableIRQ(BOARD_MU_IRQ_NUM);

    	/* Create a Command task. */
    	if (!(pdPASS == xTaskCreate(commandTask, "Command Task", APP_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY+1, NULL)))
    		goto err;
    		 	
    	/* Create a Producer task. */
	if (!(pdPASS == xTaskCreate(producerTask, "Producer Task", APP_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY+2, NULL)))
		goto err;
	
	/* Create a Heartbeat task. */	 
    	if (!(pdPASS == xTaskCreate(heartBeatTask, "Heartbeat Task", APP_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY+3, NULL)))
    		goto err;
	
	PRINTF("\r\n== Relay Click Demo ==\r\n");
	
	readFromEEPROM();
	
    	/* Start FreeRTOS scheduler. */
    	vTaskStartScheduler();
    	
err:
	PRINTF("Error ...\n");
    	/* Should never reach this point. */
    	while (true);
}

