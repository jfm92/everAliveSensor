#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "store/config/ble_store_config.h"

#define TAG             "BLE_SCANNER"
#define TARGET_NAME     "EnvSensor"
#define COMPANY_ID      0xFFFF

#include "wifi_credentials.h"

#define TIMEZONE        "CET-1CEST,M3.5.0,M10.5.0/3"

#define SCAN_ITVL       0x0010
#define SCAN_WINDOW     0x0010

#define WIFI_CONNECTED_BIT  BIT0
#define WIFI_FAIL_BIT       BIT1

static EventGroupHandle_t wifi_event_group;

#pragma pack(push, 1)
typedef struct {
    int16_t  temperature;   /* °C   × 100 */
    uint16_t humidity;      /* %RH  × 100 */
    uint16_t pressure;      /* hPa  × 10  */
} sensor_payload_t;
#pragma pack(pop)

static void print_sensor_data(const sensor_payload_t *p, int rssi,
                               const uint8_t *addr, uint16_t manufacturer_id) {
    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &t);

    ESP_LOGI(TAG, "┌─────────────────────────────────────┐");
    ESP_LOGI(TAG, "│  Time        : %-20s │", ts);
    ESP_LOGI(TAG, "│  MAC         : %02X:%02X:%02X:%02X:%02X:%02X       │",
             addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
    ESP_LOGI(TAG, "│  Mfr ID      : 0x%04X               │", manufacturer_id);
    ESP_LOGI(TAG, "│  Temperature : %6.2f C               │", p->temperature / 100.0f);
    ESP_LOGI(TAG, "│  Humidity    : %6.2f %%              │", p->humidity    / 100.0f);
    ESP_LOGI(TAG, "│  Pressure    : %6.1f hPa            │", p->pressure    / 10.0f);
    ESP_LOGI(TAG, "│  RSSI        : %5d dBm             │", rssi);
    ESP_LOGI(TAG, "└─────────────────────────────────────┘");
}

static const sensor_payload_t *parse_adv_payload(const uint8_t *data, uint8_t len,
                                                  uint16_t *out_company_id) {
    uint8_t i = 0;
    while (i < len) {
        uint8_t field_len  = data[i];
        if (field_len == 0 || i + field_len >= len) break;

        uint8_t field_type = data[i + 1];

        if (field_type == 0xFF) {
            if (field_len < 2 + sizeof(sensor_payload_t)) {
                i += field_len + 1;
                continue;
            }

            uint16_t company_id = data[i + 2] | ((uint16_t)data[i + 3] << 8);
            if (company_id != COMPANY_ID) {
                i += field_len + 1;
                continue;
            }

            if (out_company_id) *out_company_id = company_id;
            return (const sensor_payload_t *)&data[i + 4];
        }
        i += field_len + 1;
    }
    return NULL;
}

static int gap_event_handler(struct ble_gap_event *event, void *arg) {
    if (event->type != BLE_GAP_EVENT_EXT_DISC &&
        event->type != BLE_GAP_EVENT_DISC) {
        return 0;
    }

    if (event->type == BLE_GAP_EVENT_DISC) {
        const uint8_t *data     = event->disc.data;
        uint8_t        data_len = event->disc.length_data;

        struct ble_hs_adv_fields fields;
        if (ble_hs_adv_parse_fields(&fields, data, data_len) != 0) return 0;
        if (!fields.name || fields.name_len != strlen(TARGET_NAME)) return 0;
        if (memcmp(fields.name, TARGET_NAME, fields.name_len) != 0) return 0;

        uint16_t company_id = 0;
        const sensor_payload_t *payload = parse_adv_payload(data, data_len, &company_id);
        if (!payload) return 0;

        print_sensor_data(payload, event->disc.rssi, event->disc.addr.val, company_id);
    }

    return 0;
}

static void start_scan(void) {
    struct ble_gap_disc_params disc_params = {
        .itvl              = SCAN_ITVL,
        .window            = SCAN_WINDOW,
        .filter_policy     = BLE_HCI_SCAN_FILT_NO_WL,
        .limited           = 0,
        .passive           = 1,
        .filter_duplicates = 0,
    };

    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER,
                          &disc_params, gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "scan start error: %d", rc);
    } else {
        ESP_LOGI(TAG, "scanning for \"%s\"...", TARGET_NAME);
    }
}

static void on_stack_reset(int reason) {
    ESP_LOGW(TAG, "stack reset: %d", reason);
}

static void on_stack_sync(void) {
    start_scan();
}

void ble_store_config_init(void);

static void nimble_host_task(void *param) {
    nimble_port_run();
    vTaskDelete(NULL);
}

static void wifi_event_handler(void *arg, esp_event_base_t base,
                                int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static esp_err_t wifi_connect(void) {
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    wifi_config_t wifi_cfg = {
        .sta = { .ssid = WIFI_SSID, .password = WIFI_PASS },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE,
                                           pdMS_TO_TICKS(10000));
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi connected");
        return ESP_OK;
    }

    ESP_LOGE(TAG, "WiFi connection failed");
    return ESP_FAIL;
}

static void sntp_sync(void) {
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    int retries = 0;
    while (esp_sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED && retries++ < 20) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    setenv("TZ", TIMEZONE, 1);
    tzset();

    time_t now = time(NULL);
    struct tm t;
    localtime_r(&now, &t);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &t);
    ESP_LOGI(TAG, "time synced: %s", ts);

    esp_sntp_stop();
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    if (wifi_connect() == ESP_OK) {
        sntp_sync();
    }
    esp_wifi_stop();
    esp_wifi_deinit();

    ESP_ERROR_CHECK(nimble_port_init());

    ble_hs_cfg.reset_cb        = on_stack_reset;
    ble_hs_cfg.sync_cb         = on_stack_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_store_config_init();

    xTaskCreate(nimble_host_task, "nimble", 4 * 1024, NULL, 5, NULL);
}
