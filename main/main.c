/*
 * Author: Adrian Vazquez
 * Date: 10-07-2020
 */

#include "driver/gpio.h"
#include "esp_log.h"
#include "flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "modbus_slave.h"
#include <stdio.h>

#define PULSES_KW 225
char *TAG = "INFO";

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
    uart_init();
    while (1) {
        ESP_LOGI(TAG, "Modbus slave active");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
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
    xTaskCreatePinnedToCore(task_modbus_slave, "task_modbus_slave", 1024 * 2,
                            NULL, 10, NULL, 1);
#endif
}
