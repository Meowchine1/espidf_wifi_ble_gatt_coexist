set(srcs "main.c"
        "board.c"
        "cmd_wifi.c")

set(requires
	my_wifi
	my_http
	my_mqtt
    esp_event 
    esp-tls
    )
   
idf_component_register(SRCS "${srcs}"
                    PRIV_REQUIRES nvs_flash console esp_driver_gpio esp_driver_uart esp_wifi bt  mqtt
                    INCLUDE_DIRS  "." 
                    REQUIRES   ${requires}) # optional, list the public requirements (component names))