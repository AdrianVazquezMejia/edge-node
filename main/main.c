/*
 * Author: Adrian Vazquez
 * Date: 10-07-2020
 */

#include "CRC.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "modbus_slave.h"
#include "strings.h"
#include <stdio.h>

#define PULSES_KW 225

#define RX_BUF_SIZE 1024
#define TX_BUF_SIZE 1024

#define ID 255

static char *TAG = "INFO";

void task_pulse(void *arg) {
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
    }
}
void task_modbus_slave(void *arg) {
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
    }
    free(dtmp);
    dtmp = NULL;
}
void app_main() {
    ESP_LOGI(TAG, "MCU initialized");
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
}
