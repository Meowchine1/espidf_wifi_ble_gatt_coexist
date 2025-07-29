/*
 * networkInterface.h
 *
 *  Created on: 25 июн. 2025 г.
 *      Author: katev
 */

#ifndef COMPONENTS_MY_BLE_NETWORKINTERFACE_H_
#define COMPONENTS_MY_BLE_NETWORKINTERFACE_H_

#pragma once
#include "stdint.h"
/*#include "stdint.h"*/


#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

int run_internet_task(void);
 


#endif /* COMPONENTS_MY_BLE_NETWORKINTERFACE_H_ */
