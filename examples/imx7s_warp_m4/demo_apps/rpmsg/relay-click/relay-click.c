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
#include "gpio_imx.h"
#include "gpio_pins.h"


////////////////////////////////////////////////////////////////////////////////
// Definitions
////////////////////////////////////////////////////////////////////////////////
#define APP_TASK_STACK_SIZE 256

/*
 * APP decided interrupt priority
 */
#define APP_MU_IRQ_PRIORITY 3

/* Globals */
static char app_buf[512]; /* Each RPMSG buffer can carry less than 512 payload */
static bool gpioValue = false;

static void GPIO_Ctrl_InitRL1Pin(void)
{
    /* GPIO module initialize, configure "RL1" as output and drive the output level high */
    gpio_init_config_t RL1InitConfig = 
    {
        .pin = BOARD_GPIO_RL1_CONFIG->pin,
        .direction = gpioDigitalOutput,
        .interruptMode = gpioNoIntmode
    };
    GPIO_Init(BOARD_GPIO_RL1_CONFIG->base, &RL1InitConfig);
}

static void GPIO_Ctrl_InitRL2Pin(void)
{
    /* GPIO module initialize, configure "RL2" as output and drive the output level high */
    gpio_init_config_t RL2InitConfig = 
    {
        .pin = BOARD_GPIO_RL2_CONFIG->pin,
        .direction = gpioDigitalOutput,
        .interruptMode = gpioNoIntmode
    };
    GPIO_Init(BOARD_GPIO_RL2_CONFIG->base, &RL2InitConfig);
}

/*!
 * @brief Toggle RL1
 */
static void GPIO_RL1_Toggle(bool value)
{

    GPIO_WritePinOutput(BOARD_GPIO_RL1_CONFIG->base,
    			BOARD_GPIO_RL1_CONFIG->pin, 
    			value);
}

/*!
 * @brief Toggle RL2
 */
static void GPIO_RL2_Toggle(bool value)
{

    GPIO_WritePinOutput(BOARD_GPIO_RL2_CONFIG->base,
    			BOARD_GPIO_RL2_CONFIG->pin, 
    			value);
}

/*!
 * @brief Read GPIO RL1
 */
static bool readRL1(void)
{
    return gpioValue = GPIO_ReadPinInput(BOARD_GPIO_RL1_CONFIG->base, 
    					 BOARD_GPIO_RL1_CONFIG->pin);
}

/*!
 * @brief Read GPIO RL2
 */
static bool readRL2(void)
{
    return gpioValue = GPIO_ReadPinInput(BOARD_GPIO_RL2_CONFIG->base, 
    					 BOARD_GPIO_RL2_CONFIG->pin);
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

    /* Print the initial banner */
    PRINTF("\r\nRelay Click Demo with Cortex M4\r\n");

    /* RPMSG Init as REMOTE */
    PRINTF("RPMSG Init as Remote\r\n");
    result = rpmsg_rtos_init(0, &rdev, RPMSG_MASTER, &app_chnl);
    assert(result == 0);

    PRINTF("Name service handshake is done, M4 has setup a rpmsg channel [%d ---> %d]\r\n", app_chnl->src, app_chnl->dst);

    for (;;)
    {
        /* Get RPMsg rx buffer with message */
        result = rpmsg_rtos_recv_nocopy(app_chnl->rp_ept, &rx_buf, &len, &src, 0xFFFFFFFF);
        assert(result == 0);

        /* Copy string from RPMsg rx buffer */
        assert(len < sizeof(app_buf));
        memcpy(app_buf, rx_buf, len);
        app_buf[len] = 0; /* End string by '\0' */

        if ((len == 2) && (app_buf[0] == 0xd) && (app_buf[1] == 0xa))
            PRINTF("Get New Line From Master Side\r\n");
        else
        {        	
        	// Get Arg
		sscanf(app_buf, "!%[^:\n]:%d", command, &wantedValue);
		
		if (0 == strcmp(command, "out_RL1")) {
			PRINTF("RL1\n");
			GPIO_RL1_Toggle(wantedValue);
		}
		if (0 == strcmp(command, "out_RL2")) {
			PRINTF("RL2\n");
			GPIO_RL2_Toggle(wantedValue);
		}
        }

        /* Get tx buffer from RPMsg */
        tx_buf = rpmsg_rtos_alloc_tx_buffer(app_chnl->rp_ept, &size);
        assert(tx_buf);
        /* Copy string to RPMsg tx buffer */
        memcpy(tx_buf, app_buf, len);
        /* Echo back received message with nocopy send */
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
    
    GPIO_Ctrl_InitRL1Pin();	// RL1
    GPIO_Ctrl_InitRL2Pin();	// RL2

    /*
     * Prepare for the MU Interrupt
     *  MU must be initialized before rpmsg init is called
     */
    MU_Init(BOARD_MU_BASE_ADDR);
    NVIC_SetPriority(BOARD_MU_IRQ_NUM, APP_MU_IRQ_PRIORITY);
    NVIC_EnableIRQ(BOARD_MU_IRQ_NUM);

    /* Create a demo task. */
    xTaskCreate(commandTask, "Command Task", APP_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY+1, NULL);

    /* Start FreeRTOS scheduler. */
    vTaskStartScheduler();

    /* Should never reach this point. */
    while (true);
}
/*******************************************************************************
 * EOF
 ******************************************************************************/
