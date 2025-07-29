// my_mqtt.c

#include "mqtt_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <string.h>
#include <inttypes.h>
#include "my_mqtt.h"
#include <stdbool.h>
#include "pump_data.h"

 

#define TAG "MQTT"

#define MQTT_URI "mqtt://mqtt.thingsboard.cloud"
#define ACCESS_TOKEN "MyToken111"


#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1


bool mqtt_running = false;
bool send_mqtt = false;

TaskHandle_t mqtt_task_handle = NULL;
TaskHandle_t monitor_task_handle = NULL;

/*int16_t dPressure = 36546, dSpeed = 3000, V = 56, L = 7, To = 40, Tm = 48;
uint8_t id = 17;*/

esp_mqtt_client_handle_t mqtt_client = NULL;
extern EventGroupHandle_t s_wifi_event_group;
 

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT disconnected");
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error");
            break;
        default:
            break;
    }
}

static void mqtt_send_telemetry(void) {
    if (mqtt_client == NULL) return;
    char payload[128];
    // void get_pump_data(char *payload, size_t payload_size);
    get_http_pump_data(payload, sizeof(payload));
    int msg_id = esp_mqtt_client_publish(mqtt_client, "v1/devices/me/telemetry", payload, 0, 1, 0);
    if (msg_id == -1) {
        ESP_LOGE(TAG, "Failed to send telemetry");
    }
}

void mqtt_task(void *pvParameters) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_URI,
        .credentials.username = ACCESS_TOKEN,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!mqtt_client) {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        vTaskDelete(NULL);
        return;
    }

    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
    mqtt_running = true;

    send_mqtt = true;

    while (send_mqtt) {
        mqtt_send_telemetry();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    esp_mqtt_client_stop(mqtt_client);
    esp_mqtt_client_destroy(mqtt_client);
    mqtt_client = NULL;
    mqtt_task_handle = NULL;
    mqtt_running = false;

    ESP_LOGI(TAG, "MQTT task exiting");
    vTaskDelete(NULL);
}

void connection_monitor_task(void* arg) {
    while (1) {
        EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);

        if ((bits & WIFI_CONNECTED_BIT) && !mqtt_running && mqtt_task_handle == NULL) {
            ESP_LOGI("MONITOR", "Wi-Fi OK → запуск MQTT задачи");
            xTaskCreate(&mqtt_task, "mqtt_task", 8192, NULL, 5, &mqtt_task_handle);
        } 
        else if (!(bits & WIFI_CONNECTED_BIT) && mqtt_running) {
            ESP_LOGW("MONITOR", "Wi-Fi потерян → остановка MQTT задачи");
            send_mqtt = false;
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

int mqtt_init() {
    BaseType_t xReturned;

    // Запуск задачи connection_monitor
    xReturned = xTaskCreate(&connection_monitor_task, "connection_monitor", 4096, NULL, 5, &monitor_task_handle);
    if (xReturned != pdPASS) {
        ESP_LOGE(TAG, "Failed to create connection_monitor task");
        return 1;
    }

    return 0;
}
