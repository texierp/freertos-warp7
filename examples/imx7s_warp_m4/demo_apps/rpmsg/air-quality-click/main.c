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
#include "air-quality-click.h"

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
static iaq_data_t iaqData;

static void readFromSensor()
{
	IAQ_ReadData(&iaqData);
}

static void GPIO_Ctrl_InitLEDPin(void)
{
#ifdef BOARD_GPIO_LED_CONFIG
    	/* GPIO module initialize, configure "LED" as output and drive the output level high */
    	gpio_init_config_t LEDInitConfig = 
   	 {
		.pin = BOARD_GPIO_LED_CONFIG->pin,	// Pin number
        	.direction = gpioDigitalOutput,		// Pin direction
        	.interruptMode = gpioNoIntmode		// Pin mode
    	};
    	RDC_SEMAPHORE_Lock(BOARD_GPIO_LED_RDC_PDAP);
    	GPIO_Init(BOARD_GPIO_LED_CONFIG->base, &LEDInitConfig);
    	RDC_SEMAPHORE_Unlock(BOARD_GPIO_LED_RDC_PDAP);
#endif
}


/*!
 * @brief Toggle RL1
 */
static void GPIO_LED_Toggle(bool value)
{
#ifdef BOARD_GPIO_LED_CONFIG
    	RDC_SEMAPHORE_Lock(BOARD_GPIO_LED_RDC_PDAP);
    	GPIO_WritePinOutput(BOARD_GPIO_LED_CONFIG->base,
    				BOARD_GPIO_RL1_CONFIG->pin, 
    				value);
    	RDC_SEMAPHORE_Unlock(BOARD_GPIO_LED_RDC_PDAP);
#endif
}

static void heartBeatTask(void *pvParameters)
{
	for (;;) {
     		PRINTF("M4 is Alive\n");
     		vTaskDelay(300);
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
				if (0 == strcmp(command, "out_LED")) {
					GPIO_LED_Toggle(wantedValue);	
					isValid=true;
				} 
				else 
					isValid=false;
		
				/* Update Buffer */
				if (isValid) {			
					len = snprintf(buffer, sizeof(buffer), "%s:ok\n", command);
				} else {
					len = snprintf(buffer, sizeof(buffer), "%s:error\n", command);
				}
				break;
				
			case '?':
				sscanf(buffer, "?%s", command);
				readFromSensor();
				if (0 == strcmp(command, "co2")) 
					len = snprintf(buffer, sizeof(buffer), "%s:%d\n", command, (int)iaqData.CO2prediction);
				else if (0 == strcmp(command, "tvoc")) 
					len = snprintf(buffer, sizeof(buffer), "%s:%d\n", command, (int)iaqData.TVOCprediction);
				else if (0 == strcmp(command, "status")) 
					len = snprintf(buffer, sizeof(buffer), "%s:%d\n", command, (int)iaqData.status);			
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
    	
   	GPIO_Ctrl_InitLEDPin();	// Init GPIO Led
   
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
	
	/* Create a Heartbeat task. */	 
    	if (!(pdPASS == xTaskCreate(heartBeatTask, "Heartbeat Task", APP_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY+2, NULL)))
    		goto err;
	
	PRINTF("\r\n== GLMF Demo ==\r\n");
	
    	/* Start FreeRTOS scheduler. */
    	vTaskStartScheduler();
    	
err:
	PRINTF("Error ...\n");
    	/* Should never reach this point. */
    	while (true);
}
