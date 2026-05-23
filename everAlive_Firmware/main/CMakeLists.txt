idf_component_register(
    SRCS
        "main.c"
        "gap.c"
        "sensor/bme680.c"
    INCLUDE_DIRS "." "sensor/."
    REQUIRES
        nvs_flash
        bt
        driver
        esp_timer
        bme680x_component
)