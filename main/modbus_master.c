/*
 * modbus_master.c
 *
 *  Created on: Oct 14, 2020
 *      Author: adrian-estelio
 */
#include "modbus_master.h"
#include "CRC.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "stdint.h"
#define TX_BUF_SIZE 1024
static char *TAG = "MASTER";

typedef union {
    uint16_t Val;
    struct {
        uint8_t LB;
        uint8_t HB;
    } byte;

} INT_VAL;

enum modbus_function_t { READ_HOLDING = 3, READ_INPUT };

void read_input_register(uint8_t slave, uint16_t start_address,
                         uint16_t quantity) {
    INT_VAL address;
    INT_VAL Number;
    INT_VAL CRC;

    uint8_t *frame = (uint8_t *)malloc(TX_BUF_SIZE);
    frame[0]       = slave;
    frame[1]       = 0x04;
    address.Val    = start_address;
    frame[2]       = address.byte.HB;
    frame[3]       = address.byte.LB;
    Number.Val     = quantity;
    frame[4]       = Number.byte.HB;
    frame[5]       = Number.byte.LB;
    CRC.Val        = CRC16(frame, 6);
    frame[6]       = CRC.byte.LB;
    frame[7]       = CRC.byte.HB;
    uart_write_bytes(UART_NUM_1, (const char *)frame, 8);
    free(frame);
    ESP_LOGI(TAG, "Poll sent to slave : %d...", slave);
}
void save_register(uint8_t *data, uint8_t length, uint16_t **modbus_registers) {
    uint8_t FUNCTION        = data[1];
    uint16_t *inputRegister = modbus_registers[1];
    uint8_t SLAVE           = data[0];
    switch (FUNCTION) {
    case READ_INPUT:;
        uint8_t byte_count = data[2];
        INT_VAL aux_registro;
        for (uint8_t i = 0; i < byte_count / 2; i++) {
            aux_registro.byte.HB     = data[3 + 2 * i];
            aux_registro.byte.LB     = data[4 + 2 * i];
            inputRegister[SLAVE + i] = aux_registro.Val;
        }
        ESP_LOGI(TAG, "Received data saved...");
        break;
    }
}
