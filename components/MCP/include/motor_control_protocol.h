/*
 * motor_control_protocol.h
 *
 *  Created on: 8 июл. 2025 г.
 *      Author: dimer
 */

#ifndef COMPONENTS_MCP_MOTOR_CONTROL_PROTOCOL_H_
#define COMPONENTS_MCP_MOTOR_CONTROL_PROTOCOL_H_


// include/motor_control_protocol.h

#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t uart_port;
    bool connected;
    uint8_t preq_max;
    uint8_t presp_max;
    uint8_t pasync_max;
    bool crc_enabled;
} mcps_context_t;

bool mcps_start_connection(mcps_context_t *ctx);



#endif /* COMPONENTS_MCP_MOTOR_CONTROL_PROTOCOL_H_ */
