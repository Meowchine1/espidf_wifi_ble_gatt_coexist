// networkInterface.c

#include "networkInterface.h"
#include "my_mqtt.h"
#include "my_http.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


/*define the only one internet protocol here*/
#define HTTP
// #define MQTT
 
 TaskHandle_t internet_task_handle;
 
int run_http_task(void) {
    return http_create_task();
}

int run_mqtt_task() {
    return mqtt_init();
}

int run_internet_task(void)
{
#ifdef HTTP 
    return run_http_task();

#elif defined(MQTT)
    return run_mqtt_task();

#else
    // Ни HTTP, ни MQTT не определены — ошибка конфигурации
    return -1;
#endif  
}


 
void delTask() {
    if (internet_task_handle != NULL) {
        vTaskDelete(internet_task_handle);
        internet_task_handle = NULL;
    }
}
