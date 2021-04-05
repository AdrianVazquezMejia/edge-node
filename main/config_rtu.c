/*
 * config_rtu.c
 *
 *  Created on: Dec 13, 2020
 *      Author: adrian-estelio
 */
#include "config_rtu.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "math.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "stdint.h"
#include <stdio.h>
#include <string.h>
static char *TAG = "CONFIG";
#define EX_UART_NUM     UART_NUM_0
#define PATTERN_CHR_NUM (3)
#define DEFAULT_NODE_ID 255

#define EX_UART_NUM UART_NUM_0

#define BUF_SIZE    (1024)
#define RD_BUF_SIZE (BUF_SIZE)
static QueueHandle_t uart0_queue;
uint32_t INITIAL_ENERGY;
extern uint8_t NODE_ID;
extern nvs_address_t pulse_address;

static void config_save_flash(void) {

    nvs_handle_t my_handle;
    esp_err_t err;

    nvs_flash_init();
    nvs_open("storage", NVS_READWRITE, &my_handle);

    nvs_set_u8(my_handle, "NODE_ID", NODE_ID);

#ifdef CONFIG_MASTER_MODBUS
    extern uint8_t SLAVES;
    SLAVES = CONFIG_SLAVES;
    nvs_set_u8(my_handle, "SLAVES", SLAVES);
#endif
#ifdef CONFIG_PULSE_PERIPHERAL
    extern int IMPULSE_CONVERSION;
    IMPULSE_CONVERSION = CONFIG_IMPULSE_CONVERSION;
    nvs_set_i32(my_handle, "IMPULSE_CONVERSION", IMPULSE_CONVERSION);
#endif
    nvs_commit(my_handle);
    nvs_close(my_handle);
    nvs_flash_deinit();

    uint32_t initial_pulses =
        round(((float)INITIAL_ENERGY / 100 * (float)IMPULSE_CONVERSION));
    err = put_nvs(initial_pulses, &pulse_address);
    if (err != ESP_OK)
        ESP_LOGE(TAG, "FLASH ERROR");

    esp_restart();
}
static void uart_event_task(void *pvParameters) {
    uart_event_t event;
    uint8_t *dtmp = (uint8_t *)malloc(RD_BUF_SIZE);
    char delim[]  = " ";
    for (;;) {
        if (xQueueReceive(uart0_queue, (void *)&event,
                          (portTickType)portMAX_DELAY)) {
            bzero(dtmp, RD_BUF_SIZE);
            ESP_LOGI(TAG, "UART[%d] EVENT:", EX_UART_NUM);
            switch (event.type) {
            case UART_DATA:
                uart_read_bytes(EX_UART_NUM, dtmp, event.size, portMAX_DELAY);
                dtmp[event.size] = 0x0a;
                uart_write_bytes(EX_UART_NUM, (const char *)dtmp,
                                 event.size + 1);
                char *ptr = strtok((char *)dtmp, delim);

                while (ptr != NULL) {
                    if (strcmp(ptr, "-id") == 0) {
                        ptr     = strtok(NULL, delim);
                        NODE_ID = (uint8_t)atoi(ptr);
                        ESP_LOGI(TAG, "NODE ID >>> %d", NODE_ID);
                    }

                    if (strcmp(ptr, "-save") == 0) {
                        ESP_LOGI(TAG, "Saving config to flash");
                        config_save_flash();
                    }

#ifdef CONFIG_PULSE_PERIPHERAL
                    if (strcmp(ptr, "-pulse") == 0) {
                        extern int IMPULSE_CONVERSION;
                        ptr                = strtok(NULL, delim);
                        IMPULSE_CONVERSION = atoi(ptr);
                        ESP_LOGI(TAG, "IMPULSE CONVERSION >>> % d",
                                 IMPULSE_CONVERSION);
                    }
#endif
#ifdef CONFIG_MASTER_MODBUS
                    if (strcmp(ptr, "-slaves") == 0) {
                        extern uint8_t SLAVES;
                        ptr    = strtok(NULL, delim);
                        SLAVES = (uint8_t)atoi(ptr);
                        ESP_LOGI(TAG, "SLAVES >>> % d", SLAVES);
                    }
                    if (strcmp(ptr, "-energy") == 0) {
                        ptr            = strtok(NULL, delim);
                        INITIAL_ENERGY = (uint32_t)atoi(ptr);
                        ESP_LOGI(TAG, "INITIAL ENERGY % d", INITIAL_ENERGY);
                    }
#endif
                    ptr = strtok(NULL, delim);
                }
                ESP_LOGI(TAG, "Finish config");

                break;
            default:
                ESP_LOGI(TAG, "uart event type: %d", event.type);
                break;
            }
        }
    }
    free(dtmp);
    dtmp = NULL;
    vTaskDelete(NULL);
}
void check_rtu_config(void) {

    nvs_handle_t my_handle;
    nvs_flash_init();
    nvs_open("storage", NVS_READWRITE, &my_handle);

    if (nvs_get_u8(my_handle, "NODE_ID", &NODE_ID) == ESP_ERR_NVS_NOT_FOUND) {
        NODE_ID = DEFAULT_NODE_ID;
        nvs_set_u8(my_handle, "NODE_ID", NODE_ID);
    }
    ESP_LOGI(TAG, "NODE ID >>> %d", NODE_ID);
#ifdef CONFIG_MASTER_MODBUS
    if (nvs_get_u8(my_handle, "SLAVES", &SLAVES) == ESP_ERR_NVS_NOT_FOUND) {
        nvs_set_u8(my_handle, "SLAVES", SLAVES);
    }
    ESP_LOGI(TAG, "SLAVES >>> % d", SLAVES);
#endif
#ifdef CONFIG_PULSE_PERIPHERAL
    IMPULSE_CONVERSION = CONFIG_IMPULSE_CONVERSION;
    if (nvs_get_i32(my_handle, "IMPULSE_CONVERSION", &IMPULSE_CONVERSION) ==
        ESP_ERR_NVS_NOT_FOUND) {
        nvs_set_i32(my_handle, "IMPULSE_CONVERSION", IMPULSE_CONVERSION);
    }
    ESP_LOGI(TAG, "IMPULSE CONVERSION >>> % d", IMPULSE_CONVERSION);
#endif

    nvs_commit(my_handle);
    nvs_close(my_handle);
    nvs_flash_deinit();

    ESP_LOGI(TAG, "Start config task");
    uart_config_t uart_config = {.baud_rate = 115200,
                                 .data_bits = UART_DATA_8_BITS,
                                 .parity    = UART_PARITY_DISABLE,
                                 .stop_bits = UART_STOP_BITS_1,
                                 .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
    uart_param_config(EX_UART_NUM, &uart_config);

    esp_log_level_set(TAG, ESP_LOG_INFO);
    uart_set_pin(EX_UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE,
                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(EX_UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 20,
                        &uart0_queue, 0);

    xTaskCreate(uart_event_task, "uart_event_task", 2048, NULL, 12, NULL);
}
