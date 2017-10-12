#include <stdint.h>
#include <stdbool.h>
#include "i2c_xfer.h"

#ifndef MAX3010_H
#define MAX3010_H

typedef struct _max30101_handle
{

    i2c_handle_t *device;  /*!< I2C handle. */
    uint8_t       address; /*!< max30101 I2C bus address. */
} max30101_handle_t;

float max30101_read_temperature(max30101_handle_t *handle);

#endif
