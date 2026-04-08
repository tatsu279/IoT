idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        nvs_flash
        esp_wifi
        esp_event
        esp_netif
        driver
        mqtt
        wifi_provisioning
        protocomm
        qrcode
        bt
        esp_common
        esp_timer
)
