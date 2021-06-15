/*
 * config_rtu.c
 *
 *  Created on: Dec 13, 2020
 *      Author: adrian-estelio
 */
#include "config_rtu.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "global_variables.h"
#include "math.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "stdint.h"
#include <stdio.h>
#include <string.h>
#define DEFAULT_NODE_ID 255
#define RESET_WDT       25000
#define EX_UART_NUM     UART_NUM_0
#define BUF_SIZE        (1024)
#define RD_BUF_SIZE     (BUF_SIZE)

static QueueHandle_t uart0_queue;
static char *TAG = "CONFIG";
uint8_t NODE_ID;

#ifdef CONFIG_PULSE_PERIPHERAL
uint32_t INITIAL_ENERGY;
int IMPULSE_CONVERSION;
uint32_t AUX;
#endif

#ifdef CONFIG_MASTER_MODBUS
uint8_t SLAVES;
#endif

static void config_save_flash(void) {

    nvs_handle_t my_handle;

    nvs_flash_init();
    nvs_open("storage", NVS_READWRITE, &my_handle);

    nvs_set_u8(my_handle, "NODE_ID", NODE_ID);

#ifdef CONFIG_MASTER_MODBUS
    nvs_set_u8(my_handle, "SLAVES", SLAVES);
#endif

#ifdef CONFIG_PULSE_PERIPHERAL
    nvs_set_i32(my_handle, "IMPULSE_K", IMPULSE_CONVERSION);
    if (AUX != INITIAL_ENERGY)
        nvs_set_u32(my_handle, "INITIAL_E", AUX);
#endif

    nvs_commit(my_handle);
    nvs_close(my_handle);
    nvs_flash_deinit();
#ifdef CONFIG_PULSE_PERIPHERAL
    esp_err_t err;
    if (AUX != INITIAL_ENERGY) {
        INITIAL_ENERGY = AUX;
        uint32_t initial_pulses =
            round(((float)INITIAL_ENERGY / 100 * (float)IMPULSE_CONVERSION));
        err = put_nvs(initial_pulses, &pulse_address);
        if (err != ESP_OK)
            ESP_LOGE(TAG, "FLASH ERROR");
    }
#endif
    esp_restart();
}
static void uart_event_task(void *pvParameters) {

    ESP_LOGI(TAG, "Start config task");
    uart_event_t event;
    uint8_t *dtmp             = (uint8_t *)malloc(RD_BUF_SIZE);
    char delim[]              = " ";
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

    for (;;) {

        if (xQueueReceive(uart0_queue, (void *)&event,
                          pdMS_TO_TICKS(RESET_WDT))) {
            bzero(dtmp, RD_BUF_SIZE);
            switch (event.type) {
            case UART_DATA:
                ESP_LOGI(TAG, "COMMAD LINE:");
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

#ifdef CONFIG_PULSE_PERIPHERAL

                    if (strcmp(ptr, "-pulse") == 0) {
                        ptr                = strtok(NULL, delim);
                        IMPULSE_CONVERSION = atoi(ptr);
                        ESP_LOGI(TAG, "IMPULSE CONVERSION >>> %d",
                                 IMPULSE_CONVERSION);
                    }
                    if (strcmp(ptr, "-energy") == 0) {
                        ptr = strtok(NULL, delim);
                        AUX = (uint32_t)atoi(ptr);
                        ESP_LOGI(TAG, "INITIAL ENERGY % d", AUX);
                    }
#endif
#ifdef CONFIG_MASTER_MODBUS

                    if (strcmp(ptr, "-slaves") == 0) {
                        ptr    = strtok(NULL, delim);
                        SLAVES = (uint8_t)atoi(ptr);
                        ESP_LOGI(TAG, "SLAVES >>> % d", SLAVES);
                    }
#endif

                    if (strcmp(ptr, "-save") == 0) {
                        ESP_LOGI(TAG, "Saving config to flash");
                        config_save_flash();
                    }

                    ptr = strtok(NULL, delim);
                }
                break;
            default:
                ESP_LOGI(TAG, "ERROR COMMAND LINE: %d", event.type);
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
    ESP_LOGW(TAG, "NODE ID >>> %d", NODE_ID);
#ifdef CONFIG_MASTER_MODBUS
    if (nvs_get_u8(my_handle, "SLAVES", &SLAVES) == ESP_ERR_NVS_NOT_FOUND) {
        nvs_set_u8(my_handle, "SLAVES", SLAVES);
    }
    ESP_LOGW(TAG, "SLAVES >>> % d", SLAVES);
#endif
#ifdef CONFIG_PULSE_PERIPHERAL
    if (nvs_get_i32(my_handle, "IMPULSE_K", &IMPULSE_CONVERSION) ==
        ESP_ERR_NVS_NOT_FOUND) {
        IMPULSE_CONVERSION = 1;
        nvs_set_i32(my_handle, "IMPULSE_K", IMPULSE_CONVERSION);
    }
    ESP_LOGW(TAG, "IMPULSE CONVERSION >>> % d", IMPULSE_CONVERSION);

    if (nvs_get_u32(my_handle, "INITIAL_E", &INITIAL_ENERGY) ==
        ESP_ERR_NVS_NOT_FOUND) {
        INITIAL_ENERGY = 1;
        nvs_set_u32(my_handle, "INITIAL_E", INITIAL_ENERGY);
        ESP_LOGE(TAG, "IMPULSE CONVERSION NOT FOUND");
    }
    ESP_LOGW(TAG, "INITIAL_ENERGY >>> % d", INITIAL_ENERGY);

#endif

    nvs_commit(my_handle);
    nvs_close(my_handle);
    nvs_flash_deinit();

    xTaskCreate(uart_event_task, "uart_event_task", 2048, NULL,
                tskIDLE_PRIORITY, NULL);
}
