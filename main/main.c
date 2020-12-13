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
#include "modbus_lora.h"
#include "modbus_master.h"
#include "modbus_slave.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "ota_update.h"
#include "rf1276.h"
#include "strings.h"
#include <stdio.h>

#define PULSES_KW 225

#define RX_BUF_SIZE 1024
#define TX_BUF_SIZE 1024

#define TIME_SCAN  2000
#define MAX_SLAVES 255

#define TWDT_TIMEOUT_S 10
#define MODBUS_TIMEOUT 100 // in ticks == 1 s

#define PULSE_GPIO 35

#define CHECK_ERROR_CODE(returned, expected)                                   \
    ({                                                                         \
        if (returned != expected) {                                            \
            printf("TWDT ERROR\n");                                            \
            abort();                                                           \
        }                                                                      \
    })
static char *TAG      = "INFO";
static char *TAG_UART = "MODBUS";
static uint16_t *modbus_registers[4];
static uint16_t inputRegister[512] = {0};

void task_pulse(void *arg) {
    CHECK_ERROR_CODE(esp_task_wdt_add(NULL), ESP_OK);
    CHECK_ERROR_CODE(esp_task_wdt_status(NULL), ESP_OK);
    ESP_LOGI(TAG, "Pulse counter task started");
    gpio_set_direction(PULSE_GPIO, GPIO_MODE_INPUT);
    int pinLevel;
    uint32_t pulses = 0;
    bool counted    = false;
    esp_err_t err;

    nvs_address_t pulse_address;
    err = get_initial_pulse(&pulses, &pulse_address);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "FLASH ERROR");
        vTaskDelete(NULL);
    }

    while (1) {
        pinLevel = gpio_get_level(PULSE_GPIO);
        if (pinLevel == 1 && counted == false) {
            pulses++;
            flash_save(pulses);
            err = put_nvs(pulses, &pulse_address);
            if (err != ESP_OK)
                ESP_LOGE(TAG, "FLASH ERROR");
            register_save(pulses, inputRegister);
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
    QueueHandle_t uart_queue;
    uart_event_t event;
    uint8_t *dtmp       = (uint8_t *)malloc(RX_BUF_SIZE);
    modbus_registers[1] = &inputRegister[0];
    uart_init(&uart_queue);
    while (xQueueReceive(uart_queue, (void *)&event,
                         (portTickType)portMAX_DELAY)) {
        bzero(dtmp, RX_BUF_SIZE);
        switch (event.type) {
        case UART_DATA:
            if (uart_read_bytes(UART_NUM_1, dtmp, event.size, portMAX_DELAY) ==
                ESP_FAIL) {
                ESP_LOGE(TAG_UART, "Error while reading UART data");
                break;
            }
            ESP_LOGI(TAG_UART, "Received data is:");
            for (int i = 0; i < event.size; i++) {
                printf("%x ", dtmp[i]);
            }
            printf("\n");
            if (CRC16(dtmp, event.size) == 0) {
                ESP_LOGI(TAG_UART, "Modbus frame verified");
                if (dtmp[0] == CONFIG_ID) {
                    ESP_LOGI(TAG, "Frame to this slave");
                    modbus_slave_functions(dtmp, event.size, modbus_registers);
                }
            } else {
                ESP_LOGE(TAG_UART, " CRC ERROR: %d", CRC16(dtmp, event.size));
                crc_error_response(dtmp);
            }
            break;
        default:
            ESP_LOGE(TAG_UART, "UART event %d", event.type);
        }
    }
    free(dtmp);
    dtmp = NULL;
}

void task_modbus_master(void *arg) {
    CHECK_ERROR_CODE(esp_task_wdt_add(NULL), ESP_OK);
    CHECK_ERROR_CODE(esp_task_wdt_status(NULL), ESP_OK);
    QueueHandle_t uart_queue;
    uart_event_t event;
    uint8_t *dtmp       = (uint8_t *)malloc(RX_BUF_SIZE);
    modbus_registers[1] = &inputRegister[0];
    uart_init(&uart_queue);
    uint16_t quantity = 2;
    bool slaves[MAX_SLAVES + 1];
    init_slaves(slaves);
    uint8_t curr_slave = 1;
    if (CONFIG_SLAVES == 0) {
        CHECK_ERROR_CODE(esp_task_wdt_delete(NULL), ESP_OK);
        vTaskDelete(NULL);
    }
    while (1) {
        read_input_register(curr_slave, (uint16_t)curr_slave, quantity);

        if (xQueuePeek(uart_queue, (void *)&event, (portTickType)10)) {
            if (event.type == UART_BREAK) {
                ESP_LOGI(TAG, "Cleaned");
                xQueueReset(uart_queue);
            }
        }

        if (xQueueReceive(uart_queue, (void *)&event,
                          (portTickType)MODBUS_TIMEOUT)) {
            switch (event.type) {
            case UART_DATA:
                if (uart_read_bytes(UART_NUM_1, dtmp, event.size,
                                    (portTickType)MODBUS_TIMEOUT) == ESP_FAIL) {
                    ESP_LOGE(TAG_UART, "Error while reading UART data");
                    break;
                }
                ESP_LOGI(TAG, "Received data is:");
                for (int i = 0; i < event.size; i++) {
                    printf("%x ", dtmp[i]);
                }
                printf("\n");
                if (CRC16(dtmp, event.size) == 0) {
                    ESP_LOGI(TAG, "Modbus frame verified");
                    check_exceptions(dtmp);
                    save_register(dtmp, event.size, modbus_registers);
                } else {
                    ESP_LOGI(TAG, "Frame not verified: %d",
                             CRC16(dtmp, event.size));
                    crc_error_response(dtmp);
                }
                break;
            default:
                ESP_LOGE(TAG, "uart event type: %d", event.type);
                break;
            }
        } else {
            ESP_LOGI(TAG, "Timeout");
            vTaskDelay(TIME_SCAN / portTICK_PERIOD_MS);
        }
        curr_slave++;
        if (curr_slave > CONFIG_SLAVES)
            curr_slave = 1;
        CHECK_ERROR_CODE(esp_task_wdt_reset(), ESP_OK);
    }
}
void task_lora(void *arg) {
    ESP_LOGI(TAG, "Task LoRa initialized");
    QueueHandle_t lora_queue;
    uart_lora_t config = {.uart_tx = 14, .uart_rx = 15, .baud_rate = 9600};

    config_rf1276_t config_mesh = {.baud_rate    = 9600,
                                   .network_id   = 1,
                                   .node_id      = CONFIG_ID,
                                   .power        = 7,
                                   .routing_time = 1,
                                   .freq         = 433.0,
                                   .port_check   = 0};

    lora_queue = xQueueCreate(1, 128);
    struct trama_receive trama;
    start_lora_mesh(config, config_mesh, &lora_queue);
    uint16_t node_origen;
    modbus_registers[1] = &inputRegister[0];
    mb_response_t modbus_response;
    struct send_data_struct data = {
        .node_id = 1, .power = 7, .data = {0}, .tamano = 1, .routing_type = 1};

    while (xQueueReceive(lora_queue, &trama.trama_rx,
                         (portTickType)portMAX_DELAY)) {
        ESP_LOGI(TAG, "Receiving data...");
        if (receive_packet_rf1276(&trama) == 0) {
            node_origen = trama.respond.source_node.valor;
            printf("nodo origen es: %u\n", node_origen);
            modbus_response = modbus_lora_functions(trama.respond.data,
                                                    trama.respond.data_length,
                                                    modbus_registers);
            memcpy(data.data, modbus_response.frame, modbus_response.len);
            data.tamano  = modbus_response.len;
            data.node_id = node_origen;
            send_data_esp_rf1276(&data);
        }
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
#ifndef CONFIG_MASTER_MODBUS
    ESP_LOGI(TAG, "Start Modbus slave task");
    xTaskCreatePinnedToCore(task_modbus_slave, "task_modbus_slave", 2048 * 2,
                            NULL, 10, NULL, 1);
#endif
#endif

#ifdef CONFIG_MASTER_MODBUS
    ESP_LOGI(TAG, "Start Modbus master task");
    xTaskCreatePinnedToCore(task_modbus_master, "task_modbus_master", 2048 * 2,
                            NULL, 10, NULL, 1);
#endif
#ifdef CONFIG_LORA
    ESP_LOGI(TAG, "Start LoRa task");
    xTaskCreatePinnedToCore(task_lora, "task_lora", 2048 * 4, NULL, 10, NULL,
                            1);
#endif

#ifdef CONFIG_OTA
    ESP_LOGI(TAG, "Start OTA task");
    xTaskCreatePinnedToCore(task_ota, "task_ota", 2048 * 2, NULL, 10, NULL, 1);
#endif
}
