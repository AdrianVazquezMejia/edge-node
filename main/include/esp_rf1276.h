#ifndef ESP_RF1276
#define ESP_RF1276

#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "rf1276.h"
#include <esp_err.h>
#include <string.h>

EventGroupHandle_t eventos_rf1276;

#define BUF_SIZE    (1024)
#define RD_BUF_SIZE (BUF_SIZE)
#define UART_RF1276 UART_NUM_2

struct config_uart {
    uint8_t uart_tx;
    uint8_t uart_rx;
    int baud_rate;
};

int write_config_esp_rf1276(struct config_rf1276 *config);

int read_config_esp_rf1276(struct config_rf1276 *result_read_config);

int reset_esp_rf1276(void);

int send_data_esp_rf1276(struct send_data_struct *data);

int read_version_esp_rf1276(void);

int read_routing_path_esp_rf1276(struct repeaters *repetidores);

// En desarrollo
int read_routing_table_esp_rf1276(struct routing *rutas);

esp_err_t start_lora_mesh(struct config_uart config_uart,
                          struct config_rf1276 config_mesh,
                          QueueHandle_t *cola);

#endif
