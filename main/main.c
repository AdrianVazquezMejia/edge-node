/*
 * Author: Adrian Vazquez
 * Date: 10-07-2020
 */

#include "CRC.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_rf1276.h"
#include "esp_task_wdt.h"
#include "flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "modbus_master.h"
#include "modbus_slave.h"
#include "rf1276.h"
#include "strings.h"
#include <stdio.h>
#define PULSES_KW 225

#define RX_BUF_SIZE 1024
#define TX_BUF_SIZE 1024

#define ID 255

#define TIME_SCAN  2000
#define MAX_SLAVES 255

#define TWDT_TIMEOUT_S 10

#define CHECK_ERROR_CODE(returned, expected)                                   \
    ({                                                                         \
        if (returned != expected) {                                            \
            printf("TWDT ERROR\n");                                            \
            abort();                                                           \
        }                                                                      \
    })
static char *TAG = "INFO";

void task_pulse(void *arg) {
    CHECK_ERROR_CODE(esp_task_wdt_add(NULL), ESP_OK);
    CHECK_ERROR_CODE(esp_task_wdt_status(NULL), ESP_OK);
    ESP_LOGI(TAG, "Pulse counter task started");
    gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT);
    int pinLevel;
    uint32_t pulses = 0;
    bool counted    = false;
    flash_get(&pulses);
    while (1) {
        pinLevel = gpio_get_level(GPIO_NUM_0);
        if (pinLevel == 1 && counted == false) {
            pulses++;
            flash_save(pulses);
            counted = true;
            ESP_LOGI(TAG, "Pulse number %d", pulses);
        }
        if (pinLevel == 0 && counted == true) {
            counted = false;
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
        CHECK_ERROR_CODE(esp_task_wdt_reset(), ESP_OK);
    }
}
void task_modbus_slave(void *arg) {
    CHECK_ERROR_CODE(esp_task_wdt_add(NULL), ESP_OK);
    CHECK_ERROR_CODE(esp_task_wdt_status(NULL), ESP_OK);
    QueueHandle_t uart_queue;
    uart_event_t event;
    uint8_t *dtmp = (uint8_t *)malloc(RX_BUF_SIZE);
    uint16_t *modbus_registers[4];
    uint16_t inputRegister[512] = {0};
    modbus_registers[1]         = &inputRegister[0];
    inputRegister[0]            = 0x2324;
    inputRegister[1]            = 0x2526;
    uart_init(&uart_queue);
    while (xQueueReceive(uart_queue, (void *)&event,
                         (portTickType)portMAX_DELAY)) {
        bzero(dtmp, RX_BUF_SIZE);

        switch (event.type) {
        case UART_DATA:
            uart_read_bytes(UART_NUM_1, dtmp, event.size, portMAX_DELAY);
            ESP_LOGI(TAG, "Received data is:");
            for (int i = 0; i < event.size; i++) {
                printf("%x ", dtmp[i]);
            }
            printf("\n");
            if (CRC16(dtmp, event.size) == 0) {
                ESP_LOGI(TAG, "Modbus frame verified");
                if (dtmp[0] == ID) {
                    ESP_LOGI(TAG, "Frame to this slave");
                    modbus_slave_functions(dtmp, event.size, modbus_registers);
                }
            } else
                ESP_LOGI(TAG, "Frame not verified: %d",
                         CRC16(dtmp, event.size));

            break;
        default:
            ESP_LOGI(TAG, "UART event");
        }
        CHECK_ERROR_CODE(esp_task_wdt_reset(), ESP_OK);
    }
    free(dtmp);
    dtmp = NULL;
}

