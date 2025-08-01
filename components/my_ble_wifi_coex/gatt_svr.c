// Copyright 2015-2020 The Apache Software Foundation
// Modifications Copyright 2017-2020 Espressif Systems (Shanghai) CO., LTD.
//
// Portions of this software were developed at Runtime Inc, copyright 2015.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "gatt_svr.h"
#include "services/ans/ble_svc_ans.h"
#include "esp_log.h"

#include "pump_data.h"


//extern void get_ble_pump_data(uint8_t *payload, size_t payload_size);
//static const char* TAG = "wifi_prph_coex";
static const char *TAG = "GATT";

static const ble_uuid128_t uuid_telemetry = 
    BLE_UUID128_INIT(0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
                     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01);
                     
static const ble_uuid128_t uuid_telemetry_ch = 
    BLE_UUID128_INIT(0x02,0x03,0x00,0x00,0x00,0x00,0x00,0x00,
                     0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01);

static uint16_t telemetry_val_handle = 0;
uint16_t telemetry_conn_handle = BLE_HS_CONN_HANDLE_NONE;
uint8_t telemetry_data[128];
bool telemetry_notify_enabled = false;


int send_pump_data(struct ble_gatt_access_ctxt *ctxt){
	
	// Подготовка данных
    get_ble_pump_data(telemetry_data, sizeof(telemetry_data));

    // Отправка клиенту
    int rc = os_mbuf_append(ctxt->om, telemetry_data, sizeof(UiData_t));
    if (rc != 0) {
        return BLE_ATT_ERR_INSUFFICIENT_RES;
    } 
    return 0;
}

static int gatt_svr_chr_access_telemetry(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg) {
    ESP_LOGI("GATT", "gatt_svr_chr_access_telemetry");

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR: {
		
		ESP_LOGI(TAG, "BLE_GATT_ACCESS_OP_READ_CHR");
		if ((conn_handle == telemetry_conn_handle) && (attr_handle == telemetry_val_handle)){
			return send_pump_data(ctxt);
			}
		 

        return 0;
    }

    default:
        return BLE_ATT_ERR_UNLIKELY;
    }
}

#define BLE_GATT_CCC_NOTIFY   0x0001
// Callback для дескриптора Client Characteristic Configuration (CCCD)
static int gatt_svr_chr_access_cccd(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg) {
    ESP_LOGI(TAG, "gatt_svr_chr_access_cccd");

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_DSC) {
        uint16_t cccd_val;
        int rc = ble_hs_mbuf_to_flat(ctxt->om, &cccd_val, sizeof(cccd_val), NULL);
        if (rc != 0) {
            return BLE_ATT_ERR_UNLIKELY;
        }

        telemetry_notify_enabled = (cccd_val == BLE_GATT_CCC_NOTIFY);
        if (telemetry_notify_enabled) {
            telemetry_conn_handle = conn_handle;  // <- сохраняем подключение
            ESP_LOGI(TAG, "Notify enabled, conn_handle=%d", conn_handle);
        } else {
            telemetry_conn_handle = BLE_HS_CONN_HANDLE_NONE;  // <- отключились
            ESP_LOGI(TAG, "Notify disabled");
        }

        return 0;
    } else if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC) {
        uint16_t cccd_val = telemetry_notify_enabled ? BLE_GATT_CCC_NOTIFY : 0;
        return os_mbuf_append(ctxt->om, &cccd_val, sizeof(cccd_val)) == 0
            ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    return BLE_ATT_ERR_UNLIKELY;
}


extern  bool ble_data_ready;
// Функция для отправки уведомления при включенной подписке
void telemetry_notify(void)
{
    ESP_LOGI(TAG, "Notify enabled: %d telemetry_conn_handle:%d  ", telemetry_notify_enabled , telemetry_conn_handle);

    if (telemetry_notify_enabled && (telemetry_conn_handle != BLE_HS_CONN_HANDLE_NONE)) {
		ESP_LOGI(TAG, "ble_data_ready: %d", ble_data_ready );
        if (ble_data_ready) {
			ESP_LOGI(TAG, "ble_data_ready enabled:");
            struct os_mbuf *om;

            get_ble_pump_data(telemetry_data, sizeof(telemetry_data));

            om = ble_hs_mbuf_from_flat(telemetry_data, sizeof(telemetry_data));
            if (!om) {
                ESP_LOGE(TAG, "Failed to allocate mbuf for notification");
            }

            int rc = ble_gatts_notify_custom(telemetry_conn_handle, telemetry_val_handle, om);
            if (rc != 0) {
                ESP_LOGE(TAG, "Notify error: %d", rc);
            } else {
                ESP_LOGI(TAG, "Notification sent");
                ble_data_ready = 0;  // Сбрасываем только после успешной отправки
            }
        }
    }
} 

/**
 * The vendor specific security test service consists of two characteristics:
 *     o random-number-generator: generates a random 32-bit number each time
 *       it is read.
 *     o static-value: a single-byte characteristic that can always be read.
 */

