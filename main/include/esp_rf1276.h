#ifndef ESP_RF1276
#define ESP_RF1276
#include "driver/uart.h"
#include "freertos/queue.h"
#include "stdint.h"
#include <esp_err.h>

#define BUF_SIZE    (1024)
#define RD_BUF_SIZE (BUF_SIZE)
#define UART_RF1276 UART_NUM_2
//#define DEBUG_LORA

/**
 * @brief Estructura de configuración del UART
 */

typedef struct uart_lora {
    uint8_t uart_tx;
    uint8_t uart_rx;
    int baud_rate;
    uart_port_t uart_num;
} uart_lora_t;

typedef struct config_rf1276 {
    uint32_t baud_rate;
    uint16_t network_id;
    uint16_t node_id;
    uint8_t power;
    uint8_t routing_time;
    float freq;
    uint8_t port_check;
} config_rf1276_t;
esp_err_t start_lora_mesh(struct uart_lora config_uart,
                          struct config_rf1276 config_mesh,
                          QueueHandle_t *cola);
esp_err_t init_lora_uart(uart_lora_t *uartParameters);

esp_err_t init_lora_mesh(config_rf1276_t *loraParameters,
                         QueueHandle_t *loraQueue, uart_port_t uart_num);
typedef union {
    struct {
        uint8_t low_byte_;
        uint8_t high_byte_;
    };
    uint16_t word_;
} word_t;

typedef union {
    struct {
        word_t low_word_;
        word_t high_word_;
    };
    uint32_t doubleword_;
} doubleword_t;
typedef struct {
    word_t dest_address_;
    uint8_t ack_request_;
    uint8_t sending_radius_;
    uint8_t routing_type_;
    uint8_t data_len_;
    uint8_t data_[109];

} lora_rout_load_t;
typedef struct {
    word_t source_address_;
    uint8_t signal_;
    uint8_t data_len_;
    uint8_t data_[113];
} lora_recv_load_t;

typedef struct {

} lora_conf_load_t;
typedef struct {
    word_t dest_address_;
    uint8_t result;
} lora_local_resp_t;
// data request is remaining to implement
typedef union {
    lora_rout_load_t transmit_load_;
    lora_recv_load_t recv_load_;
    lora_local_resp_t local_resp_;
    lora_conf_load_t config_load_;
} lora_load_t;

typedef struct {
    uint8_t load_len_;
    uint8_t command_type_;
    uint8_t frame_number_;
    uint8_t frame_type_;
} lora_header_t;

typedef struct {
    uint8_t check_sum_;
    lora_load_t load_;
    lora_header_t header_;

} lora_mesh_t;
int lora_send(lora_mesh_t *sendFrame);

enum { INTERNAL_USE = 0x01, APPLICATION_DATA = 0x05 };
enum { ACK_SEND = 0x81, RECV_PACKAGE = 0x82 };
enum { SENDING_DATA = 0x01 };
#endif
