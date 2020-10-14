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
typedef union {
    uint16_t Val;
    struct {
        uint8_t LB;
        uint8_t HB;
    } byte;

} INT_VAL;
enum modbus_function_t { READ_HOLDING = 3, READ_INPUT };
void uart_init(QueueHandle_t *queue);
void modbus_slave_functions(const uint8_t *frame, uint8_t lengh,
                            uint16_t **modbus_registers);

#endif /* MAIN_INCLUDE_MODBUS_SLAVE_H_ */