void task_modbus_master(void *arg) {
    CHECK_ERROR_CODE(esp_task_wdt_add(NULL), ESP_OK);
    CHECK_ERROR_CODE(esp_task_wdt_status(NULL), ESP_OK);
    QueueHandle_t uart_queue;
    uart_event_t event;
    uint8_t *dtmp = (uint8_t *)malloc(RX_BUF_SIZE);
    uint16_t *modbus_registers[4];
    uint16_t inputRegister[512] = {0};
    modbus_registers[1]         = &inputRegister[0];
    uart_init(&uart_queue);
    const uint16_t start_add    = 0x0000;
    uint16_t quantity           = 2;
    bool slaves[MAX_SLAVES + 1] = {false};
    slaves[1]                   = true;
    slaves[255]                 = true;
    uint8_t curr_slave          = 0;
    while (1) {
        if (slaves[curr_slave]) {
            read_input_register(curr_slave, start_add, quantity);

            if (xQueuePeek(uart_queue, (void *)&event, (portTickType)100)) {
                ESP_LOGI(TAG, "Cleaned");
                if (event.type == UART_BREAK) {
                    xQueueReset(uart_queue);
                }
            }
            if (xQueueReceive(uart_queue, (void *)&event, (portTickType)100)) {
                switch (event.type) {
                case UART_DATA:
                    uart_read_bytes(UART_NUM_1, dtmp, event.size,
                                    portMAX_DELAY);
                    ESP_LOGI(TAG, "Received data is:");
                    for (int i = 0; i < event.size; i++) {
                        printf("%x ", dtmp[i]);
                    }
                    printf("\n");
                    if (CRC16(dtmp, event.size) == 0) {
                        ESP_LOGI(TAG, "Modbus frame verified");
                        save_register(dtmp, event.size, modbus_registers);
                    } else
                        ESP_LOGI(TAG, "Frame not verified: %d",
                                 CRC16(dtmp, event.size));
                    break;
                default:
                    ESP_LOGI(TAG, "uart event type: %d", event.type);
                    break;
                }
            } else
                ESP_LOGI(TAG, "Timeout");
            vTaskDelay(TIME_SCAN / portTICK_PERIOD_MS);
        }
        curr_slave++;
        CHECK_ERROR_CODE(esp_task_wdt_reset(), ESP_OK);
    }
}
static void task_lora(void *arg) {
    ESP_LOGI(TAG, "Task LoRa initialized");
    QueueHandle_t lora_queue;
    const struct config_uart config = {
        .uart_tx = 25, .uart_rx = 14, .baud_rate = 9600};

    const config_rf1276_t config_mesh = {.baud_rate    = 9600,
                                         .network_id   = 1,
                                         .node_id      = 2,
                                         .power        = 7,
                                         .routing_time = 1,
                                         .freq         = 433.0,
                                         .port_check   = 0};

    lora_queue = xQueueCreate(1, 128);

    start_lora_mesh(config, config_mesh, &lora_queue);
    while (1) {
        ESP_LOGI(TAG, "Sending data...");
        struct send_data_struct data = {.node_id      = 1,
                                        .power        = 7,
                                        .data         = {0x10, 0x20, 0x30},
                                        .tamano       = 3,
                                        .routing_type = 1};

        send_data_esp_rf1276(&data);

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
void app_main() {
    ESP_LOGI(TAG, "MCU initialized");
    ESP_LOGI(TAG, "Init Watchdog");
    CHECK_ERROR_CODE(esp_task_wdt_init(TWDT_TIMEOUT_S, true), ESP_OK);
#ifdef CONFIG_PULSE_PERIPHERAL
    ESP_LOGI(TAG, "Start peripheral");
    xTaskCreatePinnedToCore(task_pulse, "task_pulse", 1024 * 2, NULL, 10, NULL,
                            0);
#endif

#ifdef CONFIG_SLAVE_MODBUS
    ESP_LOGI(TAG, "Start Modbus slave task");
    xTaskCreatePinnedToCore(task_modbus_slave, "task_modbus_slave", 2048 * 2,
                            NULL, 10, NULL, 1);
#endif

#ifdef CONFIG_MASTER_MODBUS
    ESP_LOGI(TAG, "Start Modbus master task");
    xTaskCreatePinnedToCore(task_modbus_master, "task_modbus_master", 2048 * 2,
                            NULL, 10, NULL, 1);
#endif
#ifdef CONFIG_LORA
    ESP_LOGI(TAG, "Start Modbus master task");
    xTaskCreatePinnedToCore(task_lora, "task_lora", 2048 * 4, NULL, 10, NULL,
                            1);
#endif
}
