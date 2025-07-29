/*
 * my_http.h
 *
 *  Created on: 27 мая 2025 г.
 *      Author: dimer
 */

#ifndef COMPONENTS_MY_HTTP_MY_HTTP_H_
#define COMPONENTS_MY_HTTP_MY_HTTP_H_

// components/my_http/my_http.h
#pragma once

 #include "stdint.h"

int http_create_task (void) ;
void http_task(void *pvParameters);

#endif /* COMPONENTS_MY_HTTP_MY_HTTP_H_ */
