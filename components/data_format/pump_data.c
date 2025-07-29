/*
 *  c
 *
 *  Created on: 1 июл. 2025 г.
 *      Author: katev
 */


#include "stdbool.h"
#include <stdio.h>
#include <string.h>
#include "pump_data.h"
// Объявлены в других модулях
extern int16_t MaxPressure, MaxSpeed, MaxV, To, Tm, Tb, TorqueNm;
extern uint32_t ElongTime;
extern float MaxLflow;
extern uint8_t mac[6];

 

void get_http_pump_data(char *payload, size_t payload_size) {

	// Формируем JSON-строку
	snprintf(payload, payload_size,
			 "{\"ID\":%02x%02x%02x%02x%02x%02x,\"F\":%.2f,\"P\":%d,\"To\":%d,"
			 "\"Tm\":%d,\"Tb\":%d,\"Nm\":%.2f,\"U\":%d,\"t\":%.2f}",
			 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], MaxLflow,
			 MaxPressure, To, Tm,
			 (Tb * 9 / 5) + 32,			// в °F
			 (float)TorqueNm , // мН·м → Н·м
			 MaxV,
			 ((float)(ElongTime) / 1000)); // мс → сек
}


extern UiData_t ui_data;
void get_ble_pump_data(uint8_t *payload, size_t payload_size) {
    if (payload_size < sizeof(UiData_t)) return;

    memcpy(payload, &ui_data, sizeof(UiData_t));
}

