/*
 * modbus_slave.c
 *
 *  Created on: Oct 13, 2020
 *      Author: adrian-estelio
 */
#include "modbus_slave.h"
#include "driver/uart.h"
#include "esp_log.h"
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
    ESP_LOGI(TAG, "El baudarate is %d", uart_baudarate);
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