/* 59462f12-9543-9999-12c8-58b459a2712d */
static const ble_uuid128_t gatt_svr_svc_sec_test_uuid =
    BLE_UUID128_INIT(0x2d, 0x71, 0xa2, 0x59, 0xb4, 0x58, 0xc8, 0x12,
                     0x99, 0x99, 0x43, 0x95, 0x12, 0x2f, 0x46, 0x59);

/* 5c3a659e-897e-45e1-b016-007107c96df6 */
static const ble_uuid128_t gatt_svr_chr_sec_test_rand_uuid =
    BLE_UUID128_INIT(0xf6, 0x6d, 0xc9, 0x07, 0x71, 0x00, 0x16, 0xb0,
                     0xe1, 0x45, 0x7e, 0x89, 0x9e, 0x65, 0x3a, 0x5c);

/* 5c3a659e-897e-45e1-b016-007107c96df7 */
static const ble_uuid128_t gatt_svr_chr_sec_test_static_uuid =
    BLE_UUID128_INIT(0xf7, 0x6d, 0xc9, 0x07, 0x71, 0x00, 0x16, 0xb0,
                     0xe1, 0x45, 0x7e, 0x89, 0x9e, 0x65, 0x3a, 0x5c);

 

static uint8_t gatt_svr_sec_test_static_val;

static int gatt_svr_chr_access_sec_test(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /*** Service: Security test. */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_sec_test_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                /*** Characteristic: Random number generator. */
                .uuid = &gatt_svr_chr_sec_test_rand_uuid.u,
                .access_cb = gatt_svr_chr_access_sec_test,
                .flags = BLE_GATT_CHR_F_READ
            },
            {
                /*** Characteristic: Static value. */
                .uuid = &gatt_svr_chr_sec_test_static_uuid.u,
                .access_cb = gatt_svr_chr_access_sec_test,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE
            },
            { 0 }
        }
    },

    {
        /*** Service: Telemetry */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &uuid_telemetry.u,  // UUID сервиса telemetry
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                /*** Telemetry data characteristic */
                .uuid = &uuid_telemetry_ch.u,
                .access_cb = gatt_svr_chr_access_telemetry,
                .val_handle = &telemetry_val_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                .descriptors = (struct ble_gatt_dsc_def[]) {
                    {
                        .uuid = BLE_UUID16_DECLARE(BLE_GATT_DSC_CLT_CFG_UUID16),
                        .att_flags = BLE_ATT_F_READ | BLE_ATT_F_WRITE,
                        .access_cb = gatt_svr_chr_access_cccd
                    },
                    { 0 }
                }
            },
            { 0 }
        }
    },

    { 0 }
};

static int
gatt_svr_chr_write(struct os_mbuf *om, uint16_t min_len, uint16_t max_len,
                   void *dst, uint16_t *len)
{
    uint16_t om_len;
    int rc;

    om_len = OS_MBUF_PKTLEN(om);
    if (om_len < min_len || om_len > max_len) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    rc = ble_hs_mbuf_to_flat(om, dst, max_len, len);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}

static int
gatt_svr_chr_access_sec_test(uint16_t conn_handle, uint16_t attr_handle,
                             struct ble_gatt_access_ctxt *ctxt,
                             void *arg)
{
    const ble_uuid_t *uuid;
    int rand_num;
    int rc;

    uuid = ctxt->chr->uuid;

    /* Determine which characteristic is being accessed by examining its
     * 128-bit UUID.
     */

    if (ble_uuid_cmp(uuid, &gatt_svr_chr_sec_test_rand_uuid.u) == 0) {
        assert(ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR);

        /* Respond with a 32-bit random number. */
        rand_num = rand();
        rc = os_mbuf_append(ctxt->om, &rand_num, sizeof rand_num);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    if (ble_uuid_cmp(uuid, &gatt_svr_chr_sec_test_static_uuid.u) == 0) {
        switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            rc = os_mbuf_append(ctxt->om, &gatt_svr_sec_test_static_val,
                                sizeof gatt_svr_sec_test_static_val);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            rc = gatt_svr_chr_write(ctxt->om,
                                    sizeof gatt_svr_sec_test_static_val,
                                    sizeof gatt_svr_sec_test_static_val,
                                    &gatt_svr_sec_test_static_val, NULL);
            return rc;

        default:
            assert(0);
            return BLE_ATT_ERR_UNLIKELY;
        }
    }

    /* Unknown characteristic; the nimble stack should not have called this
     * function.
     */
    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
}

void
gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGD(TAG, "registered service %s with handle=%d",
                    ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                    ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGD(TAG, "registering characteristic %s with "
                    "def_handle=%d val_handle=%d\n",
                    ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                    ctxt->chr.def_handle,
                    ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGD(TAG, "registering descriptor %s with handle=%d",
                    ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                    ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}

 
 

int
gatt_svr_init(void)
{
    int rc;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_ans_init();

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }
    
    

    return 0;
}
