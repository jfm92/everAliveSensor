#include "common.h"
#include "gap.h"
#include "bme680.h"
#include "esp_timer.h"

void ble_store_config_init(void);

#define SILENT_MODE //Comment to see debug traces

#ifdef SILENT_MODE
  #define LOG_T(fmt, ...)
#else
  static int64_t t0_us = 0;
  #define LOG_T(fmt, ...) ESP_LOGI(TAG, "[%4lldms] " fmt, \
      (esp_timer_get_time() - t0_us) / 1000, ##__VA_ARGS__)
#endif

//Nimble callbacks
static void on_stack_reset(int reason) {
    ESP_LOGW(TAG, "stack reset: %d", reason);
}

static void on_stack_sync(void) {
    LOG_T("stack sync  advertising");
    adv_init();
}

static void nimble_host_config_init(void) {
    ble_hs_cfg.reset_cb        = on_stack_reset;
    ble_hs_cfg.sync_cb         = on_stack_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_store_config_init();
}

static void nimble_host_task(void *param) {
    LOG_T("host task iniciada");
    nimble_port_run();
    vTaskDelete(NULL);
}


void app_main(void) {
#ifndef SILENT_MODE
    t0_us = esp_timer_get_time();
    LOG_T("boot");
#endif

    /* NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    LOG_T("nvs OK");

    /* Sensor stuff */
    sensor_payload_t payload = {0};

    if (bme680_sensor_init() == ESP_OK) {
        LOG_T("BME680 init OK");

        if (bme680_sensor_read(&payload) == ESP_OK) {
            LOG_T("BME680 read OK - T=%d H=%u P=%u",
                  payload.temperature, payload.humidity, payload.pressure);
        } else {
            ESP_LOGW(TAG, "BME680 read failed. Returning 0 value");
        }
        bme680_sensor_deinit();
        LOG_T("BME680 deinit OK");
    } else {
        ESP_LOGW(TAG, "BME680 init failed. Advertising with payload empty");
    }

    /* BLE stuff */
    ESP_ERROR_CHECK(nimble_port_init());
    LOG_T("nimble_port_init OK");

    gap_init();
    gap_update_payload(&payload);

    nimble_host_config_init();
    xTaskCreate(nimble_host_task, "nimble", 3 * 1024, NULL, 5, NULL);
    LOG_T("Task created.");
}