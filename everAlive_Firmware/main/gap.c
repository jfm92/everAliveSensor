/*
 * gap.c — advertising non-connectable with manufacture specific data.
 *
 * Manufcature field format. Specific Data (10 bytes total):
 *   [0..1] Company ID  = 0xFFFF  (little-endian)
 *   [2..3] temperature (int16_t, °C × 100)
 *   [4..5] humidity    (uint16_t, %RH × 100)
 *   [6..7] pressure    (uint16_t, hPa × 10)
 */
#include "gap.h"
#include "common.h"
#include <string.h>

//Company DevOnChip (DE0C)
#define COMPANY_ID_LSB  0xDE 
#define COMPANY_ID_MSB  0x0C

static uint8_t  own_addr_type;
static uint8_t  addr_val[6] = {0};

/* mfr_data buffer:  2 bytes Company ID + sizeof(sensor_payload_t) -> Save company ID and env data */
static uint8_t  mfr_data[2 + sizeof(sensor_payload_t)] = {
    COMPANY_ID_LSB,
    COMPANY_ID_MSB,
    0, 0, 0, 0, 0, 0   /* Initial payload 0 -> Here will live the env values.*/
};

static TimerHandle_t adv_stop_timer = NULL;

static inline void format_addr(char *addr_str, uint8_t addr[]) {
    sprintf(addr_str, "%02X:%02X:%02X:%02X:%02X:%02X",
            addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}
static void adv_stop_and_sleep(TimerHandle_t xTimer) {
    ESP_LOGI(TAG, "Compledted advertising - going to bed.");
 
    ble_gap_adv_stop();

    nimble_port_stop();
 
    esp_sleep_enable_timer_wakeup(SLEEP_DURATION_S * 1000000ULL);
    ESP_LOGI(TAG, "Sleep for %llu s", SLEEP_DURATION_S);
    esp_sleep_config_gpio_isolate();
    

    esp_deep_sleep_start();
}

//Updated advertise payload
void gap_update_payload(const sensor_payload_t *payload) {
    if (!payload) return;
    memcpy(&mfr_data[2], payload, sizeof(sensor_payload_t));
    ESP_LOGI(TAG, "Updated payload: T=%d  H=%u  P=%u",
             payload->temperature, payload->humidity, payload->pressure);
}


static void start_advertising(void) {
    int rc;
    const char *name;
    struct ble_hs_adv_fields adv_fields  = {0};
    struct ble_gap_adv_params adv_params = {0};
 
    /* Flags */
    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
 
    /* Nombre en el adv principal (visible en passive scan) */
    name = ble_svc_gap_device_name();
    adv_fields.name             = (uint8_t *)name;
    adv_fields.name_len         = strlen(name);
    adv_fields.name_is_complete = 1;
 
    /* Manufacturer Specific Data */
    adv_fields.mfg_data     = mfr_data; //Here we add the env data
    adv_fields.mfg_data_len = sizeof(mfr_data);
 
    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "error adv fields: %d", rc);
        return;
    }
 
    /* Non-connectable, fix interval 25mS*/
    adv_params.conn_mode = BLE_GAP_CONN_MODE_NON;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min  = 40;   /* 40 × 0.625 ms = 25 ms */
    adv_params.itvl_max  = 40;
 
    rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, NULL, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "error adv start: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "Advertising initializate. Duration: %llu ms", ADVERTISE_DURATION_MS);
 
    /* Set one shot timer. When the timer trigger, it goes to sleep. Equals to non blocking sleep.*/
    if (adv_stop_timer == NULL) {
        adv_stop_timer = xTimerCreate(
            "adv_stop",
            pdMS_TO_TICKS(ADVERTISE_DURATION_MS),
            pdFALSE,                  /* one-shot */
            NULL,
            adv_stop_and_sleep
        );
    }
    xTimerStart(adv_stop_timer, 0);
}

void adv_init(void) {
    int rc;
    char addr_str[18] = {0};

    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "BT addr not available.");
        return;
    }

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Direction inference type error: %d", rc);
        return;
    }

    rc = ble_hs_id_copy_addr(own_addr_type, addr_val, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Direction copy error: %d", rc);
        return;
    }

    format_addr(addr_str, addr_val);
    ESP_LOGI(TAG, "Device Addr: %s", addr_str);

    start_advertising();
}

int gap_init(void) {
    int rc;

    ble_svc_gap_init();

    rc = ble_svc_gap_device_name_set(DEVICE_NAME);
    if (rc != 0) {
        ESP_LOGE(TAG, "Name configuration error: %d", rc);
        return rc;
    }

    rc = ble_svc_gap_device_appearance_set(BLE_GAP_APPEARANCE_GENERIC_TAG);
    if (rc != 0) {
        ESP_LOGE(TAG, "Apperance configuration error: %d", rc);
        return rc;
    }

    return 0;
}