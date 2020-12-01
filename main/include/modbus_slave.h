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
void uart_init(QueueHandle_t *queue);
void modbus_slave_functions(const uint8_t *frame, uint8_t length,
                            uint16_t **modbus_registers);
void register_save(uint32_t value, uint16_t *modbus_register);
void crc_error_response(const uint8_t *frame);
void normalize_pulses (uint32_t value, uint32_t* normalized_value);
#endif /* MAIN_INCLUDE_MODBUS_SLAVE_H_ */
