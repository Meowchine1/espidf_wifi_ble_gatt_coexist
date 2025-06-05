// components/my_wifi/my_wifi.c

#include "my_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "my_http.h"
#include <stdlib.h>

static const char *TAG = "WIFI";
 
extern TaskHandle_t http_task_handle;
#include <stdlib.h>  // для malloc/free

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        if (event_id == WIFI_EVENT_STA_START) {
            ESP_LOGI(TAG, "Wi-Fi started, scanning before connecting...");

            wifi_scan_config_t scan_config = {
                .ssid = NULL,
                .bssid = NULL,
                .channel = 0,
                .show_hidden = true,
            };

            esp_err_t err = esp_wifi_scan_start(&scan_config, true);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "WiFi scan start failed: %s", esp_err_to_name(err));
                // Можно попытаться позже повторить
            }

        } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
            ESP_LOGW(TAG, "Disconnected, reconnecting after delay...");
            vTaskDelay(pdMS_TO_TICKS(2000)); // добавляем задержку
            esp_wifi_connect();

        } else if (event_id == WIFI_EVENT_SCAN_DONE) {
            uint16_t ap_num = 0;
            esp_wifi_scan_get_ap_num(&ap_num);
            ESP_LOGI(TAG, "Scan done: %d access points found", ap_num);

            if (ap_num == 0) {
                ESP_LOGW(TAG, "No access points found, retrying scan later...");
                // Можно запланировать новый скан через таймер или просто выйти
                return;
            }

            wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * ap_num);
            if (ap_list == NULL) {
                ESP_LOGE(TAG, "Failed to allocate memory for AP list");
                return;
            }

            esp_wifi_scan_get_ap_records(&ap_num, ap_list);

            for (int i = 0; i < ap_num; i++) {
                ESP_LOGI(TAG, "SSID: %s, RSSI: %d, Authmode: %d",
                         (char *)ap_list[i].ssid,
                         ap_list[i].rssi,
                         ap_list[i].authmode);
            }
            free(ap_list);

            ESP_LOGI(TAG, "Scan complete, connecting to configured AP...");
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "Got IP, starting HTTP task...");
        xTaskCreate(&http_task, "http_task", 8192, NULL, 5, &http_task_handle);
    }
}


static void wifi_init_sta(void) {
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "Verizon-RC400L-EA",
            .password = "44424f4f",
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    
}

void wifi_app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_init_sta();
}
