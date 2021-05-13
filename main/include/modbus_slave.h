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

typedef struct mb_response {
    uint8_t frame[256];
    uint8_t len;
} mb_response_t;

void uart_init(QueueHandle_t *queue);
void modbus_slave_functions(mb_response_t *response_frame, const uint8_t *frame,
                            uint8_t length, uint16_t **modbus_registers);
void register_save(uint32_t value, uint16_t *modbus_register);
void crc_error_response(const uint8_t *frame);
uint32_t normalize_pulses(uint32_t value);
#endif /* MAIN_INCLUDE_MODBUS_SLAVE_H_ */
