/*
 * my_mqtt.c
 *
 *  Created on: 6 июн. 2025 г.
 *      Author: katev
 */

#include "mqtt_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <inttypes.h>
#include "my_mqtt.h"


#define TAG "MQTT"
/*#define MQTT_URI "mqtt://thingsboard.cloud"
#define ACCESS_TOKEN "uODHZvjR9CaivkcyWcPq" */

#define MQTT_URI "mqtt://mqtt.thingsboard.cloud"
#define ACCESS_TOKEN "MyToken111"

TaskHandle_t mqtt_task_handle = NULL;
bool send_wifi = false;

int16_t dPressure = 36546, dSpeed = 3000, V = 56, L = 7, To = 40, Tm = 48;
uint8_t id = 17;

static esp_mqtt_client_handle_t mqtt_client = NULL;

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
    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"ID\":%d,\"L\":%d,\"P\":%d,\"To\":%d,\"Tm\":%d,\"S\":%d,\"V\":%d}",
             id, L, dPressure, To, Tm, dSpeed, V);

    int msg_id = esp_mqtt_client_publish(mqtt_client, "v1/devices/me/telemetry", payload, 0, 1, 0);
    if (msg_id != -1) {
        ESP_LOGI(TAG, "Sent telemetry msg_id=%d", msg_id);
    } else {
        ESP_LOGE(TAG, "Failed to send telemetry");
    }
}
void mqtt_task(void *pvParameters) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_URI,
        .credentials.username = ACCESS_TOKEN
        // Если нужен сертификат для TLS, можно добавить здесь:
        //.broker.verification.certificate = (const char *)your_certificate_pem_start,
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);

    while (send_wifi) {
        mqtt_send_telemetry();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }

    esp_mqtt_client_stop(mqtt_client);
    esp_mqtt_client_destroy(mqtt_client);
    mqtt_client = NULL;
    mqtt_task_handle = NULL;
    ESP_LOGI(TAG, "mqtt_task exiting");
    vTaskDelete(NULL); 
}


