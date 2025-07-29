/*
 * pump_data.h
 *
 *  Created on: 27 июн. 2025 г.
 *      Author: katev
 */

#ifndef COMPONENTS_DATA_FORMAT_PUMP_DATA_H_
#define COMPONENTS_DATA_FORMAT_PUMP_DATA_H_

#pragma once

 
#include <stdint.h>
#include <stdio.h>
 

typedef struct {
    float torque;
    int speed_rpm;
    int pressure_psi;
    int pressure_threshold;
    int oil_temp_c;
    int oil_temp_f;
    int motor_temp_c;
    int motor_temp_f;
    int board_temp;
    float power;
    //float linear_cm;
    //float linear_in;
    float flow_cm;
    float flow_in;
    uint16_t elong_time_millis;
    uint16_t battery_mv;
    uint16_t max_pressure; // reading from PARAM1
    //uint16_t vref;
    uint16_t button_state;
    char debug_str[128];
    
} UiData_t;

 
 
void get_http_pump_data(char *payload, size_t payload_size);

void get_ble_pump_data(uint8_t *payload, size_t payload_size);

 


 


#endif /* COMPONENTS_DATA_FORMAT_PUMP_DATA_H_ */
