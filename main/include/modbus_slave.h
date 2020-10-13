/*
 * modbus_slave.h
 *
 *  Created on: Oct 13, 2020
 *      Author: adrian-estelio
 */

#ifndef MAIN_INCLUDE_MODBUS_SLAVE_H_
#define MAIN_INCLUDE_MODBUS_SLAVE_H_
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
void uart_init(QueueHandle_t *queue);

#endif /* MAIN_INCLUDE_MODBUS_SLAVE_H_ */
