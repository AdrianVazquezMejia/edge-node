/*
 * modbus_lora.c
 *
 *  Created on: Oct 21, 2020
 *      Author: adrian-estelio
 */
#include "modbus_lora.h"
#include "CRC.h"
#include "esp_log.h"
#include "esp_rf1276.h"
#include "rf1276.h"
enum modbus_function_t { READ_HOLDING = 3, READ_INPUT };
typedef union {
    uint16_t Val;
    struct {
        uint8_t LB;
        uint8_t HB;
    } byte;

} INT_VAL;
static char *TAG = "LORA_MODBUS";
mb_response_t modbus_lora_functions(const uint8_t *frame, uint8_t length,
                                    uint16_t **modbus_registers) {
    uint8_t FUNCTION = frame[1];
    INT_VAL address;
    address.byte.HB = frame[2];
    address.byte.LB = frame[3];
    INT_VAL value;
    value.byte.HB = frame[4];
    value.byte.LB = frame[5];
    INT_VAL CRC;
    uint8_t response_frame[255];
    uint8_t response_len   = 0;
    INT_VAL *inputRegister = (INT_VAL *)modbus_registers[1];
    response_frame[0]      = frame[0];
    response_frame[1]      = frame[1];

    mb_response_t output;
    if (CRC16(frame, length) == 0) {
        switch (FUNCTION) {
        case READ_INPUT:
            ESP_LOGI(TAG, "Reading inputs Registers");
            response_frame[2] = frame[5] * 2;
            response_len      = 3;
            for (uint16_t i = 0; i < value.Val; i++) {
                response_frame[response_len] =
                    inputRegister[address.Val + i].byte.HB;
                response_len++;
                response_frame[response_len] =
                    inputRegister[address.Val + i].byte.LB;
                response_len++;
            }
            CRC.Val = CRC16(response_frame, response_len);
            response_frame[response_len++] = CRC.byte.LB;
            response_frame[response_len++] = CRC.byte.HB;
            output.frame                   = response_frame;
            output.len                     = response_len;
            ESP_LOGI(TAG,
                     "Register read"); // BOGUS without this log crc is missing
            break;
        }
    } else {
        uart_flush(UART_NUM_2);
    }
    return output;
}
