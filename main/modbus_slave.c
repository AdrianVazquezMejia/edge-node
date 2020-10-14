/*
 * modbus_slave.c
 *
 *  Created on: Oct 13, 2020
 *      Author: adrian-estelio
 */
#include "modbus_slave.h"
#include "CRC.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "stdint.h"
static char *TAG = "UART";
#define TXD_PIN     25
#define RXD_PIN     14
#define RTS_PIN     27
#define CTS_PIN     19
#define RX_BUF_SIZE 1024
#define TX_BUF_SIZE 1024
void uart_init(QueueHandle_t *queue) {
    uart_driver_delete(UART_NUM_1);
    int uart_baudarate = 9600;
    ESP_LOGI(TAG, "Baudarate is %d", uart_baudarate);
    const uart_config_t uart_config = {
        .baud_rate = uart_baudarate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, RTS_PIN, CTS_PIN);
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, TX_BUF_SIZE * 2, 20, queue,
                        0);
    uart_set_mode(UART_NUM_1, UART_MODE_RS485_HALF_DUPLEX);
}

void modbus_slave_functions(const uint8_t *frame, uint8_t lengh,
                            uint16_t **modbus_registers) {
    uint8_t FUNCTION = frame[1];
    INT_VAL address;
    address.byte.HB = frame[2];
    address.byte.LB = frame[3];
    INT_VAL value;
    value.byte.HB = frame[4];
    value.byte.LB = frame[5];
    INT_VAL CRC;
    uint8_t *response_frame = (uint8_t *)malloc(255);
    uint8_t response_len    = 0;
    INT_VAL *inputRegister  = (INT_VAL *)modbus_registers[1];
    response_frame[0]       = frame[0];
    response_frame[1]       = frame[1];
    if (CRC16(frame, lengh) == 0) {
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
            uart_write_bytes(UART_NUM_1, (const char *)response_frame,
                             response_len);
            break;
        }
    }
}
