#include "my_http.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "pump_data.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "esp_wifi.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

// #define PATH "/api/v1/m0G6zzNJzZKh9bSc2jyC/telemetry"
//  241000SBP168H-2507-110
// #define PATH "/api/v1/T3u5OdBzdgPNlTjB9iDW/telemetry"
// 241000SBP268H-2507-111
// #define PATH "/api/v1/g2b4PPQEAtP5P6HwYMFg/telemetry"
// 241000SBP368A-2507-112
#define PATH "/api/v1/vyHliCXI34N7OzhWaDl7/telemetry"

#define HOST "thingsboard.cloud"
#define PORT 80

#define MAX_HTTP_OUTPUT_BUFFER 2048
static const char *TAG = "HTTP";

TaskHandle_t http_task_handle = NULL;
TaskHandle_t http_monitor_task_handle = NULL;
TaskHandle_t http_sender_task_handle = NULL;
// static bool http_running = false;
// static bool send_http = false;

extern EventGroupHandle_t s_wifi_event_group;
extern bool http_data_ready;
// External sensor data

esp_err_t _http_event_handler(esp_http_client_event_t *evt) { return ESP_OK; }

static void http_rest_with_url(void) {
	char response[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};

	esp_http_client_config_t config = {
		.host = HOST,
		.port = PORT,
		.path = PATH,
		.method = HTTP_METHOD_POST,
		.event_handler = _http_event_handler,
		.user_data = response,
	};

	esp_http_client_handle_t client = esp_http_client_init(&config);
	if (client == NULL) {
		ESP_LOGE(TAG, "Failed to init http client");
		return;
	}

	esp_http_client_set_header(client, "Content-Type", "application/json");

	char body[128];
	get_http_pump_data(body, sizeof(body));
	ESP_LOGI(TAG, "HTTP body: %s", body);
	esp_http_client_set_post_field(client, body, strlen(body));

	esp_err_t err = esp_http_client_perform(client);
	if (err == ESP_OK) {
		ESP_LOGI(TAG, "HTTP Status = %d",
				 esp_http_client_get_status_code(client));
	} else {
		ESP_LOGE(TAG, "HTTP POST failed: %s", esp_err_to_name(err));
	}

	esp_http_client_cleanup(client);
}
void wifi_connect() {

	ESP_ERROR_CHECK(esp_wifi_start());
	// При необходимости esp_wifi_connect(); если не auto-connect
}

void wifi_disconnect() { ESP_ERROR_CHECK(esp_wifi_stop()); }

void http_sender_task(void *arg) {
	ESP_LOGI(TAG, "http_sender_task start");
	EventBits_t bits =
		xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE,
							pdTRUE, pdMS_TO_TICKS(3000));

	ESP_LOGI(TAG, "WIFI_CONNECTED_BIT=%d bits=%d  bits & WIFI_CONNECTED_BIT=%d",
			 WIFI_CONNECTED_BIT, bits, bits & WIFI_CONNECTED_BIT);

	if (!(bits & WIFI_CONNECTED_BIT)) {
		wifi_connect();
		ESP_LOGI(TAG, "wifi_connect called");
	}

	bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE,
							   pdTRUE, pdMS_TO_TICKS(3000));

	if ((bits & WIFI_CONNECTED_BIT)) {
		ESP_LOGI(TAG, "wifi connected sucessfully, send");
		http_rest_with_url();
		wifi_disconnect();
	} else {
		ESP_LOGI(TAG, "wifi doesnot connected sucessfully");
	}

	http_sender_task_handle = NULL;
	vTaskDelete(NULL);
}

int http_create_task() {
	if (http_sender_task_handle == NULL) {
		BaseType_t xReturned =
			xTaskCreate(&http_sender_task, "http_sender_task", 10096, NULL, 5,
						&http_sender_task_handle);
		if (xReturned != pdPASS) {
			ESP_LOGE(TAG, "Failed to create HTTP http_sender_task");
			return 1;
		}
	}

	return 0;
}
