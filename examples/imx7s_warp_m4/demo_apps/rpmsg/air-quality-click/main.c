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

#define APP_TASK_STACK_SIZE 256

/*
 * APP decided interrupt priority
 */
#define APP_MU_IRQ_PRIORITY 3

// Each RPMSG buffer can carry less than 512 payload
static char buffer[512]; 

// Déclaration du Mutex
SemaphoreHandle_t thMutex = NULL;

// Structure du capteur iAQ
iaq_data_t iaqData;
	
/*!
 * @brief Init GPIO LED
 */
static void GPIO_Ctrl_InitLEDPin(void)
{
#ifdef BOARD_GPIO_LED_CONFIG
    	// GPIO module initialize, configure "LED" as output and drive the output level high
    	gpio_init_config_t LEDInitConfig = 
   	 {
		.pin = BOARD_GPIO_LED_CONFIG->pin,		// Pin number
        	.direction = gpioDigitalOutput,			// Pin direction
        	.interruptMode = gpioNoIntmode			// Pin mode
    	};
    	RDC_SEMAPHORE_Lock(BOARD_GPIO_LED_RDC_PDAP);		
    	GPIO_Init(BOARD_GPIO_LED_CONFIG->base, &LEDInitConfig);	// We pass the initialized structure
    	RDC_SEMAPHORE_Unlock(BOARD_GPIO_LED_RDC_PDAP);
#endif
}


/*!
 * @brief Toggle LED
 */
static void setLED(bool value)
{
#ifdef BOARD_GPIO_LED_CONFIG
    	RDC_SEMAPHORE_Lock(BOARD_GPIO_LED_RDC_PDAP);
    	GPIO_WritePinOutput(BOARD_GPIO_LED_CONFIG->base,	// GPIO bank
    				BOARD_GPIO_LED_CONFIG->pin, 	// GPIO pin
    				value);				// Value
    	RDC_SEMAPHORE_Unlock(BOARD_GPIO_LED_RDC_PDAP);
#endif
}

/*!
 * @brief Tâche récupération des données capteur
 */
static void iaqDataTask(void *pvParameters)
{	
	PRINTF("Task %s started\n", "iaqDataTask");
	while (1) {
			
		// On récupère la donnée
		if ( !IAQ_ReadData( &iaqData ) )
			PRINTF("Reading problem ... %s\n");			

		// On temporise 1500ms
		vTaskDelay(1500);
     	}
}

/*!
 * @brief Tâche RPMSG
 */
