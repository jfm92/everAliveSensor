idf_component_register(
    SRCS "BLERouter.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        bt
        log
        esp_wifi
        esp_netif
        esp_event
        lwip
)
