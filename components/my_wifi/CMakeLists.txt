idf_component_register(
    SRCS "my_wifi.c"
    INCLUDE_DIRS "."
    REQUIRES "esp_event" "esp_netif" "nvs_flash" "lwip" "esp_wifi"
    PRIV_REQUIRES my_http
)
