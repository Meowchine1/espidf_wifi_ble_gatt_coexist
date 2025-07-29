idf_component_register(
		SRCS "bleprph.c"  "gatt_svr.c" "networkInterface.c"  
		INCLUDE_DIRS "."
		REQUIRES  nvs_flash bt my_mqtt my_http data_format  
	)
