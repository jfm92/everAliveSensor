#pragma once

#include "esp_err.h"
#include "gap.h"   /* sensor_payload_t */

#define BME680_I2C_PORT    I2C_NUM_0
#define BME680_SDA_PIN     7
#define BME680_SCL_PIN     6
#define BME680_I2C_ADDR    0x77
#define BME680_I2C_FREQ_HZ 400000   /* Fast Mode */

esp_err_t bme680_sensor_init(void);

esp_err_t bme680_sensor_read(sensor_payload_t *payload);

void bme680_sensor_deinit(void);