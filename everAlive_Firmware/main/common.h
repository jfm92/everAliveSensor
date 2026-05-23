#pragma once

#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "store/config/ble_store_config.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/* ESP APIs */
#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

/* FreeRTOS APIs */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

/* NimBLE stack APIs */
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "store/config/ble_store_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Sensor payload. Must be the same on the receptor */
#pragma pack(push, 1)
typedef struct {
    int16_t  temperature;  /* °C   × 100 */
    uint16_t humidity;     /* %RH  × 100 */
    uint16_t pressure;     /* hPa  × 10  */
} sensor_payload_t;
#pragma pack(pop)
 
#define TAG                  "BLE_BEACON"
#define DEVICE_NAME          "EnvSensor"
#define ADVERTISE_DURATION_MS   20ULL        /* Advertising time in mS. Lower less time working -> Better battery */
#define SLEEP_DURATION_S        300ULL        /* Sleep time in seconds. */
