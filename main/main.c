/*
 * Author: Adrian Vazquez
 * Date: 10-07-2020
 */

#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

#define PULSE_PERIPHERAL
char *TAG = "INFO";

void task_pulse(void *arg) {
    ESP_LOGI(TAG, "Pulse counter task started");
    gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT);
    int pinLevel;
    int pulses   = 0;
    bool counted = false;

    while (1) {
        pinLevel = gpio_get_level(GPIO_NUM_0);
        if (pinLevel == 1 && counted == false) {
            pulses++;
            counted = true;
            ESP_LOGI(TAG, "PULSE!... level is: %d", pinLevel);
        }
        if (pinLevel == 0 && counted == true) {
            counted = false;
            ESP_LOGI(TAG, "level is: %d", pinLevel);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void app_main() {
    ESP_LOGI(TAG, "MCU initialized");

#ifdef PULSE_PERIPHERAL
    ESP_LOGI(TAG, "Start peripheral");
    xTaskCreatePinnedToCore(task_pulse, "task_pulse", 1024 * 2, NULL, 10, NULL,
                            0);
#endif
}
