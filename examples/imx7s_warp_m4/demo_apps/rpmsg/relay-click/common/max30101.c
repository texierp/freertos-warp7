#include <stdint.h>
#include <stdbool.h>
#include "board.h"
#include "max30101.h"

static const uint8_t MAX30105_PARTID = 0xFF;
static const uint8_t MAX30105_DIETEMPINT = 0x1F;
static const uint8_t MAX30105_DIETEMPFRAC = 0x20;
static const uint8_t MAX30105_DIETEMPCONFIG = 0x21;

float max30101_read_temperature(max30101_handle_t *handle)
{
	float temperature = 0.0f;
	uint8_t temp;
	//uint8_t frac;
	uint8_t rxBuffer[3];
    	uint8_t cmdBuffer[3];
	cmdBuffer[0] = handle->address;
	cmdBuffer[1] = MAX30105_DIETEMPCONFIG;
	cmdBuffer[2] = 0x01;
	I2C_XFER_SendDataBlocking(handle->device, cmdBuffer, 3, rxBuffer, 1);
	
	
	cmdBuffer[0] = handle->address;
	cmdBuffer[1] = MAX30105_DIETEMPINT;
	I2C_XFER_ReceiveDataBlocking(handle->device, cmdBuffer, 2, rxBuffer, 1);
	temp = rxBuffer[0];
	temperature = temp;
	return temperature;
	
}
