/*
 * motor_control_protocol.c
 *
 *  Created on: 8 июл. 2025 г.
 *      Author: dimer
 */

// src/motor_control_protocol.c

#include "motor_control_protocol.h"
#include "aspep.h"
#include <string.h>
#include <stdio.h>

// Реализация старта соединения
bool mcps_start_connection(mcps_context_t *ctx)
{
    aspep_beacon_t beacon = {
        .type = ASPEP_TYPE_BEACON,
        .version = ASPEP_VERSION,
        .crc_support = 1,
        .rxs_max = 3,    // 128 байт
        .txs_max = 7,    // 256 байт
        .txa_max = 8     // 512 байт
    };

    aspep_beacon_t response;

    // Попытка установить соединение
    for (int retries = 0; retries < 10; ++retries) {
        aspep_send_beacon(ctx->uart_port, &beacon);
        if (aspep_receive_beacon(ctx->uart_port, &response)) {
            if (memcmp(&beacon, &response, sizeof(beacon)) == 0) {
                ctx->connected = true;
                ctx->preq_max = (response.rxs_max + 1) * 32;
                ctx->presp_max = (response.txs_max + 1) * 32;
                ctx->pasync_max = (response.txa_max) * 64;
                ctx->crc_enabled = response.crc_support;
                return true;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(200)); // TBEACON
    }

    return false;
}



