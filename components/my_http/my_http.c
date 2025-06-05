/*
 * my_http.c
 *
 *  Created on: 27 мая 2025 г.
 *      Author: dimer
 */
 
// components/my_http/my_http.c

#include "my_http.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#define MAX_HTTP_OUTPUT_BUFFER 2048
static const char *TAG = "HTTP";
TaskHandle_t http_task_handle = NULL;
bool send_wifi = false;
// External sensor data
int16_t dPressure = 36546, dSpeed = 3000, V = 56, L = 7, To = 40, Tm = 48;
//extern int16_t dPressure, dSpeed, V, L, To, Tm;
uint8_t id = 17;
esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    return ESP_OK; // Simplified for modularity
}

static void http_rest_with_url(void) {
    char response[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};
//curl -v -X POST http://thingsboard.cloud/api/v1/uODHZvjR9CaivkcyWcPq/telemetry --header Content-Type:application/json --data "{temperature:25}"
    esp_http_client_config_t config = {
        .host = "thingsboard.cloud",//139.162.181.76
        .port = 80,
        .path = "/api/v1/uODHZvjR9CaivkcyWcPq/telemetry",
        .method = HTTP_METHOD_POST,
        .event_handler = _http_event_handler,
        .user_data = response,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");

    char body[80];
    snprintf(body, sizeof(body), "{\"ID\":%d,\"L\":%d,\"P\":%d,\"To\":%d,\"Tm\":%d,\"S\":%d,\"V\":%d}", id,
             L, dPressure, To, Tm, dSpeed, V);
	//id = 42 - id;
    esp_http_client_set_post_field(client, body, strlen(body));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP Status = %d", esp_http_client_get_status_code(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

void http_task(void *pvParameters) {
    while (send_wifi) {
        http_rest_with_url();
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    ESP_LOGI(TAG, "http_task exiting");
    vTaskDelete(NULL); // удаляет саму себя
}




