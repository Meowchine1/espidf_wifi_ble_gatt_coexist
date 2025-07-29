/*
 * aspep.c
 *
 *  Created on: 8 июл. 2025 г.
 *      Author: dimer
 */

// src/aspep.c

#include "aspep.h"
#include <string.h>
#include <stdio.h>

// Вычисление CRC-4 для заголовка
static uint8_t calculate_crch(const uint8_t *header)
{
    uint8_t crc = 0;
    for (int i = 0; i < 3.5; ++i) {
        uint8_t nibble = (i % 2 == 0) ? (header[i / 2] & 0x0F) : (header[i / 2] >> 4);
        crc ^= nibble; // Простой вариант CRC
    }
    return crc & 0x0F;
}

// Отправка BEACON пакета
bool aspep_send_beacon(uint8_t uart_port, const aspep_beacon_t *beacon)
{
    aspep_header_t packet;
    memcpy(packet.raw, beacon, sizeof(aspep_beacon_t));
    packet.raw[0] &= 0xF0; // очищаем поле crch
    uint8_t crch = calculate_crch(packet.raw);
    packet.raw[0] |= (crch & 0x0F);

    return uart_write_bytes(uart_port, (const char*)packet.raw, sizeof(packet)) == sizeof(packet);
}

// Принятие BEACON пакета
bool aspep_receive_beacon(uint8_t uart_port, aspep_beacon_t *beacon)
{
    uint8_t buffer[4];
    int len = uart_read_bytes(uart_port, buffer, sizeof(buffer), 1000 / portTICK_PERIOD_MS);  // таймаут 1 секунда
    if (len != 4) return false;

    uint8_t crch = calculate_crch(buffer);
    if ((buffer[0] & 0x0F) != crch) return false;  // Проверка CRC

    memcpy(beacon, buffer, sizeof(*beacon));
    return true;
}



