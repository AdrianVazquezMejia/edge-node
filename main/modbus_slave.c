/*
 * modbus_slave.c
 *
 *  Created on: Oct 13, 2020
 *      Author: adrian-estelio
 */
#include "modbus_slave.h"
#include "CRC.h"
#include "config_rtu.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "math.h"
#include "stdint.h"
static char *TAG = "UART";
#ifdef CONFIG_PRODUCTION
#define RTS_PIN 25
#else
#define RTS_PIN 27
#endif
#define TXD_PIN     33
#define RXD_PIN     26
#define CTS_PIN     2
#define RX_BUF_SIZE 1024
#define TX_BUF_SIZE 1024

#define EXCEPTION_LEN 3
enum modbus_function_t { READ_HOLDING = 3, READ_INPUT };
typedef union {
    uint32_t doubleword;
    struct {
        uint16_t wordL;
        uint16_t wordH;
    } word;

} WORD_VAL;

extern uint8_t NODE_ID;
extern int IMPULSE_CONVERSION;

void uart_init(QueueHandle_t *queue) {
    uart_driver_delete(UART_NUM_1);
    int uart_baudarate = 9600;
    ESP_LOGI(TAG, "Baudarate is %d", uart_baudarate);
    const uart_config_t uart_config = {
        .baud_rate = uart_baudarate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, RTS_PIN, CTS_PIN);
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, TX_BUF_SIZE * 2, 20, queue,
                        0);
    uart_set_mode(UART_NUM_1, UART_MODE_RS485_HALF_DUPLEX);
}

void modbus_slave_functions(const uint8_t *frame, uint8_t length,
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
            ESP_LOGI(TAG,
                     "Register read"); // BOGUS without this log crc is missing
            if (uart_write_bytes(UART_NUM_1, (const char *)response_frame,
                                 response_len) == ESP_FAIL) {
                ESP_LOGE(TAG, "Error writig UART data");
                free(response_frame);
                break;
            }
            for (int i = 0; i < response_len; i++)
                printf("tx[%d]: %x\n", i, response_frame[i]);
            free(response_frame);
            break;

        default:
            response_frame[0] = NODE_ID;
            response_frame[1] = frame[1] + 0x80;
            response_frame[0] = 0x01;
            response_len      = EXCEPTION_LEN;
            CRC.Val           = CRC16(response_frame, response_len);
            response_frame[response_len++] = CRC.byte.LB;
            response_frame[response_len++] = CRC.byte.HB;
            if (uart_write_bytes(UART_NUM_1, (const char *)response_frame,
                                 response_len) == ESP_FAIL) {
                ESP_LOGE(TAG, "Error writig UART data");
                free(response_frame);
                break;
            }
            ESP_LOGE(TAG, "Invalid function response");
            free(response_frame);
            break;
        }
    } else
        crc_error_response(frame);
}
void register_save(uint32_t value, uint16_t *modbus_register) {
    WORD_VAL aux_register;
    aux_register.doubleword      = normalize_pulses(value);
    modbus_register[NODE_ID]     = aux_register.word.wordH;
    modbus_register[NODE_ID + 1] = aux_register.word.wordL;
}
void crc_error_response(const uint8_t *frame) {
    uint8_t *response_frame = (uint8_t *)malloc(255);
    uint8_t response_len    = 0;
    INT_VAL CRC;

    response_frame[0]              = NODE_ID;
    response_frame[1]              = frame[1] + 0x80;
    response_frame[2]              = 0x08;
    response_len                   = EXCEPTION_LEN;
    CRC.Val                        = CRC16(response_frame, response_len);
    response_frame[response_len++] = CRC.byte.LB;
    response_frame[response_len++] = CRC.byte.HB;
    if (uart_write_bytes(UART_NUM_1, (const char *)response_frame,
                         response_len) == ESP_FAIL) {
        ESP_LOGE(TAG, "Error writig UART data");
        free(response_frame);
        return;
    }
    ESP_LOGI(TAG, "CRC error response");
    free(response_frame);
}

uint32_t normalize_pulses(uint32_t value) {
    uint32_t normalized_value;
    double value_kWh = (double)value / (double)IMPULSE_CONVERSION;
    normalized_value = round(value_kWh / 0.01);
    return normalized_value;
}
