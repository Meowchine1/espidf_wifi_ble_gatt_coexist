/*
 * aspep.h
 *
 *  Created on: 8 июл. 2025 г.
 *      Author: dimer
 */

#ifndef COMPONENTS_MCP_ASPEP_H_
#define COMPONENTS_MCP_ASPEP_H_

// include/aspep.h

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define ASPEP_VERSION 0x00
#define ASPEP_TYPE_BEACON 0x5

typedef struct {
    uint8_t type : 4;
    uint8_t crch : 4;
    uint8_t version : 3;
    uint8_t crc_support : 1;
    uint8_t rxs_max : 6;
    uint8_t txs_max : 7;
    uint8_t txa_max : 7;
} __attribute__((packed)) aspep_beacon_t;

typedef struct {
    uint8_t raw[4];
} __attribute__((packed)) aspep_header_t;

bool aspep_send_beacon(uint8_t uart_port, const aspep_beacon_t *beacon);
bool aspep_receive_beacon(uint8_t uart_port, aspep_beacon_t *beacon);
uint8_t aspep_calculate_crch(const uint8_t *header);  // CRC-4




#endif /* COMPONENTS_MCP_ASPEP_H_ */