static void commandTask(void *pvParameters)
{
	// Pour vérifier la valeur de retour
    	int result;
    	// Remote Device
    	struct remote_device *rdev = NULL;
    	// Channel RPMSG
    	struct rpmsg_channel *app_chnl = NULL;
    	// Buffer de réception
    	void *rx_buf;
    	// Buffer de transmission
    	void *tx_buf;
    	// Gestion de la longueur
    	int len;
    	// Pour la gestion du endpoint (cortex A7)
    	unsigned long src; 
    	// Pour la gestion du buffer de transmission   	
    	unsigned long size;
    	// Pour traiter les commandes provenant du cortex A7
    	char command[20];
    	// Gestion de la valeur de la GPIO
    	int wantedValue = 0;
    	// Pour la gestion des commandes de 'set'
    	int isValid = false;   	
	
    	// On initialise RPMSG en tant que 'Remote' => (RPMSG_MASTER représente le cortex A7)
    	result = rpmsg_rtos_init(0, &rdev, RPMSG_MASTER, &app_chnl); 
    	assert(result == 0);

	// On affiche sur la sortie UART la confirmation de la création du channel
    	PRINTF("Name service handshake is done, M4 has setup a rpmsg channel [%d ---> %d]\r\n", 
    											app_chnl->src, 
    											app_chnl->dst);

	while (1)
    	{   	
    		// Take Mutex
    		if( xSemaphoreTake( thMutex, 20 ) == pdTRUE )
        	{  		
        		// Récupération du message venant du cortex A7
			result = rpmsg_rtos_recv_nocopy(app_chnl->rp_ept, &rx_buf, &len, &src, 0xFFFFFFFF);
			assert(result == 0);

			// On copie le buffer de reception dans le buffer de traitement
			assert(len < sizeof(buffer));
			memcpy(buffer, rx_buf, len);
			// End string by '\0'
			buffer[len] = 0; 
        	   	
			if ((len == 2) && (buffer[0] == 0xd) && (buffer[1] == 0xa))
		    		PRINTF("Received but not handled\r\n");
			else
			{       		
				switch (buffer[0]) 
				{    						
					case '!': 							
						// On analyse la commande de set
						sscanf(buffer, "!%[^:\n]:%d", command, &wantedValue);

						// On regarde si la commande est valide
						if (0 == strcmp(command, "out_LED")) 
						{
							setLED(wantedValue);	
							isValid=true;
						} 
						else  isValid=false;
		
						// On formate le message + la longueur
						if (isValid) {			
							len = snprintf(buffer, sizeof(buffer), "%s:ok\n", command);
						} else {
							len = snprintf(buffer, sizeof(buffer), "%s:error\n", command);
						}
						break;				
					case '?':
						// On analyse la commande de get
						sscanf(buffer, "?%s", command);
						if (0 == strcmp(command, "getAirQuality")) 
						{	
							// CO2 Prediction (ppm)	
							buffer[0] = iaqData.CO2prediction >> 8;
							buffer[1] = iaqData.CO2prediction & 0x00FF;
							// TVOC prediction (ppb)
							buffer[2] = iaqData.TVOCprediction >> 8;
							buffer[3] = iaqData.TVOCprediction & 0x00FF;
							// Status (RUNNING, BUSY, ...)
							buffer[4] = iaqData.status;
							// \n
							buffer[5] = '\n';
							// Lenght
							len = 6;	
							
							// Autre moyen d'envoyer la donnée (string complète)
							//len = snprintf(buffer, sizeof(buffer), "%s:%d.%d.%d\n", command, iaqData.CO2prediction, iaqData.TVOCprediction, iaqData.status);
						}		
						else
						{
							// Erreur dans la commande -> on formate le message pour le cortex A7					
							len = snprintf(buffer, sizeof(buffer), "%s:error\n", command);	// Error
						}							
						break;
				    	default:
				    		// Mauvaise commande -> on formate le message pour le cortex A7
						len = snprintf(buffer, sizeof(buffer), "%s:wrong command\n", command);
						break;
				}
			}
			// On alloue le buffer de transmission pour envoyer notre message
			tx_buf = rpmsg_rtos_alloc_tx_buffer(app_chnl->rp_ept, &size);
			assert(tx_buf);		
			// On copie le buffer de traitement dans celui-ci
			memcpy(tx_buf, buffer, len);
			// On envoie vers le cortex A7 !
			result = rpmsg_rtos_send_nocopy(app_chnl->rp_ept, tx_buf, len, src);
			assert(result == 0);	
				
			// Release held RPMsg rx buffer 
			result = rpmsg_rtos_recv_nocopy_free(app_chnl->rp_ept, rx_buf);
			assert(result == 0);
			
			// Release Mutex
			xSemaphoreGive( thMutex );			
		}
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
	// Init RDC, Clock, memory
    	hardware_init();
    	
    	// Configuration 
    	i2c_init_config_t i2cInitConfig = {
		.baudRate     = 100000u,
		.slaveAddress = 0x00
    	};
    	// Init clock
    	i2cInitConfig.clockRate = get_i2c_clock_freq(BOARD_I2C_BASEADDR);
    	// Init i2c
    	I2C_Init(BOARD_I2C_BASEADDR, &i2cInitConfig);
    	// Enable i2c
    	I2C_Enable(BOARD_I2C_BASEADDR);
    	
    	// On initialise notre module GPIO (pour la gestion de la LES) 
   	GPIO_Ctrl_InitLEDPin();	
   	
   	// Création du mutex
   	thMutex = xSemaphoreCreateMutex();
   
    	/*
     	* Prepare for the MU Interrupt
     	*  MU must be initialized before rpmsg init is called
     	*/
    	MU_Init(BOARD_MU_BASE_ADDR);
    	NVIC_SetPriority(BOARD_MU_IRQ_NUM, APP_MU_IRQ_PRIORITY);
    	NVIC_EnableIRQ(BOARD_MU_IRQ_NUM);
    	
    	// Création de la tâche iAQ pour la récupération des données du capteur i2c	 
    	if (!(pdPASS == xTaskCreate(iaqDataTask, "iaqData Task", APP_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY+1, NULL)))
    		goto err;

    	// Creation de la tâche pour la gestion de la communication (rpmsg) 
    	if (!(pdPASS == xTaskCreate(commandTask, "Command Task", APP_TASK_STACK_SIZE, NULL, tskIDLE_PRIORITY+2, NULL)))
    		goto err;
	
	// Affichage sur la liaison série
	PRINTF("\r\n== GLMF Demo Started ==\r\n");
	
    	// Start FreeRTOS scheduler
    	vTaskStartScheduler();
    	
err:
	PRINTF("Error ...\n");
    	// Should never reach this point
    	while (true);
}

