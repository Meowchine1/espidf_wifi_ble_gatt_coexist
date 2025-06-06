/*
 * my_ble.c
 *
 *  Created on: 6 июн. 2025 г.
 *      Author: katev
 */

#include "esp_gatts_api.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"
#include "esp_bt_defs.h"
#include "esp_log.h"

#define PROFILE_NUM 1
#define PROFILE_APP_ID 0

static uint16_t gatt_handle_table[12];

static esp_gatt_char_prop_t prop_read_notify = ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_NOTIFY;

static esp_gatts_attr_db_t gatt_db[] =
{
    // Primary Service
    [0] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&primary_service_uuid, ESP_GATT_PERM_READ,
            sizeof(uint16_t), sizeof(GATTS_SERVICE_UUID_TEST), (uint8_t*)&GATTS_SERVICE_UUID_TEST}},

    // dPressure
    [1] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&character_declaration_uuid, ESP_GATT_PERM_READ,
            sizeof(uint8_t), sizeof(uint8_t), (uint8_t*)&prop_read_notify}},
    [2] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&GATTS_CHAR_UUID_PRESSURE, ESP_GATT_PERM_READ,
            sizeof(int16_t), sizeof(dPressure), (uint8_t*)&dPressure}},

    // dSpeed
    [3] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&character_declaration_uuid, ESP_GATT_PERM_READ,
            sizeof(uint8_t), sizeof(uint8_t), (uint8_t*)&prop_read_notify}},
    [4] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&GATTS_CHAR_UUID_SPEED, ESP_GATT_PERM_READ,
            sizeof(int16_t), sizeof(dSpeed), (uint8_t*)&dSpeed}},

    // V
    [5] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&character_declaration_uuid, ESP_GATT_PERM_READ,
            sizeof(uint8_t), sizeof(uint8_t), (uint8_t*)&prop_read_notify}},
    [6] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&GATTS_CHAR_UUID_VOLTAGE, ESP_GATT_PERM_READ,
            sizeof(int16_t), sizeof(V), (uint8_t*)&V}},

    // L
    [7] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&character_declaration_uuid, ESP_GATT_PERM_READ,
            sizeof(uint8_t), sizeof(uint8_t), (uint8_t*)&prop_read_notify}},
    [8] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&GATTS_CHAR_UUID_LEVEL, ESP_GATT_PERM_READ,
            sizeof(int16_t), sizeof(L), (uint8_t*)&L}},

    // To
    [9] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&character_declaration_uuid, ESP_GATT_PERM_READ,
            sizeof(uint8_t), sizeof(uint8_t), (uint8_t*)&prop_read_notify}},
    [10] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&GATTS_CHAR_UUID_TEMP_OIL, ESP_GATT_PERM_READ,
            sizeof(int16_t), sizeof(To), (uint8_t*)&To}},

    // Tm
    [11] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&character_declaration_uuid, ESP_GATT_PERM_READ,
            sizeof(uint8_t), sizeof(uint8_t), (uint8_t*)&prop_read_notify}},
    [12] = {{ESP_GATT_AUTO_RSP}, {ESP_UUID_LEN_16, (uint8_t*)&GATTS_CHAR_UUID_TEMP_MOTOR, ESP_GATT_PERM_READ,
            sizeof(int16_t), sizeof(Tm), (uint8_t*)&Tm}},
};

esp_err_t ble_gatts_init(void) {
    esp_err_t ret;

    ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret) {
        ESP_LOGW(TAG, "BT Classic memory release failed: %s", esp_err_to_name(ret));
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ret = esp_ble_gatts_register_callback(gatts_event_handler); // реализуй ниже
    if (ret) {
        ESP_LOGE(TAG, "GATTS register error");
        return ret;
    }

    ret = esp_ble_gatts_app_register(PROFILE_APP_ID);
    return ret;
}


